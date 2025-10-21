/// \file
/*
* Nirvana Core. Windows port library.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2025 Igor Popov.
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
#ifndef NIRVANA_CORE_WINDOWS_TIMER_H_
#define NIRVANA_CORE_WINDOWS_TIMER_H_
#pragma once

#include "win32.h"
#include "../Port/Thread.h"
#include "error2errno.h"
#include <AtomicCounter.h>
#include <StaticallyAllocated.h>
#include <ObjectPool.h>

namespace Nirvana {
namespace Core {

namespace Port {

class Timer;

}

namespace Windows {

class Timer : public StackElem,
	private Port::Thread,
	public SharedObject
{
	friend class ObjectCreator <Timer*>;
	using Thread = Port::Thread;
	friend class Port::Thread;

public:
	static void initialize ();
	static void terminate () noexcept;

	static Timer& create (Port::Timer& facade);
	void release () noexcept;
	void set (const LARGE_INTEGER& due_time, LONG period, bool resume);
	void cancel () noexcept;

private:
	Timer ();
	~Timer ();

	static unsigned long __stdcall thread_proc (Timer* _this) noexcept;

	void queue_apc (void (__stdcall *f) (Timer* _this)) noexcept
	{
		QueueUserAPC ((PAPCFUNC)f, Thread::handle_, (ULONG_PTR)this);
	}

	static void __stdcall apc_destruct (Timer* _this) noexcept;
	static void __stdcall apc_release (Timer* _this) noexcept;
	static void __stdcall apc_create (Timer* _this) noexcept;

	void release_to_pool () noexcept;

private:
	enum Handle
	{
		HANDLE_TIMER,
		HANDLE_TERMINATE,

		HANDLE_CNT
	};

	HANDLE handles_ [HANDLE_CNT];

	Port::Timer* facade_;

	volatile bool destruct_;
	volatile bool pooled_;

	class Global;
	static StaticallyAllocated <Global> global_;
};

}
}
}

#endif
