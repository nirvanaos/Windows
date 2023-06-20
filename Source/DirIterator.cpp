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
#include "DirIterator.h"
#include "error2errno.h"

using namespace CosNaming;

namespace Nirvana {
namespace Core {
namespace Windows {

DirIterator::DirIterator (const WinWChar* pattern) :
	handle_ (INVALID_HANDLE_VALUE)
{
	handle_ = FindFirstFileExW (pattern, FindExInfoBasic, &data_, FindExSearchNameMatch, nullptr, 0);
	if (INVALID_HANDLE_VALUE == handle_)
		throw_last_error ();
	while (false_item ()) {
		if (!move_next ())
			break;
	}
}

bool DirIterator::move_next () noexcept
{
	if (!FindNextFileW (handle_, &data_)) {
		FindClose (handle_);
		handle_ = INVALID_HANDLE_VALUE;
		return false;
	}
	return true;
}

bool DirIterator::false_item () const noexcept
{
	assert (!end ());
	return ('.' == data_.cFileName [0]
			&& ('\0' == data_.cFileName [1] || ('.' == data_.cFileName [1])
				&& '\0' == data_.cFileName [2]));
}

bool DirIterator::next_one (Binding& b)
{
	if (INVALID_HANDLE_VALUE != handle_) {
		wide_to_utf8 ((const WinWChar*)data_.cFileName, b.name);

		if (data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			b.type = BindingType::ncontext;
		else
			b.type = BindingType::nobject;

		while (move_next ()) {
			if (!false_item ())
				break;
		}

		return true;
	}
	return false;
}

}
}
}
