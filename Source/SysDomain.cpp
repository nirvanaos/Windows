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
#include "../Port/SysDomain.h"
#include "../Port/SystemInfo.h"
#include "win32.h"
#include "WinWChar.h"
#include "error2errno.h"
#include <Nirvana/string_conv.h>

namespace Nirvana {
namespace Core {
using namespace Windows;
namespace Port {

const TimeBase::TimeT SysDomain::PROCESS_STARTUP_TIMEOUT;

uint32_t SysDomain::create_prot_domain (unsigned platform, const IDL::String& host, unsigned port)
{
	WinWChar path [MAX_PATH + 1];
	DWORD cc = GetModuleFileNameW (nullptr, path, (DWORD)std::size (path));
	if (!cc || cc == std::size (path))
		throw_last_error ();

	WinWChar* name = path + cc;
	while (name > path) {
		if ('\\' == *--name)
			break;
	}
	++name;

	const WinWChar x86 [] = WINWCS ("x86");
	if (platform != PLATFORM) {
		if (SystemInfo::SUPPORTED_PLATFORM_CNT <= 1)
			throw_BAD_PARAM ();

		const WinWChar* folder;
		if (platform == PLATFORM_X86)
			folder = x86;
		else
			throw_BAD_PARAM ();

		WinWChar exe [16];
		wcscpy_s (exe, name);
		name = std::copy (folder, folder + wcslen (folder), name);
		*(name++) = '\\';
		wcscpy_s (name, std::end (path) - name, exe);
	}

	StringW cmd_line;
	size_t len = wcslen (path);
	cmd_line.reserve (len + 2);
	cmd_line += '\"';
	cmd_line.append (path, len);
	cmd_line += '\"';
	StringW dir (path, name - 1);
	STARTUPINFOW si;
	zero (si);
	PROCESS_INFORMATION pi;
	if (!CreateProcessW (path, const_cast <WinWChar*> (cmd_line.data ()), nullptr, nullptr, false,
		DETACHED_PROCESS | PROCESS_PRIORITY_CLASS,
		nullptr, dir.c_str (), &si, &pi))
		throw_last_error ();

	CloseHandle (pi.hThread);

	StartingProcess proc;
	starting_map_.emplace (pi.dwProcessId, &proc);
	proc.timer_event.wait ();
	if (!proc.started)
		TerminateProcess (pi.hProcess, -1);
	CloseHandle (pi.hProcess);

	if (!proc.started)
		throw_TIMEOUT ();

	return pi.dwProcessId;
}

IDL::String SysDomain::get_platform_dir (unsigned platform)
{
	WinWChar path [MAX_PATH + 1];
	DWORD cc = GetModuleFileNameW (nullptr, path, (DWORD)std::size (path));
	if (!cc || cc == std::size (path))
		throw_last_error ();

	WinWChar* name = path + cc;
	while (name > path) {
		if ('\\' == *--name)
			break;
	}

	const WinWChar x86 [] = WINWCS ("x86");
	if (platform != PLATFORM) {
		if (SystemInfo::SUPPORTED_PLATFORM_CNT <= 1)
			throw_BAD_PARAM ();

		const WinWChar* folder;
		if (platform == PLATFORM_X86)
			folder = x86;
		else
			throw_BAD_PARAM ();

		name = std::copy (folder, folder + wcslen (folder), ++name);
	}

	IDL::String dir;
	append_utf8 (path, name, dir);
	return dir;
}

}
}
}
