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
#include <unrecoverable_error.h>
#include <stdio.h>
#include "DebugLog.h"

namespace Nirvana {
namespace Core {
namespace Port {

NIRVANA_NORETURN void _unrecoverable_error (int code, const char* file, unsigned line)
{
	char scode [_MAX_ITOSTR_BASE10_COUNT], sline [_MAX_ITOSTR_BASE10_COUNT];
	sprintf (scode, "%d", code);
	sprintf (sline, "%u", line);
	Windows::DebugLog () << file << '(' << sline << "): Unrecoverable error " << scode << "\n";
	Windows::DebugLog::terminate ();
	ExitProcess (-1);
}

}
}
}
