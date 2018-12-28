#pragma once

class PROT_EXPORT TcpIpDriverFrame : public _ProtFrame
{
public:
    unsigned char m_ucData[1024];
    unsigned char m_ucRequest[255];
    unsigned char m_ucReply[1024];
    unsigned short m_usNbDataByte;

    int m_iCountCommErrors;
	int m_iMaxCountCommError;
	//unsigned short m_usDelay;

/*
private:
	_ProtReadCmd   pPreReadCmd;	//保存上一帧请求指针, 用于处理长度错位
	unsigned short sPreTransactionId; //保存上一帧帧编号
*/

public:
    // Called on driver start-up
    _ProtRet Start_Async(
        _ProtStartFrameCmd *pStartFrameCmd);

    // Called on driver stopped
    _ProtRet Stop_Async(
        _ProtStopFrameCmd *pStopFrameCmd);

    // Called on Read request from CimWay
    _ProtRet Read_Async(
        _ProtReadCmd *pReadCmd);

    // Called on Write request from CimWay
    _ProtRet Write_Async(
        _ProtWriteCmd *pWriteCmd);

    // TcpIpDriverFrame defined methods
    virtual unsigned short BuildReadRequest(_ProtReadCmd *pReadCmd)=0;
    virtual unsigned short BuildWriteRequest(_ProtWriteCmd *pReadCmd)=0;

    unsigned short CheckReply(CW_USHORT ucDataType);
	unsigned int   swapInt32(unsigned int value);
	unsigned short swapInt16(unsigned short value);
	
};

class PROT_EXPORT TcpIpDriverBitFrame : public TcpIpDriverFrame
{
    unsigned short BuildReadRequest(_ProtReadCmd *pReadCmd);
    unsigned short BuildWriteRequest(_ProtWriteCmd *pWriteCmd);
};

class PROT_EXPORT TcpIpDriverWordFrame : public TcpIpDriverFrame
{
    unsigned short BuildReadRequest(_ProtReadCmd *pReadCmd);
    unsigned short BuildWriteRequest(_ProtWriteCmd *pWriteCmd);
};

class PROT_EXPORT TcpIpDriverDWordFrame : public TcpIpDriverFrame
{
    unsigned short BuildReadRequest(_ProtReadCmd *pReadCmd);
    unsigned short BuildWriteRequest(_ProtWriteCmd *pWriteCmd);
};