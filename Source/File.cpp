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
#include "../Port/File.h"
#include "error2errno.h"
#include "win32.h"

namespace Nirvana {
namespace Core {

using namespace Windows;

namespace Port {

uint64_t File::size ()
{
	// File type can be unknown here, check.
	if (type () == FileType::not_found)
		throw CORBA::OBJECT_NOT_EXIST (ENOENT);

	BY_HANDLE_FILE_INFORMATION att;
	get_attributes (att);
	return make64 (att.nFileSizeLow, att.nFileSizeHigh);
}

FileType File::type () noexcept
{
	if (FileType::none == type_) {
		HANDLE h = get_handle ();
		if (INVALID_HANDLE_VALUE == h)
			type_ = FileType::not_found;
		else {
			switch (GetFileType (h)) {
			case FILE_TYPE_CHAR:
				type_ = FileType::character;
				break;

			case FILE_TYPE_DISK:
				type_ = FileType::regular;
				break;

			case FILE_TYPE_PIPE:
				type_ = FileType::fifo;
				break;

			default:
				type_ = FileType::unknown;
			}
		}
	}

	return type_;
}

void* File::open (uint32_t access, uint32_t share_mode, uint32_t creation_disposition,
	uint32_t flags_and_attributes, _SECURITY_ATTRIBUTES* psi)
{
	HANDLE h = CreateFileW (path (),
		access, share_mode, psi, creation_disposition, flags_and_attributes, nullptr);

	if (INVALID_HANDLE_VALUE == h)
		error_check_exist ();
	else {
		query_block_size (h);
		if (FileType::none == type_ || FileType::not_found == type_)
			type_ = FileType::regular;
	}

	return h;
}

void File::remove ()
{
	if (FileType::regular == type ()) {
		if (!DeleteFileW (path ()))
			throw_last_error ();
	}
}

}
}
}
