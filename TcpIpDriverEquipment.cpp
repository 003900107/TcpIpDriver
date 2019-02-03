#include "stdafx.h"
#include "IpServer.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverEquipment.h"
#include "TcpIpDriverFrame.h"


void TcpIpDriverEquipment::setConnectionContext(CConnectionContext *a_pConnectionContext)
{
	CConnectionContext* pOldConnectionContext = m_pConnectionContext;

	if(m_pConnectionContext != a_pConnectionContext)
	{
		m_pConnectionContext = a_pConnectionContext;

		if(pOldConnectionContext != NULL)
		{  //There was a connection and need to be removed before linking the newone  
			m_pNet = (CipNetwork*)(m_pCwEqt->GetProtNetwork_Sync());

			if(pOldConnectionContext->m_socketConnection != INVALID_SOCKET)
				m_pNet->Close(*pOldConnectionContext);

			if(m_pNet->RemoveConnection(pOldConnectionContext))		//ɾ�����Ӷ����еĶ���
			{
				CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* Reconnection success, old: 0x%X, new: 0x%X", 
					pOldConnectionContext, m_pConnectionContext);
				delete pOldConnectionContext;
			}
		}
	}

	m_pConnectionContext->m_ConnectionState = ConnectionConnected;
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
	m_bInitialized = true;
	
	CW_HANDLE huserAttributes;
	_CwDataItem *pdiAttributes;

	//You can get protocol user attributes (method similar on Network class with m_pCwNetwork and Frame with m_pCwFrame
	if (m_pCwEqt->OpenUserAttributes_Sync(&huserAttributes)==CW_OK)
	{
		//�������Դ���
		if (m_pCwEqt->GetUserAttributes_Sync(huserAttributes,0,CWIT_UI2,&pdiAttributes)==CW_OK)
		{
			m_iMaxCountCommError = pdiAttributes->CwValue.uiVal;
		}
		if(m_iMaxCountCommError == 0)
			m_iMaxCountCommError = 2;
		m_iMaxCountCommError = 4;		//test 1229


		//֡��ʱ
		if (m_pCwEqt->GetUserAttributes_Sync(huserAttributes,1,CWIT_UI2,&pdiAttributes)==CW_OK)
		{
			m_usDelay = pdiAttributes->CwValue.uiVal*1000;
		}
		if(m_usDelay == 0)
			m_usDelay = 5000;

		//������ʱ
		if (m_pCwEqt->GetUserAttributes_Sync(huserAttributes,2,CWIT_UI2,&pdiAttributes)==CW_OK)	
		{
			m_usStartDelay = pdiAttributes->CwValue.uiVal*1000;
		}
		if(m_usStartDelay == 0)
			m_usStartDelay = (m_pCwEqt->GetAddress_Sync()%10)*1000;

		//Sleep(m_usStartDelay);		//������ʱ

		m_pCwEqt->CloseUserAttributes_Sync(huserAttributes);
	}

#ifdef LOG_DEBUG
	initLog(m_pCwEqt->GetGlobalName());
#endif

	pStartEqtCmd->Ack();
    return PR_CMD_PROCESSED;
}

_ProtRet TcpIpDriverEquipment::Stop_Async(_ProtStopEqtCmd *pStopEqtCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT5, "%s::Stop_Async ", m_pCwEqt->GetGlobalName());
    if (m_pConnectionContext!=NULL)
    {
        // Closing connection
        CloseConnectionContext();   
    }

#ifdef LOG_DEBUG
	CWTRACE(PUS_PROTOC1, LVL_BIT5, "%s::Stop_logger ", m_pCwEqt->GetGlobalName());
	if (m_pEquLogger != NULL)
	{
		delete m_pEquLogger;
		m_pEquLogger = NULL;
	}
#endif
	CWTRACE(PUS_PROTOC1, LVL_BIT5, "%s::Stop_send_Ack ", m_pCwEqt->GetGlobalName());
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
	CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* PORT(%d) CloseConnectionContext ADDR = %d", this->m_usServerPort, this->m_ucAddress);
	m_pConnectionContext->m_ConnectionState = ConnectionClosed;
/*	
	int iResult;
	if((iResult=closesocket(m_pConnectionContext->m_socketConnection)) == SOCKET_ERROR)
	{
		iResult = WSAGetLastError();
		CWTRACE(PUS_PROTOC1, LVL_BIT2, "******* �ر��豸���� error = %d!", iResult);
	}
*/
	if(m_pConnectionContext->m_socketConnection != INVALID_SOCKET)		//���������ӿڣ��ر���������
	{
		EnterCriticalSection(&m_pConnectionContext->m_csConnectionState);

		m_pNet->Close(*m_pConnectionContext);
		m_pConnectionContext->m_socketConnection = INVALID_SOCKET;

		LeaveCriticalSection(&m_pConnectionContext->m_csConnectionState);
	}

	m_iCountCommErrors = 0;

	//TODO:Still remove from list at network level.
    //There was a connection and need to be removed before linking the newone  
     //CipNetwork *net=(CipNetwork*)(m_pCwEqt->GetProtNetwork_Sync());
     //net->RemoveConnection(m_pConnectionContext);
     //net->Close(*m_pConnectionContext);
	
	CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* CloseConnectionContext ADDR = %d  success", this->m_ucAddress);
}

int TcpIpDriverEquipment::SendRcvFrame(
    CW_LPC_UCHAR a_pcBufferToSend,
    const USHORT a_usSendBufferSize,
    CW_LPC_UCHAR a_pucReceivedBuffer,
    const USHORT a_usSizeofResponseBuffer)
{
	int iSendResult, length;

    m_usSizeofResponseBuffer = a_usSizeofResponseBuffer;
    m_iReceiveCount = 0;
    m_pcRcvBuffer = (CW_LP_UCHAR)a_pucReceivedBuffer;


	if(m_bReConnect)		//�ж��Ƿ�����������
	{
		CWTRACE(PUS_PROTOC1, LVL_BIT4, "~~~~~~~ �豸�����У���ͣ���� old_handle: 0x%X", m_pConnectionContext);
#ifdef	LOG_DEBUG	
		if(m_pEquLogger != NULL)		//��¼�˳�
			m_pEquLogger->TraceInfo("�豸�����У���ͣ���� handle: 0x%X", m_pConnectionContext);
#endif

		return CW_ERR_EQT_STOPPED;
	}

    if(m_pConnectionContext == NULL)
	{
#ifdef	LOG_DEBUG
		if(m_pEquLogger != NULL)		//��¼�˳�
			m_pEquLogger->TraceInfo("m_pConnectionContext is NULL :%s", m_pCwEqt->GetGlobalName());
#endif 

        return CW_ERR_EQT_STOPPED;
	}

	if(m_pConnectionContext->m_ConnectionState != ConnectionConnected)		//tyh 170731 ���ӷ���ǰ�ж�����״̬
	{
		return CW_ERR_EQT_STOPPED;
	}

	//iSendResult = 0;
	length = a_pcBufferToSend[5]+6;
	iSendResult = send(m_pConnectionContext->m_socketConnection, (const char*)a_pcBufferToSend, length, 0);
	if ((iSendResult == SOCKET_ERROR)||(iSendResult == 0))	//����ʧ��
	{
		CloseConnectionContext();
		CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ �������ӳ����ж�����");

		return CW_ERR_EQT_STOPPED;
    }
	else	//���ͳɹ�
	{
		int iReceiveResult = recieveData(m_usDelay, 1024);
		if (iReceiveResult == 0)
		{
			//������·����
			CloseConnectionContext();
			CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ ������·�����ж�����");		

			return CW_ERR_EQT_STOPPED;
		}
		else	
		{
			//���ճɹ�
			if(iReceiveResult == m_usSizeofResponseBuffer)	//���ճ���ƥ��
			{
				//��������
				EnterCriticalSection(&m_pConnectionContext->m_csConnectionState);
				memcpy(m_pcRcvBuffer, &m_pConnectionContext->m_pcBuffer, m_usSizeofResponseBuffer/*1024*/);
				LeaveCriticalSection(&m_pConnectionContext->m_csConnectionState);
			}
			else	//���ճ��ȳ���
			{
				CWTRACE(PUS_PROTOC1, LVL_BIT4, 
					"@@@@@@@ Response length error(ID: %d):ASK(%d), RESPONSE(%d)", 
					a_pcBufferToSend[6], m_usSizeofResponseBuffer, iReceiveResult);
#ifdef	LOG_DEBUG
				//��¼��־
				m_pEquLogger->TraceError("LenghtError(ID: %d): ASK(%d), RESPONSE(%d)", 
					a_pcBufferToSend[6], m_usSizeofResponseBuffer, iReceiveResult);
#endif 

				/*printData(a_pcBufferToSend, a_usSendBufferSize, 
				(unsigned char*)&m_pConnectionContext->m_pcBuffer, iReceiveResult);*/

				//�ٴν�������
				if ((iReceiveResult = recieveData(500, 1024)) == m_usSizeofResponseBuffer)				
				{
					//������ȷ����������
					EnterCriticalSection(&m_pConnectionContext->m_csConnectionState);
					memcpy(m_pcRcvBuffer, &m_pConnectionContext->m_pcBuffer, m_usSizeofResponseBuffer/*1024*/);
					LeaveCriticalSection(&m_pConnectionContext->m_csConnectionState);

					CWTRACE(PUS_PROTOC1, LVL_BIT4, 
						"@@@@@@@ Response length aggain (ID: %d):ASK(%d), RESPONSE(%d)", 
						a_pcBufferToSend[6], m_usSizeofResponseBuffer, iReceiveResult);
				}
				else
				{
					m_iCountCommErrors++;
					if(m_iCountCommErrors > m_iMaxCountCommError)	//���ݳ��ȳ�����
					{
						CloseConnectionContext();
						CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ Ӧ�����������ޣ��ж�����");

						return CW_ERR_EQT_STOPPED;
					}

					return CW_ERR_INVALID_POSITION_LENGTH;
#ifdef	LOG_DEBUG
					m_pEquLogger->TraceError("LenghtError ask again(ID: %d): ASK(%d), RESPONSE(%d)", 
						a_pcBufferToSend[6], m_usSizeofResponseBuffer, iReceiveResult);
#endif
				}
			}
		}
	}

	return CW_OK;
}

void TcpIpDriverEquipment::printData(const unsigned char* pTxData, short TxLength, unsigned char* pRxData, short RxLength)
{
	char str[1024];
	unsigned char i, j;
	unsigned short R_lenght;

	//TX
	for(i=0, j=0; i<TxLength; i++, j+=3)
	{
		sprintf_s(&str[j], 4, "%02x ", *(pTxData+i));
	}
	//CWTRACE(PUS_PROTOC1, LVL_BIT4, "%s::TX[%d] %s ",this->m_pCwEqt->GetGlobalName(), TxLength, str);
#ifdef	LOG_DEBUG
	m_pEquLogger->TraceError("DataTX: %s", str);
#endif

	//RX
	if(RxLength>300)
		R_lenght = 300;
	else
		R_lenght = RxLength;

	for(i=0, j=0; i<R_lenght; i++, j+=3)
	{
		sprintf_s(&str[j], 4, "%02x ", *(pRxData+i));
	}
	//CWTRACE(PUS_PROTOC1, LVL_BIT4, "%s::RX[%d] %s ",this->m_pCwEqt->GetGlobalName(), RxLength, str);
#ifdef	LOG_DEBUG
	m_pEquLogger->TraceError("DataRX: %s", str);
#endif

	return;
}

#ifdef	LOG_DEBUG
void TcpIpDriverEquipment::initLog(CString strLogName)
{
	std::string str (strLogName.GetBuffer(strLogName.GetLength()));
	
	m_pEquLogger = new CLogger (LogLevel_Info,CLogger::GetAppPathA().append("log\\"), str);

	//m_pEquLogger->TraceFatal("TraceFatal %d", 1);
	//m_pEquLogger->TraceError("TraceError %s", "sun");
	//m_pEquLogger->TraceWarning("TraceWarning");
	//m_pEquLogger->TraceInfo("TraceInfo");
	//m_pEquLogger->ChangeLogLevel(LOGGER::LogLevel_Error);
	//m_pEquLogger->TraceFatal("TraceFatal %d", 2);
	//m_pEquLogger->TraceError("TraceError %s", "sun2");
	//m_pEquLogger->TraceWarning("TraceWarning");
	//m_pEquLogger->TraceInfo("TraceInfo");

	return;
}
#endif

int TcpIpDriverEquipment::recieveData(int iTimeout, int iLenght)
{
	int iReceiveResult;

	setsockopt(m_pConnectionContext->m_socketConnection,SOL_SOCKET,SO_RCVTIMEO,(char *)&iTimeout,sizeof(int));
	iReceiveResult = recv(m_pConnectionContext->m_socketConnection, m_pConnectionContext->m_pcBuffer, iLenght, 0);

	return iReceiveResult;
}