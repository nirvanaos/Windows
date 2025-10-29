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
#include "../Port/Dir.h"
#include <Security.h>
#include "DirIterator.h"
#include "error2errno.h"
#include <Nirvana/string_conv.h>
#include "FileSecurityAttributes.h"
#include "SecurityInfo.h"

using namespace CosNaming;
using CosNaming::Core::NamingContextRoot;

namespace Nirvana {
namespace Core {

using namespace Windows;

namespace Port {

Dir::Dir (const DirItemId& id) :
	Base (DirItemId (id))
{
	// Do not throw user exceptions in file system object constructors.
	HANDLE h = get_handle ();
	if (INVALID_HANDLE_VALUE == h) {
		DWORD err = GetLastError ();
		if (ERROR_PATH_NOT_FOUND == err || ERROR_FILE_NOT_FOUND == err)
			type_ = Nirvana::FileType::not_found;
		else
			throw_win_error_sys (err); // System exceptions only.
	} else
		type_ = Nirvana::FileType::directory;
}

Dir::Dir () :
	Base (Nirvana::FileType::directory)
{}

StringW Dir::make_path () const
{
	if (Base::_non_existent ())
		throw CORBA::OBJECT_NOT_EXIST (make_minor_errno (ENOENT));
	return Base::make_path ();
}

StringW Dir::get_path (Name& n, bool dont_append_last) const
{
	assert (!n.empty ());

	// Check directory existence
	DWORD att = GetFileAttributesW (path ());
	if (0xFFFFFFFF == att)
		throw CORBA::OBJECT_NOT_EXIST (make_minor_errno (ENOENT));
	else if (!(FILE_ATTRIBUTE_DIRECTORY & att))
		throw CORBA::OBJECT_NOT_EXIST (make_minor_errno (ENOTDIR));

	StringW path = make_path ();
	if (n.size () > 1 || !dont_append_last)
		append_path (path, n.front ());
	return path;
}

void Dir::append_path (StringW& path, const NameComponent& nc)
{
	if (nc.id ().find_first_of ("<>:\"/\\|?*") != Istring::npos
		|| nc.kind ().find_first_of ("<>:\"/\\|?*.") != Istring::npos)
		throw CosNaming::NamingContext::InvalidName ();

	size_t size = nc.id ().size () + 1;
	if (!nc.kind ().empty ())
		size += nc.kind ().size () + 1;
	path.reserve (path.size () + size);
	path += '\\';
	append_wide (nc.id (), path);
	if (!nc.kind ().empty ()) {
		path += '.';
		append_wide (nc.kind (), path);
	}
}

StringW Dir::check_path (Name& n, bool dont_append_last) const
{
	assert (!n.empty ());

	// Check all name components, except for the last one, as valid directories and then erase.
	StringW path = get_path (n, dont_append_last);
	if (n.size () > 1) {
		for (;;) {
			DWORD att = GetFileAttributesW (path.c_str ());
			if (0xFFFFFFFF == att) {
				DWORD err = GetLastError ();
				if (ERROR_PATH_NOT_FOUND == err || ERROR_FILE_NOT_FOUND == err)
					throw NamingContext::NotFound (NamingContext::NotFoundReason::missing_node, std::move (n));
			} else if (!(FILE_ATTRIBUTE_DIRECTORY & att))
				throw NamingContext::NotFound (NamingContext::NotFoundReason::not_context, std::move (n));

			n.erase (n.begin ());
			if (n.size () <= 1)
				break;
			append_path (path, n.front ());
		}

		if (!dont_append_last)
			append_path (path, n.front ());
	}

	assert (n.size () == 1);

	return path;
}

void Dir::create_link (Name& n, const DirItemId& target, unsigned flags) const
{
	// Create symbolic link
	StringW path = check_path (n);

	if (flags & FLAG_REBIND) {
		flags &= ~FLAG_REBIND;

		DWORD att = GetFileAttributesW (path.c_str ());
		if (0xFFFFFFFF != att)
			unlink (path.c_str (), att);
	}

	if (!CreateSymbolicLinkW (path.c_str (), FileSystemImpl::id_to_path (target), flags))
		throw_win_error_sys (GetLastError ());
}

void Dir::unlink (const WinWChar* path, uint32_t att)
{
	BOOL ok;
	if (FILE_ATTRIBUTE_DIRECTORY & att)
		ok = RemoveDirectoryW (path);
	else
		ok = DeleteFileW (path);
	if (!ok)
		throw_win_error_sys (GetLastError ());
}

void Dir::unlink (Name& n) const
{
	StringW path = check_path (n);

	DWORD att = GetFileAttributesW (path.c_str ());
	if (0xFFFFFFFF == att) {
		DWORD err = GetLastError ();
		if (ERROR_FILE_NOT_FOUND == err || ERROR_PATH_NOT_FOUND == err)
			throw NamingContext::NotFound (NamingContext::NotFoundReason::missing_node, std::move (n));
		else
			throw_win_error_sys (err);
	}

	unlink (path.c_str (), att);
}

bool Dir::create_dir (Name& n, unsigned mode, DirItemId* pid) const
{
	StringW path = check_path (n);

	Windows::FileSecurityAttributes fsa (Core::Security::Context::current ().port (), mode, true);

	bool ret = true;
	if (!CreateDirectoryW (path.c_str (), fsa.security_attributes ())) {
		DWORD err = GetLastError ();
		if (ERROR_ALREADY_EXISTS == err)
			ret = false;
		else
			throw_win_error_sys (err);
	}

	if (pid)
		*pid = FileSystemImpl::path_to_id (path.c_str (), n, Nirvana::FileType::directory);
	
	return ret;
}

DirItemId Dir::resolve_path (Name& n) const
{
	return FileSystemImpl::path_to_id (check_path (n).c_str (), n);
}

DirItemId Dir::get_new_file_id (Name& n) const
{
	StringW path = check_path (n, true);

	// Create id for parent path
	DirItemId id = FileSystemImpl::path_to_id (path.c_str (), n, Nirvana::FileType::regular);

	// Append file name to id
	StringW name;
	append_path (name, n.back ()); // Prefixed with backslash
	size_t parent_size = id.size () - sizeof (WinWChar); // Without the zero terminator
	size_t name_size = name.size ();
	id.resize (id.size () + name_size * sizeof (WinWChar));
	const WinWChar* src = name.data ();
	WinWChar* dst = (WinWChar*)(id.data () + parent_size);
	assert (!*dst);
	std::copy (src, src + name_size + 1, dst);

	return id;
}

StringW Dir::get_pattern () const
{
	StringW pat = make_path ();
	pat += WINWCS ("\\*");
	return pat;
}

std::unique_ptr <CosNaming::Core::Iterator> Dir::make_iterator () const
{
	return std::make_unique <DirIterator> (get_pattern ().c_str ());
}

void Dir::remove ()
{
	if (special ())
		throw_BAD_OPERATION (make_minor_errno (ENOTEMPTY));
	if (FileType::directory == type ()) {
		if (!RemoveDirectoryW (path ()))
			throw_last_error ();
	}
}

unsigned Dir::access (CosNaming::Name& n) const
{
	Windows::StringW path = get_path (n, false);
	for (const auto& nc : n) {
		append_path (path, nc);
		DWORD att = GetFileAttributesW (path.c_str ());
		if (0xFFFFFFFF == att)
			return 0;
	}

	Windows::SecurityInfoDirItem si (path.c_str ());
	if (si.error ())
		return F_OK;
	else
		return si.get_access (Core::Security::Context::current ().port ());
}

}
}
}
