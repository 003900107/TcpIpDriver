#include "stdafx.h"
#include "IpServer.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverEquipment.h"
#include "TcpIpDriverFrame.h"


void TcpIpDriverEquipment::setConnectionContext(CConnectionContext *a_pConnectionContext)
{
	CConnectionContext* pOldConnectionContext = m_pConnectionContext;
	m_pConnectionContext = a_pConnectionContext;

	if(pOldConnectionContext != NULL)
	{  //There was a connection and need to be removed before linking the newone  
		m_pNet = (CipNetwork*)(m_pCwEqt->GetProtNetwork_Sync());
		
		if(pOldConnectionContext->m_socketConnection != INVALID_SOCKET)
			m_pNet->Close(*pOldConnectionContext);

		if(m_pNet->RemoveConnection(pOldConnectionContext))		//删除链接队列中的对象
		{
			CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* Reconnection success, old: 0x%X, new: 0x%X", 
				pOldConnectionContext, m_pConnectionContext);
			delete pOldConnectionContext;
		}
	}

	 m_iCountCommErrors = 0;
	 m_bReConnect = false;
 }
   
////////////////////////////////////////////////////////////////////////////////////////
_ProtRet TcpIpDriverEquipment::Start_Async(_ProtStartEqtCmd *pStartEqtCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT2, "%s::Start_Async", m_pCwEqt->GetGlobalName());
    m_pConnectionContext = NULL;
    m_ucAddress = (unsigned char) pStartEqtCmd->m_EqtAddress;
    m_usTransactionId = rand();
	m_iCountCommErrors = 0;  //init 170731 tyh
	m_bReConnect = false;
	
	CW_HANDLE huserAttributes;
	_CwDataItem *pdiAttributes;

	//You can get protocol user attributes (method similar on Network class with m_pCwNetwork and Frame with m_pCwFrame
	if (m_pCwEqt->OpenUserAttributes_Sync(&huserAttributes)==CW_OK)
	{
		//出错重试次数
		if (m_pCwEqt->GetUserAttributes_Sync(huserAttributes,0,CWIT_UI2,&pdiAttributes)==CW_OK)
		{
			m_iMaxCountCommError = pdiAttributes->CwValue.uiVal;
		}
		if(m_iMaxCountCommError == 0)
			m_iMaxCountCommError = 2;

		//帧延时
		if (m_pCwEqt->GetUserAttributes_Sync(huserAttributes,1,CWIT_UI2,&pdiAttributes)==CW_OK)
		{
			m_usDelay = pdiAttributes->CwValue.uiVal*1000;
		}
		if(m_usDelay == 0)
			m_usDelay = 5000;

		//启动延时
		if (m_pCwEqt->GetUserAttributes_Sync(huserAttributes,2,CWIT_UI2,&pdiAttributes)==CW_OK)	
		{
			m_usStartDelay = pdiAttributes->CwValue.uiVal*1000;
		}
		if(m_usStartDelay == 0)
			m_usStartDelay = (m_pCwEqt->GetAddress_Sync()%10)*1000;

		//Sleep(m_usStartDelay);		//启动延时

		m_pCwEqt->CloseUserAttributes_Sync(huserAttributes);
	}

	initLog(m_pCwEqt->GetGlobalName());

	pStartEqtCmd->Ack();
    return PR_CMD_PROCESSED;
}

_ProtRet TcpIpDriverEquipment::Stop_Async(_ProtStopEqtCmd *pStopEqtCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT2, "%s::Stop_Async ", m_pCwEqt->GetGlobalName());
    if (m_pConnectionContext!=NULL)
    {
        // Closing connection
        CloseConnectionContext();   
    }

	if (m_pLogger != NULL)
	{
		delete m_pLogger;
		m_pLogger = NULL;
	}

    pStopEqtCmd->Ack();
    return PR_CMD_PROCESSED;
}

_ProtFrame *TcpIpDriverEquipment::CreateProtFrame(CW_USHORT usProtocolDataType,CW_USHORT usCwDataType)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT3, "%s::CreateProtFrame ProtocolDataType=%d CwDataType=%d", m_pCwEqt->GetGlobalName(), usProtocolDataType, usCwDataType);

    // usCwDataType = ( CW_DATA_DWORD, CW_DATA_BIT, ...)
    // usProtocolDataType = ( DATA_TYPE_4, DATA_TYPE_5, ...)
    switch (usCwDataType)
    {
        case CW_DATA_BIT:
            switch (usProtocolDataType)
            {
                case 0:
                default:
                    return new TcpIpDriverBitFrame;
            }
            break;

        case CW_DATA_WORD:
            switch (usProtocolDataType)
            {
                case 0:
                default:
                    return new TcpIpDriverWordFrame;
            }
            break;

        case CW_DATA_DWORD:
            switch (usProtocolDataType)
            {
                case 0:
                default:
                    return new TcpIpDriverDWordFrame;
            }
            break;		
    }
    return NULL;
}


// Closing connection
void TcpIpDriverEquipment::CloseConnectionContext()
{
	CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* CloseConnectionContext ADDR = %d", this->m_ucAddress);

	EnterCriticalSection(&m_pConnectionContext->m_csConnectionState);

	m_pConnectionContext->m_ConnectionState = ConnectionClosed;
/*	
	int iResult;
	if((iResult=closesocket(m_pConnectionContext->m_socketConnection)) == SOCKET_ERROR)
	{
		iResult = WSAGetLastError();
		CWTRACE(PUS_PROTOC1, LVL_BIT2, "******* 关闭设备连接 error = %d!", iResult);
	}
*/
	if(m_pConnectionContext->m_socketConnection != INVALID_SOCKET)		//调用网络层接口，关闭网络链接
	{
		m_pNet->Close(*m_pConnectionContext);
		m_pConnectionContext->m_socketConnection = INVALID_SOCKET;
	}

	m_iCountCommErrors = 0;
	m_iCountCommLenghtErrors = 0;

	//TODO:Still remove from list at network level.
    //There was a connection and need to be removed before linking the newone  
     //CipNetwork *net=(CipNetwork*)(m_pCwEqt->GetProtNetwork_Sync());
     //net->RemoveConnection(m_pConnectionContext);
     //net->Close(*m_pConnectionContext);
	
    LeaveCriticalSection(&m_pConnectionContext->m_csConnectionState);
}

int TcpIpDriverEquipment::SendRcvFrame(
    CW_LPC_UCHAR a_pcBufferToSend,
    const USHORT a_usSendBufferSize,
    CW_LPC_UCHAR a_pucReceivedBuffer,
    const USHORT a_usSizeofResponseBuffer)
{

    m_usSizeofResponseBuffer = a_usSizeofResponseBuffer;
    m_iReceiveCount = 0;
    m_pcRcvBuffer = (CW_LP_UCHAR)a_pucReceivedBuffer;

    if(m_pConnectionContext==NULL)
        return CW_ERR_EQT_STOPPED;

    int iSendResult=0;
    int length=a_pcBufferToSend[5]+6;

	if(m_bReConnect)		//判断是否处于重新连接
	{
		CWTRACE(PUS_PROTOC1, LVL_BIT2, "~~~~~~~ 设备重连中，暂停发送 old_handle: 0x%X", m_pConnectionContext);
		return CW_ERR_EQT_STOPPED;
	}

	if(m_pConnectionContext->m_ConnectionState == ConnectionConnected)		//tyh 170731 增加发送前判断网络状态
	{
		iSendResult = send(m_pConnectionContext->m_socketConnection, 
			(const char*)a_pcBufferToSend, length, 0);
	}
	else
	{
		//return CW_ERR_CMD_NOT_PROCESSED;
		return CW_ERR_EQT_STOPPED;//CW_ERR_EQT_STOPPED;
	}

    if (iSendResult == SOCKET_ERROR) 
    {
       //error at sending request!
		if(m_iCountCommErrors >= m_iMaxCountCommError)
		{
			//closesocket(m_pConnectionContext->m_socketConnection);

			CloseConnectionContext();
			m_iCountCommErrors = 0;
			CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ 发送错误次数超限，中断连接");

			//return CW_ERR_CMD_NOT_PROCESSED;
			return CW_ERR_EQT_STOPPED;
		}
		else
		{
			m_iCountCommErrors++;
			return CW_ERR_CMD_NOT_PROCESSED;
		}
    }
	else
    {
        int iTimeout = m_usDelay;	//毫秒
        setsockopt(m_pConnectionContext->m_socketConnection,SOL_SOCKET,SO_RCVTIMEO,(char *)&iTimeout,sizeof(int));
        int iReceiveResult = recv(m_pConnectionContext->m_socketConnection, m_pConnectionContext->m_pcBuffer, 1024, 0);
            
        //if(m_pConnectionContext==NULL)
        //    return CW_ERR_CMD_NOT_PROCESSED;

		if(m_iCountCommLenghtErrors > m_iMaxCountCommError)	//数据长度出错超限
		{
			CloseConnectionContext();
			return CW_ERR_EQT_STOPPED;
		}



		if ((iReceiveResult == 0) || (iReceiveResult == SOCKET_ERROR))
		{
			if(m_iCountCommErrors >= m_iMaxCountCommError)		//增加超時发送次数
			{
				CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ 发送超时次数超限，中断连接");
				CloseConnectionContext();		
				//m_iCountCommErrors = 0;
				//return CW_ERR_CMD_NOT_PROCESSED;
				return CW_ERR_EQT_STOPPED;
			}
			else
			{
				m_iCountCommErrors++;
				return CW_ERR_CMD_NOT_PROCESSED;
			}
		}
		else
		{
			EnterCriticalSection(&m_pConnectionContext->m_csConnectionState);
			memcpy(m_pcRcvBuffer, &m_pConnectionContext->m_pcBuffer, iReceiveResult/*1024*/);
			LeaveCriticalSection(&m_pConnectionContext->m_csConnectionState);

			//Reading correctly processed
			if(iReceiveResult == m_usSizeofResponseBuffer)
			{
				m_iCountCommErrors = 0;
			}
			else
			{
				CWTRACE(PUS_PROTOC1, LVL_BIT4, 
					"@@@@@@@ Response length error:ASK(%d), RESPONSE(%d)", 
					m_usSizeofResponseBuffer, iReceiveResult);
				
				m_iCountCommErrors++;
				
				//printData(a_pcBufferToSend, a_usSendBufferSize, 
				//	(unsigned char*)&m_pConnectionContext->m_pcBuffer, iReceiveResult);

				return CW_ERR_INVALID_POSITION_LENGTH;
			}
		}
    }

    return CW_OK;
}

void TcpIpDriverEquipment::printData(const unsigned char* pTxData, short TxLength, unsigned char* pRxData, short RxLength)
{
	char str[512];
	unsigned char i, j;

	for(i=0, j=0; i<TxLength; i++, j+=3)
	{
		sprintf_s(&str[j], 4, "%02x ", *(pTxData+i));
	}
	CWTRACE(PUS_PROTOC1, LVL_BIT4, "%s::TX[%d] %s ",this->m_pCwEqt->GetGlobalName(), TxLength, str);

	for(i=0, j=0; i<RxLength; i++, j+=3)
	{
		sprintf_s(&str[j], 4, "%02x ", *(pRxData+i));
	}
	CWTRACE(PUS_PROTOC1, LVL_BIT4, "%s::RX[%d] %s ",this->m_pCwEqt->GetGlobalName(), RxLength, str);

	return;
}


void TcpIpDriverEquipment::initLog(CString strLogName)
{
	std::string str (strLogName.GetBuffer(strLogName.GetLength()));
	
	m_pLogger = new CLogger (LogLevel_Info,CLogger::GetAppPathA().append("log\\"), str);
	//logger.TraceFatal("TraceFatal %d", 1);
	//logger.TraceError("TraceError %s", "sun");
	//logger.TraceWarning("TraceWarning");
	//logger.TraceInfo("TraceInfo");
	//logger.ChangeLogLevel(LOGGER::LogLevel_Error);
	//logger.TraceFatal("TraceFatal %d", 2);
	//logger.TraceError("TraceError %s", "sun2");
	//logger.TraceWarning("TraceWarning");
	//logger.TraceInfo("TraceInfo");

	return;
}