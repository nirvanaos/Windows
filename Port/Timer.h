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
#ifndef NIRVANA_CORE_PORT_TIMER_H_
#define NIRVANA_CORE_PORT_TIMER_H_
#pragma once

#include <CORBA/CORBA.h>

namespace Nirvana {
namespace Core {

namespace Windows {

class Timer;

}

namespace Port {

class Timer
{
public:
	static const unsigned TIMER_ABSOLUTE = 0x01;

	void set (unsigned flags, TimeBase::TimeT due_time, TimeBase::TimeT period);
	void cancel () noexcept;

	static bool initialize () noexcept;
	static void terminate () noexcept;

protected:
	Timer ();
	~Timer ();

private:
	friend class Windows::Timer;

	void signal () noexcept;

private:
	Windows::Timer& timer_;
};

}
}
}

#endif
