/// \file
/// Base definitions for Win32 API.
/*
* Nirvana Core. Windows port library.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2021 Igor Popov.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*
* Send comments and/or bug reports to:
*  popov.nirvana@gmail.com
*/
#ifndef NIRVANA_CORE_WINDOWS_WIN32_H_
#define NIRVANA_CORE_WINDOWS_WIN32_H_
#pragma once

#include "WinWChar.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NO_STRICT

// We support Win 10 only.
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _ONECORE

#ifndef _WIN32
#define _WIN32
#endif

#if (_M_X64 || _M_ARM64) && !defined (_WIN64)
#define _WIN64
#endif

extern "C" int _wcsicmp (wchar_t const* _String1, wchar_t const* _String2);

#include <windows.h>

// "interface" is used as identifier in ORB.
#ifdef interface
#undef interface
#endif

#ifdef Yield
#undef Yield
#endif

#define OBJ_NAME_PREFIX WINWCS ("Nirvana")
#define MAILSLOT_PREFIX WINWCS ("\\\\.\\mailslot\\") OBJ_NAME_PREFIX WINWCS ("\\")
#define TEMP_MODULE_PREFIX "nirvana"
#define TEMP_MODULE_EXT ".tmp"

namespace Nirvana {
namespace Core {
namespace Windows {

/** Stack size for neutral fibers.
Neutral fiber performs limited number of calls: read from mailslot, extract other fibers 
from queue and remap stack memory. Default stack size for fibers is 1MB, but for neutral
fibers we can decrease stack size to save memory. If set to 0, neutral fibers have 
default stack size.
*/
const size_t NEUTRAL_FIBER_STACK_COMMIT  = 0x01000;
const size_t NEUTRAL_FIBER_STACK_RESERVE = 0x10000;

/// Worker thread priority should be greater than normal thread priority.
const int WORKER_THREAD_PRIORITY = THREAD_PRIORITY_ABOVE_NORMAL;

/// Background thread priority is normal priority.
const int BACKGROUND_THREAD_PRIORITY = THREAD_PRIORITY_NORMAL;

/// System scheduler threads must have maximal priority.
const int SCHEDULER_THREAD_PRIORITY = THREAD_PRIORITY_TIME_CRITICAL;

/// Postman thread priority must be above worker thread priority.
const int POSTMAN_THREAD_PRIORITY = THREAD_PRIORITY_HIGHEST;

/// Timer thread priority must be above worker thread priority.
const int TIMER_THREAD_PRIORITY = THREAD_PRIORITY_HIGHEST;

/// Input/output thread priority
const int IO_THREAD_PRIORITY = THREAD_PRIORITY_HIGHEST;

/// Maximal thread priority except for the scheduler
const int THREAD_PRIORITY_MAX = THREAD_PRIORITY_HIGHEST;

const DWORD PROCESS_START_ACK_TIMEOUT = 1000; // 1 sec

const size_t PAGE_SIZE = 4096;
const size_t PAGES_PER_BLOCK = 16; // Windows allocate memory by 64K blocks
const size_t ALLOCATION_GRANULARITY = PAGE_SIZE * PAGES_PER_BLOCK;

/// Offset in seconds from 15 October 1582 00:00:00 (DCE Time)
/// to 1 January 1601 12:00:00 in seconds (Windows Time).
const uint64_t WIN_TIME_OFFSET_SEC = 574862400UI64;

#ifndef NDEBUG
const uint32_t PROCESS_PRIORITY_CLASS = ABOVE_NORMAL_PRIORITY_CLASS;
#else
const uint32_t PROCESS_PRIORITY_CLASS = HIGH_PRIORITY_CLASS;
#endif

const unsigned TIMER_POOL_MIN = 8;

}
}
}

#endif  //  _WIN32_H_
