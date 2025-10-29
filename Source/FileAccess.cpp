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
#include "FileAccess.h"
#include "error2errno.h"
#include "MessageBroker.h"
#include "RequestOverlapped.h"
#include <Nirvana/posix_defs.h>

namespace Nirvana {
namespace Core {
namespace Windows {

bool FileAccess::open (Port::File& file, uint32_t access, uint32_t share_mode, uint32_t creation_disposition,
	uint32_t flags_and_attributes, _SECURITY_ATTRIBUTES* psi)
{
	handle_ = file.open (access, share_mode, creation_disposition, flags_and_attributes, psi);
	if (INVALID_HANDLE_VALUE == handle_)
		return false;
	MessageBroker::completion_port ().add_receiver (handle_, *this);
	flags_ = (access & GENERIC_WRITE) ? O_RDWR : O_RDONLY;
	return true;
}

FileAccess::~FileAccess ()
{
	if (INVALID_HANDLE_VALUE != handle_)
		CloseHandle (handle_);
}

void FileAccess::completed (_OVERLAPPED* ovl, uint32_t size, uint32_t error) noexcept
{
	IO_Result result (size, 0);
	if (error)
		result.error = error2errno (error);
	RequestOverlapped::from_overlapped (*ovl).signal (result);
}

}
}
}
