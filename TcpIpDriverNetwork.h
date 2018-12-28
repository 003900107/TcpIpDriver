#pragma once

class PROT_EXPORT TcpIpDriverNetwork : public _ProtNetwork
{
public:

    // Called at the intitialization of the driver
    //void OnInitialize(
    //    _AD *adFile);

    // Called on driver close
    //void OnTerminate();

    // Called on driver start-up
    _ProtRet Start_Async(
        _ProtStartNetworkCmd *pStartNetworkCmd);

    // Called on driver stopped
    _ProtRet Stop_Async(
        _ProtStopNetworkCmd *pStopNetworkCmd);
    
      // Called at the intitialization in order to instanciate all the Equipment classes
    _ProtEqt *CreateProtEqt(
        CW_USHORT usType);

    // Tell the behaviour of the driver to CimWay 
    // CW_FLUX_MONO - For serial Driver : Serialisation of all the request at the network level
    // CW_FLUX_MULTI - For Tcp/Ip Driver : Serialisation of the request at the equipment level if you have 4 equipment you will e activated in parallel on your 4 equipment class.
    CW_USHORT GetFluxManagement();
};
