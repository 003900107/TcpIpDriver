#include "stdafx.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverFrame.h"
#include "TcpIpDriverNetwork.h"
#include "TcpIpDriverEquipment.h"

_ProtRet TcpIpDriverFrame::Start_Async(_ProtStartFrameCmd *pStartFrameCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::Start_Async", m_pCwFrame->GetGlobalName());
    pStartFrameCmd->Ack();
    return PR_CMD_PROCESSED;
}

_ProtRet TcpIpDriverFrame::Stop_Async(_ProtStopFrameCmd *pStopFrameCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT1, "%s::Stop_Async", m_pCwFrame->GetGlobalName());
    pStopFrameCmd->Ack();
    return PR_CMD_PROCESSED;
}

//======================================================================================

_ProtRet TcpIpDriverFrame::Read_Async(_ProtReadCmd *pReadCmd)
{
    unsigned short usSendRequestSize = BuildReadRequest(pReadCmd);
    _ProtError Error;

    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();
    int iRet = pEqt->SendRcvFrame(m_ucRequest,usSendRequestSize,m_ucReply,m_usNbDataByte+9);
    if (iRet!=CW_OK)
    {
        Error.ErrorClass = PEC_TIME_OUT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = 0;
        CWTRACE(PUS_PROTOC1, LVL_BIT0, "%s::Read_Async Time out",m_pCwFrame->GetGlobalName());      
        pReadCmd->Nack(&Error);
        return PR_CMD_PROCESSED;
    }
    unsigned short usErrorCode = CheckReply();
    if (usErrorCode != NO_ERROR)
    {
        Error.ErrorClass = PEC_PROVIDE_BY_EQT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = usErrorCode;
        pReadCmd->Nack(&Error);
        return PR_CMD_PROCESSED;    
    }
    memcpy(m_ucData,&m_ucReply[9],m_usNbDataByte);
    pReadCmd->Ack(m_ucData);
    return PR_CMD_PROCESSED;
}

_ProtRet TcpIpDriverFrame::Write_Async(_ProtWriteCmd *pWriteCmd)
{
    unsigned short usSendRequestSize = BuildWriteRequest(pWriteCmd);
    _ProtError Error;

    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();
    int iRet = pEqt->SendRcvFrame(m_ucRequest,usSendRequestSize,m_ucReply,12);
    if (iRet!=CW_OK)
    {
        Error.ErrorClass = PEC_TIME_OUT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = 0;
        CWTRACE(PUS_PROTOC1, LVL_BIT0, "%s::Write_Async Time out",m_pCwFrame->GetGlobalName());      
        pWriteCmd->Nack(&Error);
        return PR_CMD_PROCESSED;
    }
    unsigned short usErrorCode = CheckReply();
    if (usErrorCode != NO_ERROR)
    {
        Error.ErrorClass = PEC_PROVIDE_BY_EQT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = usErrorCode;
        pWriteCmd->Nack(&Error);
        return PR_CMD_PROCESSED;    
    }
    pWriteCmd->Ack();
    return PR_CMD_PROCESSED;
}

unsigned short TcpIpDriverFrame::CheckReply(void)
{
    if ((m_ucReply[0]!= m_ucRequest[0]) || (m_ucReply[1]!= m_ucRequest[1])) 
    {
        CWTRACE(PUS_PROTOC1, LVL_BIT0, "%s::CheckReply Error bad Id Sent:%02x %02x  Rcv: %02x %02x",m_pCwFrame->GetGlobalName(),m_ucRequest[0],m_ucRequest[1],m_ucReply[0],m_ucReply[1]);      
        return ERROR_BAD_ID;
    }

    if (m_ucReply[6]!= m_ucRequest[6]) 
    {
        CWTRACE(PUS_PROTOC1, LVL_BIT0, "%s::CheckReply Error bad Eqt address Sent:%d  Rcv: %d",m_pCwFrame->GetGlobalName(),m_ucRequest[6],m_ucReply[6]);      
        return ERROR_BAD_EQT_ADDRESS;
    }
    if (m_ucReply[7]!= m_ucRequest[7]) 
    {
        if ((m_ucReply[7]&0x80) == 0x80)
        {
            CWTRACE(PUS_PROTOC1, LVL_BIT0, "%s::CheckReply Error Exception received %d",m_pCwFrame->GetGlobalName(),m_ucReply[8]);      
            return m_ucReply[8];
        }
        else
        {
            CWTRACE(PUS_PROTOC1, LVL_BIT0, "%s::CheckReply Error bad function code ?!? Sent:%d  Rcv: %d",m_pCwFrame->GetGlobalName(),m_ucRequest[7],m_ucReply[7]);      
            return ERROR_BAD_FCT_CODE;
        }
    }
    return NO_ERROR;
}

unsigned short TcpIpDriverWordFrame::BuildReadRequest(_ProtReadCmd *pReadCmd)
{
    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();
    unsigned short usId = pEqt->GetTransactionId();
    unsigned short usIndex = 0;
    m_ucRequest[usIndex++] = (unsigned char)(usId/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(usId%0x100);
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = READ_REQUEST_SIZE;
    m_ucRequest[usIndex++] = pEqt->GetEqtAddress();
    m_ucRequest[usIndex++] = READ_HOLDING_REG;
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress%0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize%0x100);

    m_usNbDataByte = (unsigned short) pReadCmd->m_DataSize*2;
    return usIndex;
}

unsigned short TcpIpDriverBitFrame::BuildReadRequest(_ProtReadCmd *pReadCmd)
{
    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();
    unsigned short usId = pEqt->GetTransactionId();
    unsigned short usIndex = 0;
    m_ucRequest[usIndex++] = (unsigned char)(usId/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(usId%0x100);
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = READ_REQUEST_SIZE;
    m_ucRequest[usIndex++] = pEqt->GetEqtAddress();
    m_ucRequest[usIndex++] = READ_COIL;
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress%0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize%0x100);

    m_usNbDataByte = (unsigned short) pReadCmd->m_DataSize/8;
    if ((pReadCmd->m_DataSize%8)!= 0)
        m_usNbDataByte++;

    return usIndex;
}

unsigned short TcpIpDriverWordFrame::BuildWriteRequest(_ProtWriteCmd *pWriteCmd)
{
    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();

    m_usNbDataByte = (unsigned short) pWriteCmd->m_DataSize*2;

    unsigned short usId = pEqt->GetTransactionId();
    unsigned short usIndex = 0;
    m_ucRequest[usIndex++] = (unsigned char)(usId/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(usId%0x100);
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;
    if (pWriteCmd->m_DataSize == 1)
    {
        m_ucRequest[usIndex++] = READ_REQUEST_SIZE;
        m_ucRequest[usIndex++] = pEqt->GetEqtAddress();
        m_ucRequest[usIndex++] = WRITE_SINGLE_HOLDING_REG;
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)/0x100);
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)%0x100);    
        // Data
        memcpy( &m_ucRequest[usIndex],pWriteCmd->m_pWriteData,2);
        usIndex+=2;
    }
    else
    {
        m_ucRequest[usIndex++] = READ_REQUEST_SIZE +1+m_usNbDataByte;
        m_ucRequest[usIndex++] = pEqt->GetEqtAddress();
        m_ucRequest[usIndex++] = WRITE_MULTIPLE_HOLDING_REG;
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)/0x100);
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)%0x100);
        m_ucRequest[usIndex++] = (unsigned char)(pWriteCmd->m_DataSize/0x100);
        m_ucRequest[usIndex++] = (unsigned char)(pWriteCmd->m_DataSize%0x100);
        m_ucRequest[usIndex++] = (unsigned char)m_usNbDataByte;
        // Data
        memcpy( &m_ucRequest[usIndex],pWriteCmd->m_pWriteData,m_usNbDataByte);
        usIndex+=m_usNbDataByte;
    }
    return usIndex;
}

unsigned short TcpIpDriverBitFrame::BuildWriteRequest(_ProtWriteCmd *pWriteCmd)
{
   TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();

    m_usNbDataByte = (unsigned short) pWriteCmd->m_DataSize/8;
    if ((pWriteCmd->m_DataSize%8)!= 0)
        m_usNbDataByte++;

    unsigned short usId = pEqt->GetTransactionId();
    unsigned short usIndex = 0;
    m_ucRequest[usIndex++] = (unsigned char)(usId/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(usId%0x100);
    m_ucRequest[usIndex++] = 0;
    m_ucRequest[usIndex++] = 0;

    if (pWriteCmd->m_DataSize == 1)
    {
        m_ucRequest[usIndex++] = 0;
        m_ucRequest[usIndex++] = READ_REQUEST_SIZE;
        m_ucRequest[usIndex++] = pEqt->GetEqtAddress();
        m_ucRequest[usIndex++] = WRITE_SINGLE_COIL;
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)/0x100);
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)%0x100);    
        // Data
        memcpy( &m_ucRequest[usIndex],pWriteCmd->m_pWriteData,1);
        if ((m_ucRequest[usIndex]&0x01) == 0x01)
            m_ucRequest[usIndex++] = 0xFF;
        else
            m_ucRequest[usIndex++] = 0x00;

        m_ucRequest[usIndex++] = 0;
    }
    else
    {
        m_ucRequest[usIndex++] = (unsigned char)((READ_REQUEST_SIZE +1+m_usNbDataByte)/0x100);
        m_ucRequest[usIndex++] = (unsigned char)((READ_REQUEST_SIZE +1+m_usNbDataByte)%0x100);
        m_ucRequest[usIndex++] = pEqt->GetEqtAddress();
        m_ucRequest[usIndex++] = WRITE_MULTIPLE_COIL;
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)/0x100);
        m_ucRequest[usIndex++] = (unsigned char)((pWriteCmd->m_FrameAddress+pWriteCmd->m_Index)%0x100);
        m_ucRequest[usIndex++] = (unsigned char)(pWriteCmd->m_DataSize/0x100);
        m_ucRequest[usIndex++] = (unsigned char)(pWriteCmd->m_DataSize%0x100);
        m_ucRequest[usIndex++] = (unsigned char)m_usNbDataByte;
        // Data
        memcpy( &m_ucRequest[usIndex],pWriteCmd->m_pWriteData,m_usNbDataByte);
        usIndex+=m_usNbDataByte;
    }
    return usIndex;
}
