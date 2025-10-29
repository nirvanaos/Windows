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
#include "ex2signal.h"
#include <Signals.h>
#include "error2errno.h"

namespace Nirvana {
namespace Core {
namespace Windows {

bool ex2signal (EXCEPTION_POINTERS* pex, siginfo_t& siginfo) noexcept
{
	zero (siginfo);

	DWORD exc = pex->ExceptionRecord->ExceptionCode;
	switch (exc) {
		case EXCEPTION_ACCESS_VIOLATION:
			if (pex->ExceptionRecord->NumberParameters >= 2) {
				void* p = (void*)pex->ExceptionRecord->ExceptionInformation [1];
				siginfo.si_value.sival_ptr = p;
				if (pex->ExceptionRecord->ExceptionInformation [0]) {
					// Write access
					MEMORY_BASIC_INFORMATION mbi;
					NIRVANA_VERIFY (VirtualQuery (p, &mbi, sizeof (mbi)));
					if (mbi.State != MEM_COMMIT)
						siginfo.si_code = SEGV_MAPERR;
					else
						siginfo.si_code = SEGV_ACCERR;
				} else
					siginfo.si_code = SEGV_MAPERR;
			}
#ifdef NIRVANA_C17
			[[fallthrough]];
#endif
		case EXCEPTION_GUARD_PAGE:
		case EXCEPTION_IN_PAGE_ERROR:
			siginfo.si_signo = SIGSEGV;
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			siginfo.si_signo = SIGILL;
			siginfo.si_code = ILL_ILLADR;
			break;
		case EXCEPTION_STACK_OVERFLOW:
			siginfo.si_signo = SIGILL;
			siginfo.si_code = ILL_BADSTK;
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTSUB;
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTUND;
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTDIV;
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTRES;
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTINV;
			break;
		case EXCEPTION_FLT_OVERFLOW:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTOVF;
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTSUB;
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_FLTUND;
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_INTDIV;
			break;
		case EXCEPTION_INT_OVERFLOW:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_INTOVF;
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			siginfo.si_signo = SIGILL;
			siginfo.si_code = ILL_ILLOPC;
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			siginfo.si_signo = SIGILL;
			siginfo.si_code = ILL_PRVOPC;
			break;
		default:
			if (STATUS_SIGNAL_BEGIN < exc && exc < STATUS_SIGNAL_BEGIN + NSIG)
				siginfo.si_signo = exc - STATUS_SIGNAL_BEGIN;
	}
	if (siginfo.si_signo) {

#ifdef _X86_
		siginfo.si_addr = (void*)pex->ContextRecord->Eip;
#elif defined (_AMD64_)
		siginfo.si_addr = (void*)pex->ContextRecord->Rip;
#elif defined (_ARM_)
		siginfo.si_addr = (void*)pex->ContextRecord->Pc;
#else
#error Unsupported platform
#endif

		siginfo.si_excode = Signals::signal2ex (siginfo.si_signo);
		if (0xC0000000 == (exc & 0xFFFF0000))
			siginfo.si_errno = error2errno (exc & 0x0000FFFF, 0);
		return true;
	}

	return false;
}

}
}
}
