#pragma once
#include "IpServer.h"

#include "log.h"
using namespace LOGGER;


class CipNetwork : public _ProtNetwork, public CIPServer
{
 
private:

	CW_UCHAR	m_ucFrameType;

	CConnectionContext *m_pConnectionContext;

	BOOL m_bErrorWatchdog;
	BOOL m_bConnexionOK;

	unsigned short m_usErrGetId;
	unsigned short m_usDelay;		//帧数据延时

	void *m_Eq;				//网络接口指针

public:
    CList<_ProtEqt*,_ProtEqt*>  llDevices; 
	//virtual _ProtFrame *CreateProtFrame	(CW_USHORT usProtocolDataType,CW_USHORT usCwDataType);
    virtual _ProtEqt *CreateProtEqt(CW_USHORT usType);
	virtual _ProtRet Start_Async(_ProtStartNetworkCmd *pStartNetworkCmd);
	virtual _ProtRet Stop_Async	(_ProtStopNetworkCmd *pStartNetworkCmd);

    virtual bool GetIDFromFrame(CConnectionContext &ConnectionContext);
    virtual bool OnReceive(CConnectionContext &ConnectionContext, char *pcReceivedBuffer, DWORD &dwReceivedBufferSize, DWORD &dwReceivedBufferCurrentIndex);

	virtual void OnAccept(CConnectionContext &ConnectionContext, const CString &strIpAddress, const USHORT &usPortNumber);
	//virtual void OnClose(CConnectionContext &ConnectionContext, const BOOL bCloseByRemote);

    CW_USHORT GetFluxManagement();

private:
	CLogger* m_pLogger;
	void initLog(CString strLogName);
};

