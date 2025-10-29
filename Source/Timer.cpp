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
#include "Timer.h"
#include <Timer.h>
#include <timeapi.h>

namespace Nirvana {
namespace Core {

namespace Windows {

class Timer::Global
{
public:
	Global () :
		pool_ (TIMER_POOL_MIN),
		terminate_ (CreateEventW (nullptr, true, false, nullptr)),
		inactive_ (CreateEventW (nullptr, true, true, nullptr)),
		active_timer_count_ (0)
	{
		TIMECAPS tc;
		timeGetDevCaps (&tc, sizeof (tc));
		timeBeginPeriod (min_period_ = tc.wPeriodMin);
	}

	~Global ()
	{
		timeEndPeriod (min_period_);
		SetEvent (terminate_);
		WaitForSingleObject (inactive_, INFINITE);

		// Now all timers are in the pool

		CloseHandle (terminate_);
		CloseHandle (inactive_);
	}

	HANDLE terminate_event () const noexcept
	{
		return terminate_;
	}

	Timer* create ()
	{
		if (!Core::Timer::initialized ())
			throw_BAD_INV_ORDER ();

		active_cnt_inc ();

		try {
			return pool_.create ();
		} catch (...) {
			active_cnt_dec ();
			throw;
		}
	}

	void release (Timer* timer) noexcept
	{
		pool_.release (timer);
		active_cnt_dec ();
	}

	void active_cnt_inc () noexcept
	{
		if (1 == active_timer_count_.increment_seq ())
			ResetEvent (inactive_);
	}

	void active_cnt_dec () noexcept
	{
		if (0 == active_timer_count_.decrement_seq ())
			SetEvent (inactive_);
	}

	LONG min_period () const noexcept
	{
		return min_period_;
	}

private:
	ObjectPool <Timer*> pool_;
	HANDLE terminate_;
	HANDLE inactive_;
	AtomicCounter <false> active_timer_count_;
	UINT min_period_;
};

StaticallyAllocated <Timer::Global> Timer::global_;

inline void Timer::initialize ()
{
	global_.construct ();
}

inline void Timer::terminate () noexcept
{
	global_.destruct ();
}

inline Timer::Timer () :
	facade_ (nullptr),
	destruct_ (false),
	pooled_ (false)
{
	if (!(handles_ [HANDLE_TIMER] = CreateWaitableTimerW (nullptr, false, nullptr)))
		Windows::throw_last_error ();
	handles_ [HANDLE_TERMINATE] = global_->terminate_event ();

	Thread::create (this, Windows::TIMER_THREAD_PRIORITY);
}

Timer::~Timer ()
{
	queue_apc (apc_destruct);
	Thread::join ();
	CloseHandle (handles_ [HANDLE_TIMER]);
}

inline Timer& Timer::create (Port::Timer& facade)
{
	Timer* t = global_->create ();
	t->facade_ = &facade;
	if (t->pooled_)
		t->queue_apc (apc_create);
	return *t;
}

inline void Timer::release () noexcept
{
	facade_ = nullptr;
	cancel ();

	// Actual release will be done in the thread_proc()
	queue_apc (apc_release);
}

unsigned long __stdcall Timer::thread_proc (Timer* _this) noexcept
{
	while (!_this->destruct_) {
		if (!_this->pooled_) {
			switch (WaitForMultipleObjectsEx (HANDLE_CNT, _this->handles_, false, INFINITE, true)) {
				case WAIT_OBJECT_0 + HANDLE_TIMER:
					_this->facade_->signal ();
					break;
				case WAIT_OBJECT_0 + HANDLE_TERMINATE:
					_this->release_to_pool ();
					break;
			}
		} else
			SleepEx (INFINITE, true);
	}

	return 0;
}

void __stdcall Timer::apc_destruct (Timer* _this) noexcept
{
	assert (_this->pooled_);
	_this->destruct_ = true;
}

void __stdcall Timer::apc_release (Timer* _this) noexcept
{
	if (!_this->pooled_)
		_this->release_to_pool ();
}

void __stdcall Timer::apc_create (Timer* _this) noexcept
{
	_this->pooled_ = false;
}

void Timer::release_to_pool () noexcept
{
	pooled_ = true;
	global_->release (this);
}

inline void Timer::set (const LARGE_INTEGER& due_time, LONG period, bool resume)
{
	if (period > 0 && period < global_->min_period ())
		throw_BAD_PARAM ();

	if (!SetWaitableTimer (handles_ [HANDLE_TIMER], &due_time, period, nullptr, nullptr, resume))
		Windows::throw_last_error ();
}

inline void Timer::cancel () noexcept
{
	CancelWaitableTimer (handles_ [HANDLE_TIMER]);
}

}

namespace Port {

void Timer::initialize ()
{
	Windows::Timer::initialize ();
}

void Timer::terminate () noexcept
{
	Windows::Timer::terminate ();
}

Timer::Timer () :
	timer_ (Windows::Timer::create (*this))
{}

Timer::~Timer ()
{
	if (!Core::Timer::initialized ())
		return; // timer_ is dangling here

	timer_.release ();
}

void Timer::set (unsigned flags, TimeBase::TimeT due_time, TimeBase::TimeT period)
{
	if (!Core::Timer::initialized ())
		return;

	if (period) {
		period /= TimeBase::MILLISECOND;
		if (0 == period || period > (TimeBase::TimeT)std::numeric_limits <LONG>::max ())
			throw_BAD_PARAM ();
	}
	LARGE_INTEGER dt;
	if (flags & TIMER_ABSOLUTE)
		dt.QuadPart = due_time - Windows::WIN_TIME_OFFSET_SEC * TimeBase::SECOND;
	else {
		if (due_time > (TimeBase::TimeT)std::numeric_limits <LONGLONG>::max ())
			throw_BAD_PARAM ();
		dt.QuadPart = -(LONGLONG)due_time;
	}

	timer_.set (dt, (LONG)period, false);
}

void Timer::cancel () noexcept
{
	if (!Core::Timer::initialized ())
		return;

	timer_.cancel ();
}

inline void Timer::signal () noexcept
{
	static_cast <Core::Timer&> (*this).port_signal ();
}

}
}
}
