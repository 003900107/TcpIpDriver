#pragma once

// The _WIN32_WINNT macro defines the minimum required platform. The minimum required platform is
// the earliest version of Windows that has the necessary features to run the application. The
// _WIN32_WINNT macro works by enabling all features available on platform versions up to and
// including the version specified.

// Specifies that the minimum required platform is Windows Vista / Windows Server 2008
#define _WIN32_WINNT 0x0600

#include <SDKDDKVer.h>
