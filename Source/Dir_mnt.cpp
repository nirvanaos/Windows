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
#include "Dir_mnt.h"
#include <NameService/IteratorStack.h>
#include "win32.h"
#include "WinWChar.h"
#include "error2errno.h"

using namespace CosNaming;
using namespace CosNaming::Core;

namespace Nirvana {
namespace Core {
namespace Windows {

std::unique_ptr <Iterator> Dir_mnt::make_iterator () const
{
	std::unique_ptr <IteratorStack> iter = std::make_unique <IteratorStack> ();

	DWORD mask = GetLogicalDrives ();
	char drive [3] = "A:";
	while (mask) {
		if (mask & 1)
			iter->push (drive, BindingType::ncontext);
		++drive [0];
		mask >>= 1;
	}

	return iter;
}

StringW Dir_mnt::get_path (Name& n) const
{
	assert (!n.empty ());
	const NameComponent& nc = n.front ();
	if (!nc.kind ().empty () || nc.id ().size () != 2 || nc.id () [1] != ':')
		throw NamingContext::NotFound (NamingContext::NotFoundReason::missing_node, std::move (n));
	char drive_letter = nc.id () [0];
	if ('a' <= drive_letter && drive_letter <= 'z')
		drive_letter += 'A' - 'a';
	if ('A' <= drive_letter && drive_letter <= 'Z') {
		WCHAR path [4] = WINWCS ("A:\\");
		path [0] = drive_letter;
		DWORD att = GetFileAttributesW (path);
		if (0xFFFFFFFF == att) {
			DWORD err = GetLastError ();
			if (ERROR_PATH_NOT_FOUND == err)
				throw NamingContext::NotFound (NamingContext::NotFoundReason::missing_node, std::move (n));
			else if (n.size () == 1)
				throw RuntimeError (error2errno (err));
			// If this is only beginning of a path, ignore access denied error.
			// Maybe we have access to the final directory.
		}
		n.erase (n.begin ());
		return StringW (path, 2);
	} else
		throw NamingContext::NotFound (NamingContext::NotFoundReason::missing_node, std::move (n));
}

void Dir_mnt::unlink (Name& n) const
{
	if (n.size () <= 1)
		throw RuntimeError (EACCES);
	return Base::unlink (n);
}

void Dir_mnt::create_link (CosNaming::Name& n, const DirItemId& target, unsigned flags) const
{
	if (n.size () <= 1)
		throw RuntimeError (EACCES);
	return Base::create_link (n, target, flags);
}

DirItemId Dir_mnt::create_dir (CosNaming::Name& n) const
{
	if (n.size () <= 1)
		throw RuntimeError (EACCES);
	return Base::create_dir (n);
}

}
}
}
