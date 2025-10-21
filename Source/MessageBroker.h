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
#ifndef NIRVANA_CORE_WINDOWS_MESSAGEBROKER_H_
#define NIRVANA_CORE_WINDOWS_MESSAGEBROKER_H_
#pragma once

#include "PostOffice.h"
#include "ObjectName.h"
#include <ORB/ESIOP.h>
#include <StaticallyAllocated.h>

namespace Nirvana {
namespace Core {
namespace Windows {

class MessageBroker :
	public PostOffice <MessageBroker, sizeof (ESIOP::MessageBuffer), POSTMAN_THREAD_PRIORITY>
{
	typedef PostOffice <MessageBroker, sizeof (ESIOP::MessageBuffer), POSTMAN_THREAD_PRIORITY> Base;
public:
	static void create ()
	{
		singleton_.construct ();
		singleton_->create_mailslot (Windows::ObjectName (MAILSLOT_PREFIX, GetCurrentProcessId ()));
	}

	static void initialize ()
	{
		singleton_->start ();
	}

	static void terminate () noexcept
	{
		static_cast <Base&> (singleton_).terminate ();
		singleton_.destruct ();
	}

	static CompletionPort& completion_port () noexcept
	{
		return singleton_;
	}

	void received (void* message, DWORD size) noexcept;

private:
	static StaticallyAllocated <MessageBroker> singleton_;
};

}
}
}

#endif
