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
#ifndef NIRVANA_CORE_WINDOWS_SCHEDULERMASTER_H_
#define NIRVANA_CORE_WINDOWS_SCHEDULERMASTER_H_
#pragma once

#include "SchedulerBase.h"
#include <SchedulerImpl.h>
#include "SchedulerMessage.h"
#include "CompletionPort.h"
#include "CompletionPortReceiver.h"
#include "ThreadPostman.h"
#include "WorkerThreads.h"
#include "ObjectName.h"
#include "BufferPool.h"
#include <CORBA/Servant_var.h>

namespace Nirvana {
namespace Core {

class StartupSys;

namespace Windows {

class SchedulerMaster;

class SchedulerProcess : 
	public CompletionPortReceiver,
	public SharedObject
{
	SchedulerProcess (SchedulerProcess&) = delete;
	SchedulerProcess& operator = (SchedulerProcess&) = delete;

public:
	SchedulerProcess (SchedulerMaster& scheduler, DWORD flags = 0);
	~SchedulerProcess ();

	void _add_ref () noexcept
	{
		ref_cnt_.increment ();
	}

	void _remove_ref () noexcept
	{
		if (!ref_cnt_.decrement ())
			delete this;
	}

	bool execute () noexcept;

	ULONG process_id () const noexcept
	{
		return process_id_;
	}

	HANDLE process_handle () const noexcept
	{
		return process_handle_;
	}

private:
	bool start () noexcept;
	void terminate () noexcept;

	bool is_alive () const noexcept
	{
		return semaphore_;
	}

	void connect () noexcept;
	void create_item (bool with_reschedule);
	void delete_item (bool with_reschedule);
	void core_free ();
	void schedule (DeadlineTime deadline);
	void reschedule (DeadlineTime deadline, DeadlineTime old);
	void enqueue_buffer (OVERLAPPED* buf) noexcept;
	void dispatch_message (const void* msg, size_t size);
	void broken_pipe ();

	virtual void completed (OVERLAPPED* ovl, uint32_t size, uint32_t error) noexcept override;

private:
	SchedulerMaster& scheduler_;
	HANDLE semaphore_;
	HANDLE pipe_;
	HANDLE process_handle_;
	ULONG process_id_;
	RefCounter ref_cnt_;
	AtomicCounter <false> valid_cnt_;
	AtomicCounter <false> used_cores_;
	// create_item and delete_item may be out of order, so we need signed counter.
	std::atomic <int> created_items_;
	std::atomic_flag terminated_;
	BufferPool buffers_;
};

typedef Ref <SchedulerProcess> SchedulerProcessRef;

class SchedulerItem
{
public:
	SchedulerItem () noexcept
	{}

	SchedulerItem (SchedulerProcess& process) noexcept :
		process_ (&process)
	{}

	SchedulerItem (Executor& executor) noexcept :
		local_executor_ (&executor)
	{}

	SchedulerProcess* process () const noexcept
	{
		return process_;
	}

	Executor* detach_executor () noexcept
	{
		return static_cast <PortableServer::Servant_var <Executor>&> (local_executor_)._retn ();
	}

	bool operator < (const SchedulerItem& rhs) const
	{
		// Each item must be unique even if executors are the same.
		return this < &rhs;
	}

private:
	SchedulerProcessRef process_;
	Ref <Executor> local_executor_;
};

/// SchedulerMaster class. 
class SchedulerMaster :
	public SchedulerBase,
	public ThreadPool <CompletionPort, ThreadPostman>,
	public SchedulerImpl <SchedulerMaster, SchedulerItem>
{
	typedef SchedulerImpl <SchedulerMaster, SchedulerItem> Base;
	typedef ThreadPool <CompletionPort, ThreadPostman> Pool;

	static const TimeBase::TimeT SYS_DOMAIN_CALL_DEADLINE = 1 * TimeBase::MILLISECOND;

public:
	SchedulerMaster ();
	~SchedulerMaster ();

	static SchedulerMaster& singleton ()
	{
		return static_cast <SchedulerMaster&> (SchedulerBase::singleton ());
	}

	/// Main loop.
	/// \param startup System domain startup runnable.
	/// \returns `false` if system domain is already running.
	bool run (StartupSys& startup);

	// Implementation of SchedulerAbstract.
	virtual void create_item (bool with_reschedule);
	virtual void delete_item (bool with_reschedule) noexcept;
	virtual void schedule (DeadlineTime deadline, Executor& executor) noexcept;
	virtual bool reschedule (DeadlineTime deadline, Executor& executor, DeadlineTime old) noexcept;
	virtual void shutdown () noexcept;

	// Implementation of SchedulerBase
	virtual void worker_thread_proc () noexcept;

	/// Called by SchedulerImpl
	bool execute (SchedulerItem item) noexcept
	{
		SchedulerProcess* process = item.process ();
		if (process)
			return process->execute ();
		else {
			Executor* executor = item.detach_executor ();
			assert (executor);
			worker_threads_.execute (*executor);
			return true;
		}
	}

	void schedule (DeadlineTime deadline, SchedulerProcess& process) noexcept;
	void reschedule (DeadlineTime deadline, SchedulerProcess& process, DeadlineTime old) noexcept;

	void process_started (SchedulerProcess& process) noexcept;
	void process_terminated (SchedulerProcess& process) noexcept;

private:
	static void create_folders ();
	static void cleanup_temp_files ();
	bool create_process (DWORD flags) noexcept;
	
	class SysDomainCall;
	class ProcessStart;
	class ProcessTerminate;

	template <class R>
	static void call_sys_domain (SchedulerProcess& process);

private:
	/// Helper class for executing in the current process.
	class WorkerThreads :
		public Windows::WorkerThreads <CompletionPort>,
		public CompletionPortReceiver
	{
	public:
		void execute (Executor& executor) noexcept
		{
			post (*this, reinterpret_cast <OVERLAPPED*> (&executor), 0);
		}

	private:
		virtual void completed (_OVERLAPPED* ovl, uint32_t size, uint32_t error) noexcept override;

	}
	worker_threads_;

	HANDLE sysdomainid_;
};

}
}
}

#endif
