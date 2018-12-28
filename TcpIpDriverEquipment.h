#pragma once

class IprotTCPIPCallBack;
class IcwTCPIPIntf;

class PROT_EXPORT TcpIpDriverEquipment : public _ProtEqt
{
private:
    unsigned char m_ucAddress;
    CString m_strIpAddress;
    unsigned short m_usPort;
    unsigned long m_ulReconnectionPeriod;

    unsigned short m_usTransactionId;

    //Tcp/Ip class
    IprotTCPIPCallBack * m_IprotTCPIPCallBack;
    IcwTCPIPIntf * m_pcwTCPIPIntf;

public:
    // Called at the intitialization of the driver
    //void OnInitialize(
    //    _AD *adFile);

    // Called on driver close
    //void OnTerminate();

    // Called on driver start-up
    _ProtRet Start_Async(
        _ProtStartEqtCmd *pStartEqtCmd);

    // Called on driver stopped
    _ProtRet Stop_Async(
        _ProtStopEqtCmd *pStopEqtCmd);

    // Called at the intitialization in order to instanciate all the frame classes
    _ProtFrame *CreateProtFrame(
        CW_USHORT usProtocolDataType,
        CW_USHORT usCwDataType);

    // CallBack fonction when receiving data from Tcp/Ip
    // If return true no all data received OK
    // If return false waiting to receive more data
    bool OnReceive(
        CW_LPC_CHAR a_pcRcvBuffer,  // [in] Ptr on buffer containing the received data
        int a_iReceiveResult);      // [in] Number of bytes received

    int TcpIpDriverEquipment::SendRcvFrame(
        CW_LPC_UCHAR a_pcBufferToSend,
        const USHORT a_usSendBufferSize,
        CW_LPC_UCHAR a_pucReceivedBuffer,
        const USHORT a_usSizeofResponseBuffer);

    unsigned char GetEqtAddress(void) { return m_ucAddress; }
    unsigned short GetTransactionId(void) { return m_usTransactionId++; }

    CW_LP_UCHAR m_pcRcvBuffer;          // Ptr on the buffer into which the data has to be received
    CW_USHORT m_usSizeofResponseBuffer; // Max number of byte that can be received
    CW_USHORT m_iReceiveCount;          // Total number of bytes received
};

// Driver CallBack class definition
class IprotTCPIPCallBack : public IcwTCPIPCallBack
{
private:
    TcpIpDriverEquipment *m_pEquipment;

public:
    IprotTCPIPCallBack(
        TcpIpDriverEquipment * a_pEquipment) { m_pEquipment = a_pEquipment; }
public:

    // Called when the conneciton is established
    virtual void OnConnectionReady() {}

    // Called when the conneciton is broken 
    virtual void OnConnectionAbort() {}

    // Called when dat are received
    virtual bool OnReceive(
        CW_LPC_CHAR a_pcRcvBuffer,  // [in] Ptr on buffer containing the received data
        int a_iReceiveResult);      // [in] Number of bytes received
};
