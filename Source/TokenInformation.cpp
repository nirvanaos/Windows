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
#include "TokenInformation.h"
#include "error2errno.h"

namespace Nirvana {
namespace Core {
namespace Windows {

TokenInformation::TokenInformation (HANDLE token, TOKEN_INFORMATION_CLASS type) :
	TokenInformation ()
{
	DWORD len = 0;
	GetTokenInformation (token, type, nullptr, 0, &len);
	if (!len)
		throw_last_error ();
	size_ = len;
	buffer_ = (TOKEN_USER*)the_memory->allocate (nullptr, size_, 0);
	if (!GetTokenInformation (token, type, buffer_, len, &len)) {
		DWORD err = GetLastError ();
		the_memory->release (buffer_, size_);
		throw_win_error_sys (err);
	}
}

void TokenInformation::clear () noexcept
{
	if (buffer_)
		the_memory->release (buffer_, size_);
}

void TokenInformation::move (TokenInformation&& src) noexcept
{
	buffer_ = src.buffer_;
	size_ = src.size_;
	src.buffer_ = nullptr;
	src.size_ = 0;
}

}
}
}
