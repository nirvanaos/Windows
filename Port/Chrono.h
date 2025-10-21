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
#ifndef NIRVANA_CORE_PORT_CHRONO_H_
#define NIRVANA_CORE_PORT_CHRONO_H_
#pragma once

#include <CORBA/CORBA.h>
#include <CORBA/TimeBase.h>
#include <Nirvana/time_defs.h>

extern "C" __declspec(dllimport)
void __stdcall QueryInterruptTimePrecise (uint64_t*);

namespace Nirvana {
namespace Core {
namespace Port {

/// Time API Port implementation.
class Chrono
{
public:
	/// Current system time.
	/// NOTE: For the time zone implementation see https://howardhinnant.github.io/date/tz.html
	/// 
	static TimeBase::UtcT system_clock () noexcept;

	/// System clock resolution.
	static TimeBase::TimeT& system_clock_resolution () noexcept
	{
		return clock_resolution_;
	}

	/// Current UTC time.
	static TimeBase::UtcT UTC () noexcept;

	/// Set current UTC time.
	/// \param t UTC time.
	static void set_UTC (TimeBase::TimeT t);

	/// Duration since system startup in 100 ns intervals.
	static SteadyTime steady_clock () noexcept
	{
		uint64_t t;
		QueryInterruptTimePrecise (&t);
		return t;
	}

	/// Steady clock resolution.
	static const SteadyTime& steady_clock_resolution () noexcept
	{
		return clock_resolution_;
	}

	/// Duration since system startup with maximal precision.
	static DeadlineTime deadline_clock () noexcept
	{
		return __rdtsc ();
	}

	/// Deadline clock frequency, Hz.
	static const uint64_t& deadline_clock_frequency () noexcept
	{
		return TSC_frequency_;
	}

	/// Make deadline.
	/// 
	/// NOTE: If deadline_clock_frequency () is too low (1 sec?), Port library can implement advanced
	/// algorithm to create diffirent deadlines inside one clock tick, based on atomic counter.
	///
	/// \param timeout A timeout from the current time.
	/// \return Deadline time as local steady clock value.
	static DeadlineTime make_deadline (TimeBase::TimeT timeout) noexcept;

	static void initialize ();
	static void terminate () noexcept;

private:
	static bool NTP_client_enabled ();
	static uint32_t local_clock_dispersion_sec ();
	static uint32_t max_allowed_phase_offset_sec ();
	static bool adjustment_in_progress ();

private:
	// Performance counter frequency
	static uint64_t TSC_frequency_;

#ifndef NIRVANA_FAST_RESCALE64
	// Maximal timeout fit in 64-bit multiplication
	static uint64_t max_timeout64_;
#endif

	static void* hkey_time_config_;
	static void* hkey_time_client_;

	static uint64_t clock_resolution_;
};

}
}
}

#endif
