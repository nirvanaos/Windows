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
#include "../Port/FileSystem.h"
#include <NameService/File.h>
#include "Dir_var.h"
#include "Dir_mnt.h"

namespace Nirvana {
namespace Core {
namespace Port {

const FileSystem::Root FileSystem::roots_ [] = {
	{ "etc", get_app_data_dir },
	{ "home", get_home },
	{ "mnt", get_mnt },
	{ "sbin", get_sbin },
	{ "tmp", get_tmp },
	{ "var", get_var }

//	{ "~", get_user_home }
};

Roots FileSystem::get_roots ()
{
	Roots roots;
	roots.reserve (std::size (roots_));
	for (const Root* p = roots_; p != std::end (roots_); ++p) {
		roots.push_back ({ p->dir, p->factory });
	}
	return roots;
}

PortableServer::ServantBase::_ref_type FileSystem::incarnate (const DirItemId& id)
{
	if (get_item_type (id) == Nirvana::FileType::directory) {
		switch (is_special_dir (id)) {
		case SpecialDir::var:
			return CORBA::make_reference <Windows::Dir_var> (get_app_data_dir_id (WINWCS ("var")));

		case SpecialDir::mnt:
			return CORBA::make_reference <Windows::Dir_mnt> ();

		default:
			return CORBA::make_reference <Nirvana::Core::Dir> (std::ref (id));
		}

	} else
		return CORBA::make_reference <Nirvana::Core::File> (std::ref (id));
}

bool FileSystem::translate_path (const IDL::String& path, IDL::String& translated)
{
	const char* const s = path.c_str ();
	const size_t size = path.size ();

	size_t find_pos;
	if (size >= 2 && s [1] == ':') {

		char drive = *s;
		if ('a' <= drive && drive <= 'z')
			drive += 'A' - 'a';
		else if (!('A' <= drive && drive <= 'Z'))
			throw CosNaming::NamingContext::InvalidName ();

		if (size > 2) {
			// Slash is required after drive name.
			char slash = s [2];
			if ('\\' != slash && '/' != slash)
				throw CosNaming::NamingContext::InvalidName ();
		}

		translated = "/mnt/";
		translated += drive;
		translated += ':';
		find_pos = translated.size ();
		translated += path.substr (2);

	} else {
		find_pos = path.find ('\\');
		if (find_pos == IDL::String::npos)
			return false; // Standard path
		translated = path;
		translated [find_pos] = '/';
		++find_pos;
	}

	while ((find_pos = translated.find ('\\', find_pos)) != IDL::String::npos) {
		translated [find_pos] = '/';
		++find_pos;
	}

	return true;
}

}
}
}
