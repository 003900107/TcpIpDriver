#pragma once


#define SOCKET_TIMEOUT		0xFFFFFFFE

#define MAX_SIZE_BUFFER		1024

typedef enum
{
	ServerStop,
	ServerStartInProgress,
	ServerStarted,
	ServerStopInProgress

} ServerState;

typedef enum
{
	ConnectionClosed,
	ConnectionConnected

} ConnectionState;


class CIPServer;

class CConnectionContext : public CObject
{
DECLARE_DYNAMIC(CConnectionContext)
private:
protected:
public:
	CIPServer *m_pIPServer;
	CString m_strConnectionIpAddress;
	USHORT m_usConnectionPortNumber;
	SOCKET m_socketConnection;
	HANDLE m_hConnectionThread;
	DWORD m_dwConnectionThreadId;
	CHAR m_pcBuffer[MAX_SIZE_BUFFER];
	CHAR m_pcReceiveFrame[MAX_SIZE_BUFFER];
	CRITICAL_SECTION m_csConnectionState;
	BOOL m_bLocalClose;
	DWORD m_dwSendReceiveReturnCode;
	ConnectionState m_ConnectionState;

	unsigned short m_usErrGetId;			//20181226  连接错误计数器

	CConnectionContext(
		CIPServer *pIPServer,
		const CString &strConnectionIpAddress,
		const USHORT &usConnectionPortNumber,
		const SOCKET &socketConnection);
	~CConnectionContext();
};


class CIPServer
{
DECLARE_DYNAMIC(CIPServer)
private:
	SOCKET m_socketListening;
	CList <CConnectionContext *, CConnectionContext *> m_listConnectionContexts;
	CRITICAL_SECTION m_csConnectionContexts;
	HANDLE m_hThread;
	DWORD m_dwThreadId;
	HANDLE m_hThreadAcceptReady;
	USHORT m_usPortNumber;
protected:

	static void WINAPI ThreadRoutine(CIPServer *pIPServer);
	void ThreadRun();
	static void WINAPI ConnectionThreadRoutine(CConnectionContext *pConnectionContext);
	void ConnectionThreadRun(CConnectionContext *pConnectionContext);



	ServerState m_ServerState;

public:

	CIPServer();
	virtual ~CIPServer();

	static BOOL StartWinsock();
	static BOOL StopWinsock();

	BOOL StartListening(USHORT usPortNumber);
	BOOL StopListening();
	BOOL Close(CConnectionContext &ConnectionContext);
	int SendReceiveFrame(CConnectionContext &ConnectionContext, const char *pcBuffer, DWORD dwBufferSize, DWORD dwTimeout);
	int SendFrame(CConnectionContext &ConnectionContext, const char *pcBuffer, DWORD dwBufferSize);
	BOOL RemoveConnection(CConnectionContext *pConnectionContext);

	const char *GetReceivedFrame(CConnectionContext &ConnectionContext);
	USHORT GetServerPort(void) { return m_usPortNumber; }

	virtual void OnAccept(CConnectionContext &ConnectionContext, const CString &strIpAddress, const USHORT &usPortNumber) = 0;
	virtual void OnClose(CConnectionContext &ConnectionContext, const BOOL bCloseByRemote); //= 0;
    
    virtual bool GetIDFromFrame(CConnectionContext &ConnectionContext) = 0;
	virtual bool OnReceive(CConnectionContext &ConnectionContext, char *pcReceivedBuffer, DWORD &dwReceivedBufferSize, DWORD &dwReceivedBufferCurrentIndex) = 0;
};
