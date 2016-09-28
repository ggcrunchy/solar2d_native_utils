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
	if (!luaL_getmetafield(L, arg, "__bytes")) return false;// ...[, bytes]

	lua_pop(L, 1);	// ...

	if (!luaL_getmetafield(L, arg, "__metatable")) return false;// ...[, name]

	lua_pushliteral(L, "blob_mt");	// ..., name, "blob_mt"

	bool bEq = lua_equal(L, -2, -1) != 0;

	lua_pop(L, 2);	// ...

	return bEq;
}

size_t GetBlobAlignment (lua_State * L, int arg)
{
	luaL_checktype(L, arg, LUA_TUSERDATA);
	lua_getfenv(L, arg);// ..., env
	lua_getfield(L, -1, "align");	// ..., env, align

	size_t align = luaL_optinteger(L, -1, 0);

	lua_pop(L, 2);	// ...

	return align;
}

size_t GetBlobSize (lua_State * L, int arg, bool bNSized)
{
	arg = CoronaLuaNormalize(L, arg);

	luaL_checktype(L, arg, LUA_TUSERDATA);
	lua_getfenv(L, arg);// ..., env
	lua_getfield(L, -1, "align");	// ..., env, align
	lua_getfield(L, -2, "resizable");	// ..., env, align, resizable

	size_t align = luaL_optinteger(L, -2, 0), size;

	if (lua_toboolean(L, -1))
	{
		#define ALIGNED(n) size = ((VectorType<n>::type *)lua_touserdata(L, arg))->size()
		#define UNALIGNED() size = ((ucvec *)lua_touserdata(L, arg))->size()

		switch (align)
		{
			WITH_VECTOR();
		}

		#undef ALIGNED
		#undef UNALIGNED
	}

	else
	{
		void * ud = lua_touserdata(L, arg);

		if (align) std::align(align, lua_objlen(L, arg), ud, size);
	}

	lua_pop(L, 3);	// ...

	if (bNSized && align) size /= align;

	return size;
}

unsigned char * GetBlobData (lua_State * L, int arg)
{
	arg = CoronaLuaNormalize(L, arg);

	luaL_checktype(L, arg, LUA_TUSERDATA);
	lua_getfenv(L, arg);// ..., env
	lua_getfield(L, -1, "align");	// ..., env, align
	lua_getfield(L, -2, "resizable");	// ..., env, align, resizable

	size_t align = luaL_optinteger(L, -2, 0);
	unsigned char * data;

	if (lua_toboolean(L, -1))
	{
		#define ALIGNED(n) data = ((VectorType<n>::type *)lua_touserdata(L, arg))->data()
		#define UNALIGNED() data = ((ucvec *)lua_touserdata(L, arg))->data()

		switch (align)
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

		if (align) std::align(align, size, ud, junk);

		data = (unsigned char *)data;
	}

	lua_pop(L, 3);	// ...

	return data;
}

void * GetBlobVector (lua_State * L, int arg)
{
	arg = CoronaLuaNormalize(L, arg);

	luaL_checktype(L, arg, LUA_TUSERDATA);
	lua_getfenv(L, arg);// ..., env
	lua_getfield(L, -1, "resizable");	// ..., env, resizable

	void * ud = lua_toboolean(L, -1) ? lua_touserdata(L, arg) : NULL;

	lua_pop(L, 2);	// ...

	return ud;
}

template<typename T> void * Alloc (lua_State * L)
{
	void * ud = lua_newuserdata(L, sizeof(T));

	new (ud) T;

	return ud;
}

template<int A> void * AllocN (lua_State * L)
{
	return Alloc<VectorType<A>::type>(L);
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

	BytesMetatableOpts bmopts;

	#define ALIGNED(n) GC<VectorType<n>::type>(L) // n.b. freaks out preprocessor if nested deeper :P
	#define UNALIGNED() GC<ucvec>(L)

	bmopts.mMetatableName = "blob_mt";
	bmopts.mMore = [](lua_State * L)
	{
		// STUFF!
		lua_pushcfunction(L, [](lua_State * L)
		{
			lua_getfenv(L, 1);	// blob, env
			lua_getfield(L, -1, "resizable");	// blob, env, resizable?

			if (lua_toboolean(L, -1))
			{
				lua_getfield(L, -2, "align");	// blob, env, resizable, align

				switch (lua_tointeger(L, -1))
				{
					WITH_VECTOR();
				}
			}

			return 0;
		});	// ..., ud, mt, GC
		lua_setfield(L, -2, "__gc");// ..., ud, mt = { __gc = GC }
	};

	#undef ALIGNED
	#undef UNALIGNED

	AddBytesMetatable(L, type, &bmopts);

	// Store the blob traits as small tables in the metatable, creating these on the first
	// instance of any particular configuration. For lookup purposes, keep a reference as
	// the blob userdata's environment as well.
	lua_getmetatable(L, -1);// ..., ud, mt
	lua_pushfstring(L, "%i:%s", align, bCanResize ? "true" : "false");	// ..., ud, mt, key
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
// suggests implementing a revised ByteReader, that is some sort of point_to rather than "vector", etc. (would also obviate offsets)