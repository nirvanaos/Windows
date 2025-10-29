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
#include "../Port/ProtDomain.h"
#include "win32.h"
#include <Lmcons.h>
#include <Nirvana/string_conv.h>
#include "WinWChar.h"
#include "error2errno.h"
#include "win32.h"

namespace Nirvana {
namespace Core {

using namespace Windows;

namespace Port {

IDL::String ProtDomain::binary_dir ()
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

	IDL::String dir;
	append_utf8 (path, name, dir);
	return dir;
}

}
}
}
