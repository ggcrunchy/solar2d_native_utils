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

#include "ByteReader.h"
#include "ByteUtils.h"
#include "LuaUtils.h"
#include "aligned_allocator.h"
#include <memory>
#include <type_traits>
#include <vector>

#define WITH_SIZE(n)	case n:					\
							ALIGNED(n);	break
#define WITH_VECTOR()	case 0:					\
							UNALIGNED(); break;	\
						WITH_SIZE(4);			\
						WITH_SIZE(8);			\
						WITH_SIZE(16);			\
						WITH_SIZE(32);			\
						WITH_SIZE(64);			\
						WITH_SIZE(128);			\
						WITH_SIZE(256);			\
						WITH_SIZE(512);			\
						WITH_SIZE(1024)

using namespace simdpp::SIMDPP_ARCH_NAMESPACE;
using ucvec = std::vector<unsigned char>;

bool IsBlob (lua_State * L, int arg, const char * type)
{
	if (lua_type(L, arg) != LUA_TUSERDATA) return false;
	if (type && !IsType(L, type, arg)) return false;
	if (!luaL_getmetafield(L, arg, "__bytes")) return false;// ...[, bytes]

	lua_pop(L, 1);	// ...

	if (!luaL_getmetafield(L, arg, "__metatable")) return false;// ...[, name]

	lua_pushliteral(L, "blob_mt");	// ..., name, "blob_mt"

	bool bEq = lua_equal(L, -2, -1) != 0;

	lua_pop(L, 2);	// ...

	return bEq;
}

static bool GetLock (lua_State * L, int arg)
{
	lua_getfield(L, LUA_REGISTRYINDEX, "BLOB_LOCKS");	// ..., locks?

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);	// ...

		return false;
	}

	lua_pushvalue(L, arg);	// ..., locks, blob
	lua_rawget(L, -2);	// ..., locks, lock?

	return true;
}

bool IsBlobLocked (lua_State * L, int arg)
{
	arg = CoronaLuaNormalize(L, arg);

	if (!IsBlob(L, arg) || !GetLock(L, arg)) return false;	// ...[, locks, lock?]

	bool bLocked = !lua_isnil(L, -1);

	lua_pop(L, 2);	// ...

	return bLocked;
}

bool LockBlob (lua_State * L, int arg, void * key)
{
	arg = CoronaLuaNormalize(L, arg);

	int top = lua_gettop(L);

	if (!IsBlob(L, arg) || !key) return false;

	lua_getfield(L, LUA_REGISTRYINDEX, "BLOB_LOCKS");	// ..., locks?

	if (!lua_isnil(L, -1))
	{
		lua_pushvalue(L, arg);	// ..., locks, blob
		lua_rawget(L, -2);	// ..., locks, lock?

		if (!lua_isnil(L, -1))
		{
			lua_pushlightuserdata(L, key);	// ..., locks, lock?, key

			bool bOK = lua_equal(L, -2, -1) != 0;

			lua_pop(L, 3);	// ...

			return bOK;
		}

		lua_pop(L, 1);	// ..., locks
	}

	else
	{
		lua_pop(L, 1);	// ...
		lua_newtable(L);// ..., locks
		lua_createtable(L, 0, 1);	// ..., locks, locks_mt
		lua_pushliteral(L, "k");// ..., locks, locks_mt, "k"
		lua_setfield(L, -2, "__mode");	// ..., locks, locks_mt = { __mode = "k" }
		lua_setmetatable(L, -2);// ..., locks
		lua_pushvalue(L, -1);	// ..., locks, locks
		lua_setfield(L, LUA_REGISTRYINDEX, "BLOB_LOCKS");	// ..., locks; registry.BLOB_LOCKS = locks
	}

	lua_pushvalue(L, arg);	// ..., locks, blob
	lua_pushlightuserdata(L, key);	// ..., locks, blob, key
	lua_rawset(L, -3);	// ..., locks = { [blob] = key }
	lua_pop(L, 1);	// ...

	return true;
}

bool UnlockBlob (lua_State * L, int arg, void * key)
{
	arg = CoronaLuaNormalize(L, arg);

	int top = lua_gettop(L);

	if (!IsBlob(L, arg) || !key || !GetLock(L, arg)) return false;	// ...[, locks, lock?]

	lua_pushlightuserdata(L, key);	// ..., locks, lock?, key

	bool bFound = lua_equal(L, -2, -1) != 0;

	if (bFound)
	{
		lua_pop(L, 2);	// ..., locks
		lua_pushvalue(L, arg);	// ..., locks, blob
		lua_pushnil(L);	// ..., locks, blob, nil
		lua_rawset(L, -3);	// ..., locks
	}

	lua_settop(L, top);	// ...

	return bFound;
}

struct BlobProps {
	size_t mAlign;
	bool mResizable;

	BlobProps (lua_State * L, int arg)
	{
		luaL_checktype(L, arg, LUA_TUSERDATA);
		lua_getfenv(L, arg);// ..., env
		lua_getfield(L, -1, "align");	// ..., env, align
		lua_getfield(L, -2, "resizable");	// ..., env, align, resizable

		mAlign = luaL_optinteger(L, -2, 0);
		mResizable = lua_toboolean(L, -1) != 0;

		lua_pop(L, 3);	// ...
	}
};

size_t GetBlobAlignment (lua_State * L, int arg)
{
	return BlobProps(L, arg).mAlign;
}

size_t GetBlobSize (lua_State * L, int arg, bool bNSized)
{
	BlobProps props(L, arg);
	size_t size;

	if (props.mResizable)
	{
		#define ALIGNED(n) size = ((VectorType<n>::type *)lua_touserdata(L, arg))->size()
		#define UNALIGNED() size = ((ucvec *)lua_touserdata(L, arg))->size()

		switch (props.mAlign)
		{
			WITH_VECTOR();
		}

		#undef ALIGNED
		#undef UNALIGNED
	}

	else
	{
		void * ud = lua_touserdata(L, arg);

		if (props.mAlign) std::align(props.mAlign, lua_objlen(L, arg), ud, size);
	}

	if (bNSized && props.mAlign) size /= props.mAlign;

	return size;
}

unsigned char * GetBlobData (lua_State * L, int arg)
{
	BlobProps props(L, arg);
	unsigned char * data;

	if (props.mResizable)
	{
		#define ALIGNED(n) data = ((VectorType<n>::type *)lua_touserdata(L, arg))->data()
		#define UNALIGNED() data = ((ucvec *)lua_touserdata(L, arg))->data()

		switch (props.mAlign)
		{
			WITH_VECTOR();
		}

		#undef ALIGNED
		#undef UNALIGNED
	}

	else
	{
		void * ud = lua_touserdata(L, arg);
		size_t size = lua_objlen(L, arg), junk;

		if (props.mAlign) std::align(props.mAlign, size, ud, junk);

		data = (unsigned char *)ud;
	}

	return data;
}

void * GetBlobVector (lua_State * L, int arg)
{
	return BlobProps(L, arg).mResizable ? lua_touserdata(L, arg) : NULL;
}

template<typename T> void * Alloc (lua_State * L)
{
	void * ud = lua_newuserdata(L, sizeof(T));

	new (ud) T;

	return ud;
}

template<int A> void * AllocN (lua_State * L)
{
	return Alloc<typename VectorType<A>::type>(L);
}

template<typename T> void GC (lua_State * L)
{
	T * v = (T *)lua_touserdata(L, 1);

	v->~T();
}

void NewBlob (lua_State * L, size_t size, const BlobOpts * opts)
{
	size_t align = 0;
	const char * type = "xs.blob";
	bool bCanResize = false;

	if (opts)
	{
		bCanResize = opts->mResizable;

		if (opts->mType) type = opts->mType;
		if (opts->mAlignment)
		{
			luaL_argcheck(L, opts->mAlignment >= 4 && (opts->mAlignment & (opts->mAlignment - 1)) == 0, -1, "Alignment must be a power-of-2 >= 4");
			luaL_argcheck(L, opts->mAlignment <= 1024, -1, "Alignments over 1024 not yet supported"); 

			align = opts->mAlignment;
		}
	}

	void * ud;

	if (bCanResize)
	{
		#define ALIGNED(n) ud = AllocN<n>(L)
		#define UNALIGNED() ud = Alloc<ucvec>(L)

		switch (align)
		{
			WITH_VECTOR();
		}	// ..., ud

		#undef ALIGNED
		#undef UNALIGNED
	}

	else if (align > std::alignment_of<double>::value)	// Lua gives back double-aligned memory
	{
		size_t cushioned = size + align - 1;

		void * ud = lua_newuserdata(L, cushioned);	// ..., ud

		std::align(align, size, ud, cushioned);

		luaL_argcheck(L, ud, -1, "Unable to align blob");
	}

	else lua_newuserdata(L, size);	// ..., ud

	AttachMethods(L, type, [](lua_State * L)
	{
		luaL_Reg blob_methods[] = {
			{
				"__gc", [](lua_State * L)
				{
					BlobProps props(L, 1);

					if (props.mResizable)
					{
						#define ALIGNED(n) GC<VectorType<n>::type>(L) // n.b. freaks out preprocessor if nested deeper :P
						#define UNALIGNED() GC<ucvec>(L)

						switch (props.mAlign)
						{
							WITH_VECTOR();
						}
						
						#undef ALIGNED
						#undef UNALIGNED
					}

					return 0;
				}
			}, {
				"__len", [](lua_State * L)
				{
					lua_pushinteger(L, GetBlobSize(L, 1));	// blob, size

					return 1;
				}
			},
			{ NULL, NULL }
		};

		luaL_register(L, NULL, blob_methods);

		ByteReaderFunc * func = ByteReader::Register(L);

		func->mGetBytes = [](lua_State * L, ByteReader & reader, int arg, void *)
		{
			reader.mBytes = GetBlobData(L, arg);
			reader.mCount = GetBlobSize(L, arg);
		};
		func->mContext = NULL;

		lua_pushlightuserdata(L, func);	// ..., mt, reader_func
		lua_setfield(L, -2, "__bytes");	// ..., mt = { ..., __bytes = reader_func }
		lua_pushliteral(L, "blob_mt");	// ..., mt, "blob_mt"
		lua_setfield(L, -2, "__metatable");	// ..., mt = { ..., __bytes, __metatable = "blob_mt" }
	});

	// Store the blob traits as small tables in the metatable, creating these on the first
	// instance of any particular configuration. For lookup purposes, keep a reference as
	// the blob userdata's environment as well.
	lua_getmetatable(L, -1);// ..., ud, mt
	lua_pushfstring(L, "%d:%s", align, bCanResize ? "true" : "false");	// ..., ud, mt, key
	lua_pushvalue(L, -1);	// ..., ud, mt, key, key
	lua_rawget(L, -3);	// ..., ud, mt, key, t?

	if (!lua_isnil(L, -1))
	{
		lua_setfenv(L, -4);	// ..., ud, mt, key
		lua_pop(L, 2);	// ..., ud
	}

	else
	{
		lua_pop(L, 1);	// ..., ud, mt, key
		lua_createtable(L, 0, 2);	// ..., ud, mt, key, info
		lua_pushboolean(L, bCanResize ? 1 : 0);	// ..., ud, mt, key, info, can_resize
		lua_pushinteger(L, align);	// ..., ud, mt, key, info, can_resize, align
		lua_setfield(L, -3, "align");	// ..., ud, mt, key, info = { align = align }, can_resize
		lua_setfield(L, -2, "resizable");	// ..., ud, mt, key, info = { align, resizable = can_resize }
		lua_pushvalue(L, -1);	// ..., ud, mt, key, info, info
		lua_insert(L, -4);	// ..., ud, info, mt, key, info
		lua_rawset(L, -3);	// ..., ud, info, mt = { ..., key = info }
		lua_pop(L, 1);	// ..., ud, info
		lua_setfenv(L, -2);	// ..., ud
	}
}

#undef WITH_SIZE
#undef WITH_VECTOR