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
#ifndef NIRVANA_CORE_PORT_SECURITY_H_
#define NIRVANA_CORE_PORT_SECURITY_H_
#pragma once

#include <Nirvana/Nirvana.h>

extern "C" __declspec (dllimport)
int __stdcall CloseHandle (void* handle);

typedef void* PSID;

namespace Nirvana {

typedef IDL::Sequence <CORBA::Octet> SecurityId;

namespace Core {
namespace Port {

class Thread;

/// \brief System security API.
class Security
{
public:
	class Context
	{
	public:
		typedef uint32_t ABI;

		Context () noexcept :
			data_ (0)
		{}

		explicit Context (ABI data) noexcept :
			data_ (data)
		{}

		Context (const Context& src) :
			data_ (src.duplicate ())
		{}

		Context (Context&& src) noexcept :
			data_ (src.data_)
		{
			src.data_ = 0;
		}

		~Context ()
		{
			clear ();
		}

		Context& operator = (const Context& src)
		{
			clear ();
			data_ = src.duplicate ();
			return *this;
		}

		Context& operator = (Context&& src) noexcept
		{
			clear ();
			data_ = src.data_;
			src.data_ = 0;
			return *this;
		}

		void clear () noexcept
		{
			if (data_) {
				NIRVANA_VERIFY (CloseHandle ((void*)(uintptr_t)data_));
				data_ = 0;
			}
		}

		bool empty () const noexcept
		{
			return data_ == 0;
		}

		SecurityId security_id () const;

		ABI abi () const noexcept
		{
			return data_;
		}

		void detach () noexcept
		{
			data_ = 0;
		}

		//------ Windows-specific ------------

		operator void* () const noexcept
		{
			return (void*)(uintptr_t)data_;
		}

	private:
		ABI duplicate () const;

	private:
		ABI data_;
	};

	static bool is_valid_context (Context::ABI context) noexcept;

	static const Context& prot_domain_context () noexcept
	{
		return *reinterpret_cast <const Security::Context*> (&process_token_);
	}

	static IDL::String get_name (const SecurityId& id);

	static void get_entropy (void* buf, size_t size);

	//------ Windows-specific ------------

	static SecurityId make_security_id (PSID sid);

	static bool initialize () noexcept;
	static void terminate () noexcept;

	static PSID everyone () noexcept
	{
		return everyone_;
	}

private:
	static void* process_token_;
	static unsigned everyone_ [];
};

}
}
}

#endif
