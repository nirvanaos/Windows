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
#ifndef NIRVANA_CORE_PORT_THREAD_H_
#define NIRVANA_CORE_PORT_THREAD_H_
#pragma once

#include "ExecContext.h"
#include "../Port/Security.h"

typedef unsigned long (__stdcall *PTHREAD_START_ROUTINE) (void* lpThreadParameter);

extern "C" __declspec (dllimport)
void* __stdcall TlsGetValue (unsigned long dwTlsIndex);

typedef void* HANDLE;

extern "C" __declspec (dllimport)
int __stdcall ImpersonateLoggedOnUser (HANDLE hToken);

extern "C" __declspec (dllimport)
int __stdcall RevertToSelf ();

namespace Nirvana {
namespace Core {

class Thread;

namespace Port {

/// Thread class port base.
class Thread
{
	Thread (const Thread&) = delete;
	Thread& operator = (const Thread&) = delete;

	///@{
	/// Members called from Core.
public:
	/// \returns If thread class is derived from Core::Thread
	/// returns a pointer of the current Core::Thread object.
	/// Otherwise returns nullptr.
	static Core::Thread* current () noexcept
	{
		return (Core::Thread*)TlsGetValue (current_);
	}

	/// Returns a pointer of the current execution context.
	/// For Windows it is implemented via FlsGetValue().
	/// If the system does not support fiber local storage,
	/// then pointer to current context must be stored as
	/// variable in Core::Thread class and updated for each
	/// ExecContext::switch_to() call. So context() can be
	/// implemented as Thread::current ().port ().context_.
	static Core::ExecContext* context () noexcept
	{
		return ExecContext::current ();
	}

	/// Wait for the thread termination
	void join () const noexcept;

	static void impersonate (const Security::Context& sec_context) noexcept
	{
		if (sec_context.empty ())
			NIRVANA_VERIFY (RevertToSelf ());
		else
			NIRVANA_VERIFY (ImpersonateLoggedOnUser (sec_context));
	}

	// Used to assign equal priority to all concurrenting threads
	class PriorityBoost
	{
	public:
		PriorityBoost () noexcept :
			PriorityBoost (current ())
		{}

		PriorityBoost (Core::Thread* thread) noexcept :
			PriorityBoost (thread, DEFAULT_PRIORITY_BOOST)
		{}

		PriorityBoost (int priority) noexcept :
			PriorityBoost (current (), priority)
		{}

		~PriorityBoost ();

	private:
		PriorityBoost (Core::Thread* thread, int priority) noexcept;

	private:
		Core::Thread* thread_;
		int saved_priority_;
	};

	///@}

public:
	static bool initialize () noexcept;
	static void terminate () noexcept;

	template <class T>
	void create (T* p, int priority) // THREAD_PRIORITY_NORMAL = 0
	{
		create ((PTHREAD_START_ROUTINE)T::thread_proc, p, priority);
	}

	static void current (Core::Thread* core_thread) noexcept;

	int priority () const noexcept;
	void priority (int i) const noexcept;

protected:
	Thread () noexcept :
		handle_ (nullptr)
	{}

	~Thread ();

private:
	void create (PTHREAD_START_ROUTINE thread_proc, void* param, int priority);

protected:
	HANDLE handle_;

	static unsigned long current_;
	static const int DEFAULT_PRIORITY_BOOST;
};

}
}
}

#endif
