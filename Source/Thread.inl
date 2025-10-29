/// \file
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
#ifndef NIRVANA_CORE_WINDOWS_THREAD_INL_
#define NIRVANA_CORE_WINDOWS_THREAD_INL_
#pragma once

#include "../Port/Thread.h"
#include "ExecContext.inl"

namespace Nirvana {
namespace Core {
namespace Port {

inline
bool Thread::initialize () noexcept
{
	if (TLS_OUT_OF_INDEXES == (current_ = TlsAlloc ()))
		return false;
	return ExecContext::initialize ();
}

inline
void Thread::terminate () noexcept
{
	ExecContext::terminate ();
	TlsFree (current_);
}

inline
void Thread::current (Core::Thread* core_thread) noexcept
{
	TlsSetValue (current_, core_thread);
}

inline
Thread::~Thread ()
{
	if (handle_)
		CloseHandle (handle_);
}

inline
void Thread::join () const noexcept
{
	if (handle_ && (GetThreadId (handle_) != GetCurrentThreadId ()))
		WaitForSingleObject (handle_, INFINITE);
}

inline
int Thread::priority () const noexcept
{
	return GetThreadPriority (handle_);
}

inline
void Thread::priority (int i) const noexcept
{
	NIRVANA_VERIFY (SetThreadPriority (handle_, i));
}

}
}
}

#endif
