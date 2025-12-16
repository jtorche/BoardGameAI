#pragma once

#include "type.h"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <new>
#include <algorithm>
#include <cassert>

namespace core
{
	class LinearAllocator
	{
	public:
		explicit LinearAllocator(size_t pageSize);
		~LinearAllocator();

		void* allocate(size_t size)
		{
			if (size == 0)
				return nullptr;

			const size_t alignment = alignof(std::max_align_t);
			// ensure user requested size considers alignment
			size_t reqSize = (size + (alignment - 1)) & ~(alignment - 1);

			// try current page
			if (!m_pages.empty())
			{
				Page& cur = m_pages.back();
				std::uintptr_t base = reinterpret_cast<std::uintptr_t>(cur.data);
				std::uintptr_t curr = base + cur.used;
				std::uintptr_t aligned = (curr + (alignment - 1)) & ~(alignment - 1);
				u32 padding = static_cast<u32>(aligned - curr);
				if (cur.used + padding + reqSize <= cur.capacity)
				{
					void* ptr = cur.data + cur.used + padding;
					cur.used += padding + static_cast<u32>(reqSize);
					return ptr;
				}
			}

			// need a new page: allocate at least m_pageSize, but if request is bigger allocate a larger page
			u32 newCapacity = m_pageSize;
			if (reqSize > newCapacity)
				newCapacity = static_cast<u32>(reqSize);

			Page p;
			p.data = static_cast<ubyte*>(operator new[](newCapacity, std::nothrow));
			if (!p.data)
				return nullptr; // allocation failed, return nullptr instead of throwing
			p.capacity = newCapacity;
			p.used = 0;

			// compute aligned pointer on new page (base is naturally aligned from operator new[], but still align)
			std::uintptr_t base = reinterpret_cast<std::uintptr_t>(p.data);
			std::uintptr_t curr = base + p.used;
			std::uintptr_t aligned = (curr + (alignment - 1)) & ~(alignment - 1);
			u32 padding = static_cast<u32>(aligned - curr);

			// allocate from new page
			void* ptr = p.data + p.used + padding;
			p.used += padding + static_cast<u32>(reqSize);

			m_pages.push_back(p);
			return ptr;
		}

		template<typename T, typename ...Args>
		T* allocate(Args&& ...args)
		{
			void* p = allocate(sizeof(T));
			new(p) T(std::forward<Args>(args)...);
			return static_cast<T*>(p);
		}

		void reset()
		{
			// reset used counters to reuse allocated pages (do not free memory)
			for (auto &page : m_pages)
				page.used = 0;
		}

	private:
		struct Page
		{
			ubyte* data = nullptr;
			u32 capacity = 0;
			u32 used = 0;
		};

		u32 m_pageSize;
		std::vector<Page> m_pages;

		// non-copyable
		LinearAllocator(const LinearAllocator&) = delete;
		LinearAllocator& operator=(const LinearAllocator&) = delete;
	};
	
	// Inline ctor/dtor definitions
	inline LinearAllocator::LinearAllocator(size_t pageSize)
		: m_pageSize(static_cast<u32>(pageSize))
	{
		assert(pageSize > 0);
	}

	inline LinearAllocator::~LinearAllocator()
	{
		for (auto &p : m_pages)
		{
			if (p.data)
				operator delete[](p.data);
		}
		m_pages.clear();
	}
}