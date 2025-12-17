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
#include "../Port/Module.h"
#include "win32.h"
#include "error2errno.h"
#include <Nirvana/string_conv.h>
#include <Nirvana/posix_defs.h>
#include <Nirvana/BindErrorUtl.h>
#include <ORB/Services.h>
#include <TimerEvent.h>
#include <append_path.h>

namespace Nirvana {
namespace Core {

using namespace Windows;

namespace Port {

Module::Module (AccessDirect::_ptr_type file) :
	module_ (nullptr),
	temp_path_ (MemContext::current ().heap ())
{
	FilePath tmp_path (temp_path_.get_allocator ());
	Dir::_ref_type tmp_dir;
	{
		WinWChar buf [MAX_PATH + 1];
		DWORD cc = GetTempPathW ((DWORD)countof (buf), buf);
		if (!cc)
			throw_UNKNOWN ();
		IDL::String path;
		append_utf8 (buf, buf + cc - 1, path);
		tmp_path.assign (buf, cc);
		CosNaming::Name tmp_dir_name;
		append_path (tmp_dir_name, path, true);
		auto ns = CosNaming::NamingContextExt::_narrow (CORBA::Core::Services::bind (CORBA::Core::Services::NameService));
		tmp_dir = Dir::_narrow (ns->resolve (tmp_dir_name));
		if (!tmp_dir)
			throw_UNKNOWN ();
	}

	try {
		AccessDirect::_ref_type tmp_file_access;

		{
			IDL::String name (TEMP_MODULE_PREFIX "XXXXXX" TEMP_MODULE_EXT);
			tmp_file_access = AccessDirect::_narrow (tmp_dir->mkostemps (name, 4, O_DIRECT, 0)->_to_object ());
			// The name is ASCII so we can append it without a conversion
			tmp_path.append (name.begin (), name.end ());
			temp_path_ = std::move (tmp_path);
		}

		{
			IDL::Sequence <uint8_t> buf;
			file->read (0, std::numeric_limits <uint32_t>::max (), buf);
			tmp_file_access->write (0, buf, false);
			tmp_file_access->close ();
		}

		module_ = LoadLibraryW (temp_path_.c_str ());
		if (!module_)
			throw_BAD_PARAM (make_minor_errno (ENOEXEC));

		Nirvana::Core::PortableExecutable pe (module_);
		size_ = pe.pe32_header ()->SizeOfImage;

		if (!(metadata_.address = pe.find_OLF_section (metadata_.size)))
			BindError::throw_message ("Metadata not found");

		const COFF::PE32Header* pehdr = pe.pe32_header ();
		if (!pehdr || pehdr->AddressOfEntryPoint)
			BindError::throw_message ("Image must be linked with /NOENTRY");

	} catch (...) {
		unload ();
		throw;
	}
}

void Module::unload () noexcept
{
	if (module_) {
		NIRVANA_VERIFY (FreeLibrary (module_));
		module_ = nullptr;
	}
	if (!temp_path_.empty ()) {
		FilePath path (std::move (temp_path_));
		static const DWORD RETRY_WAIT_MS = 100;
		unsigned retry_cnt = 10;
		while (retry_cnt && !DeleteFileW (path.c_str ())) {
			assert (ERROR_ACCESS_DENIED == GetLastError ());
			if (!--retry_cnt)
				break;
			if (Core::Timer::initialized ()) {
				TimerEvent timer;
				timer.set (0, TimeBase::MILLISECOND * RETRY_WAIT_MS, 0);
				timer.wait ();
			} else {
				// System shutdown started, timers are not working.
				Sleep (RETRY_WAIT_MS);
			}
		}

		// LLDB keeps DLL loaded (and therefore the file locked) even after the FreeLibrary is called.
#ifndef NDEBUG
		if (!IsDebuggerPresent ())
			assert (retry_cnt);
#endif
	}
}

void Module::get_data_sections (DataSections& sections) const
{
	Nirvana::Core::PortableExecutable pe (module_);
	for (const COFF::Section* s = pe.sections (), *end = s + pe.section_count (); s != end; ++s) {
		if (s->Characteristics & IMAGE_SCN_MEM_WRITE
			&& !COFF::is_section (*s, ".msvcjmc")
			&& !COFF::is_section (*s, OLF_BIND)
			)
			sections.push_back ({ pe.section_address (*s), s->Misc.VirtualSize });
	}
}

}
}
}
