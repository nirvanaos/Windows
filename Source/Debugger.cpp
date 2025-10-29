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
#include "../Port/Debugger.h"
#include "WinWChar.h"
#include "DebugLog.h"
#include <Nirvana/string_conv.h>

namespace Nirvana {
namespace Core {
namespace Port {

void Debugger::output_debug_string (Nirvana::Debugger::DebugEvent level, const char* msg)
{
	// Use shared string to avoid possible infinite recursion in assertions.
	if (IsDebuggerPresent ()) {
		Windows::SharedStringW ws;
		append_wide (msg, ws);
		OutputDebugStringW (ws.c_str ());
	} else if (level > Nirvana::Debugger::DebugEvent::DEBUG_INFO || Windows::DebugLog::trace_)
		Windows::DebugLog () << msg;
}

bool Debugger::debug_break (Nirvana::Debugger::DebugEvent level)
{
	if (IsDebuggerPresent ()) {
		__debugbreak ();
		return true;
	} else if (level >= Nirvana::Debugger::DebugEvent::DEBUG_ASSERT) {
		Windows::DebugLog log;
		log << "Debug break\n";
		log.stack_trace ();
	}
	return false;
}

bool Debugger::is_debugger_present () noexcept
{
	return IsDebuggerPresent ();
}

}
}
}
