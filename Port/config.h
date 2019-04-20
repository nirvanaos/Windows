// Nirvana Project
// Compile configuration parameters

#ifndef NIRVANA_CORE_CONFIG_H_
#define NIRVANA_CORE_CONFIG_H_

#include <CORBA/BasicTypes.h>

namespace Nirvana {
namespace Core {

using namespace ::CORBA;

// Heap parameters

/*
	HEAP_UNIT - ����������� ������ ����������� ����� ������ �� ���������.
	������ ����������� ����� ������������� � ������� ���������� �� ��� ��������.
	����� �������, �� ���� ������������, ��������� ������� ����������
	HEAP_UNIT/2 ���� �� ������ ���������� ����.
	����� ����, ������ ������� ����� ���������� 2 ���� �� HEAP_UNIT ���� ����.
	����� �������, ����������� �������� HEAP_UNIT ������� �� ���������� � ��������
	������� ���������� ������.
	� ������������ ����������� ����, ��������� ������� ���������� ������ �� ����� 8 ����
	�� ���������� ����. ��� ����������� ��������-��������������� �������� ���������� �������
	���������� ��������� ������ ������. ����� �������, ��������� ������� � �������
	���� ���������� ������.
	� ������ ���������� ������ ����� �� ��������� ����� 16 ����.
*/

const ULong HEAP_UNIT_MIN = 16;
const ULong HEAP_UNIT_DEFAULT = 16;
const ULong HEAP_UNIT_MAX = 4096;

/**	������ ������������ ����� ����. 
������ ������ ���� ������ ������������� ������ ������ ������ - �������������
�������� MAX (ALLOCATION_UNIT, PROTECTION_UNIT, SHARING_UNIT). ���� HEAP_DIRECTORY_SIZE
������ ���� ��������, ��������� ���� �������� ��������� ����������� ������, � ���� ����
������� �� ��������������� ���������� ������, ������ �� ������� �������� ��������.
��� Windows ������ ��������� ����� 64K. ��� ������ � �������� ��������� ALLOCATION_UNIT
� SHARING_UNIT ��� ����� ������� ������.
*/
const ULong HEAP_DIRECTORY_SIZE = 0x10000;

/** Use exceptions to handle uncommitted pages in heap directory.
When set to `false`, heap algorithm uses `Memory::is_readable ()`
to detect uncommitted pages. `true` provides better performance, but maybe not for all platforms.
*/
const Boolean HEAP_DIRECTORY_USE_EXCEPTION = true;

/** Maximum count of levels in PriorityQueue.
To provide best performance with a probabilistic time complexity of
O(logN) where N is the maximum number of elements, the queue should
have PRIORITY_QUEUE_LEVELS = logN. Too large value degrades the performance.
*/
const ULong SYNC_DOMAIN_PRIORITY_QUEUE_LEVELS = 10; ///< For syncronization domain.
const ULong SYS_DOMAIN_PRIORITY_QUEUE_LEVELS = 10; ///< For system-wide scheduler.

}
}

#endif
