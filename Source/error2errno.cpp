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
#include "error2errno.h"
#include <algorithm>
#include <assert.h>
#include "win32.h"

// Convert Windows error code to POSIX error code.

namespace Nirvana {
namespace Core {
namespace Windows {

static const struct ErrMap
{
	unsigned w;
	int e;
} errmap [] = {
	{ ERROR_FILE_NOT_FOUND,          ENOENT },
	{ ERROR_PATH_NOT_FOUND,          ENOENT },
	{ ERROR_TOO_MANY_OPEN_FILES,     EMFILE },
	{ ERROR_ACCESS_DENIED,           EACCES },
	{ ERROR_NOT_ENOUGH_MEMORY,       ENOMEM },
	{ ERROR_INVALID_DRIVE,           ENODEV },
	{ ERROR_WRITE_PROTECT,           EROFS },
	{ ERROR_BAD_UNIT,                ENODEV },
	{ ERROR_NOT_READY,               EBUSY },
	{ ERROR_CRC,                     EIO },
	{ ERROR_SEEK,                    ESPIPE },
	{ ERROR_NOT_DOS_DISK,            ENOTDIR },
	{ ERROR_SECTOR_NOT_FOUND,        EINVAL },
	{ ERROR_WRITE_FAULT,             EIO },
	{ ERROR_READ_FAULT,              EIO },
	{ ERROR_GEN_FAILURE,             EIO },
	{ ERROR_SHARING_VIOLATION,       EACCES },
	{ ERROR_LOCK_VIOLATION,          EACCES },
	{ ERROR_SHARING_BUFFER_EXCEEDED, ENOLCK },
	{ ERROR_HANDLE_EOF,              ENODATA },
	{ ERROR_HANDLE_DISK_FULL,        ENOSPC },
	{ ERROR_NOT_SUPPORTED,           ENOSYS },
	{ ERROR_REM_NOT_LIST,            ENETUNREACH},
	{ ERROR_DUP_NAME,                EEXIST },
	{ ERROR_BAD_NETPATH,             ENOENT },
	{ ERROR_NETWORK_BUSY,            EBUSY },
	{ ERROR_DEV_NOT_EXIST,           ENOENT },
	{ ERROR_BAD_NET_RESP,            ENOSYS },
	{ ERROR_UNEXP_NET_ERR,           EIO },
	{ ERROR_NETNAME_DELETED,         ENOENT },
	{ ERROR_NETWORK_ACCESS_DENIED,   ENETUNREACH },
	{ ERROR_BAD_NET_NAME,            ENOENT },
	{ ERROR_FILE_EXISTS,             EEXIST },
	{ ERROR_CANNOT_MAKE,             EPERM },
	{ ERROR_INVALID_PARAMETER,       EINVAL },
	{ ERROR_NET_WRITE_FAULT,         EIO },
	{ ERROR_BROKEN_PIPE,             EPIPE },
	{ ERROR_OPEN_FAILED,             EIO },
	{ ERROR_DISK_FULL,               ENOSPC },
	{ ERROR_CALL_NOT_IMPLEMENTED,    ENOSYS },
	{ ERROR_INVALID_NAME,            ENOENT },
	{ ERROR_NEGATIVE_SEEK,           ESPIPE },
	{ ERROR_SEEK_ON_DEVICE,          ESPIPE },
	{ ERROR_DIR_NOT_EMPTY,           ENOTEMPTY },
	{ ERROR_PATH_BUSY,               EBUSY },
	{ ERROR_BAD_PATHNAME,            ENOENT },
	{ ERROR_BUSY,                    EBUSY },
	{ ERROR_ALREADY_EXISTS,          EEXIST },
	{ ERROR_BAD_EXE_FORMAT,          ENOEXEC },
	{ ERROR_FILENAME_EXCED_RANGE,    ENAMETOOLONG },
	{ ERROR_META_EXPANSION_TOO_LONG, EINVAL },
	{ ERROR_DIRECTORY,               ENOTDIR },
	{ ERROR_OPERATION_ABORTED,       ECANCELED },
	{ ERROR_END_OF_MEDIA,            ENOSPC },
	{ ERROR_IO_DEVICE,               EIO },
	{ ERROR_TOO_MANY_LINKS,          EMLINK },
	{ ERROR_BAD_DEVICE,              ENODEV },
	{ ERROR_CANCELLED,               EINTR },
	{ ERROR_DISK_CORRUPT,            EIO },
	{ ERROR_TIMEOUT,                 EBUSY },
	{ ERROR_NOT_CONNECTED,           ENOLINK }
};

struct ErrPred
{
	bool operator () (const ErrMap& l, unsigned r) const
	{
		return l.w < r;
	}
	bool operator () (unsigned l, const ErrMap& r) const
	{
		return l < r.w;
	}
};

int error2errno (unsigned err, int default_errno) noexcept
{
	assert (err);
	if (err) {
		const ErrMap* p = std::lower_bound (errmap, std::end (errmap), err, ErrPred ());
		if (p != std::end (errmap) && p->w == err)
			return p->e;
	}
	return default_errno;
}

NIRVANA_NORETURN void throw_win_error_sys (unsigned err)
{
	switch (err) {

	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		throw_OBJECT_NOT_EXIST (make_minor_errno (ENOENT));

	case ERROR_ACCESS_DENIED:
		throw_NO_PERMISSION (make_minor_errno (EACCES));

	case ERROR_NOT_ENOUGH_MEMORY:
		throw_NO_MEMORY (make_minor_errno (ENOMEM));

	case ERROR_INVALID_PARAMETER:
		throw_BAD_PARAM (make_minor_errno (EINVAL));

	case ERROR_CALL_NOT_IMPLEMENTED:
		throw_NO_IMPLEMENT (make_minor_errno (ENOSYS));

	case ERROR_TIMEOUT:
		throw_TIMEOUT (make_minor_errno (EBUSY));

	default:
		throw_INTERNAL (make_minor_errno (error2errno (err)));
	}
}

NIRVANA_NORETURN void throw_last_error ()
{
	throw_win_error_sys (GetLastError ());
}

}
}
}
