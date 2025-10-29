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
#include "../Port/ThreadBackground.h"
#include <ThreadBackground.h>
#include <ExecDomain.h>
#include "Thread.inl"

namespace Nirvana {
namespace Core {
namespace Port {

DWORD CALLBACK ThreadBackground::thread_proc (ThreadBackground* _this) noexcept
{
	Core::ThreadBackground& thread = static_cast <Core::ThreadBackground&> (*_this);
	Port::Thread::current (&thread);
	thread.neutral_context ().port ().convert_to_fiber ();

	for (;;) {
		DWORD ret = WaitForSingleObject (_this->event_, INFINITE);
		assert (WAIT_OBJECT_0 == ret);
		if (_this->finish_)
			break;
		thread.run ();
		RevertToSelf ();
	}

	// The object may be destructed here
	thread.on_thread_proc_end ();

	ExecContext::convert_to_thread ();
	return 0;
}

ThreadBackground::ThreadBackground () :
	finish_ (false)
{
	if (!(event_ = CreateEventW (nullptr, false, false, nullptr)))
		throw_NO_MEMORY ();
}

ThreadBackground::~ThreadBackground ()
{
	neutral_context ().port ().detach (); // Prevent DeleteFiber in ~ExecContext ()

	CloseHandle (event_);
}

void ThreadBackground::start ()
{
	Thread::create (this, Windows::BACKGROUND_THREAD_PRIORITY);
}

}
}
}
