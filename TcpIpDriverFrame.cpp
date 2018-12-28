#include "stdafx.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverFrame.h"
#include "TcpIpDriverNetwork.h"
#include "TcpIpDriverEquipment.h"

#define LITTLE_ENDIAN
#if defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
 
   #define htons(A) (A)
   #define htonl(A) (A)
   #define ntohs(A) (A)
   #define ntohl(A) (A)
 
#elif defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
 
   #define htons(A) ((((uint16_t)(A) & 0xff00) >> 8 ) | \\
                      (((uint16_t)(A) & 0x00ff) << 8 ))
   #define htonl(A) ((((uint32_t)(A) & 0xff000000) >> 24) | \\
                      (((uint32_t)(A) & 0x00ff0000) >> 8 ) | \\
                      (((uint32_t)(A) & 0x0000ff00) << 8 ) | \\
                      (((uint32_t)(A) & 0x000000ff) << 24))
   #define ntohs     htons
   #define ntohl     htohl
 
#else
 
   #error Either BIG_ENDIAN or LITTLE_ENDIAN must be #defined, but not both.
 
#endif


_ProtRet TcpIpDriverFrame::Start_Async(_ProtStartFrameCmd *pStartFrameCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT3, "%s::Start_Async", m_pCwFrame->GetGlobalName());

	//m_usNetErrCount= 0;	//init net error count  170731 tyh
	CW_HANDLE hUserAttributes;
	_CwDataItem *pdiAttributes;
	//unsigned char ip0,ip1,ip2,ip3;
	//unsigned short test;
	m_iCountCommErrors = 0;  //init 170731 tyh

	unsigned short usReturn = m_pCwFrame->OpenUserAttributes_Sync(&hUserAttributes);
	if (usReturn == CW_OK)
	{
		//无用
		//usReturn = m_pCwFrame->GetUserAttributes_Sync(hUserAttributes,0,CWIT_UI1,&pdiAttributes);
		//if (usReturn == CW_OK)
		//	m_usDelay = pdiAttributes->CwValue.ucVal*1000;
		//if(m_usDelay == 0)
		//	m_usDelay = 10000;

		usReturn = m_pCwFrame->GetUserAttributes_Sync(hUserAttributes,1,CWIT_UI1,&pdiAttributes);
		if (usReturn == CW_OK)
			m_iMaxCountCommError = pdiAttributes->CwValue.ucVal; 
		if((m_iMaxCountCommError == 0)||(m_iMaxCountCommError > 7))
			m_iMaxCountCommError = 2;


		//usReturn = m_pCwFrame->GetUserAttributes_Sync(hUserAttributes,2,CWIT_UI1,&pdiAttributes);
		//if (usReturn == CW_OK)
		//	ip2 = pdiAttributes->CwValue.ucVal; 

		//usReturn = m_pCwFrame->GetUserAttributes_Sync(hUserAttributes,3,CWIT_UI1,&pdiAttributes);
		//if (usReturn == CW_OK)
		//	ip3 = pdiAttributes->CwValue.ucVal; 

		//usReturn = m_pCwFrame->GetUserAttributes_Sync(hUserAttributes,4,CWIT_UI2,&pdiAttributes);
		//if (usReturn == CW_OK)
		//	test = pdiAttributes->CwValue.uiVal; 


		m_pCwFrame->CloseUserAttributes_Sync(hUserAttributes);
	}

    return PR_CMD_PROCESSED;
}

_ProtRet TcpIpDriverFrame::Stop_Async(_ProtStopFrameCmd *pStopFrameCmd)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT3, "%s::Stop_Async", m_pCwFrame->GetGlobalName());
    pStopFrameCmd->Ack();

    return PR_CMD_PROCESSED;
}

//======================================================================================

_ProtRet TcpIpDriverFrame::Read_Async(_ProtReadCmd *pReadCmd)
{
    unsigned short usSendRequestSize = BuildReadRequest(pReadCmd);
    _ProtError Error;
	unsigned short i, usvalue;
	unsigned int uivalue;

    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();
    int iRet = pEqt->SendRcvFrame(m_ucRequest,usSendRequestSize,m_ucReply,m_usNbDataByte+9);
    if (iRet!=CW_OK)
    {
        Error.ErrorClass = PEC_TIME_OUT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = 0;
        
		if(CW_ERR_CMD_NOT_PROCESSED == iRet)
			CWTRACE(PUS_PROTOC1, LVL_BIT3, "%s::Read_Async Time out",m_pCwFrame->GetGlobalName());

		if (CW_ERR_CMD_NOT_PROCESSED == iRet)	//长度出错
		{
			//添加长度出错处理
			CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ Response length error, 0x%x", pReadCmd);
		}
     
		pReadCmd->Nack(&Error);
        return PR_CMD_PROCESSED;
    }

    //Check reply if every field is ok
    unsigned short usErrorCode = CheckReply(pReadCmd->m_CwDataType);
    if (usErrorCode != NO_ERROR)
    {
        Error.ErrorClass = PEC_PROVIDE_BY_EQT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = usErrorCode;

		pEqt->printData(m_ucRequest, usSendRequestSize, m_ucReply, m_usNbDataByte+9);

        pReadCmd->Nack(&Error);
        
		return PR_CMD_PROCESSED;    
    }
    
	//添加大小端转换
	switch(pReadCmd->m_CwDataType)
	{
	case CW_DATA_WORD:
		for(i=0; i<(m_ucReply[8]/2); i++)
		{
			memcpy(&usvalue, m_ucReply+9+i*2, 2);
			usvalue = swapInt16(usvalue);
			memcpy(m_ucReply+9+i*2, &usvalue, 2);
		}
		break;

	case CW_DATA_DWORD:
		for(i=0; i<(m_ucReply[8]/2); i++)
		{
			memcpy(&uivalue, m_ucReply+9+i*4, 4);
			uivalue = swapInt32(uivalue);
			memcpy(m_ucReply+9+i*4, &uivalue, 4);
		}
		break;

	default:
		break;
	}

	memcpy(m_ucData, &m_ucReply[9], m_ucReply[8]);//TODO m_usNbDataByte);
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
        CWTRACE(PUS_PROTOC1, LVL_BIT3, "%s::Write_Async Time out",m_pCwFrame->GetGlobalName());      
        pWriteCmd->Nack(&Error);
        return PR_CMD_PROCESSED;
    }

    //Check reply if every field is ok
    unsigned short usErrorCode = CheckReply(pWriteCmd->m_CwDataType);
    if (usErrorCode != NO_ERROR)
    {
        Error.ErrorClass = PEC_PROVIDE_BY_EQT; 
        Error.ErrorLevel = PEL_WARNING;
        Error.ErrorCode = usErrorCode;

		pEqt->printData(m_ucRequest, usSendRequestSize, m_ucReply, 12);
		pEqt->m_iCountCommLenghtErrors ++;

        pWriteCmd->Nack(&Error);
        return PR_CMD_PROCESSED;    
    }

    pWriteCmd->Ack();

    return PR_CMD_PROCESSED;
}

unsigned short TcpIpDriverFrame::CheckReply(CW_USHORT ucDataType)
{
    bool bOnError=false;
	bool bWriteCmd = false;


    
# ifndef DEBUG_C
	if ((m_ucReply[6]!= m_ucRequest[6])		/*Not same device_ID*/
		|| (m_ucReply[7]!= m_ucRequest[7])	/*Not same function code */
		|| (m_ucReply[0]!= m_ucRequest[0])	/*Not same send ID_1 */
		|| (m_ucReply[1]!= m_ucRequest[1])	/*Not same send ID_2 */)
#else
	if ((m_ucReply[6]!= m_ucRequest[6])		/*Not same device_ID*/
		|| (m_ucReply[7]!= m_ucRequest[7]))	/*Not same function code */
#endif
    {
        bOnError=true;
		CWTRACE(PUS_PROTOC1, LVL_BIT4, 
			"@@@@@@@ CheckReply error:TX(%02x)(%02x)(%02x)(%02x), RX(%02x)(%02x)(%02x)(%02x)",
			m_ucRequest[0], m_ucRequest[1], m_ucRequest[6], m_ucRequest[7],
			m_ucReply[0], m_ucReply[1], m_ucReply[6], m_ucReply[7]); 
    }
	else
    {
        int iTo;
        //int iFrom;
        int iNumOfElements;

		iTo=m_ucRequest[11]+(m_ucRequest[10]<<8);
		//iFrom=m_ucRequest[8]+(m_ucRequest[9]<<8);
        switch(m_ucReply[7])
        {
		case READ_COIL:
		case READ_DIS_INPUT:
			iNumOfElements=(iTo/8)+(((iTo%8)>0)?1:0);
			break;		//tyh hack

        case READ_HOLDING_REG://Read function
		case READ_INPUT_REG:
			switch(ucDataType)
			{
			case CW_DATA_WORD:
			case CW_DATA_DWORD:
			//case CW_DATA_REAL:
				//iNumOfElements=iTo-iFrom;
				iNumOfElements = iTo*2;
				break;

			case CW_DATA_BIT:
				iNumOfElements=(iTo/8)+(((iTo%8)>0)?1:0);
				break;

			default:
				iNumOfElements = 0;
			}
            break;

		case WRITE_SINGLE_COIL:
		case WRITE_SINGLE_HOLDING_REG:

		case WRITE_MULTIPLE_COIL:
		case WRITE_MULTIPLE_HOLDING_REG:
			bWriteCmd = true;
			break;

		default:
			iNumOfElements = 0;
			break;
		}

		//Checking size match with shown in frame
		if(bWriteCmd)
			bOnError = false;
		else
		{
			if((iNumOfElements != m_ucReply[8])||(iNumOfElements+3 != m_ucReply[5]))
			{
				CWTRACE(PUS_PROTOC1, LVL_BIT4, 
					"@@@@@@@ CheckReply error:TX_L(%02x)(%02x), RX_L(%02x)(%02x)",
					iNumOfElements, iNumOfElements+3, m_ucReply[8], m_ucReply[5]);

				bOnError=true;
			}
		}
	}

    if(bOnError)
    {
        if(m_iCountCommErrors >= m_iMaxCountCommError)
        {
            m_iCountCommErrors=0;
            TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();
            _ProtError pError;
            pError.ErrorClass=PEC_RECEIVE;
            pEqt->m_pCwEqt->SetInvalid_Sync(&pError);

            // Close connection
			CWTRACE(PUS_PROTOC1, LVL_BIT4, "@@@@@@@ 报文错误次数超限，中断连接");
			pEqt->CloseConnectionContext();

            return ERROR_INVALID_BLOCK;
        }

        m_iCountCommErrors++;
        return ERROR_INVALID_BLOCK;
    }
    
    m_iCountCommErrors=0;
    return NO_ERROR;
}

unsigned short TcpIpDriverFrame::swapInt16(unsigned short value)
{
	return ((value & 0x00FF) << 8) | 
		((value & 0xFF00) >> 8);  
}

unsigned int TcpIpDriverFrame::swapInt32(unsigned int value)
{
	return ((value & 0x000000FF) << 24) | 
		((value & 0x0000FF00) << 8) |
		((value & 0x00FF0000) >> 8) |
		((value & 0xFF000000) >> 24);  
}


///////////////////////////////////////////////////////////////////////////////////////
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
    m_ucRequest[usIndex++] = READ_DIS_INPUT;//READ_COIL;
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress%0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize%0x100);

    m_usNbDataByte = (unsigned short) pReadCmd->m_DataSize/8;
    if ((pReadCmd->m_DataSize%8)!= 0)
        m_usNbDataByte++;


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

//////////////////////////////////////////////////////////////////////////////////////////////
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
    m_ucRequest[usIndex++] = READ_INPUT_REG;//READ_HOLDING_REG;
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress%0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize%0x100);

    m_usNbDataByte = (unsigned short) pReadCmd->m_DataSize*2;

    return usIndex;
}

unsigned short TcpIpDriverWordFrame::BuildWriteRequest(_ProtWriteCmd *pWriteCmd)
{
	unsigned short i, usvalue;
	unsigned short *pWriteData;

    TcpIpDriverEquipment * pEqt = (TcpIpDriverEquipment *)m_pCwFrame->GetProtEqt_Sync();

	pWriteData = (unsigned short*)pWriteCmd->m_pWriteData;		//数据指针
    m_usNbDataByte = (unsigned short) pWriteCmd->m_DataSize*2;	//数据长度，单位字节

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
		//添加大小端转换
		memcpy(&usvalue, pWriteData, 2);
		usvalue = swapInt16(usvalue);
		memcpy(pWriteData, &usvalue, 2);

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
		for(i=0; i<pWriteCmd->m_DataSize; i++)
		{
			memcpy(&usvalue, pWriteData+i, 2);
			usvalue = swapInt16(usvalue);
			memcpy(pWriteData+i, &usvalue, 2);
		}

        memcpy(&m_ucRequest[usIndex], pWriteCmd->m_pWriteData, m_usNbDataByte);
        usIndex+=m_usNbDataByte;
    }
    return usIndex;
}

//////////////////////////////////////////////////////////////////////////////////////////////
unsigned short TcpIpDriverDWordFrame::BuildReadRequest(_ProtReadCmd *pReadCmd)
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
    m_ucRequest[usIndex++] = READ_INPUT_REG;//READ_HOLDING_REG;
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress/0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_FrameAddress%0x100);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize/0x100*2);
    m_ucRequest[usIndex++] = (unsigned char)(pReadCmd->m_DataSize%0x100*2);

    m_usNbDataByte = (unsigned short) pReadCmd->m_DataSize*4;
    return usIndex;
}

unsigned short TcpIpDriverDWordFrame::BuildWriteRequest(_ProtWriteCmd *pWriteCmd)
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