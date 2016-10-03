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

#define BLOBXS_PROPS "BLOBXS_PROPS" // n.b. please do not change or hijack this :) Exposed as a string in case the
									// blob API is defined in multiple compilation units, namely plugins

static bool AuxIsBlob (lua_State * L, int arg)
{
	if (lua_type(L, arg) != LUA_TUSERDATA) return false;

	lua_pushvalue(L, arg);	// ..., blob?
	lua_getfield(L, LUA_REGISTRYINDEX, BLOBXS_PROPS);	// ..., blob?, props

	if (lua_istable(L, -1))
	{
		lua_insert(L, -2);	// ..., props, blob?
		lua_rawget(L, -2);	// ..., props, prop?

		return lua_type(L, -1) == LUA_TUSERDATA;
	}

	return false;
}

bool BlobXS::IsBlob (lua_State * L, int arg, const char * type)
{
	if (type && !IsType(L, type, arg)) return false;

	int top = lua_gettop(L);
	bool bIsBlob = AuxIsBlob(L, arg);

	lua_settop(L, top);	// ...

	return bIsBlob;
}

// Various per-blob properties (corresponding to `Version`)
struct BlobProps {
	size_t mAlign;	// Size of which memory will be aligned to some multiple
	bool mResizable;// Is the memory resizable?
	int mVersion;	// Version of API used to create blob
	void * mKey;// If locked, the key

	BlobProps (void) : mAlign(0U), mResizable(false), mVersion(BlobXS::Version), mKey(NULL) {}
};

struct BlobPropViewer {
	BlobProps * mProps;	// Properties being viewed
	lua_State * mL;	// Current state
	int mArg, mTop;	// Argument position; stack top

	BlobPropViewer (lua_State * L, int arg) : mProps(NULL), mL(L)
	{
		mArg = CoronaLuaNormalize(L, arg);
		mTop = lua_gettop(L);

		if (AuxIsBlob(L, arg)) mProps = (BlobProps *)lua_touserdata(L, -1);	// ...[, props, prop]
	}

	~BlobPropViewer (void)
	{
		lua_settop(mL, mTop);
	}
};

bool BlobXS::IsLocked (lua_State * L, int arg, void * key)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps) return false;
	if (key) return bpv.mProps->mKey == key;
	else return bpv.mProps->mKey != NULL;
}

bool BlobXS::Lock (lua_State * L, int arg, void * key)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps || !key) return false;
	if (bpv.mProps->mKey) return bpv.mProps->mKey == key;

	bpv.mProps->mKey = key;

	return true;
}

bool BlobXS::Unlock (lua_State * L, int arg, void * key)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps || !key || !bpv.mProps->mKey) return false;
	if (bpv.mProps->mKey != key) return false;

	bpv.mProps->mKey = NULL;

	return true;
}

size_t BlobXS::GetAlignment (lua_State * L, int arg)
{
	BlobPropViewer bpv(L, arg);

	return bpv.mProps ? bpv.mProps->mAlign : 0U;
}

size_t BlobXS::GetSize (lua_State * L, int arg, bool bNSized)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps) return 0U;

	size_t size;

	if (bpv.mProps->mResizable)
	{
		#define ALIGNED(n) size = ((VectorType<n>::type *)lua_touserdata(L, bpv.mArg))->size()
		#define UNALIGNED() size = ((ucvec *)lua_touserdata(L, bpv.mArg))->size()

		switch (bpv.mProps->mAlign)
		{
			WITH_VECTOR();
		}

		#undef ALIGNED
		#undef UNALIGNED
	}

	else
	{
		void * ud = lua_touserdata(L, bpv.mArg);

		if (bpv.mProps->mAlign) std::align(bpv.mProps->mAlign, lua_objlen(L, bpv.mArg), ud, size);
	}

	if (bNSized && bpv.mProps->mAlign) size /= bpv.mProps->mAlign;

	return size;
}

int BlobXS::GetVersion (lua_State * L, int arg)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps) return 0;

	return bpv.mProps->mVersion;
}

unsigned char * BlobXS::GetData (lua_State * L, int arg)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps) return NULL;

	unsigned char * data;

	if (bpv.mProps->mResizable)
	{
		#define ALIGNED(n) data = ((VectorType<n>::type *)lua_touserdata(L, bpv.mArg))->data()
		#define UNALIGNED() data = ((ucvec *)lua_touserdata(L, bpv.mArg))->data()

		switch (bpv.mProps->mAlign)
		{
			WITH_VECTOR();
		}

		#undef ALIGNED
		#undef UNALIGNED
	}

	else
	{
		void * ud = lua_touserdata(L, bpv.mArg);
		size_t size = lua_objlen(L, bpv.mArg), junk;

		if (bpv.mProps->mAlign) std::align(bpv.mProps->mAlign, size, ud, junk);

		data = (unsigned char *)ud;
	}

	return data;
}

void * BlobXS::GetVector (lua_State * L, int arg)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps || !bpv.mProps->mResizable) return NULL;

	return lua_touserdata(L, bpv.mArg);
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

void BlobXS::NewBlob (lua_State * L, size_t size, const CreateOpts * opts)
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

		if (size > 0U)
		{
			#define ALIGNED(n) GetVectorN<n>(L, -1)->resize(size)
			#define UNALIGNED() ((ucvec *)GetVector(L, -1))->resize(size)

			switch (align)
			{
				WITH_VECTOR();
			}	// ..., ud

			#undef ALIGNED
			#undef UNALIGNED
		}
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
				"Clone", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);
					CreateOpts copts;

					if (bpv.mProps)
					{
						copts.mAlignment = bpv.mProps->mAlign;
						copts.mResizable = bpv.mProps->mResizable;
					}

					copts.mType = NULL;

					lua_getmetatable(L, 1);	// blob[, props, prop], mt

					for (lua_pushnil(L); lua_next(L, LUA_REGISTRYINDEX); lua_pop(L, 1)) // blob[, props, prop], mt[, key, value]
					{
						if (lua_equal(L, -3, -1) && lua_type(L, -2) == LUA_TSTRING)
						{
							copts.mType = lua_tostring(L, -2);

							break;
						}
					}

					NewBlob(L, GetSize(L, 1), &copts);	// blob[, props, prop], mt[, key, value], new_blob

					return 1;
				}
			}, {
				"__gc", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);

					if (bpv.mProps && bpv.mProps->mResizable)
					{
						#define ALIGNED(n) GC<VectorType<n>::type>(L) // n.b. freaks out preprocessor if nested deeper :P
						#define UNALIGNED() GC<ucvec>(L)

						switch (bpv.mProps->mAlign)
						{
							WITH_VECTOR();
						}
						
						#undef ALIGNED
						#undef UNALIGNED
					}

					return 0;
				}
			}, {
				"GetProperties", [](lua_State * L)
				{
					lua_settop(L, 2);	// blob, out?

					if (!lua_istable(L, 2))
					{
						lua_newtable(L);// blob, ???, out
						lua_replace(L, 2);	// blob, out
					}

					BlobPropViewer bpv(L, 1);
					BlobProps props;

					if (bpv.mProps) props = *bpv.mProps;

					lua_pushinteger(L, props.mAlign);	// blob, out, alignment
					lua_setfield(L, 2, "alignment");// blob, out = { alignment = alignment }
					lua_pushboolean(L, props.mResizable ? 1 : 0);	// blob, out, resizable
					lua_setfield(L, 2, "resizable");// blob, out = { alignment, resizable = resizable }
					lua_pushinteger(L, props.mVersion);	// blob, out, version
					lua_setfield(L, 2, "version");	// blob, out = { alignment, resizable, version = version }

					return 1;
				}
			}, {
				"IsLocked", [](lua_State * L)
				{
					lua_toboolean(L, IsLocked(L, 1) ? 1 : 0);	// blob, is_locked

					return 1;
				}
			}, {
				"__len", [](lua_State * L)
				{
					lua_pushinteger(L, GetSize(L, 1));	// blob, size

					return 1;
				}
			}, {
				"ToBytes", [](lua_State * L)
				{
					ByteReader reader(L, 1);

					if (!reader.mBytes) lua_pushliteral(L, "");	// blob, ""

					else lua_pushlstring(L, (const char *)reader.mBytes, reader.mCount);// blob, data

					return 1;
				}
			},
			{ NULL, NULL }
		};

		luaL_register(L, NULL, blob_methods);

		ByteReaderFunc * func = ByteReader::Register(L);

		func->mGetBytes = [](lua_State * L, ByteReader & reader, int arg, void *)
		{
			reader.mBytes = GetData(L, arg);
			reader.mCount = GetSize(L, arg);
		};
		func->mContext = NULL;

		lua_pushlightuserdata(L, func);	// ..., mt, reader_func
		lua_setfield(L, -2, "__bytes");	// ..., mt = { ..., __bytes = reader_func }
		lua_pushliteral(L, "blob");	// ..., mt, "blob"
		lua_setfield(L, -2, "__metatable");	// ..., mt = { ..., __bytes, __metatable = "blob_mt" }
	});

	// Create the blob properties table, if necessary.
	lua_getfield(L, LUA_REGISTRYINDEX, BLOBXS_PROPS);	// ..., ud, props?

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);	// ..., ud

		NewWeakKeyedTable(L);	// ..., ud, props

		lua_pushvalue(L, -1);	// ..., ud, props, props
		lua_setfield(L, LUA_REGISTRYINDEX, BLOBXS_PROPS);	// ..., ud, props; registry[BLOBXS_PROPS] = props
	}

	lua_pushvalue(L, -2);	// ..., ud, props, ud

	// Create a property object and store it in that table, hooked up to the blob.
	BlobProps * props = (BlobProps *)lua_newuserdata(L, sizeof(BlobProps));	// ..., ud, props, ud, prop

	new (props) BlobProps;

	props->mAlign = align;
	props->mResizable = bCanResize;

	lua_rawset(L, -3);	// ..., ud, props = { ..., [ud] = prop }
	lua_pop(L, 1);	// ..., ud
}

#undef WITH_SIZE
#undef WITH_VECTOR