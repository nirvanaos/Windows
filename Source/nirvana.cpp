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
#include "initialize.h"
#include <Windows/CmdLineParser.h>
#include "SchedulerMaster.h"
#include "SchedulerSlave.h"
#include "shutdown.h"
#include "ErrConsole.h"
#include "DebugLog.h"
#include <StartupProt.h>
#include <StartupSys.h>
#include <SharedAllocator.h>

namespace Nirvana {
namespace Core {
namespace Windows {

static void swallow_arg (int& argc, char* argv [])
{
	real_copy (argv + 2, argv + argc, argv + 1);
	--argc;
}

inline int nirvana (int argc, char* argv [])
{
	bool system = false;
	while (argc > 1 && '-' == *argv [1]) {
		char cmd = argv [1][1];
		switch (cmd) {

		case 't':
			DebugLog::trace_ = true;
			swallow_arg (argc, argv);
			break;

		case 's':
			system = true;
			swallow_arg (argc, argv);
			break;

		case 'd':
		case 'f':
			if (shutdown ('f' == cmd))
				return 0;
			else {
				ErrConsole () << "System is not running.\n";
				return -1;
			}

		default:
			goto start;
		}
	}

start:

	if (system) {
		if (BUILD_NO_SYS_DOMAIN) {
			ErrConsole () << "Wrong platform. Use 64-bit version.\n";
			return -1;
		} else {
			StartupSys startup (argc, argv);
			if (!SchedulerMaster ().run (startup)) {
				ErrConsole () << "System is already running.\n";
				return -1;
			} else {
				startup.check ();
				return startup.ret ();
			}
		}
	} else {
		StartupProt startup (argc, argv);
		if (!SchedulerSlave ().run (startup, StartupProt::default_deadline ())) {
			ErrConsole () << "System is not running.\n";
			return -1;
		} else {
			startup.check ();
			return startup.ret ();
		}
	}
}

inline int run_nirvana () noexcept
{
	int ret = -1;
	try {
		Nirvana::Windows::CmdLineParser <SharedAllocator> cmdline;
		ret = nirvana (cmdline.argc (), cmdline.argv ());
	} catch (const std::exception& ex) {
		ErrConsole () << ex.what () << '\n';
	}
	return ret;
}

}
}
}

#if defined (_MSC_VER) && !defined (__clang__)

// Use MS UCRT

extern "C" DWORD WinMainCRTStartup (void);

extern "C" DWORD nirvana_startup (void)
{
	Nirvana::Core::Windows::initialize_windows ();
	return WinMainCRTStartup ();
	// Actually it does not return here
}

int CALLBACK WinMain (HINSTANCE, HINSTANCE, LPSTR, int)
{
	atexit (Nirvana::Core::Windows::terminate_windows);
	return Nirvana::Core::Windows::run_nirvana ();
}

#else

// Use Nirvana CRTL

#include <initterm/initterm.h>

extern "C" DWORD nirvana_startup (void)
{
	Nirvana::Core::Windows::initialize_windows ();
	if (!CRTL::initialize ())
		return -1;
	int ret = Nirvana::Core::Windows::run_nirvana ();
	CRTL::terminate ();
	Nirvana::Core::Windows::terminate_windows ();
	ExitProcess (ret);
}

#endif
