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
#include "SchedulerSlave.h"
#include <Scheduler.h>
#include <StartupProt.h>
#include <ThreadWorker.h>
#include "SchedulerMessage.h"
#include "app_data.h"
#include "ObjectName.h"
#include "MessageBroker.h"
#include <unrecoverable_error.h>

//#define DEBUG_SHUTDOWN

#ifdef DEBUG_SHUTDOWN
#include "../Port/Debugger.h"
#endif

namespace Nirvana {
namespace Core {
namespace Windows {

SchedulerSlave::SchedulerSlave () :
	sys_process_ (nullptr),
	terminate_event_ (nullptr),
	watchdog_thread_ (nullptr),
	scheduler_pipe_ (INVALID_HANDLE_VALUE),
	queue_ (Port::SystemInfo::hardware_concurrency ())
{}

SchedulerSlave::~SchedulerSlave ()
{
	if (watchdog_thread_) {
		SetEvent (terminate_event_);
		WaitForSingleObject (watchdog_thread_, INFINITE);
		CloseHandle (watchdog_thread_);
	}

	if (INVALID_HANDLE_VALUE != scheduler_pipe_)
		CloseHandle (scheduler_pipe_);

	worker_threads_.terminate ();
	
	if (sys_process_)
		CloseHandle (sys_process_);
	if (terminate_event_)
		CloseHandle (terminate_event_);
}

bool SchedulerSlave::run (StartupProt& startup, DeadlineTime startup_deadline)
{
	SetPriorityClass (GetCurrentProcess (), PROCESS_PRIORITY_CLASS);

	if (!get_sys_process_id ())
		return false; // System domain is not running

	if (!(terminate_event_ = CreateEventW (nullptr, TRUE, FALSE, nullptr)))
		throw_INITIALIZE ();

	if (!(sys_process_ = OpenProcess (SYNCHRONIZE | PROCESS_DUP_HANDLE, FALSE, sys_process_id)))
		throw_COMM_FAILURE ();

	if (!(watchdog_thread_ = CreateThread (nullptr, NEUTRAL_FIBER_STACK_RESERVE, s_watchdog_thread_proc,
		this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr)))
		throw_INITIALIZE ();

	cur_process_id = GetCurrentProcessId ();
	HANDLE sem = CreateSemaphoreW (nullptr, 0, (LONG)Port::SystemInfo::hardware_concurrency (),
		ObjectName (SCHEDULER_SEMAPHORE_PREFIX, cur_process_id));
	if (!sem)
		throw_INITIALIZE ();

	worker_threads_.semaphore (sem);

	Windows::MessageBroker::create (); // Create mailslot

	scheduler_pipe_ = CreateFileW (SCHEDULER_PIPE_NAME, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (INVALID_HANDLE_VALUE == scheduler_pipe_)
		throw_COMM_FAILURE ();

	worker_threads_.run (startup, startup_deadline);

	return true;
}

void SchedulerSlave::create_item (bool with_reschedule)
{
	try {
		send (SchedulerMessage::Tagged (with_reschedule ?
			SchedulerMessage::Tagged::CREATE_ITEM2
			:
			SchedulerMessage::Tagged::CREATE_ITEM
		));
	} catch (const CORBA::SystemException& ex) {
		on_error (ex.__code ());
	}
	queue_.create_item ();
	if (with_reschedule)
		queue_.create_item ();
}

void SchedulerSlave::delete_item (bool with_reschedule) noexcept
{
	try {
		send (SchedulerMessage::Tagged (with_reschedule ?
			SchedulerMessage::Tagged::DELETE_ITEM2
			:
			SchedulerMessage::Tagged::DELETE_ITEM
		));
	} catch (const CORBA::SystemException& ex) {
		on_error (ex.__code ());
	}
	queue_.delete_item ();
	if (with_reschedule)
		queue_.delete_item ();
}

void SchedulerSlave::schedule (DeadlineTime deadline, Executor& executor) noexcept
{
	try {
		Port::Thread::PriorityBoost boost;
		NIRVANA_VERIFY (queue_.insert (deadline, &executor));
		send (SchedulerMessage::Schedule (deadline));
	} catch (const CORBA::SystemException& ex) {
		on_error (ex.__code ());
	}
}

bool SchedulerSlave::reschedule (DeadlineTime deadline, Executor& executor, DeadlineTime old) noexcept
{
	try {
		Port::Thread::PriorityBoost boost;
		if (!queue_.reorder (deadline, &executor, old))
			return false;
		send (SchedulerMessage::ReSchedule (deadline, old));
	} catch (const CORBA::SystemException& ex) {
		on_error (ex.__code ());
		return false;
	}
	return true;
}

void SchedulerSlave::shutdown () noexcept
{
#ifdef DEBUG_SHUTDOWN
	Port::Debugger::output_debug_string (Debugger::DebugEvent::DEBUG_INFO, "Shutdown 2\n");
#endif

	CloseHandle (scheduler_pipe_);
	scheduler_pipe_ = INVALID_HANDLE_VALUE;

	worker_threads_.shutdown ();
}

void SchedulerSlave::worker_thread_proc () noexcept
{
	worker_threads_.thread_proc (*this);
}

inline
void SchedulerSlave::core_free () noexcept
{
	try {
		send (SchedulerMessage::Tagged (SchedulerMessage::Tagged::CORE_FREE));
	} catch (const CORBA::SystemException& ex) {
		on_error (ex.__code ());
	}
}

void SchedulerSlave::execute () noexcept
{
	ThreadWorker& worker = static_cast <ThreadWorker&> (Thread::current ());

	Ref <Executor> executor;
	{
		Port::Thread::PriorityBoost boost (&worker);
		NIRVANA_VERIFY (queue_.delete_min (executor));
	}
	worker.execute (std::move (executor));

	core_free ();
}

DWORD WINAPI SchedulerSlave::s_watchdog_thread_proc (void* _this)
{
	SchedulerSlave* p = (SchedulerSlave*)_this;
	HANDLE wait_handles [2] = { p->terminate_event_, p->sys_process_ };
	if ((WAIT_OBJECT_0 + 1) == WaitForMultipleObjects (2, wait_handles, FALSE, INFINITE)) {
		// System process was terminated unexpectedly.
		DWORD exit_code = -1;
		GetExitCodeProcess (wait_handles [1], &exit_code);
		unrecoverable_error (exit_code);
	}
	return 0;
}

}
}
}
