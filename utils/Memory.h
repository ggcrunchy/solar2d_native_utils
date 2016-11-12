/*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#pragma once

#include "CoronaLua.h"

//
namespace MemoryXS {
	struct LuaMemory {
		lua_State * mL;	// Main Lua state for this 
		int mIndex;	// Index of memory
		int mRegistrySlot;	// Slot in registry, if used
		int mStoreSlot;	// Slot in registry for table storage, if used

		struct BookmarkDualTables {
			LuaMemory * mOwner;	// Memory TLS to repair

			~BookmarkDualTables (void) { mOwner->UnloadTable(); }
		};

		struct BookmarkIndex {
			LuaMemory * mOwner;	// Memory TLS to repair
			int mIndex;	// Saved index

			~BookmarkIndex (void) { mOwner->mIndex = mIndex; }
		};

		BookmarkDualTables BindTable (void);
		BookmarkIndex SavePosition (bool bRelocate = true);

		static LuaMemory * New (lua_State * L);

		size_t GetOldSize (int slot, void * ptr);

		void * Add (int slot, size_t size);

		int Begin (void);

		void End (void);
		void LoadTable (void);
		void PrepDualTables (void);
		void PrepMemory (int slot = 0);
		void PrepRegistry (void);
		void PushObject (int slot, void * ptr);
		void Remove (int slot, void * ptr);
		void UnloadTable (void);

		// Lifetime
		LuaMemory (void) : mL(NULL), mIndex(0), mRegistrySlot(LUA_NOREF), mStoreSlot(LUA_NOREF) {}

		// Interface
		void FailAssert (const char * what);
		void * Malloc (size_t size);
		void * Calloc (size_t num, size_t size);
		void * Realloc (void * ptr, size_t size);
		void Free (void * ptr);
		size_t GetSize (void * ptr);
		bool Emit (void * ptr, bool bRemove = true);
		void Push (void * ptr, const char * type, bool bAsUserdata, bool bRemove = true);
	};
}