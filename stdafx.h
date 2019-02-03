// stdafx.h : Include file for standard system include files, or project specific
//            include files that are used frequently, but are changed infrequently.

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#include "ProtocolInfo.h"   // Protocol information

#include <cwdef.h>          // CimWay headers
#include <cwtcpipintf.h>

//#define DEBUG_C
//#define LOG_DEBUG
