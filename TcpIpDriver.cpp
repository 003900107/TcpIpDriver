///////////////////////////////////////////
// DLL Entry point
///////////////////////////////////////////

#include "stdafx.h"
#include "TcpIpDriver.h"
#include "TcpIpDriverNetwork.h"

// Display the driver name and version in the Event viewer
CW_C_EXPORT void GetProtocolVersion(struct _CwVersion *pProtVersion)
{
    CWTRACE(PUS_PROTOC1, LVL_BIT0, "TcpIpDriver.GetProtocolVersion");
    pProtVersion->MajorVersion = CWPI_MAJOR_VERSION;
    pProtVersion->MinorVersion = CWPI_MINOR_VERSION;
    pProtVersion->BuildNumber  = 0L;
    _tcscpy_s(pProtVersion->VendorInfo, CWPI_VENDOR_INFO);
}

// Entry point. Instanciation of the Driver network class
CW_C_EXPORT _ProtNetwork *CreateProtNetwork(CW_USHORT usType)
{
    return new TcpIpDriverNetwork;
}
