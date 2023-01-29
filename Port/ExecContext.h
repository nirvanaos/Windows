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
#ifndef NIRVANA_CORE_PORT_EXECCONTEXT_H_
#define NIRVANA_CORE_PORT_EXECCONTEXT_H_
#pragma once

#include <Nirvana/Nirvana.h>
#include <atomic>

extern "C" __declspec (dllimport)
void __stdcall DeleteFiber (void*);

extern "C" __declspec (dllimport)
void __stdcall SwitchToFiber (void* lpFiber);

extern "C" __declspec (dllimport)
void* __stdcall FlsGetValue (unsigned long dwFlsIndex);

namespace Nirvana {
namespace Core {

class ExecContext;
class ExecDomain;

namespace Port {

/// Implements execution context
class ExecContext
{
	///@{
	/// Members called from Core.
public:
	/// \returns Current execution context.
	static Core::ExecContext* current () NIRVANA_NOEXCEPT
	{
		return (Core::ExecContext*)FlsGetValue (current_);
	}

	/// Constructor.
	/// 
	/// \param neutral `true` if neutral context is created.
	ExecContext (bool neutral);

	/// Destructor.
	~ExecContext ();

	/// Raise signal. raise(SIGABRT) to abort process.
	/// Dangerous method used for POSIX compatibility.
	NIRVANA_NORETURN void raise (int signal);

protected:
	/// Switch to this context.
	void switch_to () NIRVANA_NOEXCEPT
	{
		assert (fiber_);
		SwitchToFiber (fiber_);
	}

	///@}

public:
	void convert_to_fiber () NIRVANA_NOEXCEPT;
	void convert_to_thread () NIRVANA_NOEXCEPT;

	void attach (void* fiber) NIRVANA_NOEXCEPT
	{
		assert (!fiber_);
		fiber_ = fiber;
	}

	void* detach () NIRVANA_NOEXCEPT
	{
		void* f = fiber_;
		fiber_ = nullptr;
		return f;
	}

	static bool initialize () NIRVANA_NOEXCEPT;
	static void terminate () NIRVANA_NOEXCEPT;

	static void current (Core::ExecContext* context) NIRVANA_NOEXCEPT;

	static void main_fiber_proc ();

	static void* main_fiber () NIRVANA_NOEXCEPT
	{
		return main_fiber_;
	}

private:
	static void run (ExecDomain& ed);
	static void __stdcall fiber_proc (Core::ExecContext* context);

private:
	static unsigned long current_;
	static void* main_fiber_;
	static std::atomic_flag main_fiber_allocated_;
	static Core::ExecContext* main_fiber_context_;
#ifdef _DEBUG
	static unsigned long dbg_main_thread_id_;
#endif

	void* fiber_;
};

}
}
}

#endif
