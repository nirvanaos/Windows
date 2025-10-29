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
#include "BufferPool.h"
#include <Heap.h>

namespace Nirvana {
namespace Core {
namespace Windows {

BufferPool::BufferPool (size_t buffer_count, size_t buffer_size) noexcept :
	begin_ (nullptr),
	end_ (nullptr),
	buffer_size_ (round_up (buffer_size, sizeof (LONG_PTR)))
{
	size_t size = buffer_count * (sizeof (OVERLAPPED) + buffer_size_);
	size_t cb = size;
	begin_ = (OVERLAPPED*)Heap::shared_heap ().allocate (nullptr, cb, 0);
	end_ = (OVERLAPPED*)(((BYTE*)begin_) + size);
}

BufferPool::~BufferPool () noexcept
{
	Heap::shared_heap ().release (begin_, (BYTE*)end_ - (BYTE*)begin_);
}

}
}
}
