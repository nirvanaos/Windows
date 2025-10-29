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
#include "app_data.h"
#include <stdlib.h>
#include <algorithm>

namespace Nirvana {
namespace Core {
namespace Windows {

uint32_t cur_process_id;
uint32_t sys_process_id;

size_t get_app_data_path (WCHAR* path, size_t size, bool create) noexcept
{
	size_t cc = GetEnvironmentVariableW (WINWCS ("ProgramData"), path, (DWORD)size);
	if (!cc)
		return 0;

	WCHAR* end = path + cc;
	while (end > path && *(end - 1) == L'\\')
		--end;

	static const WCHAR nirvana [] = WINWCS ("\\Nirvana");
	for (size_t i = 0; i < 2; ++i) {
		end = real_copy (nirvana, nirvana + std::size (nirvana), end) - 1;
		if (create && !CreateDirectoryW (path, nullptr)) {
			DWORD err = GetLastError ();
			if (ERROR_ALREADY_EXISTS != err)
				return 0;
			if (!(GetFileAttributesW (path) & FILE_ATTRIBUTE_DIRECTORY))
				return 0;
		}
	}
	static const WCHAR term [] = L"\\";
	end = real_copy (term, term + 2, end);
	return end - path - 1;
}

size_t create_app_data_folder (const WCHAR* path, WCHAR* end, const WCHAR* folder) noexcept
{
	const WCHAR* folder_end = folder + wcslen (folder);
	for (const WCHAR* dir = folder;;) {
		const WCHAR* slash = std::find (dir, folder_end, L'\\');
		end = real_copy (dir, slash, end);
		*end = L'\0';
		if (!CreateDirectoryW (path, nullptr)) {
			DWORD err = GetLastError ();
			if (ERROR_ALREADY_EXISTS != err)
				return 0;
			if (!(GetFileAttributesW (path) & FILE_ATTRIBUTE_DIRECTORY))
				return 0;
		}
		if (slash == folder_end)
			break;
		*(end++) = L'\\';
		dir = slash + 1;
	}
	return end - path;
}

size_t get_app_data_folder (WCHAR* path, size_t size, const WCHAR* folder, bool create) noexcept
{
	size_t cc = get_app_data_path (path, size, create);
	if (cc) {
		if (create)
			cc = create_app_data_folder (path, path + cc, folder);
		else {
			size_t ccf = wcslen (folder);
			if (cc + ccf < size) {
				real_copy (folder, folder + ccf + 1, path + cc);
				cc += ccf;
			} else
				return 0;
		}
	}
	return cc;
}

HANDLE open_sysdomainid (bool write)
{
	WCHAR path [MAX_PATH + 1];
	size_t cc = get_app_data_path (path, std::size (path), write);
	if (!cc)
		throw_INITIALIZE ();
	static const WCHAR sysdomainid [] = WINWCS ("sysdomainid");
	real_copy (sysdomainid, sysdomainid + std::size (sysdomainid), path + cc);
	DWORD access, share, disposition, flags;
	if (write) {
		access = GENERIC_WRITE;
		share = FILE_SHARE_READ;
		disposition = CREATE_ALWAYS;
		flags = FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;
	} else {
		access = GENERIC_READ;
		share = FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE;
		disposition = OPEN_EXISTING;
		flags = FILE_ATTRIBUTE_TEMPORARY;
	}
	HANDLE hf = CreateFileW (path, access, share, nullptr, disposition, flags, nullptr);
	if (INVALID_HANDLE_VALUE == hf) {
		DWORD err = GetLastError ();
		if (write) {
			if (ERROR_SHARING_VIOLATION == err)
				return hf;
		} else if (ERROR_FILE_NOT_FOUND == err)
			return hf;

		throw_INITIALIZE (HRESULT_FROM_WIN32 (err));
	}
	return hf;
}

bool get_sys_process_id ()
{
	HANDLE sysdomainid = open_sysdomainid (false);
	if (INVALID_HANDLE_VALUE == sysdomainid)
		return false;
	DWORD cbread = 0;
	bool OK = ReadFile (sysdomainid, &sys_process_id, sizeof (DWORD), &cbread, nullptr)
		&& sizeof (DWORD) == cbread;
	CloseHandle (sysdomainid);
	return OK;
}

}
}
}
