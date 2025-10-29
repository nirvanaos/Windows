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
#include "MailslotReader.h"
#include <Nirvana/throw_exception.h>

namespace Nirvana {
namespace Core {
namespace Windows {

MailslotReader::MailslotReader (size_t buffer_count, DWORD max_msg_size) :
	BufferPool (buffer_count, max_msg_size),
	handle_ (INVALID_HANDLE_VALUE),
	max_msg_size_ (max_msg_size)
{}

MailslotReader::~MailslotReader ()
{
	if (INVALID_HANDLE_VALUE != handle_)
		CloseHandle (handle_);
}

bool MailslotReader::create_mailslot (LPCWSTR mailslot_name)
{
	assert (INVALID_HANDLE_VALUE == handle_);
	handle_ = CreateMailslotW (mailslot_name, max_msg_size_, MAILSLOT_WAIT_FOREVER, nullptr);
	if (INVALID_HANDLE_VALUE == handle_) {
		DWORD err = GetLastError ();
		if (ERROR_ALREADY_EXISTS == err)
			return false;
		throw_INITIALIZE ();
	}
	return true;
}

void MailslotReader::start (CompletionPort& port)
{
	assert (handle_ != INVALID_HANDLE_VALUE);
	port.add_receiver (handle_, *this);

	for (OVERLAPPED* p = begin (); p != end (); p = next (p)) {
		enqueue_buffer (p);
	}
}

}
}
}
