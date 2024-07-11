/*
* Nirvana Core.
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
#ifndef NIRVANA_PE_PORTABLEEXECUTABLE_H_
#define NIRVANA_PE_PORTABLEEXECUTABLE_H_
#pragma once

#include "COFF.h"
#include <Nirvana/File.h>

namespace Nirvana {
namespace Core {

class PortableExecutable : public COFF
{
public:
	static uint_fast16_t get_platform (AccessDirect::_ptr_type binary);

	PortableExecutable (const void* base_address);

	const void* base_address () const noexcept
	{
		return base_address_;
	}

	const void* section_address (const COFF::Section& s) const noexcept
	{
		return (const uint8_t*)base_address_ + s.VirtualAddress;
	}

	const void* find_OLF_section (size_t& size) const noexcept;

private:
	typedef pe_bliss::pe_win::image_dos_header DOSHeader;
	typedef pe_bliss::pe_win::image_nt_headers32 NTHeaders32;

	static void check_header (const DOSHeader& hdr);
	static void check_header (const NTHeaders32& hdr);

	static const void* get_COFF (const void* base_address);

private:
	const void* base_address_;
};


}
}

#endif
