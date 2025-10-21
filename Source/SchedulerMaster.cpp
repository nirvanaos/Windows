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
#include "SchedulerMaster.h"
#include "app_data.h"
#include <StartupSys.h>
#include <SysManager.h>
#include <Chrono.h>
#include <ThreadWorker.h>
#include <ORB/Services.h>
#include "error2errno.h"
#include "../Port/Security.h"
#include "MessageBroker.h"

//#define DEBUG_SHUTDOWN

#ifdef DEBUG_SHUTDOWN
#include "../Port/Debugger.h"
#endif

namespace Nirvana {
namespace Core {
namespace Windows {

SchedulerProcess::SchedulerProcess (SchedulerMaster& scheduler, DWORD flags) :
	scheduler_ (scheduler),
	semaphore_ (nullptr),
	pipe_ (INVALID_HANDLE_VALUE),
	process_handle_ (nullptr),
	process_id_ (0),
	valid_cnt_ (0),
	used_cores_ (0),
	created_items_ (0),
	terminated_ ATOMIC_FLAG_INIT,
	buffers_ (scheduler.thread_count (), sizeof (SchedulerMessage::Buffer))
{
	pipe_ = CreateNamedPipeW (SCHEDULER_PIPE_NAME, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | flags,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
		PIPE_UNLIMITED_INSTANCES, 0, 0, 0, nullptr);
	if (INVALID_HANDLE_VALUE != pipe_) {
		scheduler.add_receiver (pipe_, *this);
		connect ();
	} else
		throw_INITIALIZE ();
}

SchedulerProcess::~SchedulerProcess ()
{
	if (semaphore_)
		CloseHandle (semaphore_);
	if (INVALID_HANDLE_VALUE != pipe_)
		CloseHandle (pipe_);
	if (process_handle_)
		CloseHandle (process_handle_);
}

inline
void SchedulerProcess::connect () noexcept
{
	OVERLAPPED* ovl = buffers_.begin ();
	zero (*ovl);
	if (!ConnectNamedPipe (pipe_, ovl))
		assert (ERROR_IO_PENDING == GetLastError ());
}

void SchedulerProcess::enqueue_buffer (OVERLAPPED* buf) noexcept
{
	_add_ref ();
	zero (*buf);
	if (!ReadFile (pipe_, BufferPool::data (buf), sizeof (SchedulerMessage::Buffer), nullptr, buf)) {
		uint32_t err = GetLastError ();
		if (ERROR_BROKEN_PIPE == err)
			broken_pipe ();
#ifndef NDEBUG
		else
			assert (ERROR_IO_PENDING == GetLastError ());
#endif
	}
}

inline
bool SchedulerProcess::start () noexcept
{
	assert (!semaphore_);
	if (!GetNamedPipeClientProcessId (pipe_, &process_id_))
		return false;

	if (!(process_handle_ = OpenProcess (READ_CONTROL | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE,
		FALSE, process_id_)))
		return false;

	semaphore_ = OpenSemaphoreW (SEMAPHORE_MODIFY_STATE, false,
		ObjectName (SCHEDULER_SEMAPHORE_PREFIX, process_id_));
	if (!semaphore_) {
		DisconnectNamedPipe (pipe_);
		return false;
	}

	valid_cnt_.increment ();

	OVERLAPPED* buf = buffers_.begin ();
	do {
		enqueue_buffer (buf);
		buf = buffers_.next (buf);
	} while (buf != buffers_.end ());

	return true;
}

void SchedulerProcess::terminate () noexcept
{
	CloseHandle (semaphore_);
	semaphore_ = nullptr;
	CloseHandle (pipe_);
	pipe_ = INVALID_HANDLE_VALUE;
	for (auto cnt = used_cores_.load (); cnt > 0; --cnt) {
		scheduler_.core_free ();
	}
	for (auto cnt = created_items_.load (); cnt > 0; --cnt) {
		scheduler_.delete_item (false);
	}
}

inline
bool SchedulerProcess::execute () noexcept
{
	if (valid_cnt_.increment_if_not_zero ()) {
		used_cores_.increment ();
		bool ret = true;
		if (!ReleaseSemaphore (semaphore_, 1, nullptr)) {
			ret = false;
			used_cores_.decrement ();
		}
		if (!valid_cnt_.decrement_seq ())
			terminate ();
		return ret;
	}
	return false;
}

inline
void SchedulerProcess::core_free ()
{
	used_cores_.decrement ();
	scheduler_.core_free ();
}

void SchedulerProcess::create_item (bool with_reschedule)
{
	scheduler_.create_item (with_reschedule);
	created_items_.fetch_add (with_reschedule ? 2 : 1, std::memory_order_relaxed);
}

void SchedulerProcess::delete_item (bool with_reschedule)
{
	created_items_.fetch_add (with_reschedule ? -2 : -1, std::memory_order_relaxed);
	scheduler_.create_item (with_reschedule);
}

void SchedulerProcess::schedule (DeadlineTime deadline)
{
	scheduler_.schedule (deadline, *this);
}

void SchedulerProcess::reschedule (DeadlineTime deadline, DeadlineTime old)
{
	scheduler_.reschedule (deadline, *this, old);
}

void SchedulerProcess::broken_pipe ()
{
	if (!terminated_.test_and_set ()) {
		scheduler_.process_terminated (*this);
		if (!valid_cnt_.decrement_seq ())
			terminate ();
	}
}

void SchedulerProcess::completed (OVERLAPPED* ovl, uint32_t size, uint32_t error) noexcept
{
	if (ERROR_BROKEN_PIPE == error) {
		broken_pipe ();
	} else {
		assert (!error);
		if (!error) {
			if (!size) {
				// ConnectNamedPipe completed
				if (0 == valid_cnt_.load ()) {
					assert (!size);
					if (start ())
						scheduler_.process_started (*this);
					else {
						connect ();
						return; // No _remove_ref here
					}
				}
#ifndef NDEBUG
				else
					assert (false);
#endif
			} else if (valid_cnt_.increment_if_not_zero ()) {

				static const size_t MAX_WORDS = (sizeof (SchedulerMessage::Buffer) + sizeof (LONG_PTR) - 1) / sizeof (LONG_PTR);
				LONG_PTR buf [MAX_WORDS];
				LONG_PTR* msg = (LONG_PTR*)BufferPool::data (ovl);
				real_copy (msg, msg + (size + sizeof (LONG_PTR) - 1) / sizeof (LONG_PTR), buf);

				// Enqueue buffer to read a next message.
				enqueue_buffer (ovl);

				dispatch_message (buf, size);

				if (!valid_cnt_.decrement_seq ())
					terminate ();
			}
		}
	}
	_remove_ref ();
}

void SchedulerProcess::dispatch_message (const void* msg, size_t size)
{
	switch (size) {
		case sizeof (SchedulerMessage::Tagged) : {
			auto tag = ((SchedulerMessage::Tagged*)msg)->tag;
			switch (tag) {
				case SchedulerMessage::Tagged::CREATE_ITEM:
				case SchedulerMessage::Tagged::CREATE_ITEM2:
					create_item (SchedulerMessage::Tagged::CREATE_ITEM2 == tag);
					break;
				case SchedulerMessage::Tagged::DELETE_ITEM:
				case SchedulerMessage::Tagged::DELETE_ITEM2:
					delete_item (SchedulerMessage::Tagged::DELETE_ITEM2 == tag);
					break;
				case SchedulerMessage::Tagged::CORE_FREE:
					core_free ();
					break;

				default:
					assert (false);
			}
		} break;

		case sizeof (SchedulerMessage::Schedule) :
			schedule (((SchedulerMessage::Schedule*)msg)->deadline);
			break;

		case sizeof (SchedulerMessage::ReSchedule) :
			reschedule (((SchedulerMessage::ReSchedule*)msg)->deadline,
				((SchedulerMessage::ReSchedule*)msg)->deadline_prev);
			break;

		default:
			assert (false);
	}
}

SchedulerMaster::SchedulerMaster () :
	sysdomainid_ (INVALID_HANDLE_VALUE)
{}

SchedulerMaster::~SchedulerMaster ()
{
	if (INVALID_HANDLE_VALUE != sysdomainid_) {
		CloseHandle (sysdomainid_);
		sysdomainid_ = INVALID_HANDLE_VALUE;
	}
	Pool::terminate ();
	worker_threads_.terminate ();
}

inline void SchedulerMaster::create_folders ()
{
	WinWChar path [MAX_PATH + 1];
	size_t cc = get_app_data_path (path, std::size (path), true);
	if (!cc)
		throw_INITIALIZE ();
	WinWChar* root_end = path + cc;

	static const WinWChar* const folders [] = {
		WINWCS ("var\\log"),
		WINWCS ("etc")
	};

	for (const WinWChar* const* p = folders; p != std::end (folders); ++p) {
		if (!create_app_data_folder (path, root_end, *p))
			throw_INITIALIZE ();
	}
}

inline void SchedulerMaster::cleanup_temp_files ()
{
	WinWChar buf [MAX_PATH + 1];
	DWORD cc = GetTempPathW ((DWORD)countof (buf), buf);
	if (cc) {
		const char mask [] = TEMP_MODULE_PREFIX "??????" TEMP_MODULE_EXT;
		WinWChar* name = buf + cc;
		std::copy (mask, mask + sizeof (mask), name);
		WIN32_FIND_DATAW fd;
		HANDLE hf = FindFirstFileW (buf, &fd);
		if (hf != INVALID_HANDLE_VALUE) {
			do {
				wcscpy (name, fd.cFileName);
				DeleteFileW (buf);
			} while (FindNextFileW (hf, &fd));
			FindClose (hf);
		}
	}
}

bool SchedulerMaster::run (StartupSys& startup)
{
	SetPriorityClass (GetCurrentProcess (), PROCESS_PRIORITY_CLASS);

	cur_process_id = sys_process_id = GetCurrentProcessId ();

	sysdomainid_ = open_sysdomainid (true);
	if (INVALID_HANDLE_VALUE == sysdomainid_)
		return false; // System domain is already running

	try {
		DWORD written;
		if (!WriteFile (sysdomainid_, &sys_process_id, sizeof (DWORD), &written, nullptr))
			throw_INITIALIZE ();

		cleanup_temp_files ();
		create_folders ();

		if (!create_process (FILE_FLAG_FIRST_PIPE_INSTANCE))
			throw_INITIALIZE ();

		Windows::MessageBroker::create (); // Create mailslot

		Pool::start (SCHEDULER_THREAD_PRIORITY);
		worker_threads_.run (startup, INFINITE_DEADLINE);

	} catch (...) {
		CloseHandle (sysdomainid_);
		sysdomainid_ = INVALID_HANDLE_VALUE;
		throw;
	}
	return true;
}

bool SchedulerMaster::create_process (DWORD flags) noexcept
{
	try {
		new SchedulerProcess (*this, flags);
	} catch (...) {
		assert (false);
		return false;
	}
	return true;
}

void SchedulerMaster::create_item (bool with_reschedule)
{
	Base::create_item (with_reschedule);
}

void SchedulerMaster::delete_item (bool with_reschedule) noexcept
{
	Base::delete_item (with_reschedule);
}

void SchedulerMaster::schedule (DeadlineTime deadline, Executor& executor) noexcept
{
	try {
		Port::Thread::PriorityBoost boost (SCHEDULER_THREAD_PRIORITY);
		Base::schedule (deadline, executor);
	} catch (...) {
		on_error (CORBA::SystemException::EC_NO_MEMORY);
	}
}

bool SchedulerMaster::reschedule (DeadlineTime deadline, Executor& executor, DeadlineTime old) noexcept
{
	try {
		Port::Thread::PriorityBoost boost (SCHEDULER_THREAD_PRIORITY);
		if (Base::reschedule (deadline, executor, old))
			return true;
	} catch (...) {
		on_error (CORBA::SystemException::EC_NO_MEMORY);
	}
	return false;
}

inline
void SchedulerMaster::schedule (DeadlineTime deadline, SchedulerProcess& process) noexcept
{
	try {
		Base::schedule (deadline, process);
	} catch (...) {
		on_error (CORBA::SystemException::EC_NO_MEMORY);
	}
}

inline
void SchedulerMaster::reschedule (DeadlineTime deadline, SchedulerProcess& process, DeadlineTime old) noexcept
{
	try {
		Base::reschedule (deadline, process, old);
	} catch (...) {
		on_error (CORBA::SystemException::EC_NO_MEMORY);
	}
}

class SchedulerMaster::SysDomainCall : public SysManager::AsyncCall
{
protected:
	SysDomainCall (SysManager::AsyncCallContext&& ctx, SchedulerProcess& process) :
		SysManager::AsyncCall (std::move (ctx)),
		process_ (&process)
	{}

protected:
	Ref <SchedulerProcess> process_;
};

class SchedulerMaster::ProcessStart : public SysDomainCall
{
public:
	ProcessStart (SysManager::AsyncCallContext&& ctx, SchedulerProcess& process) noexcept :
		SysDomainCall (std::move (ctx), process)
	{}

private:
	virtual void run () override;
};

class SchedulerMaster::ProcessTerminate : public SysDomainCall
{
public:
	ProcessTerminate (SysManager::AsyncCallContext&& ctx, SchedulerProcess& process) noexcept :
		SysDomainCall (std::move (ctx), process)
	{}

private:
	virtual void run () override;
};

template <class R> inline
void SchedulerMaster::call_sys_domain (SchedulerProcess& process)
{
	if (!SysManager::async_call <R> (Chrono::make_deadline (SYS_DOMAIN_CALL_DEADLINE), std::ref (process)))
		ESIOP::send_shutdown (process.process_id (), 0); // Shutdown is in the final stage
}

inline
void SchedulerMaster::process_started (SchedulerProcess& process) noexcept
{
	try {
		call_sys_domain <ProcessStart> (process);
	} catch (...) {
		// TODO: Log
		assert (false);
		TerminateProcess (process.process_handle (), -1);
	}
	create_process (0);
}

inline
void SchedulerMaster::process_terminated (SchedulerProcess& process) noexcept
{
	try {
		call_sys_domain <ProcessTerminate> (process);
	} catch (...) {
		assert (false);
		// TODO: Log
	}
}

void SchedulerMaster::ProcessStart::run ()
{
	try {
		HANDLE process = process_->process_handle ();
		USHORT platform;
		USHORT process_machine, host_machine;
		if (!IsWow64Process2 (process, &process_machine, &host_machine))
			throw_last_error ();
		platform = (IMAGE_FILE_MACHINE_UNKNOWN == process_machine) ? host_machine : process_machine;

		HANDLE token;
		if (!OpenProcessToken (process, TOKEN_READ, &token))
			throw_last_error ();

		Port::Security::Context context ((Port::Security::Context::ABI)(uintptr_t)token);
		sys_manager ().domain_created (process_->process_id (), platform, context.security_id ());
	} catch (...) {
		// TODO: Log
		assert (false);
		TerminateProcess (process_->process_handle (), -1);
	}
}

void SchedulerMaster::ProcessTerminate::run ()
{
	sys_manager ().domain_destroyed (process_->process_id ());
}

void SchedulerMaster::shutdown () noexcept
{
#ifdef DEBUG_SHUTDOWN
	Port::Debugger::output_debug_string (Debugger::DebugEvent::DEBUG_INFO, "Shutdown 2\n");
#endif
	Pool::terminate ();
	worker_threads_.shutdown ();
}

void SchedulerMaster::worker_thread_proc () noexcept
{
	worker_threads_.thread_proc ();
}

void SchedulerMaster::WorkerThreads::completed (_OVERLAPPED* ovl, uint32_t size, uint32_t error) noexcept
{
	PortableServer::Servant_var <Executor> ref (reinterpret_cast <Executor*> (ovl));
	static_cast <ThreadWorker&> (Thread::current ()).execute (std::move (ref));
	SchedulerMaster::singleton ().core_free ();
}

}
}
}
