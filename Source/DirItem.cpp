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
#include "DirItem.h"
#include <Security.h>
#include "error2errno.h"
#include "SecurityInfo.h"
#include "TokenInformation.h"

namespace Nirvana {
namespace Core {
namespace Windows {

const DirItem::FileSystemTraits DirItem::file_systems_ [FS_UNKNOWN] = {
	{ WINWCS ("FAT"), { 10 * TimeBase::MILLISECOND, 1 * TimeBase::DAY, 2 * TimeBase::SECOND } },
	{ WINWCS ("NTFS"), { 10 * TimeBase::MILLISECOND, 1 * TimeBase::HOUR, 2 * TimeBase::SECOND } }
};

DirItem::DirItem (DirItemId&& id) :
	id_ (std::move (id)),
	handle_ (INVALID_HANDLE_VALUE),
	type_ (FileType::none),
	file_system_type_ (FS_UNKNOWN),
	file_system_flags_ (0),
	max_component_len_ (0),
	block_size_ (0)
{}

DirItem::DirItem (FileType type) :
	handle_ (INVALID_HANDLE_VALUE),
	type_ (type),
	file_system_type_ (FS_UNKNOWN),
	file_system_flags_ (0),
	max_component_len_ (0),
	block_size_ (0)
{}

DirItem::~DirItem ()
{
	if (INVALID_HANDLE_VALUE != handle_)
		CloseHandle (handle_);
}

void* DirItem::get_handle () noexcept
{
	if (INVALID_HANDLE_VALUE == handle_) {
		handle_ = CreateFileW (path (), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

		if (INVALID_HANDLE_VALUE != handle_) {
			WinWChar buf [MAX_PATH + 1];
			if (GetVolumeInformationByHandleW (handle_, nullptr, 0, nullptr, &max_component_len_,
				&file_system_flags_, buf, (DWORD)std::size (buf))
				) {
				for (const FileSystemTraits* pfs = file_systems_; pfs != std::end (file_systems_); ++pfs) {
					if (!wcscmp (pfs->name, buf)) {
						file_system_type_ = (FileSystemType)(pfs - file_systems_);
						break;
					}
				}
			}

			query_block_size (handle_);
		}
	}
	return handle_;
}

void* DirItem::handle ()
{
	if (INVALID_HANDLE_VALUE == handle_) {
		get_handle ();
		if (INVALID_HANDLE_VALUE == handle_)
			throw_last_error ();
	}
	return handle_;
}

void DirItem::etherealize () noexcept
{
	if (INVALID_HANDLE_VALUE != handle_) {
		CloseHandle (handle_);
		handle_ = INVALID_HANDLE_VALUE;
	}
	type_ = FileType::not_found;
}

bool DirItem::_non_existent () const noexcept
{
	assert (FileType::not_found != type_);
	if (special ())
		return false;
	assert (INVALID_HANDLE_VALUE != handle_);
	_BY_HANDLE_FILE_INFORMATION att;
	if (!GetFileInformationByHandle (handle_, &att)) {
		DWORD err = GetLastError ();
		return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
	} else
		return 0 == att.nNumberOfLinks;
}

void DirItem::get_attributes (_BY_HANDLE_FILE_INFORMATION& att)
{
	assert (FileType::not_found != type_);
	assert (FileType::unknown != type_);

	if (!GetFileInformationByHandle (handle (), &att)) {
		DWORD err = error_check_exist ();
		throw_INTERNAL (make_minor_errno (error2errno (err)));
	} else if (0 == att.nNumberOfLinks)
		throw_OBJECT_NOT_EXIST (make_minor_errno (ENOENT));
}

uint32_t DirItem::error_check_exist ()
{
	DWORD err = GetLastError ();
	if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
		throw_OBJECT_NOT_EXIST (make_minor_errno (ENOENT));

	return err;
}

inline
TimeBase::TimeT DirItem::make_time (const _FILETIME& ft) noexcept
{
	return make64 (ft.dwLowDateTime, ft.dwHighDateTime) + WIN_TIME_OFFSET_SEC * TimeBase::SECOND;
}

inline
void DirItem::set_inacc (TimeBase::UtcT& t, uint64_t inac) noexcept
{
	t.inacclo ((uint32_t)inac);
	t.inacchi ((uint16_t)(inac / 0x100000000));
}

void DirItem::stat (FileStat& st)
{
	assert (FileType::not_found != type_);
	assert (FileType::unknown != type_);

	BY_HANDLE_FILE_INFORMATION att;
	get_attributes (att);

	st.id (id ());

	SecurityInfoDirItem si (handle ());

	st.owner (Port::Security::make_security_id (si.owner ()));
	st.group (Port::Security::make_security_id (si.group ()));

	uint_fast16_t mode = si.get_mode () | (FileType::directory == type_ ? S_IFDIR : S_IFREG);
	st.mode (mode);

	st.dev (att.dwVolumeSerialNumber);
	st.size (make64 (att.nFileSizeLow, att.nFileSizeHigh));
	st.nlink (att.nNumberOfLinks);
	st.creation_time ().time (make_time (att.ftCreationTime));
	st.last_access_time ().time (make_time (att.ftLastAccessTime));
	st.last_write_time ().time (make_time (att.ftLastWriteTime));

	if (file_system_type_ < FS_UNKNOWN) {
		const FileSystemTraits& fst = file_systems_ [file_system_type_];
		set_inacc (st.creation_time (), fst.time_inaccuracy.creation);
		set_inacc (st.last_access_time (), fst.time_inaccuracy.last_access);
		set_inacc (st.last_write_time (), fst.time_inaccuracy.last_write);
	}

	st.blksize (block_size ());
	st.blkcnt ((st.size () + block_size () - 1) / block_size ());
}

void DirItem::query_block_size (void* handle) noexcept
{
	if (!block_size_) {
		FILE_STORAGE_INFO fsi;
		if (GetFileInformationByHandleEx (handle, FileStorageInfo, &fsi, sizeof (fsi)))
			block_size_ = fsi.FileSystemEffectivePhysicalBytesPerSectorForAtomicity;
		else
			assert (false);
	}
}

}
}
}
