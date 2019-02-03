#include "stdafx.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverNetwork.h"
#include "TcpIpDriverEquipment.h"
#include "log.h"

//#ifdef _DEBUG  
//#define DEBUG_CLIENTBLOCK new( _CLIENT_BLOCK, __FILE__, __LINE__)  
//#else  
//#define DEBUG_CLIENTBLOCK  
//#endif  
//#define _CRTDBG_MAP_ALLOC  
//#include <stdlib.h>  
//#include <crtdbg.h>  
//#ifdef _DEBUG  
//#define new DEBUG_CLIENTBLOCK  
//#endif  

using namespace LOGGER;

_ProtEqt *CipNetwork::CreateProtEqt(CW_USHORT usType)
{
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);			//tyh

    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::CreateProtNetwork  Type=%d", m_pCwNetwork->GetGlobalName(), usType);
    TcpIpDriverEquipment * Eq = new TcpIpDriverEquipment();
    llDevices.AddHead((_ProtEqt*)Eq);
	int count = llDevices.GetCount();

	Eq->SetInitializedState(false);

	m_Eq = Eq;
    return Eq;
}

void CipNetwork::OnAccept(CConnectionContext &ConnectionContext, const CString &strIpAddress, const USHORT &usPortNumber)
{
}

//void CipNetwork::OnClose(CConnectionContext &ConnectionContext,const BOOL bCloseByRemote)
//{
//	ConnectionContext.m_bLocalClose = TRUE;
//}

_ProtRet CipNetwork::Start_Async(_ProtStartNetworkCmd *pStartNetworkCmd)
{
	CW_HANDLE huserAttributes;
	_CwDataItem *pdiAttributes;

	USHORT usPortNumber;

	if (m_pCwNetwork->OpenUserAttributes_Sync(&huserAttributes)==CW_OK)
	{
		if (m_pCwNetwork->GetUserAttributes_Sync(huserAttributes,0,CWIT_UI2,&pdiAttributes)==CW_OK)
		{
			usPortNumber = (CW_USHORT)pdiAttributes->CwValue.uiVal;
		}

		if (m_pCwNetwork->GetUserAttributes_Sync(huserAttributes,1,CWIT_UI2,&pdiAttributes)==CW_OK)	//tyh 增加应答超时参数
		{
			m_usDelay = (CW_USHORT)pdiAttributes->CwValue.uiVal;
			if(m_usDelay == 0)
				m_usDelay = 5000;
		}
		m_usDelay = 10000;		//test 1229


		m_pCwNetwork->CloseUserAttributes_Sync(huserAttributes);
	}

	if (usPortNumber == 0)
		usPortNumber = 502;

	m_usErrGetId = 0;  //tyh 170731 init

#ifdef	LOG_DEBUG
	initLog(m_pCwNetwork->GetGlobalName());
#endif

	StartListening(usPortNumber);
    pStartNetworkCmd->Ack();
	return PR_CMD_PROCESSED;
}

_ProtRet CipNetwork::Stop_Async(_ProtStopNetworkCmd *pStartNetworkCmd)
{
	CWTRACE(PUS_PROTOC1, LVL_BIT5, "%s::Stop_Async", m_pCwNetwork->GetGlobalName());
	StopListening();

	POSITION pos = llDevices.Find((TcpIpDriverEquipment*)m_Eq);
	if(pos != NULL)
	{
		llDevices.RemoveAt(pos);
		
		m_Eq = NULL;
	}

#ifdef LOG_DEBUG
	if (m_pLogger != NULL)
	{
		delete m_pLogger;
		m_pLogger = NULL;
	}
#endif

	pStartNetworkCmd->Ack();

	return PR_CMD_PROCESSED;
}

bool CipNetwork::GetIDFromFrame(CConnectionContext &ConnectionContext)
{
	//Modbus frame to get the ID
	char ucRequest[255];
	unsigned short usId    = 0;
	unsigned short usIndex = 0;

	ucRequest[usIndex++] = (unsigned char)(usId/0x100);
	ucRequest[usIndex++] = (unsigned char)(usId%0x100);
	ucRequest[usIndex++] = 0;
	ucRequest[usIndex++] = 0;
	ucRequest[usIndex++] = 0;
	ucRequest[usIndex++] = READ_REQUEST_SIZE;
	ucRequest[usIndex++] = 0;//Boradcast //pEqt->GetEqtAddress();
	ucRequest[usIndex++] = READ_HOLDING_REG;
	ucRequest[usIndex++] = 0;//FrameAdd
	ucRequest[usIndex++] = 0;//FrameAdd
	ucRequest[usIndex++] = 0;//DataSize
	ucRequest[usIndex++] = 1;//DataSize
	//--------------------------

    int iSendResult=0;

    iSendResult = send(ConnectionContext.m_socketConnection, ucRequest, usIndex, 0);
    if ((iSendResult == SOCKET_ERROR)||(iSendResult == 0)) 
	{
       //Error at sending request!
        OnClose(ConnectionContext,false);
		CWTRACE(PUS_PROTOC1, LVL_BIT2, "@@@@@@@ 设备地址召唤链路出错，关闭链接");
 
		return true;
    }
	else
	{
        int iTimeout = m_usDelay;
        setsockopt(ConnectionContext.m_socketConnection,SOL_SOCKET,SO_RCVTIMEO,(char *)&iTimeout,sizeof(int));

		int iReceiveResult = recv(ConnectionContext.m_socketConnection, ConnectionContext.m_pcBuffer, 1024, 0);
		if (iReceiveResult == 0/*SOCKET_ERROR*/)
		{
			OnClose(ConnectionContext,true);
			CWTRACE(PUS_PROTOC1, LVL_BIT2, "@@@@@@@ 接收设备地址链路出错，关闭链接");

			return true;
		}
		else
		{
			// Correct reading 
			DWORD dwReceivedBufferSize=1024;
			DWORD dwReceivedBufferCurrentIndex=0;

			if(iReceiveResult > 0)
			{
				//Process frame and find device in supervisor
				if(OnReceive(ConnectionContext, ConnectionContext.m_pcBuffer, dwReceivedBufferSize, dwReceivedBufferCurrentIndex))
				{
					ConnectionContext.m_usErrGetId = 0;
					return true;
				}
				else
					ConnectionContext.m_usErrGetId++;
			}
			else
			{
				ConnectionContext.m_usErrGetId++;
			}

			if(ConnectionContext.m_usErrGetId > 3)	
			{
				OnClose(ConnectionContext,true);
				CWTRACE(PUS_PROTOC1, LVL_BIT2, "@@@@@@@ 设备地址召唤出错超限，关闭链接");

				return true;
			}
		}
    }

    return false;
}

bool CipNetwork::OnReceive(CConnectionContext &ConnectionContext, char *pcReceivedBuffer, DWORD &dwReceivedBufferSize, DWORD &dwReceivedBufferCurrentIndex)
{
    //Include more checkings
    int length = pcReceivedBuffer[8];
	bool bResult = false;
    
    if(length==2)
    {
         int iID=(int)pcReceivedBuffer[10]+(int)(pcReceivedBuffer[9]<<8);
        _CwNetwork *cwNet=this->m_pCwNetwork;
        
        //Finding ID on devices in supervisor
        POSITION pos = llDevices.GetHeadPosition();
        for (int i = 0; i < llDevices.GetCount(); i++)
        {
            TcpIpDriverEquipment* pEq= (TcpIpDriverEquipment*)llDevices.GetNext(pos);
            unsigned long lIDEq=pEq->m_pCwEqt->GetAddress_Sync();
			if(iID == lIDEq)
			{
				//Sleep(pEq->m_usStartDelay);		//启动延时
				if(pEq->GetInitializedState())
				{
					CWTRACE(PUS_PROTOC1, LVL_BIT2, "@@@@@@@ PORT(%d)设备接入（ID：%d）",this->GetServerPort(), iID);
					pEq->SetReconnect(true);
					pEq->setConnectionContext(&ConnectionContext);
					pEq->SetServerPort(this->GetServerPort());

					ConnectionContext.m_usErrGetId = 0;

					bResult = true;
				}
				else
				{
					CWTRACE(PUS_PROTOC1, LVL_BIT2, "@@@@@@@ PORT(%d)设备接入（ID：%d）尚未初始化",this->GetServerPort(), iID);
					//OnClose(ConnectionContext,true);
				}
			}
		}
	}

    return bResult;
}

CW_USHORT CipNetwork::GetFluxManagement()
{
    return CW_FLUX_MULTI;
}

#ifdef	LOG_DEBUG
void CipNetwork::initLog(CString strLogName)
{
	std::string str (strLogName.GetBuffer(strLogName.GetLength()));
	
	m_pLogger = new CLogger (LogLevel_Info,CLogger::GetAppPathA().append("log\\"), str);
	//CLogger logger(LogLevel_Info,CLogger::GetAppPathA().append("log\\"),"test");
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
#endif

