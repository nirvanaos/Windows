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
#include "pch.h"
#include "AddressSpace.inl"
#include <Nirvana/real_copy.h>
#include <ExecDomain.h>
#include <Nirvana/signal_defs.h>
#include "ex2signal.h"
#include <winternl.h>
#include "DebugLog.h"

#ifndef NDEBUG
#include <DbgHelp.h>
#endif

#pragma comment (lib, "ntdll.lib")

// wsprintfW
#pragma comment (lib, "User32.lib")

namespace Nirvana {
namespace Core {
namespace Windows {

StaticallyAllocated <AddressSpace <sizeof (void*) == 8> > local_address_space;

#if !defined (_WIN64)

DWORD64 wow64_func [WOW64_FUNC_CNT];

const WCHAR* const wow64_dll_name = L"ntdll.dll";

const char* const wow64_func_names [WOW64_FUNC_CNT] = {
	"NtQueryVirtualMemory",
	"NtProtectVirtualMemory",
	"NtAllocateVirtualMemoryEx",
	"NtFreeVirtualMemory",
	"NtMapViewOfSectionEx",
	"NtUnmapViewOfSectionEx"
};

#endif

inline ULONG handle_count (HANDLE h)
{
	PUBLIC_OBJECT_BASIC_INFORMATION info;
	NTSTATUS err = NtQueryObject (h, ObjectBasicInformation, &info, sizeof (info), nullptr);
	if (!err)
		return info.HandleCount;
	assert (false);
	return 0;
}

void BlockState::query (HANDLE process)
{
	NIRVANA_VERIFY (QueryWorkingSetEx (process, page_state, sizeof (page_state)));
	PageState* ps = page_state;
	do {
		assert (ps->VirtualAttributes.Valid || 0 == ps->VirtualAttributes.Win32Protection);
		// If committed page was not accessed, it's state remains invalid.
		// For such pages we call VirtualQuery to obtain memory protection.
		if (!ps->VirtualAttributes.Valid && ps->VirtualAttributes.Shared) {
			MEMORY_BASIC_INFORMATION mbi;
			NIRVANA_VERIFY (VirtualQueryEx (process, ps->VirtualAddress, &mbi, sizeof (mbi)));
			BYTE* end = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
			do {
				ps->VirtualAttributes.Win32Protection = mbi.Protect;
				++ps;
			} while (ps < std::end (page_state) && ps->VirtualAddress < end);
		} else
			++ps;
	} while (ps < std::end (page_state));
}

}

using namespace Windows;

namespace Port {

const BlockState& Memory::Block::state ()
{
	assert (State::RESERVED != state_);
	if (State::PAGE_STATE_UNKNOWN == state_) {
		block_state_.query (space_.process ());
		state_ = State::MAPPED;
	}
	return block_state_;
}

bool Memory::Block::has_data (size_t offset, size_t size, uint32_t mask)
{
	if (State::RESERVED == state_)
		return false;

	size_t offset_end = offset + size;
	assert (offset_end <= ALLOCATION_GRANULARITY);
	auto page_state = state ().page_state;
	for (auto ps = page_state + (offset + PAGE_SIZE - 1) / PAGE_SIZE, end = page_state + offset_end / PAGE_SIZE; ps < end; ++ps) {
		if (mask & ps->state ())
			return true;
	}
	return false;
}

bool Memory::Block::has_data_outside_of (size_t offset, size_t size, uint32_t mask)
{
	if (State::RESERVED == state_)
		return false;

	size_t offset_end = offset + size;
	assert (offset_end <= ALLOCATION_GRANULARITY);
	if (offset || size < ALLOCATION_GRANULARITY) {
		auto page_state = state ().page_state;
		if (offset) {
			for (auto ps = page_state, end = page_state + (offset + PAGE_SIZE - 1) / PAGE_SIZE; ps < end; ++ps) {
				if (mask & ps->state ())
					return true;
			}
		}
		if (offset_end < ALLOCATION_GRANULARITY) {
			for (auto ps = page_state + offset_end / PAGE_SIZE, end = page_state + PAGES_PER_BLOCK; ps < end; ++ps) {
				if (mask & ps->state ())
					return true;
			}
		}
	}
	return false;
}

DWORD Memory::Block::commit (size_t offset, size_t size)
{ // This operation must be thread-safe.
	assert (size);
	assert (offset + size <= ALLOCATION_GRANULARITY);

	if (INVALID_HANDLE_VALUE == mapping ())
		exclusive_lock ();
restart:
	DWORD ret = 0;	// Page state bits in committed region
	if (INVALID_HANDLE_VALUE == mapping ()) {
		HANDLE hm = new_mapping ();
		try {
			map (hm, hm);
		} catch (...) {
			CloseHandle (hm);
			throw;
		}
		size_t begin = round_down (offset, PAGE_SIZE);
		size = round_up (offset + size, PAGE_SIZE) - begin;
		if (!VirtualAlloc ((BYTE*)address () + begin, size, MEM_COMMIT, PageState::READ_WRITE_PRIVATE))
			throw_NO_MEMORY ();
		ret = PageState::READ_WRITE_PRIVATE;
	} else {
		const BlockState& bs = state ();
		auto page = bs.page_state + offset / PAGE_SIZE,
			end_page = bs.page_state + (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;
		do {
			DWORD access = PageState::MASK_ACCESS & page->state ();
			if (!access)
				break;
			ret |= access;
		} while (end_page != ++page);

		if (page != end_page) {

			DWORD protection = PageState::READ_WRITE_PRIVATE;

			// If the memory section is shared, we mustn't commit pages.
			// If the page is not committed and we commit it, it will become committed in another block.
			if (handle_count (mapping ()) > 1) {
				auto not_committed = page;
				do {
					if (PageState::MASK_NOT_COMMITTED & not_committed->state ())
						break;
				} while (end_page != ++not_committed);

				if (not_committed != end_page) {
					if (exclusive_lock ())
						goto restart;
					remap ();
					invalidate_state ();
					state ();
				} else
					protection = PageState::READ_WRITE_SHARED;
			}

			do {
				auto region_end = page + 1;
				if (PageState::MASK_NOT_COMMITTED & page->state ()) {
					// The region is not committed, commit
					for (; region_end != end_page; ++region_end) {
						if (!(PageState::MASK_NOT_COMMITTED & region_end->state ()))
							break;
					}
					if (!VirtualAlloc (page->VirtualAddress, (region_end - page) * PAGE_SIZE,
						MEM_COMMIT, PageState::READ_WRITE_PRIVATE)) {
#ifndef NDEBUG
						DWORD err = GetLastError ();
						MEMORY_BASIC_INFORMATION mbi;
						VirtualQuery (page->VirtualAddress, &mbi, sizeof (mbi));
						invalidate_state ();
						state ();
#endif
						throw_NO_MEMORY ();
					}
					ret |= PageState::READ_WRITE_PRIVATE;
				} else {
					// The regions has no access, enable access
					assert (page->is_mapped ());
					for (; region_end != end_page; ++region_end) {
						if ((PageState::MASK_NOT_COMMITTED | PageState::MASK_ACCESS) & region_end->state ())
							break;
						assert (region_end->is_mapped ());
					}
					protect ((page - bs.page_state) * PAGE_SIZE, (region_end - page) * PAGE_SIZE, protection);
					ret |= protection;
				}
				for (page = region_end; page != end_page; ++page) {
					DWORD access = PageState::MASK_ACCESS & page->state ();
					if (!access)
						break;
					ret |= access;
				}
			} while (page != end_page);

			invalidate_state ();
		}
	}
	return ret;
}

DWORD Memory::Block::check_committed (size_t offset, size_t size)
{
	assert (offset + size <= ALLOCATION_GRANULARITY);
	if (reserved ())
		throw_BAD_PARAM ();
	const BlockState& bs = state ();
	DWORD state_bits = 0;
	for (auto ps = bs.page_state + offset / PAGE_SIZE, end = bs.page_state + (offset + size + PAGE_SIZE - 1) / PAGE_SIZE; ps < end; ++ps) {
		DWORD state = ps->state ();
		if (!(PageState::MASK_ACCESS & state))
			throw_BAD_PARAM ();
		state_bits |= state;
	}
	return state_bits;
}

bool Memory::Block::need_remap_to_share (size_t offset, size_t size)
{
	if (reserved ())
		throw_BAD_PARAM ();
	const BlockState& st = state ();
	for (auto ps = st.page_state + offset / PAGE_SIZE, end = st.page_state + (offset + size + PAGE_SIZE - 1) / PAGE_SIZE; ps < end; ++ps) {
		if (!ps->is_committed ())
			throw_BAD_PARAM ();
		if (!ps->is_mapped ())
			return true;
	}

	return false;
}

void Memory::Block::prepare_to_share_no_remap (size_t offset, size_t size)
{
	assert (offset + size <= ALLOCATION_GRANULARITY);
	assert (!reserved ());

	// Prepare pages
	const BlockState& st = state ();
	auto region_begin = st.page_state + offset / PAGE_SIZE, block_end = st.page_state + (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;
	do {
		auto region_end = region_begin;
		DWORD protection = region_begin->protection ();
		do
			++region_end;
		while (region_end < block_end && protection == region_end->protection ());

		if (PageState::READ_WRITE_PRIVATE == protection)
			protect ((region_begin - st.page_state) * PAGE_SIZE, (region_end - region_begin) * PAGE_SIZE, PageState::READ_WRITE_SHARED);

		region_begin = region_end;
	} while (region_begin < block_end);
}

void Memory::Block::remap (const CopyReadOnly* copy_rgn)
{
	assert (exclusive_locked ());

	// Create a new memory section.
	HANDLE hm = new_mapping ();
	try {
		// Map memory section to temporary address.
		BYTE* ptmp = (BYTE*)MapViewOfFile (hm, FILE_MAP_WRITE, 0, 0, ALLOCATION_GRANULARITY);
		if (!ptmp)
			throw_NO_MEMORY ();

		// Create copy of page protection array
		DWORD page_protection [PAGES_PER_BLOCK];
		{
			DWORD* dst = page_protection;
			for (const PageState* ps = state ().page_state, *end = ps + PAGES_PER_BLOCK; ps != end; ++ps, ++dst) {
				*dst = ps->protection ();
			}
		}

		// Copy data to the temporary address.
		try {
			DWORD* region_begin = page_protection;
			DWORD* copy_begin = nullptr, * copy_end = nullptr;
			if (copy_rgn) {
				copy_begin = page_protection + (copy_rgn->offset + PAGE_SIZE - 1) / PAGE_SIZE;
				copy_end = page_protection + (copy_rgn->offset + copy_rgn->size) / PAGE_SIZE;
				std::fill (copy_begin, copy_end, 0);
			}
			do {
				auto region_end = region_begin;
				if (PageState::MASK_ACCESS & *region_begin) {
					DWORD access_mask = 0;
					do {
						access_mask |= *region_end;
						++region_end;
					} while (region_end < std::end (page_protection) && (PageState::MASK_ACCESS & *region_end));

					size_t offset = (region_begin - page_protection) * PAGE_SIZE;
					LONG_PTR* dst = (LONG_PTR*)(ptmp + offset);
					size_t size = (region_end - region_begin) * PAGE_SIZE;
					if (!VirtualAlloc (dst, size, MEM_COMMIT, PageState::READ_WRITE_PRIVATE))
						throw_NO_MEMORY ();
					
					// Temporary disable write access to save data consistency.
					if (access_mask & PageState::MASK_RW)
						protect (offset, size, PAGE_READONLY);

					LONG_PTR* src = (LONG_PTR*)((BYTE*)address () + offset);
					real_copy (src, src + size / sizeof (LONG_PTR), dst);
				} else {
					do
						++region_end;
					while (region_end < std::end (page_protection) && !(PageState::MASK_ACCESS & *region_end));
				}

				region_begin = region_end;
			} while (region_begin < std::end (page_protection));

			if (copy_rgn) {
				if (copy_begin < copy_end) {
					std::fill (copy_begin, copy_end, PageState::READ_ONLY);
					size_t offset = (copy_begin - page_protection) * PAGE_SIZE;
					LONG_PTR* dst = (LONG_PTR*)(ptmp + offset);
					size_t size = (copy_end - copy_begin) * PAGE_SIZE;
					if (!VirtualAlloc (dst, size, MEM_COMMIT, PageState::READ_WRITE_PRIVATE))
						throw_NO_MEMORY ();
				}
				const BYTE* src = (const BYTE*)copy_rgn->src;
				real_copy (src, src + copy_rgn->size, ptmp + copy_rgn->offset);
			}

		} catch (...) {
			NIRVANA_VERIFY (UnmapViewOfFile (ptmp));
			// Restore write access
			for (const DWORD* p = page_protection, *end = p + PAGES_PER_BLOCK; p != end; ++p) {
				DWORD protection = *p;
				if (PageState::MASK_RW & protection)
					protect ((p - page_protection) * PAGE_SIZE, PAGE_SIZE, protection);
			}
			throw;
		}

		// Unmap memory section from the temporary address.
		NIRVANA_VERIFY (UnmapViewOfFile (ptmp));

		// Change this block mapping to the new.
		map (hm, hm);

		// Restore protection.
		adjust_protection (page_protection);

	} catch (...) {
		CloseHandle (hm);
		throw;
	}
}

void Memory::Block::copy_aligned (Block& src_block, void* src, size_t size, unsigned flags)
{
	// NOTE: Memory::SRC_DECOMMIT and Memory::SRC_RELEASE flags used only for optimization.
	// We don't perform actual decommit or release here.
	assert (size);
	size_t offset = (uintptr_t)src % ALLOCATION_GRANULARITY;
	assert (offset + size <= ALLOCATION_GRANULARITY);
	assert (exclusive_locked ());
	assert (src_block.exclusive_locked ());

	if ((flags & Nirvana::Memory::SIMPLE_COPY) && has_data (offset, size, PageState::MASK_NO_WRITE))
		throw_NO_PERMISSION ();

	bool remap = true;
	bool src_remap = false;
	if (INVALID_HANDLE_VALUE != mapping ()) {
		if (CompareObjectHandles (mapping (), src_block.mapping ())) {
			// Source and target handles are the same
			if (src_block.need_remap_to_share (offset, size)) {
				if (has_data_outside_of (offset, size, PageState::MASK_UNMAPPED))
					goto fallback;
				else
					src_remap = true;
			} else if (!need_remap_to_share (offset, size)) {
				remap = false; // Current mapping is OK
				// But we have to check and change protection if need
			} else if (has_data_outside_of (offset, size, PageState::MASK_UNMAPPED))
				goto fallback;
		} else if (has_data_outside_of (offset, size))
			goto fallback;
		else
			src_remap = src_block.need_remap_to_share (offset, size);
	} else
		src_remap = src_block.need_remap_to_share (offset, size);

	if (src_remap)
		src_block.remap ();

	// Virtual copy or virtual move?
	if (remap && !(flags & Nirvana::Memory::SRC_DECOMMIT))	// Memory::SRC_RELEASE includes flag DECOMMIT.
		src_block.prepare_to_share_no_remap (offset, size);

	{
		bool move = src_block.can_move (offset, size, flags);

		DWORD copied_pages_state;
		if (Nirvana::Memory::READ_ONLY & flags)
			copied_pages_state = PageState::READ_ONLY;
		else if (!move || handle_count (src_block.mapping ()) > 1)
			copied_pages_state = PageState::READ_WRITE_SHARED;
		else
			copied_pages_state = PageState::READ_WRITE_PRIVATE;

		if (remap && !has_data_outside_of (offset, size))
			Base::copy (src_block, offset, size, copied_pages_state);
		else {
			const Windows::BlockState& block_state = state ();
			size_t page_begin = offset / PAGE_SIZE;
			size_t page_end = (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;
			if (Nirvana::Memory::READ_ONLY & flags) {

				// Align first page up
				if (offset % PAGE_SIZE
					&& (block_state.page_state [page_begin].protection () & PageState::MASK_ACCESS))
					++page_begin;

				// Align last page down
				size_t page_last = page_end - 1;
				if (page_last > page_begin && (offset + size) % PAGE_SIZE
					&& (block_state.page_state [page_last].protection () & PageState::MASK_ACCESS))
					page_end = page_last;
			}
			if (remap) {
				// Source and target blocks had the same mapping handle,
				// but source block was remapped.
				// We need to save page state and then restore it.
				DWORD page_protection [PAGES_PER_BLOCK];
				for (size_t i = 0; i < page_begin; ++i) {
					page_protection [i] = block_state.page_state [i].protection ();
				}
				for (size_t i = page_begin; i < page_end; ++i) {
					page_protection [i] = copied_pages_state;
				}
				for (size_t i = page_end; i < PAGES_PER_BLOCK; ++i) {
					page_protection [i] = block_state.page_state [i].protection ();
				}

				map_copy (src_block.mapping ());
				adjust_protection (page_protection);

			} else {
				// Manage access to copied pages
				while (page_begin < page_end
					&& block_state.page_state [page_begin].protection () == copied_pages_state) {
					++page_begin;
				}
				if (page_begin < page_end) {
					// We need to change access of the copied pages
					protect (page_begin * PAGE_SIZE, (page_end - page_begin) * PAGE_SIZE, copied_pages_state);
					invalidate_state ();
				}
			}
		}
	}
	return;

fallback:
	// Real copy
	commit (offset, size);
	copy_unaligned (offset, size, src, flags);
}

void Memory::Block::adjust_protection (const DWORD page_protection [PAGES_PER_BLOCK])
{
	const Windows::BlockState& block_state = state ();
	size_t page = 0;

	do {
		// Skip not committed pages
		while (!block_state.page_state [page].is_committed ()) {
			assert (!(PageState::MASK_ACCESS & page_protection [page]));
			++page;
		}
		if (page == PAGES_PER_BLOCK)
			break;

		// Get range of pages with the same protection
		const size_t start_page = page;
		DWORD protection = page_protection [page];
		assert (protection);
		for (++page; page < PAGES_PER_BLOCK; ++page) {
			if (protection != page_protection [page])
				break;
			if (!block_state.page_state [page].is_committed ()) {
				assert (!(PageState::MASK_ACCESS & protection));
				break;
			}
		}
		protect (start_page * PAGE_SIZE, (page - start_page) * PAGE_SIZE, protection);
		invalidate_state ();

	} while (page < PAGES_PER_BLOCK && block_state.page_state [page].is_committed ());
}

void Memory::Block::copy_unaligned (size_t offset, size_t size, const void* src, unsigned flags)
{
	assert (size);
	assert (offset + size <= ALLOCATION_GRANULARITY);

repeat:
	DWORD page_state = check_committed (offset, size);
	if (PageState::MASK_RO & page_state) {
		// Some target pages are read-only
		if (flags & Nirvana::Memory::READ_ONLY) {
			if (PageState::MASK_UNMAPPED & page_state) {
				if (exclusive_lock ())
					goto repeat;
				// Remap
				CopyReadOnly cr = { offset, size, src };
				remap (&cr);
			} else {
				// Map memory section to temporary address.
				BYTE* ptmp = (BYTE*)MapViewOfFile (mapping (), FILE_MAP_WRITE, 0, 0, ALLOCATION_GRANULARITY);
				if (!ptmp)
					throw_NO_MEMORY ();
				real_copy ((const BYTE*)src, (const BYTE*)src + size, ptmp + offset);
				NIRVANA_VERIFY (UnmapViewOfFile (ptmp));
				if (PageState::MASK_RW & page_state)
					change_protection (offset, size, Nirvana::Memory::READ_ONLY);
			}
		} else {
			change_protection (offset, size, Nirvana::Memory::READ_WRITE);
			real_copy ((const BYTE*)src, (const BYTE*)src + size, (BYTE*)address () + offset);
		}
	} else {
		real_copy ((const BYTE*)src, (const BYTE*)src + size, (BYTE*)address () + offset);
		if (flags & Nirvana::Memory::READ_ONLY)
			change_protection (offset, size, Nirvana::Memory::READ_ONLY);
	}
}

void Memory::Block::decommit (size_t offset, size_t size)
{
	size_t offset_end = round_down (offset + size, PAGE_SIZE);
	assert (offset_end <= ALLOCATION_GRANULARITY);
	offset = round_up (offset, PAGE_SIZE);
	size = offset_end - offset;
	if (size) {
		if (!offset && offset_end == ALLOCATION_GRANULARITY)
			unmap ();
		else {
			retry:
			if (!reserved ()) {
				if (!has_data_outside_of (offset, size)) {
					if (exclusive_lock ())
						goto retry;
					unmap ();
				} else if (has_data (offset, size)) {
					// Disable access to decommitted pages. We can't use VirtualFree and MEM_DECOMMIT with mapped memory.
					protect (offset, size, PageState::DECOMMITTED | PAGE_REVERT_TO_FILE_MAP);
					NIRVANA_VERIFY (VirtualAlloc ((BYTE*)address () + offset, size, MEM_RESET, PageState::DECOMMITTED));

					// Invalidate block state.
					invalidate_state ();
				}
			}
		}
	}
}

void Memory::Block::change_protection (size_t offset, size_t size, unsigned flags)
{
	size_t offset_end = offset + size;
	assert (offset_end <= ALLOCATION_GRANULARITY);
	assert (size);

	if (flags & Nirvana::Memory::READ_ONLY) {
		offset = round_up (offset, PAGE_SIZE);
		offset_end = round_down (offset_end, PAGE_SIZE);
		ptrdiff_t cb = offset_end - offset;
		if (cb > 0)
			protect (offset, cb, PageState::READ_ONLY);
		return;
	}
	offset = round_down (offset, PAGE_SIZE);
	offset_end = round_up (offset_end, PAGE_SIZE);

	if (handle_count (mapping ()) <= 1) {
		// Not shared
		protect (offset, offset_end - offset, PageState::READ_WRITE_PRIVATE);
		return;
	}

	const PageState* page_state = state ().page_state;
	auto region_begin = page_state + offset / PAGE_SIZE, state_end = page_state + offset_end / PAGE_SIZE;
	while (region_begin < state_end) {
		auto mapped = region_begin->is_mapped ();
		auto region_end = region_begin;
		do
			++region_end;
		while (region_end < state_end && region_end->is_mapped () == mapped);

		DWORD new_prot = mapped ? PageState::READ_WRITE_SHARED : PageState::READ_WRITE_PRIVATE;
		protect ((region_begin - page_state) * PAGE_SIZE, (region_end - region_begin) * PAGE_SIZE, new_prot);

		region_begin = region_end;
	}
}

inline
bool Memory::Block::is_copy (Block& other, size_t offset, size_t size)
{
	if (reserved () || other.reserved ())
		return false;
	if (!CompareObjectHandles (mapping (), other.mapping ()))
		return false;
	const BlockState& st = state (), & other_st = other.state ();
	size_t page_begin = offset / PAGE_SIZE;
	auto pst = st.page_state + page_begin;
	auto other_pst = other_st.page_state + page_begin;
	auto pst_end = st.page_state + (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;
	for (; pst != pst_end; ++pst, ++other_pst) {
		DWORD ps = pst->state (), other_ps = other_pst->state ();
		if ((ps | other_ps) & PageState::MASK_UNMAPPED)
			return false;
		else if (!(ps & PageState::MASK_ACCESS) || !(other_ps & PageState::MASK_ACCESS))
			return false;
	}
	return true;
}

inline
bool Memory::Block::is_private (size_t offset, size_t size)
{
	if (reserved ())
		return true;
	const BlockState& st = state ();
	auto pst = st.page_state + offset / PAGE_SIZE;
	auto pst_end = st.page_state + (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;
	for (; pst != pst_end; ++pst) {
		DWORD state = pst->state ();
		if ((state & PageState::MASK_RW) && (state & PageState::MASK_MAPPED))
			return handle_count (mapping ()) <= 1;
	}
	return true;
}

inline void Memory::query (const void* address, MEMORY_BASIC_INFORMATION& mbi) noexcept
{
	NIRVANA_VERIFY (VirtualQuery (address, &mbi, sizeof (mbi)));
}

HANDLE Memory::new_mapping ()
{
	HANDLE mapping = CreateFileMappingW (INVALID_HANDLE_VALUE, 0, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, ALLOCATION_GRANULARITY, 0);
	if (!mapping)
		throw_NO_MEMORY ();
	return mapping;
}

void Memory::prepare_to_share (void* src, size_t size, unsigned flags)
{
	if (!size)
		return;
	if (!src)
		throw_BAD_PARAM ();

	for (BYTE* p = (BYTE*)src, *end = p + size; p < end;) {
		Block block (p);
		BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
		if (block_end > end)
			block_end = end;
		block.prepare_to_share (p - (BYTE*)block.address (), block_end - p, flags);
		p = block_end;
	}
}

void Memory::change_protection (void* ptr, size_t size, unsigned flags)
{
	if (!size)
		return;
	if (!ptr)
		throw_BAD_PARAM ();

	BYTE* begin = (BYTE*)ptr;
	BYTE* end = begin + size;
	for (BYTE* p = begin; p < end;) {
		Block block (p);
		BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
		if (block_end > end)
			block_end = end;
		block.change_protection (p - (BYTE*)block.address (), block_end - p, flags);
		p = block_end;
	}
}

void* Memory::allocate (void* dst, size_t& size, unsigned flags)
{
	if (!size)
		throw_BAD_PARAM ();

	if (flags & ~(Nirvana::Memory::RESERVED | Nirvana::Memory::EXACTLY | Nirvana::Memory::ZERO_INIT))
		throw_INV_FLAG ();

	void* ret;
	try {
		if (!(ret = local_address_space->reserve ((uint8_t*)dst, size, flags)))
			return nullptr;

		if (!(Nirvana::Memory::RESERVED & flags)) {
			try {
#ifndef NDEBUG
				uint32_t prot_mask =
#endif
					commit_no_check (ret, size, true);
				assert (!(prot_mask & PageState::MASK_RO));
			} catch (...) {
				local_address_space->release ((uint8_t*)ret, size);
				throw;
			}
		}
	} catch (const CORBA::NO_MEMORY&) {
		if (flags & Nirvana::Memory::EXACTLY)
			ret = nullptr;
		else
			throw;
	}
	return ret;
}

void Memory::release (void* dst, size_t size)
{
	local_address_space->release ((uint8_t*)dst, size);
}

void Memory::commit (void* ptr, size_t size)
{
	if (!size)
		return;

	if (!ptr)
		throw_BAD_PARAM ();

	// Memory must be allocated.
	local_address_space->check_allocated ((uint8_t*)ptr, size);

	for (BYTE* p = (BYTE*)ptr, *end = p + size; p < end;) {
		Block block (p, (uintptr_t)p % ALLOCATION_GRANULARITY == 0 && size >= ALLOCATION_GRANULARITY);
		BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
		if (block_end > end)
			block_end = end;
		size_t offset = p - (BYTE*)block.address ();
		size_t size = block_end - p;
		DWORD state = block.commit (offset, size);
		if (state & PageState::MASK_RO)
			block.change_protection (offset, size, Nirvana::Memory::READ_WRITE);
		p = block_end;
	}
}

uint32_t Memory::commit_no_check (void* ptr, size_t size, bool exclusive)
{
	uint32_t state_bits = 0; // Page states bit mask
	for (BYTE* p = (BYTE*)ptr, *end = p + size; p < end;) {
		Block block (p, exclusive || ((uintptr_t)p % ALLOCATION_GRANULARITY == 0 && size >= ALLOCATION_GRANULARITY));
		BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
		if (block_end > end)
			block_end = end;
		state_bits |= block.commit (p - (BYTE*)block.address (), block_end - p);
		p = block_end;
	}
	return state_bits;
}

void Memory::decommit (void* ptr, size_t size)
{
	if (!size)
		return;

	// Memory must be allocated.
	local_address_space->check_allocated ((uint8_t*)ptr, size);

	for (BYTE* p = (BYTE*)ptr, *end = p + size; p < end;) {
		Block block (p, (uintptr_t)p % ALLOCATION_GRANULARITY == 0 && size >= ALLOCATION_GRANULARITY);
		BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
		if (block_end > end)
			block_end = end;
		block.decommit (p - (BYTE*)block.address (), block_end - p);
		p = block_end;
	}
}

inline
uint32_t Memory::check_committed (void* ptr, size_t size, uint32_t& type)
{
	uint32_t state_bits = 0;
	uint32_t t = 0;
	for (const BYTE* begin = (const BYTE*)ptr, *end = begin + size; begin < end;) {
		MEMORY_BASIC_INFORMATION mbi;
		query (begin, mbi);
		if (!(mbi.Protect & PageState::MASK_ACCESS))
			throw_BAD_PARAM ();
		state_bits |= mbi.Protect;
		t |= mbi.Type;
		begin = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
	}
	type = t;
	return state_bits;
}

void* Memory::copy (void* dst, void* src, size_t& size, unsigned flags)
{
	if (!size)
		return dst;
	if (!src)
		throw_BAD_PARAM ();

	if (flags != Nirvana::Memory::SIMPLE_COPY && (flags & ~(Nirvana::Memory::READ_ONLY
		| Nirvana::Memory::SRC_RELEASE
		| Nirvana::Memory::DST_ALLOCATE
		| Nirvana::Memory::EXACTLY
		)))
		throw_INV_FLAG ();

	bool src_own = false, dst_own = false;

	// release_flags can be 0, Memory::SRC_RELEASE, Memory::SRC_DECOMMIT.
	unsigned release_flags = flags & Nirvana::Memory::SRC_RELEASE;

	// Source range has to be committed.
	uint32_t src_state_mask;
	uint32_t src_type;
	if (local_address_space->allocated_block ((uint8_t*)src)) {
		src_state_mask = 0;
		for (BYTE* p = (BYTE*)src, *end = p + size; p < end;) {
			Block block (p);
			BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
			if (block_end > end)
				block_end = end;
			src_state_mask |= block.check_committed (p - (BYTE*)block.address (), block_end - p);
			p = block_end;
		}
		src_own = true;
	} else {
		if (release_flags)
			throw_FREE_MEM (); // Can't release memory that is not own.
		src_state_mask = check_committed (src, size, src_type);
	}

	uintptr_t src_align = (uintptr_t)src % ALLOCATION_GRANULARITY;
	try {
		Region allocated = {0, 0};
		if (!dst || (flags & Nirvana::Memory::DST_ALLOCATE)) {
			if (flags & Nirvana::Memory::SIMPLE_COPY)
				throw_INV_FLAG ();

			if (dst) {
				if (dst == src) {
					if ((Nirvana::Memory::EXACTLY & flags) && Nirvana::Memory::SRC_RELEASE != (flags & Nirvana::Memory::SRC_RELEASE))
						return nullptr;
					dst = nullptr;
				} else {
					// Try reserve space exactly at dst.
					// Target region can overlap with source.
					allocated.ptr = dst;
					allocated.size = size;
					allocated.subtract (round_down (src, ALLOCATION_GRANULARITY), round_up ((BYTE*)src + size, ALLOCATION_GRANULARITY));
					dst = local_address_space->reserve ((uint8_t*)allocated.ptr, allocated.size, flags | Nirvana::Memory::EXACTLY);
					if (!dst) {
						if (flags & Nirvana::Memory::EXACTLY)
							return nullptr;
						allocated.size = 0;
					}
				}
			}
			if (!dst) {
				if (Nirvana::Memory::SRC_RELEASE == release_flags)
					dst = src; // Memory::SRC_RELEASE is specified, so we can use source block as destination
				else {
					size_t dst_align = src_own ? src_align : 0;
					size_t cb_res = size + dst_align;
					BYTE* res = (BYTE*)local_address_space->reserve (nullptr, cb_res, flags);
					if (!res) {
						assert (flags & Nirvana::Memory::EXACTLY);
						return nullptr;
					}
					dst = res + dst_align;
					allocated.ptr = res;
					allocated.size = cb_res;
				}
			}
			assert (dst);
			dst_own = true;
		} else {
			if (local_address_space->allocated_block ((uint8_t*)dst)) {
				local_address_space->check_allocated ((uint8_t*)dst, size);
				dst_own = true;
			} else if ((dst != src) && !is_writable (dst, size))
				throw_NO_PERMISSION ();
		}

		assert (dst);

		if (dst == src) { // Special case - change protection.
			// We allow to change protection of the not-allocated memory.
			// This is necessary for core services like module load.
			if (flags & Nirvana::Memory::SIMPLE_COPY)
				throw_NO_PERMISSION ();

			if (src_state_mask & ((flags & Nirvana::Memory::READ_ONLY) ? PageState::MASK_RW : PageState::MASK_RO)) {
				if (dst_own)
					change_protection (src, size, flags);
				else if ((uintptr_t)dst % PAGE_SIZE == 0) {
					uint32_t prot;
					if (flags & Nirvana::Memory::READ_ONLY)
						prot = PAGE_READONLY;
					else if (src_type == MEM_IMAGE)
						prot = PAGE_WRITECOPY;
					else
						prot = PAGE_READWRITE;
					protect (src, round_up (size, PAGE_SIZE), prot);
				} else
					throw_BAD_PARAM ();
			}
			return src;
		}

		try {
			if (dst_own) {
				if (src_own && (uintptr_t)dst % ALLOCATION_GRANULARITY == src_align) {
					// Share (regions may overlap).
					// To avoid deadlock, we must always lock source and target blocks in the same order.
					// Block with lesser address is locked first.
					if (dst < src) {
						// Destination block lock first
						BYTE* d_p = (BYTE*)dst, * d_end = d_p + size;
						BYTE* s_p = (BYTE*)src;
						if (d_end > src) {
							// Copy overlapped part with Memory::SRC_DECOMMIT.
							BYTE* overlapped_end = round_up (d_end - ((BYTE*)src + size - d_end), ALLOCATION_GRANULARITY);
							assert (overlapped_end < d_end);
							unsigned overlapped_flags = (flags & ~Nirvana::Memory::SRC_RELEASE) | Nirvana::Memory::SRC_DECOMMIT;
							while (d_p < overlapped_end) {
								size_t cb = round_down (d_p, ALLOCATION_GRANULARITY) + ALLOCATION_GRANULARITY - d_p;
								Block block (d_p, true);
								Block src_block (s_p, true);
								block.copy_aligned (src_block, s_p, cb, overlapped_flags);
								d_p += cb;
								s_p += cb;
							}
						}
						while (d_p < d_end) {
							size_t cb = std::min (round_down (d_p, ALLOCATION_GRANULARITY) + ALLOCATION_GRANULARITY, d_end) - d_p;
							Block block (d_p, true);
							Block src_block (s_p, true);
							block.copy_aligned (src_block, s_p, cb, flags);
							d_p += cb;
							s_p += cb;
						}
					} else {
						// Source block lock first
						BYTE* s_end = (BYTE*)src + size;
						BYTE* d_p = (BYTE*)dst + size, * s_p = (BYTE*)src + size;
						if (dst < s_end) {
							// Copy overlapped part with Memory::SRC_DECOMMIT.
							BYTE* overlapped_begin = round_down ((BYTE*)dst + ((BYTE*)dst - (BYTE*)src), ALLOCATION_GRANULARITY);
							assert (overlapped_begin > dst);
							unsigned overlapped_flags = (flags & ~Nirvana::Memory::SRC_RELEASE) | Nirvana::Memory::SRC_DECOMMIT;
							while (d_p > overlapped_begin) {
								BYTE* block_begin = round_down (d_p - 1, ALLOCATION_GRANULARITY);
								size_t cb = d_p - block_begin;
								s_p -= cb;
								Block src_block (s_p, true);
								Block block (block_begin, true);
								block.copy_aligned (src_block, s_p, cb, overlapped_flags);
								d_p = block_begin;
							}
						}
						while (d_p > dst) {
							BYTE* block_begin = round_down (d_p - 1, ALLOCATION_GRANULARITY);
							if (block_begin < dst)
								block_begin = (BYTE*)dst;
							size_t cb = d_p - block_begin;
							s_p -= cb;
							Block src_block (s_p, true);
							Block block (block_begin, true);
							block.copy_aligned (src_block, s_p, cb, flags);
							d_p = block_begin;
						}
					}
				} else {
					// Physical copy.
					uint32_t dst_state_mask = commit_no_check (dst, size);
					if ((dst_state_mask & PageState::MASK_RO) || (flags & Nirvana::Memory::READ_ONLY)) {
						if (flags & Nirvana::Memory::SIMPLE_COPY)
							throw_NO_PERMISSION ();

						if (dst < src) {
							BYTE* d_p = (BYTE*)dst, *d_end = d_p + size;
							BYTE* s_p = (BYTE*)src;
							while (d_p < d_end) {
								size_t cb = std::min (round_down (d_p, ALLOCATION_GRANULARITY) + ALLOCATION_GRANULARITY, d_end) - d_p;
								Block block (d_p, cb == ALLOCATION_GRANULARITY);
								block.copy_unaligned (d_p - (BYTE*)block.address (), cb, s_p, flags);
								d_p += cb;
								s_p += cb;
							}
						} else {
							BYTE* d_p = (BYTE*)dst + size, * s_p = (BYTE*)src + size;
							while (d_p > dst) {
								BYTE* block_begin = round_down (d_p - 1, ALLOCATION_GRANULARITY);
								if (block_begin < dst)
									block_begin = (BYTE*)dst;
								size_t cb = d_p - block_begin;
								Block block (block_begin, cb == ALLOCATION_GRANULARITY);
								s_p -= cb;
								block.copy_unaligned (block_begin - (BYTE*)block.address (), cb, s_p, flags);
								d_p = block_begin;
							}
						}
					} else
						real_move ((const BYTE*)src, (const BYTE*)src + size, (BYTE*)dst);
				}
			} else {
				// Physical copy not own memory.
				real_move ((const BYTE*)src, (const BYTE*)src + size, (BYTE*)dst);
			}

			if (flags & Nirvana::Memory::SRC_DECOMMIT) {
				// Release or decommit source region. Source and target regions can overlap.
				Region reg = {src, size}; // source region
				if (flags & (Nirvana::Memory::SRC_RELEASE & ~Nirvana::Memory::SRC_DECOMMIT)) {
					// Release
					if (reg.subtract (round_down (dst, ALLOCATION_GRANULARITY), round_up ((BYTE*)dst + size, ALLOCATION_GRANULARITY)))
						release (reg.ptr, reg.size);
				} else {
					// Decommit
					if (reg.subtract (round_down (dst, PAGE_SIZE), round_up ((BYTE*)dst + size, PAGE_SIZE)))
						decommit (reg.ptr, reg.size);
				}
			}
		} catch (...) {
			release (allocated.ptr, allocated.size);
			throw;
		}

		if (allocated.size)
			size = allocated.size;

	} catch (const CORBA::NO_MEMORY&) {
		if (Nirvana::Memory::EXACTLY & flags)
			dst = nullptr;
		else
			throw;
	}

	return dst;
}

uintptr_t Memory::query (const void* p, Nirvana::Memory::QueryParam q)
{
	switch (q) {

		case Nirvana::Memory::QueryParam::ALLOCATION_SPACE_BEGIN:
		{
			SYSTEM_INFO sysinfo;
			GetSystemInfo (&sysinfo);
			return (uintptr_t)sysinfo.lpMinimumApplicationAddress;
		}

		case Nirvana::Memory::QueryParam::ALLOCATION_SPACE_END:
			return (uintptr_t)local_address_space->end ();

		case Nirvana::Memory::QueryParam::ALLOCATION_UNIT:
		case Nirvana::Memory::QueryParam::GRANULARITY:
		case Nirvana::Memory::QueryParam::SHARING_ASSOCIATIVITY:
		case Nirvana::Memory::QueryParam::OPTIMAL_COMMIT_UNIT:
			return ALLOCATION_GRANULARITY;

		case Nirvana::Memory::QueryParam::PROTECTION_UNIT:
		case Nirvana::Memory::QueryParam::COMMIT_UNIT:
		case Nirvana::Memory::QueryParam::SHARING_UNIT:
			return PAGE_SIZE;

		case Nirvana::Memory::QueryParam::FLAGS:
			return FLAGS;

		case Nirvana::Memory::QueryParam::MEMORY_STATE:
		{
			MEMORY_BASIC_INFORMATION mbi;
			query (p, mbi);
			switch (mbi.State) {
				case MEM_FREE:
					return (uintptr_t)Nirvana::Memory::MemoryState::MEM_NOT_ALLOCATED;
				case MEM_RESERVE:
					return (uintptr_t)Nirvana::Memory::MemoryState::MEM_RESERVED;
				default:
					if (mbi.Protect & PageState::MASK_RW)
						return (uintptr_t)Nirvana::Memory::MemoryState::MEM_READ_WRITE;
					else
						return (uintptr_t)Nirvana::Memory::MemoryState::MEM_READ_ONLY;
			}
		}

	}

	throw_BAD_PARAM ();
}

bool Memory::is_readable (const void* p, size_t size)
{
	for (const BYTE* begin = (const BYTE*)p, *end = begin + size; begin < end;) {
		MEMORY_BASIC_INFORMATION mbi;
		query (begin, mbi);
		if (!(mbi.Protect & PageState::MASK_ACCESS))
			return false;
		begin = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
	}
	return true;
}

bool Memory::is_private (const void* p, size_t size)
{
	for (const BYTE* begin = (const BYTE*)p, *end = begin + size; begin < end;) {
		Block block (const_cast <BYTE*> (begin));
		const BYTE* block_end = (BYTE*)block.address () + ALLOCATION_GRANULARITY;
		if (block_end > end)
			block_end = end;
		if (!block.is_private (begin - (BYTE*)block.address (), block_end - begin))
			return false;
		begin = block_end;
	}
	return true;
}

bool Memory::is_writable (const void* p, size_t size)
{
	for (const BYTE* begin = (const BYTE*)p, *end = begin + size; begin < end;) {
		MEMORY_BASIC_INFORMATION mbi;
		query (begin, mbi);
		if (!(mbi.Protect & PageState::MASK_RW))
			return false;
		begin = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
	}
	return true;
}

bool Memory::is_copy (const void* p, const void* plocal, size_t size)
{
	if ((uintptr_t)p % ALLOCATION_GRANULARITY == (uintptr_t)plocal % ALLOCATION_GRANULARITY) {
		try {
			for (BYTE* begin1 = (BYTE*)p, *end1 = begin1 + size, *begin2 = (BYTE*)plocal; begin1 < end1;) {
				Block block1 (begin1);
				Block block2 (begin2);
				BYTE* block_end1 = (BYTE*)block1.address () + ALLOCATION_GRANULARITY;
				if (block_end1 > end1)
					block_end1 = end1;
				if (!block1.is_copy (block2, begin1 - (BYTE*)block1.address (), block_end1 - begin1))
					return false;
				begin1 = block_end1;
				begin2 = (BYTE*)block2.address () + ALLOCATION_GRANULARITY;
			}
			return true;
		} catch (...) {
			return false;
		}
	} else
		return false;
}

} // namespace Port

namespace Windows {

long __stdcall exception_filter (EXCEPTION_POINTERS* pex)
{
	DWORD exc = pex->ExceptionRecord->ExceptionCode;
	if (
		EXCEPTION_ACCESS_VIOLATION == exc
		&&
		pex->ExceptionRecord->NumberParameters >= 2
		&&
		!(pex->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
		) {
		void* address = (void*)pex->ExceptionRecord->ExceptionInformation [1];

		// Probably this is a transient failure related to the memory remapping.

		BlockInfo* block = local_address_space->allocated_block ((uint8_t*)address);
		if (block) {
			HANDLE mapping = block->mapping.lock ();
			if (INVALID_HANDLE_VALUE == mapping || nullptr == mapping) {
				block->mapping.unlock ();
			} else {
				MEMORY_BASIC_INFORMATION mbi;
				NIRVANA_VERIFY (VirtualQuery (address, &mbi, sizeof (mbi)));
				block->mapping.unlock ();
				if (pex->ExceptionRecord->ExceptionInformation [0]) { // Write access
					if (mbi.Protect & PageState::MASK_RW)
						return EXCEPTION_CONTINUE_EXECUTION;
				} else if (mbi.Protect & PageState::MASK_ACCESS)
					return EXCEPTION_CONTINUE_EXECUTION;
			}
		}
	}

	siginfo_t signal;
	if (ex2signal (pex, signal))
		return ExecDomain::on_signal (signal) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
	else
		return EXCEPTION_CONTINUE_SEARCH;
}

long __stdcall unhandled_exception_filter (EXCEPTION_POINTERS* pex);

} // namespace Windows

namespace Port {

void* Memory::exception_handler_;

bool Memory::initialize () noexcept
{
	SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

#if !defined (NDEBUG) && defined (_WIN32)
	if (!IsDebuggerPresent ()) {
		_CrtSetReportMode (_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_WARN, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode (_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	}
#endif

	SetUnhandledExceptionFilter (&unhandled_exception_filter);

	DebugLog::initialize ();

	if (!address_space_init ())
		return false;
	
	if (!(exception_handler_ = AddVectoredExceptionHandler (TRUE, &exception_filter)))
		return false;
	
	return true;
}

void Memory::terminate () noexcept
{
	RemoveVectoredExceptionHandler (exception_handler_);
	
	address_space_term ();

	DebugLog::terminate ();

	SetUnhandledExceptionFilter (nullptr);
}

}
}
}
