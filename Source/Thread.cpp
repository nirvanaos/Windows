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
#include "Thread.inl"
#include <Thread.h>
#include "error2errno.h"

namespace Nirvana {
namespace Core {
namespace Port {

unsigned long Thread::current_;
const int Thread::DEFAULT_PRIORITY_BOOST = Windows::THREAD_PRIORITY_MAX;

void Thread::create (PTHREAD_START_ROUTINE thread_proc, void* param, int priority)
{
	assert (!handle_);
	handle_ = CreateThread (nullptr, Windows::NEUTRAL_FIBER_STACK_RESERVE, thread_proc, param,
		STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
	if (!handle_)
		throw_NO_MEMORY ();
	if (THREAD_PRIORITY_NORMAL != priority)
		NIRVANA_VERIFY (SetThreadPriority (handle_, priority));
}

void Thread::join () const noexcept
{
	if (handle_ && (GetThreadId (handle_) != GetCurrentThreadId ()))
		WaitForSingleObject (handle_, INFINITE);
}

Thread::PriorityBoost::PriorityBoost (Core::Thread* thread, int priority) noexcept :
	thread_ (thread)
{
	// In some cases (startup, test), thread nay be null.
	if (thread) {
		if ((saved_priority_ = thread->port ().priority ()) != priority)
			thread->port ().priority (priority);
		else
			thread_ = nullptr; // Do not restore
	}
}

Thread::PriorityBoost::~PriorityBoost ()
{
	if (thread_)
		thread_->port ().priority (saved_priority_);
}

}
}
}
