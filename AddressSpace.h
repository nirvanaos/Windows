// Nirvana project.
// Windows implementation.
// Protection domain (process) address space.

#ifndef NIRVANA_CORE_WINDOWS_ADDRESSSPACE_H_
#define NIRVANA_CORE_WINDOWS_ADDRESSSPACE_H_

#include "Win32.h"
#include <Memory.h>

namespace Nirvana {
namespace Core {
namespace Windows {

///	<summary>
///	Page states for mapped block.
///	</summary>
///	<remarks>
///	<para>
///	We use "execute" protection to distinct private pages from shared pages.
///	Page states:
///	<list>
///		<item><term><c>0</c></term><description>Page not committed (entire block never was shared).</description></item>
///		<item><term><c>PAGE_NOACCESS</c></term><description>Decommitted.</description></item>
///		<item><term><c>PAGE_READWRITE</c></term><description>The page is mapped and never was shared.</description></item>
///		<item><term><c>PAGE_EXECUTE_WRITECOPY</c></term><description>The page is mapped and was shared.</description></item>
///		<item><term><c>PAGE_EXECUTE_READWRITE</c></term><description>The page is write-copyed (private, disconnected from mapping).</description></item>
///		<item><term><c>PAGE_READONLY</c></term><description>The read-only mapped page never was shared.</description></item>
///		<item><term><c>PAGE_EXECUTE</c></term><description>The read-only mapped page was shared.</description></item>
///		<item><term><c>PAGE_EXECUTE_READ</c></term><description>The page is not mapped. Page was write-copyed, than access was changed from PAGE_READWRITE to PAGE_READONLY.</description></item>
///	</list>
///	Note: "Page was shared" means that page has been shared at least once. Currently, page may be still shared or already not.
///	</para><para>
///	Page state changes.
///	<list>
///		<item>
///		Prepare to share:
///		<list>
///			<item><term><c>RW_MAPPED_PRIVATE</c>-><c>RW_MAPPED_SHARED</c></term></item>
///			<item><term><c>RO_MAPPED_PRIVATE</c>, <c>RW_MAPPED_SHARED</c>, <c>RO_MAPPED_SHARED</c>, <c>NOT_COMMITTED</c>, <c>DECOMMITTED</c></term><description>Unchanged.</description></item>
///			<item><term><c>RW_UNMAPPED</c>, <c>RO_UNMAPPED</c></term><description>We need to remap the block.</description></item>
///		</list>
///		</item><item>
///		Remap:
///		<list>
///			<item><term><c>RW_MAPPED_SHARED</c>, <c>RW_UNMAPPED</c>-><c>RW_MAPPED_PRIVATE</c></term>
///			<item><term><c>RO_MAPPED_SHARED</c>, <c>RO_UNMAPPED</c>-><c>RO_MAPPED_PRIVATE</c></term>
///		</list>
///		</item><item>
///		Write-protection:
///		<list>
///			<item><term><c>RW_MAPPED_PRIVATE</c><-><c>RO_MAPPED_PRIVATE</c></term>
///			<item><term><c>RW_MAPPED_SHARED</c><-><c>RO_MAPPED_SHARED</c></term>
///			<item><term><c>RW_UNMAPPED</c><-><c>RO_UNMAPPED</c></term>
///		</list>
///		</item>
///	</list>
///	</para>
///	</remarks>
class PageState
{
public:
	enum
	{
		/// <summary>Page not committed (entire block never was shared).</summary>
		NOT_COMMITTED = 0,
		/// <summary>Decommitted.</summary>
		DECOMMITTED = PAGE_NOACCESS,
		/// <summary>The page is mapped and never was shared.</summary>
		RW_MAPPED_PRIVATE = PAGE_READWRITE,
		/// <summary>The page is mapped and was shared.</summary>
		RW_MAPPED_SHARED = PAGE_EXECUTE_WRITECOPY,
		/// <summary>The page is write-copyed (private, disconnected from mapping).</summary>
		RW_UNMAPPED = PAGE_EXECUTE_READWRITE,
		/// <summary>The read-only mapped page never was shared.</summary>
		RO_MAPPED_PRIVATE = PAGE_READONLY,
		/// <summary>The read-only mapped page was shared.</summary>
		RO_MAPPED_SHARED = PAGE_EXECUTE,
		/// <summary>The page is not mapped. Page was write-copyed, than access was changed from <c>PAGE_READWRITE</c> to <c>PAGE_READONLY</c>.</summary>
		RO_UNMAPPED = PAGE_EXECUTE_READ,

		// Page state masks.
		MASK_RW = RW_MAPPED_PRIVATE | RW_MAPPED_SHARED | RW_UNMAPPED,
		MASK_RO = RO_MAPPED_PRIVATE | RO_MAPPED_SHARED | RO_UNMAPPED,
		MASK_ACCESS = MASK_RW | MASK_RO,
		MASK_UNMAPPED = RW_UNMAPPED | RO_UNMAPPED,
		MASK_MAPPED = RW_MAPPED_PRIVATE | RW_MAPPED_SHARED | RO_MAPPED_PRIVATE | RO_MAPPED_SHARED,
		MASK_MAY_BE_SHARED = RW_MAPPED_SHARED | RO_MAPPED_SHARED | MASK_UNMAPPED | DECOMMITTED
	};
};

/// <summary>
/// Logical address space of some Windows process.
/// </summary>
class AddressSpace
{
public:
	/// <summary>
	/// Initializes this instance for current process.
	/// </summary>
	void initialize()
	{
		initialize(GetCurrentProcessId(), GetCurrentProcess());
	}

	/// <summary>
	/// Terminates this instance.
	/// </summary>
	void terminate();

	HANDLE process() const
	{
		return process_;
	}

	bool is_current_process() const
	{
		return GetCurrentProcess() == process_;
	}

	void* end() const
	{
		return (void*)(directory_size_ * ALLOCATION_GRANULARITY);
	}

	struct BlockInfo
	{
		// Currently we are using only one field. But we create structure for possible future extensions.
		HANDLE mapping;
	};

	BlockInfo& block(const void* address);
	BlockInfo* allocated_block(const void* address);

	enum MappingType
	{
		MAP_PRIVATE = PAGE_EXECUTE_READWRITE,
		MAP_SHARED = PAGE_EXECUTE_WRITECOPY
	};

	class Block
	{
	public:
		Block(AddressSpace& space, void* address);

		BYTE* address() const
		{
			return address_;
		}

		HANDLE& mapping()
		{
			return info_.mapping;
		}

		void copy(Block& src, SIZE_T offset, SIZE_T size, LONG flags);
		void unmap(HANDLE reserve = INVALID_HANDLE_VALUE, bool no_close_handle = false);
		DWORD check_committed(SIZE_T offset, SIZE_T size);
		void change_protection(SIZE_T offset, SIZE_T size, LONG flags);
		void decommit(SIZE_T offset, SIZE_T size);
		bool is_copy(Block& other, SIZE_T offset, SIZE_T size);

		struct State
		{
			enum
			{
				INVALID = 0,
				RESERVED = MEM_RESERVE,
				MAPPED = MEM_MAPPED
			};

			DWORD state;

			/// <summary>
			/// The OR of all page_state.
			/// </summary>
			DWORD page_state_bits;

			union
			{
				/// <summary>
				/// Valid if block is mapped.
				/// </summary>
				struct
				{
					DWORD page_state[PAGES_PER_BLOCK];
				}
				mapped;

				/// <summary>
				/// Valid if block is reserved.
				/// </summary>
				struct
				{
					BYTE* begin;
					BYTE* end;
				}
				reserved;
			};

			State() :
				state(INVALID)
			{}
		};

		const State& state();

	protected:
		friend class MemoryWindows;

		void invalidate_state()
		{
			state_.state = State::INVALID;
		}

		void map(HANDLE mapping, MappingType protection, bool commit = false);

		bool has_data_outside_of(SIZE_T offset, SIZE_T size, DWORD mask = PageState::MASK_ACCESS);

	private:
		friend class AddressSpace;
		void copy(bool remap, bool move, Block& src, SIZE_T offset, SIZE_T size, LONG flags);

		bool can_move(SIZE_T offset, SIZE_T size, LONG flags)
		{
			bool move = false;
			if (flags & Memory::DECOMMIT) {
				if (flags & (Memory::RELEASE & ~Memory::DECOMMIT))
					move = true;
				else
					move = !has_data_outside_of(offset, size);
			}
			return move;
		}

	private:
		AddressSpace & space_;
		BYTE* address_;
		BlockInfo& info_;
		State state_;
	};

	void* reserve(SIZE_T size, LONG flags = 0, void* dst = 0);
	void release(void* ptr, SIZE_T size);

	// Quick copy for block sizes <= ALLOCATION_GRANULARITY
	void* copy(Block& src, SIZE_T offset, SIZE_T size, LONG flags);

	void query(const void* address, MEMORY_BASIC_INFORMATION& mbi) const
	{
		verify(VirtualQueryEx(process_, address, &mbi, sizeof(mbi)));
	}

	void check_allocated(void* ptr, SIZE_T size);
	DWORD check_committed(void* ptr, SIZE_T size);
	void change_protection(void* ptr, SIZE_T size, LONG flags);
	void decommit(void* ptr, SIZE_T size);

	bool is_readable(const void* p, SIZE_T size);
	bool is_writable(const void* p, SIZE_T size);
	bool is_private(const void* p, SIZE_T size);
	bool is_copy(const void* p, const void* plocal, SIZE_T size);

protected:
	void initialize(DWORD process_id, HANDLE process_handle);

private:
	friend class MemoryWindows;

	void* map(HANDLE mapping, MappingType protection);
	void protect(void* address, SIZE_T size, DWORD protection)
	{
		DWORD old;
		verify(VirtualProtectEx(process_, address, size, protection, &old));
	}

private:
	static void concurrency()
	{
		Sleep(0);
	}

private:
	HANDLE process_;
	HANDLE mapping_;
#ifdef _WIN64
	static const size_t SECOND_LEVEL_BLOCK = ALLOCATION_GRANULARITY / sizeof(BlockInfo);
	BlockInfo** directory_;
#else
	BlockInfo* directory_;
#endif
	size_t directory_size_;
};

inline bool AddressSpace::is_private(const void* p, SIZE_T size)
{
	for (const BYTE* begin = (const BYTE*)p, *end = begin + size; begin < end;) {
		MEMORY_BASIC_INFORMATION mbi;
		query(begin, mbi);
		if (mbi.Protect & (PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))
			return false;
		begin = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
	}
	return true;
}

inline bool AddressSpace::is_readable(const void* p, SIZE_T size)
{
	for (const BYTE* begin = (const BYTE*)p, *end = begin + size; begin < end;) {
		MEMORY_BASIC_INFORMATION mbi;
		query(begin, mbi);
		if (!(mbi.Protect & PageState::MASK_ACCESS))
			return false;
		begin = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
	}
	return true;
}

inline bool AddressSpace::is_writable(const void* p, SIZE_T size)
{
	for (const BYTE* begin = (const BYTE*)p, *end = begin + size; begin < end;) {
		MEMORY_BASIC_INFORMATION mbi;
		query(begin, mbi);
		if (!(mbi.Protect & PageState::MASK_RW))
			return false;
		begin = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
	}
	return true;
}

}
}
}

#endif
