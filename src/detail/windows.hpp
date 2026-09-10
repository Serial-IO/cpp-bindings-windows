#pragma once

#ifdef _WIN32

#include <winsdkver.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef STRICT
#define STRICT
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#endif

#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif

#include <sdkddkver.h>
#include <windows.h>

#endif
