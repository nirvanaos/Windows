#include "ObjectName.h"
#include <algorithm>

namespace Nirvana {
namespace Core {
namespace Windows {

void ObjectNameBase::to_string (unsigned id, WinWChar* s) noexcept
{
	WinWChar* p = s;
	do {
		unsigned d = id % 16;
		id /= 16;
		*(p++) = (d <= 9) ? ('0' + d) : ('A' + d - 10);
	} while (id);
	std::reverse (s, p);
}

}
}
}
