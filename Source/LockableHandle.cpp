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
#include "LockableHandle.h"
#include <random>

namespace Nirvana {
namespace Core {
namespace Windows {

// System domain scheduler threads may compete for handle locking
// with other threads in other domains.
// So we use BackOff bo (THREAD_PRIORITY_MAX_SYS_DOMAIN);

void* LockableHandle::lock () noexcept
{
	for (BackOff bo;;) {
		IntegralType cur = val_.load (std::memory_order_acquire);
		while ((cur & LOCK_MASK) != LOCK_MASK) {
			if (val_.compare_exchange_weak (cur, cur + LOCK_INC))
				return val2handle (cur);
		}
		bo ();
	}
}

void* LockableHandle::exclusive_lock () noexcept
{
	for (BackOff bo;;) {
		IntegralType cur = val_.load ();
		while ((cur & LOCK_MASK) == 0) {
			if (val_.compare_exchange_weak (cur, cur | LOCK_MASK))
				return val2handle (cur);
		}
		bo ();
	}
}

LockableHandle::BackOff::BackOff () :
	iterations_ (1),
	rndgen_ ((RandomGen::result_type)(uintptr_t)this)
{}

void LockableHandle::BackOff::operator () ()
{
	static const unsigned MAX_MS = 100;

	static const unsigned ITER_PER_MS = 1;
	static const unsigned ITER_MAX = MAX_MS * ITER_PER_MS;

	typedef std::uniform_int_distribution <unsigned> Dist;

	unsigned iterations = iterations_;
	if (iterations >= 4)
		iterations = Dist (iterations / 2 + 1, iterations) (rndgen_);

	if (iterations > 1 || !SwitchToThread ())
		Sleep ((iterations + ITER_PER_MS - 1) / ITER_PER_MS);

	if ((iterations_ <<= 1) > ITER_MAX)
		iterations_ = ITER_MAX;
}


}
}
}
