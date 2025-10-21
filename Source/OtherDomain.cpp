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
#include "../Port/OtherDomain.h"
#include "OtherSpace.inl"
#include "ObjectName.h"
#include "error2errno.h"
#include <ExecDomain.h>

using namespace Nirvana::Core;
using namespace Nirvana::Core::Windows;

namespace ESIOP {
namespace Windows {

OtherDomainBase::OtherDomainBase (ProtDomainId domain_id) :
	process_ (::OpenProcess (PROCESS_QUERY_INFORMATION
		| PROCESS_VM_OPERATION | PROCESS_DUP_HANDLE , FALSE, domain_id))
{
	if (!process_ || !Mailslot::open (Nirvana::Core::Windows::ObjectName (MAILSLOT_PREFIX, domain_id)))
		Nirvana::throw_COMM_FAILURE ();
}

OtherDomainBase::~OtherDomainBase ()
{
	if (process_)
		CloseHandle (process_);
}

inline bool OtherDomainBase::is_64_bit () const noexcept
{
	bool x64 =
#ifdef _WIN64
	true;
#else
	false;
#endif
	USHORT process_machine, host_machine;
	if (::IsWow64Process2 (process_, &process_machine, &host_machine)) {
		USHORT machine = (IMAGE_FILE_MACHINE_UNKNOWN == process_machine) ? host_machine : process_machine;
		x64 = IMAGE_FILE_MACHINE_I386 != machine;
	}
	return x64;
}

Security::Context OtherDomainBase::create_security_context (
	const Nirvana::Core::Security::Context& local_context,
	HANDLE target_process)
{
	HANDLE token;
	if (!DuplicateHandle (GetCurrentProcess (),
		local_context.port (),
		target_process, &token, 0, false, DUPLICATE_SAME_ACCESS)
		)
		throw_last_error ();

	return Security::Context ((Nirvana::Core::Security::Context::ABI)(uintptr_t)token);
}

}

OtherDomainSinglePlatform::OtherDomainSinglePlatform (ProtDomainId domain_id) :
	Windows::OtherDomainBase (domain_id),
	Space (domain_id, Windows::OtherDomainBase::process ())
{}

OtherDomainSinglePlatform::~OtherDomainSinglePlatform ()
{}

SharedMemPtr OtherDomainSinglePlatform::reserve (size_t size)
{
	return Space::reserve (size);
}

SharedMemPtr OtherDomainSinglePlatform::copy (SharedMemPtr reserved, void* src, size_t& size, unsigned flags)
{
	return Space::copy (reserved, src, size, flags);
}

void OtherDomainSinglePlatform::release (SharedMemPtr p, size_t size)
{
	Space::release (p, size);
}

namespace Windows {

template <bool x64>
class OtherDomainImpl :
	public OtherDomain,
	private OtherSpace <x64>
{
	typedef OtherSpace <x64> Space;

public:
	OtherDomainImpl (ProtDomainId process_id, HANDLE process_handle) :
		Space (process_id, process_handle)
	{}

	virtual SharedMemPtr reserve (size_t size) override
	{
		return Space::reserve (size);
	}

	virtual SharedMemPtr copy (SharedMemPtr reserved, void* src, size_t& size, unsigned flags) override
	{
		return Space::copy (reserved, src, size, flags);
	}

	virtual void release (SharedMemPtr p, size_t size) override
	{
		Space::release (p, size);
	}

	virtual void get_sizes (PlatformSizes& sizes) noexcept override
	{
		Space::get_sizes (sizes);
	}

	virtual void* store_pointer (void* where, SharedMemPtr p) noexcept override
	{
		return Space::store_pointer (where, p);
	}

	virtual void* store_size (void* where, size_t size) noexcept override
	{
		return Space::store_size (where, size);
	}
};

}

OtherDomainMultiPlatform::OtherDomainMultiPlatform (ProtDomainId domain_id) :
	Windows::OtherDomainBase (domain_id),
	implementation_ (nullptr)
{
	if (is_64_bit ())
		implementation_ = new Windows::OtherDomainImpl <true> (domain_id, process ());
	else
		implementation_ = new Windows::OtherDomainImpl <false> (domain_id, process ());
}

}
