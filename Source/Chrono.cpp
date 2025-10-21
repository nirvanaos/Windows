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
#include "../Port/Chrono.h"
#include <intrin.h>
#include <limits>
#include <Nirvana/rescale.h>
#include "error2errno.h"

namespace Nirvana {
namespace Core {
namespace Port {

uint64_t Chrono::TSC_frequency_;

#ifndef NIRVANA_FAST_RESCALE64
uint64_t Chrono::max_timeout64_;
#endif

void* Chrono::hkey_time_config_;
void* Chrono::hkey_time_client_;
uint64_t Chrono::clock_resolution_;

void Chrono::initialize ()
{
	TSC_frequency_ = 0;
	{
		int info [4];
		__cpuid (info, 0);
		int maxfunc = info [0];
		if (maxfunc >= 0x15) {
			__cpuid (info, 0x15);
			if (info [1] && info [2])
				TSC_frequency_ = UInt32x32To64 (info [2], info [1]) / info [0];
		}
	}

	if (!TSC_frequency_) {
		LARGE_INTEGER pf;
		if (!QueryPerformanceFrequency (&pf))
			throw_INITIALIZE ();

		LARGE_INTEGER pc_start;
		int prio = GetThreadPriority (GetCurrentThread ());
		NIRVANA_VERIFY (SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL));
		QueryPerformanceCounter (&pc_start);
		uint64_t start = __rdtsc ();
		NIRVANA_VERIFY (SetThreadPriority (GetCurrentThread (), prio));

		Sleep (100);

		LARGE_INTEGER pc_end;
		NIRVANA_VERIFY (SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL));
		QueryPerformanceCounter (&pc_end);
		uint64_t end = __rdtsc ();
		NIRVANA_VERIFY (SetThreadPriority (GetCurrentThread (), prio));

		TSC_frequency_ = rescale64 (end - start, pf.QuadPart, 0, pc_end.QuadPart - pc_start.QuadPart);
		uint32_t clock_freq = pf.QuadPart > 10000000 ? 10000000 : (uint32_t)pf.QuadPart;
		clock_resolution_ = 10000000 / clock_freq;

		HKEY time_service;
		if (RegOpenKeyW (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\W32Time", &time_service))
			throw_INITIALIZE ();
		if (RegOpenKeyW (time_service, L"Config", &hkey_time_config_))
			throw_INITIALIZE ();
		if (RegOpenKeyW (time_service, L"TimeProviders\\NtpClient", &hkey_time_client_))
			throw_INITIALIZE ();
		RegCloseKey (time_service);
	}

#ifndef NIRVANA_FAST_RESCALE64
	max_timeout64_ = std::numeric_limits <uint64_t>::max () / TSC_frequency_;
#endif
}

void Chrono::terminate () noexcept
{
	RegCloseKey (hkey_time_config_);
	RegCloseKey (hkey_time_client_);
}

inline bool Chrono::NTP_client_enabled ()
{
	DWORD d, cb;
	return ERROR_SUCCESS == RegQueryValueExW (hkey_time_client_, L"Enabled",
		nullptr, nullptr, (BYTE*)&d, &cb) && d;
}

inline uint32_t Chrono::local_clock_dispersion_sec ()
{
	DWORD d = 10, cb;
	RegQueryValueExW (hkey_time_config_, L"LocalClockDispersion", nullptr, nullptr, (BYTE*)&d, &cb);
	return d;
}

inline uint32_t Chrono::max_allowed_phase_offset_sec ()
{
	DWORD d = 1, cb;
	RegQueryValueExW (hkey_time_config_, L"MaxAllowedPhaseOffset", nullptr, nullptr, (BYTE*)&d, &cb);
	return d;
}

inline bool Chrono::adjustment_in_progress ()
{
	DWORD adj, inc;
	BOOL disabled;
	if (GetSystemTimeAdjustment (&adj, &inc, &disabled))
		return !disabled && adj != 0;
	else
		return false;
}

TimeBase::UtcT Chrono::UTC () noexcept
{
	FILETIME ft;
	GetSystemTimePreciseAsFileTime (&ft);
	ULARGE_INTEGER ui;
	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;

	uint32_t inacclo;
	if (!NTP_client_enabled ())
		inacclo = local_clock_dispersion_sec () * 10000000;
	else if (adjustment_in_progress ())
		inacclo = max_allowed_phase_offset_sec () * 10000000;
	else
		inacclo = 1;

	return TimeBase::UtcT (ui.QuadPart + Windows::WIN_TIME_OFFSET_SEC * TimeBase::SECOND, inacclo, 0, 0);
}

void Chrono::set_UTC (TimeBase::TimeT t)
{
	if (t < Windows::WIN_TIME_OFFSET_SEC * TimeBase::SECOND)
		throw_BAD_PARAM ();

	t -= Windows::WIN_TIME_OFFSET_SEC * TimeBase::SECOND;
	FILETIME ft;
	ft.dwHighDateTime = (DWORD)(t >> 32);
	ft.dwLowDateTime = (DWORD)t;

	SYSTEMTIME st;
	if (!FileTimeToSystemTime (&ft, &st) || !SetSystemTime (&st))
		Windows::throw_last_error ();
}

TimeBase::UtcT Chrono::system_clock () noexcept
{
	TimeBase::UtcT t = UTC ();

	TIME_ZONE_INFORMATION tzi;
	GetTimeZoneInformation (&tzi);
	t.tdf (-(TimeBase::TdfT)tzi.Bias);

	return t;
}

DeadlineTime Chrono::make_deadline (TimeBase::TimeT timeout) noexcept
{
	uint64_t dt_timeout = 
#ifndef NIRVANA_FAST_RESCALE64
	(timeout <= max_timeout64_) ? ((timeout * deadline_clock_frequency () + 9999999) / 10000000) :
#endif
	rescale64 (timeout, deadline_clock_frequency (), 9999999, 10000000);
	return deadline_clock () + dt_timeout;
}

}
}
}
