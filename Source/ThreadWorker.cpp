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
#include "ThreadWorker.h"
#include "WorkerThreads.h"
#include "WorkerSemaphore.h"
#include "CompletionPort.h"
#include <ExecDomain.h>
#include <Startup.h>
#include "SchedulerBase.h"

namespace Nirvana {
namespace Core {
namespace Windows {

unsigned long __stdcall ThreadWorker::thread_proc (ThreadWorker* _this)
{
	Core::Thread& thread = static_cast <Core::Thread&> (*_this);
	Port::Thread::current (&thread);
	thread.neutral_context ().port ().convert_to_fiber ();
	SchedulerBase::singleton ().worker_thread_proc ();
	Port::ExecContext::convert_to_thread ();
	thread.neutral_context ().port ().detach (); // Prevent DeleteFiber in ~ExecContext ()
	return 0;
}

struct ThreadWorker::MainNeutralFiberParam
{
	Startup& startup;
	DeadlineTime deadline;
	ThreadWorker* other_workers;
	size_t other_worker_cnt;
};

void CALLBACK ThreadWorker::main_neutral_fiber_proc (MainNeutralFiberParam* param)
{
	Port::ExecContext::current (&Core::Thread::current ().neutral_context ());
	// Schedule startup runnable
	param->startup.launch (param->deadline);
	// Do worker thread proc.
	SchedulerBase::singleton ().worker_thread_proc ();
	// Wait while all other workers terminate
	for (ThreadWorker* t = param->other_workers, *end = t + param->other_worker_cnt; t != end; ++t) {
		t->join ();
	}
	// Switch back to main fiber.
	SwitchToFiber (Port::ExecContext::main_fiber ());
}

void ThreadWorker::run_main (Startup& startup, DeadlineTime deadline, ThreadWorker* other_workers,
	size_t other_worker_cnt)
{
	HANDLE process = GetCurrentProcess ();
	HANDLE thread_handle;
	NIRVANA_VERIFY (DuplicateHandle (process, GetCurrentThread (), process, &thread_handle, 0, false, DUPLICATE_SAME_ACCESS));
	handle_ = thread_handle;
	Core::Thread& thread = static_cast <Core::ThreadWorker&> (*this);
	Port::Thread::current (&thread);

	// Create fiber for neutral context
	MainNeutralFiberParam param { startup, deadline, other_workers, other_worker_cnt };
	void* worker_fiber = CreateFiberEx (NEUTRAL_FIBER_STACK_COMMIT, NEUTRAL_FIBER_STACK_RESERVE, 0,
		(LPFIBER_START_ROUTINE)main_neutral_fiber_proc, &param);
	if (!worker_fiber)
		throw_NO_MEMORY ();
	thread.neutral_context ().port ().attach (worker_fiber); // worker_fiber will be deleted in the neutral context destructor

#ifndef NDEBUG
	DWORD dbg_main_thread = GetCurrentThreadId ();
#endif

	// Set main thread priority to WORKER_THREAD_PRIORITY
	int prio = GetThreadPriority (GetCurrentThread ());
	SetThreadPriority (GetCurrentThread (), Windows::WORKER_THREAD_PRIORITY);

	// Switch to neutral context and run main_neutral_fiber_proc
	SwitchToFiber (worker_fiber);

	// Do fiber_proc for this worker thread
	Port::ExecContext::main_fiber_proc ();

	assert (dbg_main_thread == GetCurrentThreadId ());
	CloseHandle (handle_);
	handle_ = nullptr; // Prevent join to self.

	Port::Thread::current (nullptr);
	Port::ExecContext::current (nullptr);

	// Restore priority and release resources
	SetThreadPriority (GetCurrentThread (), prio);
}

}
}
}
