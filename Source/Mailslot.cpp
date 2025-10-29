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
#include "Mailslot.h"
#include "win32.h"

namespace Nirvana {
namespace Core {
namespace Windows {

bool Mailslot::open (const WinWChar* name)
{
	attach (CreateFileW (name, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
	if (!is_valid ()) {
		DWORD err = GetLastError ();
		if (ERROR_FILE_NOT_FOUND == err)
			return false;
		throw_INTERNAL ();
	}
	return true;
}

void Mailslot::send (const void* msg, uint32_t size)
{
	DWORD cb;
	if (!WriteFile (*this, msg, size, &cb, nullptr))
		throw_COMM_FAILURE ();
}

}
}
}
