#include "stdafx.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverEquipment.h"
#include "TcpIpDriverFrame.h"

_ProtRet TcpIpDriverEquipment::Start_Async(_ProtStartEqtCmd *pStartEqtCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::Start_Async", m_pCwEqt->GetGlobalName());

    m_ucAddress = (unsigned char) pStartEqtCmd->m_EqtAddress;
    m_usTransactionId = rand();

    //Get protocol user attributes (method similar on Network class with m_pCwNetwork and Frame with m_pCwFrame
    CW_HANDLE hUserAttributes;
    _CwDataItem *pdiAttributes;
    unsigned char ip0,ip1,ip2,ip3;

    unsigned short usReturn = m_pCwEqt->OpenUserAttributes_Sync(&hUserAttributes);
    if (usReturn == CW_OK)
    {
         usReturn = m_pCwEqt->GetUserAttributes_Sync(hUserAttributes,0,CWIT_UI1,&pdiAttributes);
         if (usReturn == CW_OK)
            ip0 = pdiAttributes->CwValue.ucVal; 
         
         usReturn = m_pCwEqt->GetUserAttributes_Sync(hUserAttributes,1,CWIT_UI1,&pdiAttributes);
         if (usReturn == CW_OK)
            ip1 = pdiAttributes->CwValue.ucVal; 

         usReturn = m_pCwEqt->GetUserAttributes_Sync(hUserAttributes,2,CWIT_UI1,&pdiAttributes);
         if (usReturn == CW_OK)
            ip2 = pdiAttributes->CwValue.ucVal; 

         usReturn = m_pCwEqt->GetUserAttributes_Sync(hUserAttributes,3,CWIT_UI1,&pdiAttributes);
         if (usReturn == CW_OK)
            ip3 = pdiAttributes->CwValue.ucVal; 

         usReturn = m_pCwEqt->GetUserAttributes_Sync(hUserAttributes,4,CWIT_UI2,&pdiAttributes);
         if (usReturn == CW_OK)
            m_usPort = pdiAttributes->CwValue.uiVal; 

        usReturn = m_pCwEqt->GetUserAttributes_Sync(hUserAttributes,5,CWIT_UI2,&pdiAttributes);
         if (usReturn == CW_OK)
            m_ulReconnectionPeriod = 1000 * pdiAttributes->CwValue.uiVal; 
         m_pCwEqt->CloseUserAttributes_Sync(hUserAttributes);
    }
    m_strIpAddress.Format("%d.%d.%d.%d",ip0,ip1,ip2,ip3);

    m_IprotTCPIPCallBack = new IprotTCPIPCallBack(this);
    m_pcwTCPIPIntf = CreatecwTCPIPClient(m_IprotTCPIPCallBack);

    m_pcwTCPIPIntf->Start(m_strIpAddress, m_usPort, m_ulReconnectionPeriod);

    pStartEqtCmd->Ack();
    return PR_CMD_PROCESSED;
}

_ProtRet TcpIpDriverEquipment::Stop_Async(_ProtStopEqtCmd *pStopEqtCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::Stop_Async ", m_pCwEqt->GetGlobalName());

    m_pcwTCPIPIntf->Stop();
    delete m_IprotTCPIPCallBack;
    delete m_pcwTCPIPIntf;

    pStopEqtCmd->Ack();
    return PR_CMD_PROCESSED;
}

_ProtFrame *TcpIpDriverEquipment::CreateProtFrame(CW_USHORT usProtocolDataType,CW_USHORT usCwDataType)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::CreateProtFrame ProtocolDataType=%d CwDataType=%d", m_pCwEqt->GetGlobalName(), usProtocolDataType, usCwDataType);

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
    }
    return NULL;
}

int TcpIpDriverEquipment::SendRcvFrame(
    CW_LPC_UCHAR a_pcBufferToSend,
    const USHORT a_usSendBufferSize,
    CW_LPC_UCHAR a_pucReceivedBuffer,
    const USHORT a_usSizeofResponseBuffer)
{
    // Initilaisatin des data befroe sending the request
    m_usSizeofResponseBuffer = a_usSizeofResponseBuffer;
    m_iReceiveCount = 0;
    m_pcRcvBuffer = (CW_LP_UCHAR)a_pucReceivedBuffer;

   return m_pcwTCPIPIntf->SendRcvFrame(a_pcBufferToSend,a_usSendBufferSize, m_pCwEqt->GetTimeOut_Sync());
}

bool TcpIpDriverEquipment::OnReceive(
    CW_LPC_CHAR a_pcRcvBuffer,
    int a_iReceiveResult)
{
    // Make sure the reception buffer is big enough to receive the bytes received
    if ( m_iReceiveCount + a_iReceiveResult <= m_usSizeofResponseBuffer)
        memcpy( m_pcRcvBuffer+ m_iReceiveCount, a_pcRcvBuffer, a_iReceiveResult);
    else
        return true;

    m_iReceiveCount += a_iReceiveResult;

    if ( m_iReceiveCount == m_usSizeofResponseBuffer)
        return true;
    
    if ( m_iReceiveCount == 9) // Management of exception frame
    {
        if( (a_pcRcvBuffer[7] & 0x80) == 0x80)
        {
            return true;
        }
    }
    return false;
}

bool IprotTCPIPCallBack::OnReceive(
    CW_LPC_CHAR a_pcRcvBuffer,
    int a_iReceiveResult)
{
   return m_pEquipment->OnReceive(a_pcRcvBuffer, a_iReceiveResult);
}
