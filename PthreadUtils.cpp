#include "PthreadUtils.h"

#ifndef PTHREADUTILS_NOPUSH
#include "ByteUtils.h"
#endif

MemoryTLS::BookmarkDualTables MemoryTLS::BindTable (void)
{
	BookmarkDualTables bm;

	bm.mOwner = this;

	LoadTable();

	return bm;
}

MemoryTLS::BookmarkIndex MemoryTLS::SavePosition (bool bRelocate)
{
	BookmarkIndex bm;

	bm.mOwner = this;
	bm.mIndex = mIndex;

	if (bRelocate) mIndex = CoronaLuaNormalize(mL, -1);

	return bm;
}

size_t MemoryTLS::GetOldSize (int slot, void * ptr)
{
	PushObject(slot, ptr);	// ..., old

	size_t oldsize = lua_objlen(mL, -1);

	lua_pop(mL, 1);	// ...

	return oldsize;
}

void * MemoryTLS::Add (int slot, size_t size)
{
	void * ud = lua_newuserdata(mL, size);	// ..., ud

	lua_pushlightuserdata(mL, ud);	// ..., ud, ptr
	lua_insert(mL, -2);	// ..., ptr, ud
	lua_settable(mL, slot);	// ..., env = { ..., [ptr] = ud }, ...

	return ud;
}

int MemoryTLS::Begin (void)
{
	if (mRegistrySlot == LUA_NOREF) return mIndex;

	else
	{
		lua_rawgeti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ..., t
			
		return lua_gettop(mL);
	}
}

void MemoryTLS::End (void)
{
	if (mRegistrySlot != LUA_NOREF) lua_pop(mL, 1);	// ...
}

void MemoryTLS::LoadTable (void)
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

void MemoryTLS::PrepDualTables (void)
{
	lua_pushboolean(mL, 0);	// ..., false
	lua_pushboolean(mL, 0);	// ..., false, false

	mRegistrySlot = lua_ref(mL, 1);	// ..., false
	mStoreSlot = lua_ref(mL, 1);// ...
}

void MemoryTLS::PrepMemory (int slot)
{
	lua_newtable(mL);	// ..., memory

	if (slot > 0) lua_replace(mL, slot);	// ..., memory, ...

	else slot = CoronaLuaNormalize(mL, -1);

	mIndex = slot;
}

void MemoryTLS::PrepRegistry (void)
{
	lua_newtable(mL);	// ..., memory

	mRegistrySlot = lua_ref(mL, 1);	// ...
}

void MemoryTLS::PushObject (int slot, void * ptr)
{
	lua_pushlightuserdata(mL, ptr);	// ..., ptr
	lua_gettable(mL, slot);	// ..., object
}

void MemoryTLS::Remove (int slot, void * ptr)
{
	lua_pushlightuserdata(mL, ptr);	// ..., ptr
	lua_pushnil(mL);// ..., ptr, nil
	lua_settable(mL, slot);	// ..., env = { ..., [ptr] = nil }, ...
}

void MemoryTLS::UnloadTable (void)
{
	lua_rawgeti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ..., t?
	lua_rawseti(mL, LUA_REGISTRYINDEX, mStoreSlot);	// ...
	lua_pushboolean(mL, 0);	// ..., false
	lua_rawseti(mL, LUA_REGISTRYINDEX, mRegistrySlot);	// ...
}

void FailAssertTLS (MemoryTLS * tls, const char * what)
{
	luaL_error(tls->mL, what);
}

void * MallocTLS (MemoryTLS * tls, size_t size)
{
	void * ud = tls->Add(tls->Begin(), size);	// ...[, reg]

	tls->End();	// ...

	return ud;
}

void * CallocTLS (MemoryTLS * tls, size_t num, size_t size)
{
	void * ud = MallocTLS(tls, num * size);

	memset(ud, 0, num * size);

	return ud;
}

void * ReallocTLS (MemoryTLS * tls, void * ptr, size_t size)
{
	if (size == 0)
	{
		FreeTLS(tls, ptr);

		return NULL;
	}

	else if (!ptr) return MallocTLS(tls, size);

	else
	{
		int slot = tls->Begin();// ...[, reg]	
		size_t oldsize = tls->GetOldSize(slot, ptr);

		if (oldsize < size)
		{
			void * ud = tls->Add(slot, size);

			memcpy(ud, ptr, oldsize);

			tls->Remove(slot, ptr);

			ptr = ud;
		}

		tls->End();	// ...

		return ptr;
	}
}

void FreeTLS (MemoryTLS * tls, void * ptr)
{
	if (!ptr) return;

	tls->Remove(tls->Begin(), ptr);	// ...[, reg]
	tls->End();	// ...
}

size_t GetSizeTLS (MemoryTLS * tls, void * ptr)
{
	size_t size = tls->GetOldSize(tls->Begin(), ptr);	// ...[, reg]

	tls->End();	// ...

	return size;
}

bool EmitTLS (MemoryTLS * tls, void * ptr, bool bRemove)
{
	if (!ptr) return false;

	int top = lua_gettop(tls->mL);

	lua_pushnil(tls->mL);	// ..., nil

	int slot = tls->Begin();// ..., nil[, reg]

	tls->PushObject(slot, ptr);	// ..., nil[, reg], object?

	lua_replace(tls->mL, top + 1);	// ..., object?[, reg]

	if (bRemove) tls->Remove(slot, ptr);

	tls->End();

	if (lua_isnil(tls->mL, -1)) lua_pop(tls->mL, 1);// ...[, object]

	return lua_gettop(tls->mL) > top;
}

#ifndef PTHREADUTILS_NOPUSH

void PushTLS (MemoryTLS * tls, void * ptr, const char * type, bool as_userdata, bool bRemove)
{
	if (as_userdata)
	{
		EmitTLS(tls, ptr, bRemove);	// ..., ud
		AddBytesMetatable(tls->mL, type);
	}

	else
	{
		lua_pushlstring(tls->mL, (const char *)ptr, GetSizeTLS(tls, ptr));	// ..., bytes

		if (bRemove) FreeTLS(tls, ptr);
	}
}

#endif

#ifndef PTHREADUTILS_NONEON

#ifdef __ANDROID__
#include <pthread.h>
#include <cpu-features.h>
#endif
bool CanUseNeon (void)
{
#ifdef _WIN32
	return false;
#elif __APPLE__
	#if !TARGET_OS_SIMULATOR && (TARGET_OS_IPHONE || TARGET_OS_TV)
		return true;
	#else
		return false;
	#endif
#elif __ANDROID__
	static bool using_neon;
	static pthread_once_t neon_once = PTHREAD_ONCE_INIT;

	pthread_once(&neon_once, []()
	{
		using_neon = android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM && (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
	});

	return using_neon;
#endif
}

#endif