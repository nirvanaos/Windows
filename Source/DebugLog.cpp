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
#include "pch.h"
#include "DebugLog.h"
#include <stdio.h>
#include "app_data.h"

//#ifndef NDEBUG
#define DEBUG_SYMBOLS
//#endif

#ifdef DEBUG_SYMBOLS
#include <DbgHelp.h>
#pragma comment (lib, "Dbghelp.lib")
#endif

namespace Nirvana {
namespace Core {
namespace Windows {

HANDLE DebugLog::handle_ = nullptr;
CRITICAL_SECTION DebugLog::cs_;
bool DebugLog::trace_ = false;

void DebugLog::initialize () noexcept
{
	InitializeCriticalSection (&cs_);
#ifdef DEBUG_SYMBOLS
	SymSetOptions (
#ifdef NDEBUG
		SYMOPT_DEFERRED_LOADS |
#endif
		SYMOPT_NO_PROMPTS | SYMOPT_FAIL_CRITICAL_ERRORS);
	char path [MAX_PATH + 1];
	GetModuleFileNameA (nullptr, path, sizeof (path));
	*(char*)strrchr (path, '\\') = '\0';
	if (!SymInitialize (GetCurrentProcess (), path, TRUE)) {
		char buf [_MAX_ITOSTR_BASE16_COUNT];
		sprintf (buf, "%x", (unsigned int)GetLastError ());
		DebugLog log;
		log << "SymInitialize failed, error 0x" << buf << '\n';
	}
#endif
}

HANDLE DebugLog::get_handle () noexcept
{
	if (!handle_) {
		WCHAR path [MAX_PATH + 1];
		size_t cc = get_app_data_folder (path, std::size (path), WINWCS ("var\\log"), true);
		if (cc) {
			WCHAR* name = path + cc;
			*(name++) = L'\\';
			SYSTEMTIME t;
			GetSystemTime (&t);
			wsprintfW (name, WINWCS ("debug%4u%02u%02u_%02u%02u%02u_%u.txt"),
				t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, GetCurrentProcessId ());
			handle_ = CreateFileW (path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
				nullptr);
			if (INVALID_HANDLE_VALUE != handle_) {
				char path [MAX_PATH + 1];
				size_t cc = GetModuleFileNameA (nullptr, path, sizeof (path));
				write (path, cc);
				char lf = '\n';
				write (&lf, 1);
			}
		} else
			handle_ = INVALID_HANDLE_VALUE;
	}
	return handle_;
}

void DebugLog::terminate () noexcept
{
	EnterCriticalSection (&cs_);
	if (handle_ && INVALID_HANDLE_VALUE != handle_) {
		CloseHandle (handle_);
		handle_ = INVALID_HANDLE_VALUE;
	}
#ifdef DEBUG_SYMBOLS
	SymCleanup (GetCurrentProcess ());
#endif
	LeaveCriticalSection (&cs_);
}

DebugLog::DebugLog ()
{
	EnterCriticalSection (&cs_);
	get_handle ();
}

DebugLog::~DebugLog ()
{
	LeaveCriticalSection (&cs_);
}

const DebugLog& DebugLog::operator << (const char* text) const noexcept
{
	write (text, strlen (text));
	return *this;
}

void DebugLog::write (const char* text, size_t len) noexcept
{
	if (INVALID_HANDLE_VALUE == handle_)
		return;

	const char* line = text;
	const char* end = text + len;
	for (;;) {
		const char* eol = std::find (line, end, '\n');

		DWORD cb;
		WriteFile (handle_, line, (DWORD)(eol - line), &cb, nullptr);
		if (eol != end) {
			const char crlf [] = "\r\n";
			WriteFile (handle_, crlf, 2, &cb, nullptr);
			line = eol + 1;
		} else
			break;
	}
}

void report_unhandled (EXCEPTION_POINTERS* pex)
{
	DWORD exc = pex->ExceptionRecord->ExceptionCode;

	DebugLog log;

	char buf [_MAX_I64TOSTR_BASE16_COUNT];
	sprintf (buf, "%x", (unsigned int)exc);
	log << "Unhandled exception 0x" << buf << '\n';
	switch (exc) {
	case EXCEPTION_ACCESS_VIOLATION:
		if (pex->ExceptionRecord->NumberParameters >= 2) {
			intptr_t address = (intptr_t)pex->ExceptionRecord->ExceptionInformation [1];
			if (pex->ExceptionRecord->ExceptionInformation [0])
				log << "Write to";
			else
				log << "Read from";
			log << " address 0x";
			sprintf (buf, "%zx", address);
			log << buf << '\n';
		} break;

	case 0xe06d7363: // Visual C++
		// Decoding the parameters of a thrown C++ exception (0xE06D7363)
		// https://devblogs.microsoft.com/oldnewthing/20100730-00/?p=13273
#ifdef _WIN64
		if (pex->ExceptionRecord->NumberParameters >= 4) {
			const BYTE* hinst = (const BYTE*)pex->ExceptionRecord->ExceptionInformation [3];
#else
		if (pex->ExceptionRecord->NumberParameters >= 3) {
			const BYTE* hinst = 0;
#endif
			const DWORD* ptr = (const DWORD*)pex->ExceptionRecord->ExceptionInformation [2];
			ptr = (const DWORD*)(hinst + ptr [3]);
			ptr = (const DWORD*)(hinst + ptr [1]);
			ptr = (const DWORD*)(hinst + ptr [1]);
			const char* class_name = (const char*)(((const void**)ptr) + 2);
			log << class_name << '\n';
		} break;
	}

	log.stack_trace ();
}

void DebugLog::stack_trace () const noexcept
{
#ifdef DEBUG_SYMBOLS
	void* stack [63];
	int frame_cnt = CaptureStackBackTrace (2, (DWORD)std::size (stack), stack, nullptr);
	if (frame_cnt <= 0)
		*this << "Stack trace is not available\n";

	IMAGEHLP_LINE64 line;
	line.SizeOfStruct = sizeof (IMAGEHLP_LINE64);
	DWORD displacement;

	HANDLE process = GetCurrentProcess ();
	for (int i = 0; i < frame_cnt; ++i) {
		if (SymGetLineFromAddr64 (process, (DWORD64)(stack [i]), &displacement, &line)) {
			char buf [_MAX_ITOSTR_BASE10_COUNT];
			sprintf (buf, "%u", (unsigned int)line.LineNumber);
			*this << line.FileName << '(' << buf << ")\n";
		} else {
			*this << "Line not found\n";
		}
	}

	if (frame_cnt == std::size (stack))
		*this << "More stack frames...\n";
#endif
}

long __stdcall unhandled_exception_filter (EXCEPTION_POINTERS* pex)
{
	report_unhandled (pex);

	DebugLog::terminate ();
	// Do not display message box
	ExitProcess (pex->ExceptionRecord->ExceptionCode);
	return EXCEPTION_CONTINUE_SEARCH;
}

}
}
}
