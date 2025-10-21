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
#include <ORB/ESIOP.h>
#include "../Port/OtherDomain.h"
#include "Mailslot.h"
#include "ObjectName.h"
#include "error2errno.h"

using namespace Nirvana::Core;
using namespace Nirvana::Core::Windows;

namespace ESIOP {

void send_error_message (ProtDomainId domain_id, const void* msg, size_t size) noexcept
{
	try {
		Mailslot ms;
		ms.open (ObjectName (MAILSLOT_PREFIX, domain_id));
		ms.send (msg, (DWORD)size);
	} catch (...) {
	}
}

void send_shutdown (ProtDomainId domain_id, unsigned flags)
{
	Handle process = OpenProcess (PROCESS_DUP_HANDLE, false, domain_id);
	if (!process)
		throw_last_error ();

	Security::Context sc = Windows::OtherDomainBase::create_security_context (
		Nirvana::Core::Security::prot_domain_context (), process);

	HANDLE token;
	if (!DuplicateHandle (GetCurrentProcess (),
		Nirvana::Core::Security::prot_domain_context ().port (),
		process, &token, 0, false, DUPLICATE_SAME_ACCESS)
		)
		throw_last_error ();

	Shutdown msg ((Nirvana::Core::Security::Context::ABI)(uintptr_t)token, flags);
	Mailslot ms;
	ms.open (ObjectName (MAILSLOT_PREFIX, domain_id));
	ms.send (msg);
	sc.detach ();
}

}
