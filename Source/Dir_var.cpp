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
#include "Dir_var.h"
#include "error2errno.h"
#include "DirIteratorEx.h"
#include "FileSystemImpl.h"

using namespace CosNaming;

namespace Nirvana {
namespace Core {
namespace Windows {

bool Dir_var::is_tmp (const NameComponent& nc)
{
	return nc.id () == "tmp" && nc.kind ().empty ();
}

StringW Dir_var::get_path (Name& n, bool create_file) const
{
	assert (!n.empty ());
	if (is_tmp (n.front ())) {
		if (n.size () <= 1 && create_file)
			throw_NO_PERMISSION (make_minor_errno (EACCES));
		WinWChar buf [MAX_PATH + 1];
		size_t cc = FileSystemImpl::get_temp_path (buf);
		return StringW (buf, cc);
	} else
		return Base::get_path (n, create_file);
}

void Dir_var::unlink (Name& n) const
{
	if (n.size () == 1 && is_tmp (n.front ()))
		throw CORBA::NO_PERMISSION (make_minor_errno (EACCES));
	Base::unlink (n);
}

void Dir_var::create_link (CosNaming::Name& n, const DirItemId& target, unsigned flags) const
{
	if (n.size () == 1 && is_tmp (n.front ()))
		throw CORBA::INTERNAL (make_minor_errno (EEXIST));
	Base::create_link (n, target, flags);
}

bool Dir_var::create_dir (CosNaming::Name& n, unsigned mode, DirItemId* pid) const
{
	if (n.size () == 1 && is_tmp (n.front ()))
		throw CORBA::INTERNAL (make_minor_errno (EEXIST));
	return Base::create_dir (n, mode, pid);
}

std::unique_ptr <CosNaming::Core::Iterator> Dir_var::make_iterator () const
{
	std::unique_ptr <DirIteratorEx> iter (std::make_unique <DirIteratorEx> (get_pattern ().c_str ()));
	iter->push (NameComponent ("tmp", IDL::String ()), BindingType::ncontext);
	return iter;
}

void Dir_var::remove ()
{
	throw_BAD_OPERATION (make_minor_errno (ENOTEMPTY));
}

}
}
}
