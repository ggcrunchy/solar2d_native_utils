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

#include "utils/Compat.h"
#include "utils/Memory.h"
#include "utils/Byte.h"
#include "utils/LuaEx.h"

namespace CEU = CompatXS::ns_compat;

MemoryXS::LuaMemory::BookmarkDualTables MemoryXS::LuaMemory::BindTable (void)
{
	BookmarkDualTables bm;

	bm.mOwner = this;

	LoadTable();

	return bm;
}

MemoryXS::LuaMemory::BookmarkIndex MemoryXS::LuaMemory::SavePosition (bool bRelocate)
{
	BookmarkIndex bm;

	bm.mOwner = this;
	bm.mIndex = mIndex;

	if (bRelocate) mIndex = CoronaLuaNormalize(mL, -1);

	return bm;
}

MemoryXS::LuaMemory * MemoryXS::LuaMemory::New (lua_State * L)
{
	LuaMemory * memory = LuaXS::NewTyped<LuaMemory>(L);	// ..., memory

	memory->mL = L;

	lua_pushlightuserdata(L, memory);	// ..., memory, memory_ptr
	lua_insert(L, -2);	// ..., memory_ptr, memory
	lua_rawset(L, LUA_REGISTRYINDEX);	// ...; registry = { ..., [memory_ptr] = memory }

	return memory;
}

size_t MemoryXS::LuaMemory::GetOldSize (int slot, void * ptr)
{
	PushObject(slot, ptr);	// ..., old

	size_t oldsize = lua_objlen(mL, -1);

	lua_pop(mL, 1);	// ...

	return oldsize;
}

void * MemoryXS::LuaMemory::Add (int slot, size_t size)
{
	void * ud = lua_newuserdata(mL, size);	// ..., ud

	lua_pushlightuserdata(mL, ud);	// ..., ud, ptr
	lua_insert(mL, -2);	// ..., ptr, ud
	lua_settable(mL, slot);	// ..., env = { ..., [ptr] = ud }, ...

	return ud;
}

int MemoryXS::LuaMemory::Begin (void)
{
	if (mRegistrySlot == LUA_NOREF) return mIndex;

	else
	{
		lua_rawgeti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ..., t
			
		return lua_gettop(mL);
	}
}

void MemoryXS::LuaMemory::End (void)
{
	if (mRegistrySlot != LUA_NOREF) lua_pop(mL, 1);	// ...
}

void MemoryXS::LuaMemory::LoadTable (void)
{
	lua_rawgeti(mL, LUA_REGISTRYINDEX, mStoreSlot);	// ..., t?

	if (!lua_istable(mL, -1))
	{
		lua_newtable(mL);	// ..., false, t
		lua_replace(mL, -2);// ..., t
	}

	lua_rawseti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ...
	lua_pushboolean(mL, 0);	// ..., false
	lua_rawseti(mL, LUA_REGISTRYINDEX, mStoreSlot);	// ...
}

void MemoryXS::LuaMemory::PrepDualTables (void)
{
	lua_pushboolean(mL, 0);	// ..., false
	lua_pushboolean(mL, 0);	// ..., false, false

	mRegistrySlot = lua_ref(mL, 1);	// ..., false
	mStoreSlot = lua_ref(mL, 1);// ...
}

void MemoryXS::LuaMemory::PrepMemory (int slot)
{
	lua_newtable(mL);	// ..., memory

	if (slot > 0) lua_replace(mL, slot);	// ..., memory, ...

	else slot = CoronaLuaNormalize(mL, -1);

	mIndex = slot;
}

void MemoryXS::LuaMemory::PrepRegistry (void)
{
	lua_newtable(mL);	// ..., memory

	mRegistrySlot = lua_ref(mL, 1);	// ...
}

void MemoryXS::LuaMemory::PushObject (int slot, void * ptr)
{
	lua_pushlightuserdata(mL, ptr);	// ..., ptr
	lua_gettable(mL, slot);	// ..., object
}

void MemoryXS::LuaMemory::Remove (int slot, void * ptr)
{
	lua_pushlightuserdata(mL, ptr);	// ..., ptr
	lua_pushnil(mL);// ..., ptr, nil
	lua_settable(mL, slot);	// ..., env = { ..., [ptr] = nil }, ...
}

void MemoryXS::LuaMemory::UnloadTable (void)
{
	lua_rawgeti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ..., t?
	lua_rawseti(mL, LUA_REGISTRYINDEX, mStoreSlot);	// ...
	lua_pushboolean(mL, 0);	// ..., false
	lua_rawseti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ...
}

void MemoryXS::LuaMemory::FailAssert (const char * what)
{
	luaL_error(mL, what);
}

void * MemoryXS::LuaMemory::Malloc (size_t size)
{
	void * ud = Add(Begin(), size);	// ...[, reg]

	End();	// ...

	return ud;
}

void * MemoryXS::LuaMemory::Calloc (size_t num, size_t size)
{
	void * ud = Malloc(num * size);

	memset(ud, 0, num * size);

	return ud;
}

void * MemoryXS::LuaMemory::Realloc (void * ptr, size_t size)
{
	if (size == 0)
	{
		Free(ptr);

		return nullptr;
	}

	else if (!ptr) return Malloc(size);

	else
	{
		int slot = Begin();	// ...[, reg]	
		size_t oldsize = GetOldSize(slot, ptr);

		if (oldsize < size)
		{
			void * ud = Add(slot, size);

			memcpy(ud, ptr, oldsize);

			Remove(slot, ptr);

			ptr = ud;
		}

		End();	// ...

		return ptr;
	}
}

void MemoryXS::LuaMemory::Free (void * ptr)
{
	if (!ptr) return;

	Remove(Begin(), ptr);	// ...[, reg]
	End();	// ...
}

size_t MemoryXS::LuaMemory::GetSize (void * ptr)
{
	size_t size = GetOldSize(Begin(), ptr);	// ...[, reg]

	End();	// ...

	return size;
}

bool MemoryXS::LuaMemory::Emit (void * ptr, bool bRemove)
{
	if (!ptr) return false;

	int top = lua_gettop(mL);

	lua_pushnil(mL);// ..., nil

	int slot = Begin();	// ..., nil[, reg]

	PushObject(slot, ptr);	// ..., nil[, reg], object?

	lua_replace(mL, top + 1);	// ..., object?[, reg]

	if (bRemove) Remove(slot, ptr);

	End();

	if (lua_isnil(mL, -1)) lua_pop(mL, 1);	// ...[, object]

	return lua_gettop(mL) > top;
}

void MemoryXS::LuaMemory::Push (void * ptr, const char * type, bool as_userdata, bool bRemove)
{
	if (as_userdata)
	{
		Emit(ptr, bRemove);	// ..., ud
		ByteXS::AddBytesMetatable(mL, type);
	}

	else
	{
		lua_pushlstring(mL, static_cast<const char *>(ptr), GetSize(ptr));	// ..., bytes

		if (bRemove) Free(ptr);
	}
}

void * MemoryXS::Align (size_t bound, size_t size, void *& ptr, size_t * space)
{
	size_t cushion = size + bound - 1U;
	size_t & space_ref = space ? *space : cushion;

	return CEU::align(bound, size, ptr, space_ref);
}