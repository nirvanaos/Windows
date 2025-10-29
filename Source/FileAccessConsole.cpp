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
#include "../Port/FileAccessConsole.h"
#include "error2errno.h"
#include "win32.h"

namespace Nirvana {
namespace Core {

using namespace Windows;

namespace Port {

unsigned FileAccessConsole::access_mode () noexcept
{
	return O_RDWR;
}

FileAccessConsole::Init::Init () :
	out (INVALID_HANDLE_VALUE),
	in (INVALID_HANDLE_VALUE),
	mode (O_RDWR)
{
	if (!IsDebuggerPresent ()) {
		if (!AttachConsole (ATTACH_PARENT_PROCESS))
			AllocConsole ();
	} else
		AllocConsole ();
	out = GetStdHandle (STD_OUTPUT_HANDLE);
	in = GetStdHandle (STD_INPUT_HANDLE);
	if (INVALID_HANDLE_VALUE == out && INVALID_HANDLE_VALUE == in)
		throw_last_error ();
	if (INVALID_HANDLE_VALUE == out)
		mode = O_RDONLY;
	else
		SetConsoleMode (out, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	if (INVALID_HANDLE_VALUE == in)
		mode = O_WRONLY;
	else
		SetConsoleMode (in, ENABLE_VIRTUAL_TERMINAL_INPUT);
}

FileAccessConsole::~FileAccessConsole ()
{
	FreeConsole ();
}

}
}
}
