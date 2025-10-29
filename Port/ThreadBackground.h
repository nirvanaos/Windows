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
#ifndef NIRVANA_CORE_PORT_THREADBACKGROUND_H_
#define NIRVANA_CORE_PORT_THREADBACKGROUND_H_
#pragma once

#include <Thread.h>

extern "C" __declspec (dllimport)
int __stdcall SwitchToThread (void);

extern "C" __declspec (dllimport)
int __stdcall SetEvent (void* hEvent);

namespace Nirvana {
namespace Core {
namespace Port {

/// Platform-specific background thread implementation.
class ThreadBackground : public Core::Thread
{
protected:
	///@{
	/// Members called from Core.
	ThreadBackground ();
	~ThreadBackground ();

	/// Create thread.
	void start ();

	/// Execute step.
	void execute () const noexcept
	{
		resume ();
	}

	/// Yield execution to another thread that is ready to run on the current processor.
	static void yield () noexcept
	{
		SwitchToThread ();
	}

	/// Terminate thread.
	void stop () noexcept
	{
		finish_ = true;
		resume ();
	}

	///@}

private:
	friend class Nirvana::Core::Port::Thread;
	static unsigned long __stdcall thread_proc (ThreadBackground* _this) noexcept;

	void resume () const noexcept
	{
		NIRVANA_VERIFY (SetEvent (event_));
	}

private:
	void* event_;
	volatile bool finish_;
};

}
}
}

#endif
