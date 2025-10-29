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
#include "CompletionPort.h"
#include "CompletionPortReceiver.h"
#include "error2errno.h"

namespace Nirvana {
namespace Core {
namespace Windows {

CompletionPort::CompletionPort () :
	completion_port_ (nullptr)
{}

void CompletionPort::create (HANDLE hfile, CompletionPortReceiver* receiver)
{
	HANDLE port = CreateIoCompletionPort (hfile, completion_port_, (ULONG_PTR)receiver, thread_count ());
	if (!port)
		throw_last_error ();
	assert (completion_port_ == nullptr || completion_port_ == port);
	completion_port_ = port;
}

void CompletionPort::thread_proc ()
{
	for (;;) {
		ULONG_PTR key;
		OVERLAPPED* ovl;
		DWORD size;
		HANDLE cp = completion_port_;
		if (!cp)
			break;
		BOOL ok = GetQueuedCompletionStatus (cp, &size, &key, &ovl, INFINITE);
		if (ovl) {
			DWORD error = 0;
			if (!ok)
				error = GetLastError ();
			reinterpret_cast <CompletionPortReceiver*> (key)->completed (ovl, size, error);
		} else {
#ifndef NDEBUG
			DWORD error = GetLastError ();
			assert (ERROR_ABANDONED_WAIT_0 == error);
#endif
			break;
		}
	}
}

}
}
}
