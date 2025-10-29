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
#include "ExecContext.inl"
#include <ExecDomain.h>
#include <Thread.h>
#include <unrecoverable_error.h>
#include "ex2signal.h"

namespace Nirvana {
namespace Core {
namespace Port {

unsigned long ExecContext::current_;
void* ExecContext::main_fiber_;
std::atomic_flag ExecContext::main_fiber_allocated_ = ATOMIC_FLAG_INIT;
Core::ExecContext* ExecContext::main_fiber_context_;

#ifndef NDEBUG
unsigned long ExecContext::dbg_main_thread_id_;
#endif

ExecContext::ExecContext (bool neutral) :
	fiber_ (nullptr)
{
	if (!neutral) {
		Core::ExecContext* core_context = static_cast <Core::ExecContext*> (this);
		if (!main_fiber_allocated_.test_and_set ()) {
			main_fiber_context_ = core_context;
			fiber_ = main_fiber_;
		} else {
			fiber_ = CreateFiberEx (0, 0, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE)fiber_proc, core_context);
			if (!fiber_)
				throw CORBA::NO_MEMORY ();
		}
	}
}

ExecContext::~ExecContext ()
{
	if (fiber_) {
		if (fiber_ == main_fiber_)
			main_fiber_allocated_.clear ();
		else {
			assert (fiber_ != GetCurrentFiber ());
			DeleteFiber (fiber_);
		}
	}
}

void ExecContext::run (ExecDomain& ed)
{
	siginfo_t siginfo;
	__try {
		ed.run ();
	} __except (Windows::ex2signal (GetExceptionInformation (), siginfo) ?
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		ed.on_crash (siginfo);
	}
}

void __stdcall ExecContext::fiber_proc (Core::ExecContext* context)
{
	assert (context);
	current (context);
	for (;;) {
		run (ExecDomain::current ());
	}
	// Fiber procedures never complete.
}

void ExecContext::main_fiber_proc ()
{
	for (;;) {
		Core::Thread& th = Core::Thread::current ();
		if (!th.executing ())
			break;
		ExecDomain* ed = th.exec_domain ();
		assert (ed);
		current (main_fiber_context_);
		run (*ed);
	}
	assert (dbg_main_thread_id_ == GetCurrentThreadId ());
}

void ExecContext::convert_to_fiber () noexcept
{
	assert (!fiber_);
	// If dwFlags parameter is zero, the floating - point state on x86 systems is not switched and data 
	// can be corrupted if a fiber uses floating - point arithmetic.
	// This causes faster switching. Neutral execution context does not use floating - point arithmetic.
	if (!(fiber_ = ConvertThreadToFiberEx (nullptr, 0)))
		unrecoverable_error (GetLastError ());

	current (static_cast <Core::ExecContext*> (this));
}

NIRVANA_NORETURN void ExecContext::raise (int signal)
{
	RaiseException (Windows::STATUS_SIGNAL_BEGIN + signal, EXCEPTION_NONCONTINUABLE, 0, nullptr);
	// Under some circumstances, RaiseException can return.
	unrecoverable_error (signal);
}

}
}
}
