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
#ifndef NIRVANA_CORE_WINDOWS_FILESYSTEMIMPL_H_
#define NIRVANA_CORE_WINDOWS_FILESYSTEMIMPL_H_
#pragma once

#include <NameService/Roots.h>
#include <Nirvana/File.h>
#include "WinWChar.h"

namespace Nirvana {
namespace Core {
namespace Windows {

// File system implementation details
class FileSystemImpl
{
public:
	static Nirvana::FileType get_item_type (const DirItemId& id) noexcept
	{
		return (Nirvana::FileType) * (const Windows::WinWChar*)id.data ();
	}

	static DirItemId path_to_id (const Windows::WinWChar* path, CosNaming::Name& last, FileType type = FileType::unknown);
	static DirItemId dir_path_to_id (const Windows::WinWChar* path);

	static const Windows::WinWChar* id_to_path (const DirItemId& id) noexcept
	{
		return ((const Windows::WinWChar*)id.data ()) + 1;
	}

	static size_t path_len (const DirItemId& id) noexcept
	{
		return id.size () / 2 - 2;
	}

	static Windows::StringW make_path (const DirItemId& id)
	{
		return Windows::StringW (id_to_path (id), path_len (id));
	}

	static size_t get_temp_path (Windows::WinWChar* buf);

	static DirItemId get_app_data_dir (const IDL::String& name, bool& may_cache);
	static DirItemId get_app_data_dir_id (const Windows::WinWChar* name);
	static DirItemId get_var (const IDL::String&, bool& may_cache);
	static DirItemId get_mnt (const IDL::String&, bool& may_cache);
	static DirItemId get_home (const IDL::String&, bool& may_cache);
	static DirItemId get_user_home (const IDL::String&, bool& may_cache);
	static DirItemId get_sbin (const IDL::String&, bool& may_cache);
	static DirItemId get_tmp (const IDL::String&, bool& may_cache);

	enum class SpecialDir
	{
		var,
		mnt,
		// dev, ...

		END
	};

	static SpecialDir is_special_dir (const DirItemId& id) noexcept
	{
		if (id.size () == 4 && get_item_type (id) == Nirvana::FileType::directory)
			return (SpecialDir)((const Windows::WinWChar*)id.data ()) [1];
		else
			return SpecialDir::END;
	}

private:
	static NIRVANA_NORETURN void path_to_id_error (CosNaming::Name& n);

	static DirItemId make_special_id (SpecialDir dir);

};

}
}
}

#endif
