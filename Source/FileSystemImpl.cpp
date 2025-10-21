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
#include "FileSystemImpl.h"
#include "app_data.h"
#include "error2errno.h"
#include <NameService/Dir.h>
#include <NameService/File.h>

BOOL WINAPI GetUserProfileDirectoryW (
	_In_                            HANDLE  hToken,
	_Out_writes_opt_(*lpcchSize)    LPWSTR lpProfileDir,
	_Inout_                         LPDWORD lpcchSize);

using namespace CosNaming;

namespace Nirvana {
namespace Core {
namespace Windows {

NIRVANA_NORETURN void FileSystemImpl::path_to_id_error (Name& n)
{
	DWORD err = GetLastError ();
	if (!n.empty () && (ERROR_PATH_NOT_FOUND == err || ERROR_FILE_NOT_FOUND == err))
		throw NamingContext::NotFound (NamingContext::NotFoundReason::missing_node, std::move (n));
	else
		throw_win_error_sys (err);
}

DirItemId FileSystemImpl::path_to_id (const WinWChar* path, Name& last, FileType type)
{
	assert (path);
	assert (last.size () <= 1);

	DirItemId id;

	assert (path [0] && path [1]);
	if (!path [2]) {

		// Drive
		assert ('A' <= path [0] && path [0] <= 'Z');
		assert (':' == path [1]);
		if (Nirvana::FileType::unknown == type)
			type = Nirvana::FileType::directory;
		assert (type == Nirvana::FileType::directory);
		id.resize (4 * sizeof (WinWChar));
		WinWChar* dst = (WinWChar*)id.data () + 1;
		dst [0] = path [0];
		dst [1] = path [1];
		dst [2] = 0;

	} else {

		// Get final path name
		HANDLE h = CreateFileW (path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (INVALID_HANDLE_VALUE == h)
			path_to_id_error (last);

		try {
			DWORD cc = GetFinalPathNameByHandleW (h, nullptr, 0, 0);
			if (!cc)
				path_to_id_error (last);
			id.resize ((cc + 1) * sizeof (WinWChar));
			if (!GetFinalPathNameByHandleW (h, (WinWChar*)(id.data () + 2), cc, 0))
				path_to_id_error (last);

			if (Nirvana::FileType::unknown == type) {
				BY_HANDLE_FILE_INFORMATION bhfi;
				if (!GetFileInformationByHandle (h, &bhfi))
					throw_last_error ();
				if (bhfi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					type = Nirvana::FileType::directory;
				else
					type = Nirvana::FileType::regular;
			}
		} catch (...) {
			CloseHandle (h);
			throw;
		}
		CloseHandle (h);
	}

	// Save file type as first WinWChar
	*((WinWChar*)id.data ()) = (WinWChar)type;
	return id;
}

DirItemId FileSystemImpl::dir_path_to_id (const WinWChar* path)
{
	Name n;
	return path_to_id (path, n, FileType::directory);
}

DirItemId FileSystemImpl::make_special_id (SpecialDir dir)
{
	DirItemId id (4);
	WinWChar* p = (WinWChar*)id.data ();
	*(p++) = (WinWChar)Nirvana::FileType::directory;
	*(p++) = (WinWChar)dir;
	return id;
}

DirItemId FileSystemImpl::get_app_data_dir (const IDL::String& name, bool& may_cache)
{
	if (!name.size () || name.size () > 3)
		throw CORBA::BAD_PARAM ();

	WinWChar wname [4] = { 0 };
	std::copy (name.begin (), name.end (), wname);

	may_cache = true;
	return get_app_data_dir_id (wname);
}

DirItemId FileSystemImpl::get_app_data_dir_id (const WinWChar* name)
{
	WinWChar path [MAX_PATH + 1];
	size_t cc = get_app_data_folder (path, std::size (path), name, false);
	if (!cc)
		throw CORBA::UNKNOWN ();
	return dir_path_to_id (path);
}

DirItemId FileSystemImpl::get_var (const IDL::String&, bool& may_cache)
{
	may_cache = true;
	return make_special_id (SpecialDir::var);
}

DirItemId FileSystemImpl::get_mnt (const IDL::String&, bool& may_cache)
{
	may_cache = true;
	return make_special_id (SpecialDir::mnt);
}

DirItemId FileSystemImpl::get_home (const IDL::String&, bool& may_cache)
{
	may_cache = false;

	WinWChar path [MAX_PATH];
	DWORD cc = MAX_PATH;
	if (GetUserProfileDirectoryW (Security::Context::current ().port (), path, &cc))
		return dir_path_to_id (path);
	else
		throw_last_error ();
}

size_t FileSystemImpl::get_temp_path (WinWChar* buf)
{
	size_t cc = GetTempPathW (MAX_PATH + 1, buf);
	if (!cc)
		throw_last_error ();
	return cc;
}

DirItemId FileSystemImpl::get_tmp (const IDL::String&, bool& may_cache)
{
	may_cache = false;

	WinWChar buf [MAX_PATH + 1];
	get_temp_path (buf);
	return dir_path_to_id (buf);
}

DirItemId FileSystemImpl::get_sbin (const IDL::String&, bool& may_cache)
{
	may_cache = true;

	WinWChar buf [MAX_PATH + 1];
	DWORD cc = GetModuleFileNameW (nullptr, buf, (DWORD)std::size (buf));
	if (!cc)
		throw_win_error_sys (GetLastError ());
	*(char*)wcsrchr (buf, '\\') = 0;

	return dir_path_to_id (buf);
}

}
}
}
