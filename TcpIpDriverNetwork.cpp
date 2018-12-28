#include "stdafx.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverNetwork.h"
#include "TcpIpDriverEquipment.h"

// Called when starting the communication
_ProtRet TcpIpDriverNetwork::Start_Async(_ProtStartNetworkCmd *pStartNetworkCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::Start_Async Name=%s", m_pCwNetwork->GetGlobalName());
    pStartNetworkCmd->Ack();
    return PR_CMD_PROCESSED;
}

// Referred to the judgment of the communication
_ProtRet TcpIpDriverNetwork::Stop_Async(_ProtStopNetworkCmd *pStopNetworkCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::Stop_Async", m_pCwNetwork->GetGlobalName());
    pStopNetworkCmd->Ack();
    return PR_CMD_PROCESSED;
}

// Called to create a new equipment
_ProtEqt *TcpIpDriverNetwork::CreateProtEqt(CW_USHORT usType)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::CreateProtEqt  Type=%d", m_pCwNetwork->GetGlobalName(), usType);
    return new TcpIpDriverEquipment;
}

CW_USHORT TcpIpDriverNetwork::GetFluxManagement()
{
    return CW_FLUX_MULTI;
}
