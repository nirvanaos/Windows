#include <core.h>
#include "../Port/ProtDomainMemory.h"
#include "../Source/AddressSpace.h"
#include <gtest/gtest.h>

namespace TestMemory {

using namespace ::Nirvana;
using namespace ::Nirvana::Core::Port;
using namespace ::Nirvana::Core::Windows;
using namespace ::CORBA;

class TestMemory :
	public ::testing::Test
{
protected:
	TestMemory ()
	{}

	virtual ~TestMemory ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		ProtDomainMemory::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		ProtDomainMemory::terminate ();
	}
};

TEST_F (TestMemory, Allocate)
{
	size_t BLOCK_SIZE = 0x10000000;	// 256M

	static const size_t ITER_CNT = 2;
	static const UWord iter_flags [ITER_CNT] = {
		Memory::READ_WRITE | Memory::RESERVED,
		Memory::READ_WRITE
	};
	for (int iteration = 0; iteration < ITER_CNT; ++iteration) {

		UWord flags = iter_flags [iteration];

		// Allocate and release memory.
		BYTE* block = (BYTE*)ProtDomainMemory::allocate (0, BLOCK_SIZE, flags);
		ASSERT_TRUE (block);
		ProtDomainMemory::release (block, BLOCK_SIZE);

		flags |= Memory::EXACTLY;

		// Allocate memory at the same range.
		ASSERT_EQ (block, (BYTE*)ProtDomainMemory::allocate (block, BLOCK_SIZE, flags));
		
		// Release the first half.
		ProtDomainMemory::release (block, BLOCK_SIZE / 2);
		
		// Release the second half.
		ProtDomainMemory::release (block + BLOCK_SIZE / 2, BLOCK_SIZE / 2);

		// Allocate the range again.
		ASSERT_EQ (block, (BYTE*)ProtDomainMemory::allocate (block, BLOCK_SIZE, flags));
		
		// Release the second half.
		ProtDomainMemory::release (block + BLOCK_SIZE / 2, BLOCK_SIZE / 2);
		
		// Release the first half.
		ProtDomainMemory::release (block, BLOCK_SIZE / 2);

		// Allocate the range again.
		ASSERT_EQ (block, (BYTE*)ProtDomainMemory::allocate (block, BLOCK_SIZE, flags));

		// Release half at center
		ProtDomainMemory::release (block + BLOCK_SIZE / 4, BLOCK_SIZE / 2);
		
		// Release the first quarter.
		ProtDomainMemory::release (block, BLOCK_SIZE / 4);
		
		// Release the last quarter.
		ProtDomainMemory::release (block + BLOCK_SIZE / 4 * 3, BLOCK_SIZE / 4);

		// Allocate the first half.
		ASSERT_EQ (block, (BYTE*)ProtDomainMemory::allocate (block, BLOCK_SIZE / 2, flags));
		
		// Allocate the second half.
		ASSERT_EQ (ProtDomainMemory::allocate (block + BLOCK_SIZE / 2, BLOCK_SIZE / 2, flags), block + BLOCK_SIZE / 2);
		
		// Release all range
		ProtDomainMemory::release (block, BLOCK_SIZE);

		// Allocate and release range to check that it is free.
		ASSERT_EQ (block, (BYTE*)ProtDomainMemory::allocate (block, BLOCK_SIZE, flags));
		ProtDomainMemory::release (block, BLOCK_SIZE);
	}
}

TEST_F (TestMemory, Commit)
{
	size_t BLOCK_SIZE = 0x20000000;	// 512M
	BYTE* block = (BYTE*)ProtDomainMemory::allocate (0, BLOCK_SIZE, Memory::READ_WRITE | Memory::RESERVED);
	ASSERT_TRUE (block);
	BYTE* end = block + BLOCK_SIZE;
	
	EXPECT_THROW (*block = 1, MEM_NOT_COMMITTED);

	ProtDomainMemory::commit (block, BLOCK_SIZE);
	for (int* p = (int*)block, *end = (int*)(block + BLOCK_SIZE); p < end; ++p)
		*p = rand ();
	
	EXPECT_TRUE (ProtDomainMemory::is_private (block, BLOCK_SIZE));

	ProtDomainMemory::decommit (block, BLOCK_SIZE);
	ProtDomainMemory::decommit (block, BLOCK_SIZE);

	ProtDomainMemory::commit (block, BLOCK_SIZE);
	ProtDomainMemory::commit (block, BLOCK_SIZE);

	ProtDomainMemory::release (block, BLOCK_SIZE);
}

// Test the sharing of the large memory block.
TEST_F (TestMemory, Share)
{
	size_t BLOCK_SIZE = 0x20000000;	// 512M
	BYTE* block = (BYTE*)ProtDomainMemory::allocate (0, BLOCK_SIZE, 0);
	ASSERT_TRUE (block);
	BYTE* end = block + BLOCK_SIZE;

	BYTE b = 0;
	for (BYTE* p = block; p < end; ++p)
		*p = b++;
	EXPECT_EQ (block [1], 1);

	EXPECT_TRUE (ProtDomainMemory::is_private (block, BLOCK_SIZE));

	BYTE* sblock = (BYTE*)ProtDomainMemory::copy (0, block, BLOCK_SIZE, 0);
	ASSERT_TRUE (sblock);
	BYTE* send = sblock + BLOCK_SIZE;
	EXPECT_EQ (sblock [1], 1);

	EXPECT_FALSE (ProtDomainMemory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (ProtDomainMemory::is_private (sblock, BLOCK_SIZE));
	EXPECT_TRUE (ProtDomainMemory::is_copy (sblock, block, BLOCK_SIZE));
	EXPECT_TRUE (ProtDomainMemory::is_copy (block, sblock, BLOCK_SIZE));

	EXPECT_EQ (sblock, (BYTE*)ProtDomainMemory::copy (sblock, block, BLOCK_SIZE, 0));
	EXPECT_EQ (sblock [1], 1);

	EXPECT_FALSE (ProtDomainMemory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (ProtDomainMemory::is_private (sblock, BLOCK_SIZE));
	EXPECT_TRUE (ProtDomainMemory::is_copy (sblock, block, BLOCK_SIZE));
	EXPECT_TRUE (ProtDomainMemory::is_copy (block, sblock, BLOCK_SIZE));

	b = 1;
	for (BYTE* p = block; p < end; ++p)
		*p = b++;
	EXPECT_EQ (block [1], 2);
	EXPECT_EQ (sblock [1], 1);
	
	EXPECT_TRUE (ProtDomainMemory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (ProtDomainMemory::is_copy (sblock, block, BLOCK_SIZE));

	b = 2;
	for (BYTE* p = sblock; p < send; ++p)
		*p = b++;
	EXPECT_EQ (block [1], 2);
	EXPECT_EQ (sblock [1], 3);
	
	EXPECT_TRUE (ProtDomainMemory::is_private (sblock, BLOCK_SIZE));

	EXPECT_EQ (block, ProtDomainMemory::copy (block, sblock, BLOCK_SIZE, 0));
	EXPECT_EQ (block [1], 3);

	EXPECT_FALSE (ProtDomainMemory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (ProtDomainMemory::is_private (sblock, BLOCK_SIZE));
	EXPECT_TRUE (ProtDomainMemory::is_copy (sblock, block, BLOCK_SIZE));
	EXPECT_TRUE (ProtDomainMemory::is_copy (block, sblock, BLOCK_SIZE));

	ProtDomainMemory::release (block, BLOCK_SIZE);
	ProtDomainMemory::release (sblock, BLOCK_SIZE);
}

TEST_F (TestMemory, Move)
{
	size_t BLOCK_SIZE = 0x20000000;	// 512M
	size_t SHIFT = ALLOCATION_GRANULARITY;
	// Allocate block.
	int* block = (int*)ProtDomainMemory::allocate (0, BLOCK_SIZE + SHIFT, Memory::ZERO_INIT | Memory::RESERVED);
	ASSERT_TRUE (block);
	ProtDomainMemory::commit (block, BLOCK_SIZE);

	int i = 0;
	for (int* p = block, *end = block + BLOCK_SIZE / sizeof (int); p != end; ++p)
		*p = ++i;

	// Shift block right on SHIFT
	int* shifted = (int*)ProtDomainMemory::copy (block + SHIFT / sizeof (int), block, BLOCK_SIZE, Memory::EXACTLY | Memory::RELEASE);
	EXPECT_EQ (shifted, block + SHIFT / sizeof (int));
	i = 0;
	for (int* p = shifted, *end = shifted + BLOCK_SIZE / sizeof (int); p != end; ++p)
		EXPECT_EQ (*p, ++i);
	EXPECT_TRUE (ProtDomainMemory::is_private (shifted, BLOCK_SIZE));

	// Allocate region to ensure that it is free.
	EXPECT_TRUE (ProtDomainMemory::allocate (block, SHIFT, Memory::RESERVED | Memory::EXACTLY));
	ProtDomainMemory::release (block, SHIFT);

	// Shift it back.
	EXPECT_EQ (block, (int*)ProtDomainMemory::copy (block, shifted, BLOCK_SIZE, Memory::ALLOCATE | Memory::EXACTLY | Memory::RELEASE));
	i = 0;
	for (int* p = block, *end = block + BLOCK_SIZE / sizeof (int); p != end; ++p)
		EXPECT_EQ (*p, ++i);
	EXPECT_TRUE (ProtDomainMemory::is_private (block, BLOCK_SIZE));

	// Allocate region to ensure that it is free.
	EXPECT_TRUE (ProtDomainMemory::allocate (block + BLOCK_SIZE / sizeof (int), SHIFT, Memory::RESERVED | Memory::EXACTLY));
	ProtDomainMemory::release (block + BLOCK_SIZE / sizeof (int), SHIFT);

	ProtDomainMemory::release (block, BLOCK_SIZE);
}

TEST_F (TestMemory, SmallBlock)
{
	int* block = (int*)ProtDomainMemory::allocate (0, sizeof (int), Memory::ZERO_INIT);
	ASSERT_TRUE (block);
	EXPECT_TRUE (ProtDomainMemory::is_private (block, sizeof (int)));
	*block = 1;
	{
		int* copy = (int*)ProtDomainMemory::copy (0, block, sizeof (int), 0);
		ASSERT_TRUE (copy);
		EXPECT_EQ (*copy, *block);
		EXPECT_TRUE (ProtDomainMemory::is_readable (copy, sizeof (int)));
		EXPECT_TRUE (ProtDomainMemory::is_writable (copy, sizeof (int)));
		EXPECT_TRUE (ProtDomainMemory::is_copy (copy, block, sizeof (int)));
		EXPECT_FALSE (ProtDomainMemory::is_private (block, sizeof (int)));
		*copy = 2;
		EXPECT_EQ (*block, 1);
		ProtDomainMemory::release (copy, sizeof (int));
	}
	{
		int* copy = (int*)ProtDomainMemory::copy (0, block, sizeof (int), Memory::READ_ONLY);
		ASSERT_TRUE (copy);
		EXPECT_EQ (*copy, *block);
		EXPECT_TRUE (ProtDomainMemory::is_readable (copy, sizeof (int)));
		EXPECT_FALSE (ProtDomainMemory::is_writable (copy, sizeof (int)));
		EXPECT_TRUE (ProtDomainMemory::is_copy (copy, block, sizeof (int)));
		EXPECT_THROW (*copy = 2, NO_PERMISSION);
		ProtDomainMemory::release (copy, sizeof (int));
	}
	ProtDomainMemory::decommit (block, PAGE_SIZE);
	ProtDomainMemory::commit (block, sizeof (int));
	*block = 1;
	{
		EXPECT_TRUE (ProtDomainMemory::is_private (block, sizeof (int)));
		int* copy = (int*)ProtDomainMemory::copy (0, block, PAGE_SIZE, Memory::DECOMMIT);
		EXPECT_EQ (*copy, 1);
		EXPECT_TRUE (ProtDomainMemory::is_readable (copy, sizeof (int)));
		EXPECT_TRUE (ProtDomainMemory::is_writable (copy, sizeof (int)));
		EXPECT_FALSE (ProtDomainMemory::is_readable (block, sizeof (int)));
		EXPECT_FALSE (ProtDomainMemory::is_writable (block, sizeof (int)));
		EXPECT_THROW (*block = 2, MEM_NOT_COMMITTED);
		ProtDomainMemory::commit (block, sizeof (int));
		*block = 2;
		EXPECT_TRUE (ProtDomainMemory::is_private (block, sizeof (int)));
		EXPECT_TRUE (ProtDomainMemory::is_private (copy, sizeof (int)));
		EXPECT_FALSE (ProtDomainMemory::is_copy (copy, block, sizeof (int)));
		ProtDomainMemory::release (copy, sizeof (int));
	}
	{
		int* copy = (int*)ProtDomainMemory::copy (0, block, sizeof (int), Memory::RELEASE);
		ASSERT_EQ (copy, block);
	}
	ProtDomainMemory::release (block, sizeof (int));
}

void stack_test (void* limit, bool first)
{
	BYTE data [4096];
	data [0] = 1;
	MEMORY_BASIC_INFORMATION mbi;
	ASSERT_TRUE (VirtualQuery (data, &mbi, sizeof (mbi)));
	ASSERT_EQ (mbi.Protect, PageState::RW_MAPPED_PRIVATE);
	if (current_TIB ()->StackLimit != limit)
		if (first)
			first = false;
		else
			return;
	stack_test (limit, first);
}

TEST_F (TestMemory, NotShared)
{
	static const char test_const [] = "test";
	char* copy = (char*)ProtDomainMemory::copy (0, (void*)test_const, sizeof (test_const), Memory::ALLOCATE);
	static char test [sizeof (test_const)];
	ProtDomainMemory::copy (test, copy, sizeof (test_const), 0);
	EXPECT_STREQ (test, test_const);
	ProtDomainMemory::release (copy, sizeof (test_const));
}

}
