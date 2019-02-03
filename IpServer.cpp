#include "stdafx.h"

#include "IpServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNAMIC(CIPServer, CObject)
IMPLEMENT_DYNAMIC(CConnectionContext, CObject)


CConnectionContext::CConnectionContext(
		CIPServer *pIPServer,
		const CString &strConnectionIpAddress,
		const USHORT &usConnectionPortNumber,
		const SOCKET &socketConnection) :
	m_pIPServer(pIPServer),
	m_strConnectionIpAddress(strConnectionIpAddress),
	m_usConnectionPortNumber(usConnectionPortNumber),
	m_socketConnection(socketConnection),
	m_hConnectionThread(NULL),
	m_dwConnectionThreadId(0),
	m_bLocalClose(FALSE),
	m_ConnectionState(ConnectionConnected)

{
    //for(int i=0;i<MAX_SIZE_BUFFER;i++)
    //    m_pcBuffer[i]=0;

	memset(m_pcBuffer, 0, sizeof(CHAR)*MAX_SIZE_BUFFER);
	
    InitializeCriticalSection(&m_csConnectionState);
}

CConnectionContext::~CConnectionContext()
{
	if(m_hConnectionThread)
		CloseHandle(m_hConnectionThread);

	DeleteCriticalSection(&m_csConnectionState);
}


CIPServer::CIPServer() :
	m_ServerState(ServerStop), m_hThread(NULL), m_socketListening(INVALID_SOCKET)
{
	m_hThreadAcceptReady = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&m_csConnectionContexts);
}

CIPServer::~CIPServer()
{
	DeleteCriticalSection(&m_csConnectionContexts);
	CloseHandle(m_hThreadAcceptReady);
}

void CIPServer::OnClose(CConnectionContext &ConnectionContext,const BOOL bCloseByRemote)
{
	ConnectionContext.m_ConnectionState = ConnectionClosed;
	ConnectionContext.m_bLocalClose = TRUE;
	ConnectionContext.m_usErrGetId = 0;

	//shutdown(ConnectionContext.m_socketConnection, SD_SEND);
	closesocket(ConnectionContext.m_socketConnection);

	ConnectionContext.m_socketConnection = INVALID_SOCKET;

	EnterCriticalSection(&ConnectionContext.m_csConnectionState);

	//int count = m_listConnectionContexts.GetCount();		//test	
	if(RemoveConnection(&ConnectionContext))		//地址请求出错，从链表中删除链接符
	{
		delete &ConnectionContext;
	}
	//count = m_listConnectionContexts.GetCount();		//test
	
	LeaveCriticalSection(&ConnectionContext.m_csConnectionState);
}

BOOL CIPServer::StartListening(USHORT usPortNumber)
{
	DWORD dw = 0;

	if(m_ServerState != ServerStop)
		return FALSE;

	m_ServerState = ServerStartInProgress;

	m_usPortNumber = usPortNumber;
	
	m_socketListening = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

	if(m_socketListening == INVALID_SOCKET)
		return FALSE;

	struct sockaddr_in saiAddress;

	saiAddress.sin_family = AF_INET;
	saiAddress.sin_addr.s_addr = INADDR_ANY;
	saiAddress.sin_port = htons(m_usPortNumber);
	
	if(bind(m_socketListening,(struct sockaddr *)&saiAddress,sizeof(saiAddress)) == SOCKET_ERROR)
	{
		closesocket(m_socketListening);
		m_socketListening = INVALID_SOCKET;
		m_ServerState = ServerStop;
		return FALSE;
	}
	
	if(listen(m_socketListening,SOMAXCONN) == SOCKET_ERROR)
	{
		closesocket(m_socketListening);
		m_socketListening = INVALID_SOCKET;
		m_ServerState = ServerStop;
		return FALSE;
	}
	
	m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ThreadRoutine, this, 0, &m_dwThreadId);

	if(m_hThread == NULL)
		return FALSE;

	dw = WaitForSingleObject(m_hThreadAcceptReady, 10000/*INFINITE*/);
	switch(dw)
	{
	case WAIT_OBJECT_0:
		//CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* m_hThreadAcceptReady is signaled");
		// The process is signaled.
		break;

	case WAIT_TIMEOUT:
		CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* m_hThreadAcceptReady WAIT_TIMEOUT!!");
		break;

	case WAIT_FAILED:
		CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* m_hThreadAcceptReady WAIT_FAILED@@");
		// Bad call to function (invalid handle?)
		break;

	default:
		CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* m_hThreadAcceptReady unknow..");
		break;
	}

	if(m_ServerState == ServerStop)
		return FALSE;

	return TRUE;

}

BOOL CIPServer::StopListening()
{
	DWORD dw = 0;
	int iResult;

	if(m_ServerState != ServerStarted)
		return FALSE;

	m_ServerState = ServerStopInProgress;

	//关闭服务端下的客户连接
	CWTRACE(PUS_PROTOC1, LVL_BIT5, "PORT（%d）::Stop_listening", m_usPortNumber);
	EnterCriticalSection(&m_csConnectionContexts);		//进入临界区
	
	while(m_listConnectionContexts.IsEmpty() == FALSE)
	{
		CConnectionContext *pConnectionContext = m_listConnectionContexts.RemoveHead();

		CWTRACE(PUS_PROTOC1, LVL_BIT5, "******* StopListening -> close client: %s", pConnectionContext->m_strConnectionIpAddress);
		Close(*pConnectionContext);

		delete pConnectionContext;
	}

	LeaveCriticalSection(&m_csConnectionContexts);		//退出临界区
	CWTRACE(PUS_PROTOC1, LVL_BIT5, "PORT（%d）::Stop_listening_success", m_usPortNumber);

	//关闭服务端socket
	if((iResult = closesocket(m_socketListening)) == SOCKET_ERROR)
	{
		iResult = WSAGetLastError();
		CWTRACE(PUS_PROTOC1, LVL_BIT5, "PORT（%d）::关闭服务端 error = 0x%x", m_usPortNumber, iResult);
	}
	else
	{
		dw = WaitForSingleObject(m_hThread, 5000/*INFINITE*/);
		CWTRACE(PUS_PROTOC1, LVL_BIT5, "PORT（%d）::等待服务器关闭", m_usPortNumber);
		switch(dw)
		{
		case WAIT_OBJECT_0:
			CWTRACE(PUS_PROTOC1, LVL_BIT5, "******* PORT（%d）::服务器关闭成功！", m_usPortNumber);
			// The process is signaled.
			iResult = 1;
			break;

		case WAIT_TIMEOUT:
			CWTRACE(PUS_PROTOC1, LVL_BIT5, "******* m_hThread not terminate!!");
			// The process did not terminate within 5000 milliseconds.
			iResult = 0;
			break;

		case WAIT_FAILED:
			CWTRACE(PUS_PROTOC1, LVL_BIT5, "******* m_hThread terminate failed@@");
			// Bad call to function (invalid handle?)
			iResult = 0;
			break;

		default:
			CWTRACE(PUS_PROTOC1, LVL_BIT5, "******* m_hThread unknow..");
			iResult = 0;
			break;
		}
	}

	CloseHandle(m_hThread);

	m_ServerState = ServerStop;

	if(iResult)
		return TRUE;
	else
		return FALSE;
}

BOOL CIPServer::StartWinsock()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD( 2, 0 );
	
	if(WSAStartup( wVersionRequested, &wsaData ))
		return FALSE;

	return TRUE;

}

BOOL CIPServer::StopWinsock()
{
	if(WSACleanup())
		return FALSE;
	return TRUE;
}


void WINAPI CIPServer::ThreadRoutine(CIPServer *pIPServer)
{
	pIPServer->ThreadRun();
}

void CIPServer::ThreadRun()
{

	m_ServerState = ServerStarted;

	SetEvent(m_hThreadAcceptReady);

	BOOL bContinue = TRUE;

	struct sockaddr saConnection;
	int iConnectionSize = sizeof(saConnection);

	
	while(bContinue)
	{
		SOCKET socketConnection = accept(m_socketListening, &saConnection, &iConnectionSize);
		if(socketConnection == INVALID_SOCKET)
		{
			bContinue = FALSE;
		}
		else
		{
			USHORT usConnectionPortNumber = saConnection.sa_data[0];
			usConnectionPortNumber <<= 8;
			usConnectionPortNumber |= saConnection.sa_data[1] & 0x00FF;

			CString strConnectionIpAddress;
			strConnectionIpAddress.Format("%u.%u.%u.%u", (UCHAR)saConnection.sa_data[2], (UCHAR)saConnection.sa_data[3],
				(UCHAR)saConnection.sa_data[4], (UCHAR)saConnection.sa_data[5]);

			CConnectionContext *pConnectionContext = new CConnectionContext(this, strConnectionIpAddress, usConnectionPortNumber, socketConnection);
			pConnectionContext->m_usErrGetId = 0;	//初始化错误计数器

			EnterCriticalSection(&m_csConnectionContexts);	//进入临界区
			m_listConnectionContexts.AddTail(pConnectionContext); 
			LeaveCriticalSection(&m_csConnectionContexts);	//退出临界区

			CWTRACE(PUS_PROTOC1, LVL_BIT5, "@@ --_-- @@ PORT:(%d)m_listConnectionContexts count(add): %d",
				m_usPortNumber, m_listConnectionContexts.GetCount());

			OnAccept(*pConnectionContext, strConnectionIpAddress, usConnectionPortNumber);

			pConnectionContext->m_hConnectionThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ConnectionThreadRoutine,
				pConnectionContext, 0, &pConnectionContext->m_dwConnectionThreadId);
		}
	}
}




void WINAPI CIPServer::ConnectionThreadRoutine(CConnectionContext *pConnectionContext)
{
	pConnectionContext->m_pIPServer->ConnectionThreadRun(pConnectionContext);
}

void CIPServer::ConnectionThreadRun(CConnectionContext *pConnectionContext)
{
	//BOOL bReadContinue = TRUE;
	
	//DWORD dwBufferSize = 1024;
	//DWORD dwBufferCurrentIndex = 0;

	//int count = m_listConnectionContexts.GetCount();		//test
    while(!pConnectionContext->m_bLocalClose)
    {
		if(pConnectionContext->m_ConnectionState != ConnectionClosed)
		{
			if(GetIDFromFrame(*pConnectionContext))
				break;
		}
    }	
}


BOOL CIPServer::Close(CConnectionContext &ConnectionContext)
{
	DWORD dw = 0;
	int iResult;

	ConnectionContext.m_bLocalClose = TRUE;

	if((iResult=closesocket(ConnectionContext.m_socketConnection)) == SOCKET_ERROR)
	{
		iResult = WSAGetLastError();
		CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* 关闭客户端 error = 0x%x", iResult);
	}
	else
	{
		dw = WaitForSingleObject(ConnectionContext.m_hConnectionThread, 5000/*INFINITE*/);
		CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* 等待客户端关闭");
		switch(dw)
		{
		case WAIT_OBJECT_0:
			CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* 关闭客户端成功");
			// The process is signaled.
			iResult = 1;
			break;

		case WAIT_TIMEOUT:
			CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* ConnectionContext.m_hConnectionThread WAIT_TIMEOUT!!");
			// The process did not terminate within 5000 milliseconds.
			iResult = 0;
			break;

		case WAIT_FAILED:
			CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* ConnectionContext.m_hConnectionThread WAIT_FAILED@@");
			// Bad call to function (invalid handle?)
			iResult = 0;
			break;

		default:
			CWTRACE(PUS_PROTOC1, LVL_BIT1, "******* ConnectionContext.m_hConnectionThread unknow..");
			iResult = 0;
			break;
		}
	}
	
	if(iResult)
		return TRUE;
	else
		return FALSE;
}

BOOL CIPServer::RemoveConnection(CConnectionContext *pConnectionContext)
{
	BOOL result = FALSE;

	EnterCriticalSection(&m_csConnectionContexts);	//进入临界区

	POSITION posConnectionContext = m_listConnectionContexts.Find(pConnectionContext);
	if(posConnectionContext != NULL)
	{
		m_listConnectionContexts.RemoveAt(posConnectionContext);	
		//delete posConnectionContext;

		CWTRACE(PUS_PROTOC1, LVL_BIT5, "@@ --_-- @@ PORT:(%d)m_listConnectionContexts count(remove): %d",
			m_usPortNumber, m_listConnectionContexts.GetCount());
		result = TRUE;
	}

	LeaveCriticalSection(&m_csConnectionContexts);	//退出临界区

	return result;
}

const char *CIPServer::GetReceivedFrame(CConnectionContext &ConnectionContext)
{
	return ConnectionContext.m_pcReceiveFrame;
}
