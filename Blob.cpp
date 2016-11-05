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
#include "BlobUtils.h"
#include "LuaUtils.h"
#include "aligned_allocator.h"
#include <vector>
/*
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#ifdef TARGET_OS_IPHONE
#include <tr1/type_traits>
#include <stdlib.h>

static void * Align (size_t align, size_t size, void *& ptr, size_t & space)
{
	uintptr_t p = uintptr_t(ptr);
	uintptr_t q = (p + align - 1) & ~(align - 1);
	uintptr_t diff = q - p;
    
	if (space - diff < size) return nullptr;
    
	space -= diff;
	ptr = ((unsigned char *)ptr) + q - p;
    
	return ptr;
}

#define ALIGN(to, size, ud, n) Align(to, size, ud, n)
#else
#include <type_traits>
#include <memory>
#define ALIGN(to, size, ud, n) std::align(to, size, ud, n)
#endif

#define WITH_SIZE(n)	case n:					\
							ALIGNED(n);	break
#define WITH_VECTOR()	case 0:					\
							UNALIGNED(); break;	\
							WITH_SIZE(4);		\
							WITH_SIZE(8);		\
							WITH_SIZE(16);		\
							WITH_SIZE(32);		\
							WITH_SIZE(64);		\
							WITH_SIZE(128);		\
							WITH_SIZE(256);		\
							WITH_SIZE(512);		\
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
	size_t mShift;	// Align-related memory shift
	bool mResizable;// Is the memory resizable?
	int mVersion;	// Version of API used to create blob
	void * mKey;// If locked, the key

	BlobProps (void) : mAlign(0U), mShift(0U), mResizable(false), mVersion(BlobXS::Version), mKey(NULL) {}
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

bool BlobXS::IsResizable (lua_State * L, int arg)
{
	 return GetVector(L, arg) != NULL;
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

	else size = lua_objlen(L, bpv.mArg) - bpv.mProps->mShift;

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
		size_t size = lua_objlen(L, bpv.mArg), junk = size;

		if (bpv.mProps->mAlign) ALIGN(bpv.mProps->mAlign, size, ud, junk);

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
	void * ud = lua_newuserdata(L, sizeof(T));	// ..., ud

	new (ud) T;

	return ud;
}

template<int A> void * AllocN (lua_State * L)
{
	return Alloc<typename BlobXS::VectorType<A>::type>(L);	// ..., ud
}

template<typename T> void GC (lua_State * L)
{
	((T *)lua_touserdata(L, 1))->~T();
}

static int AddBytesReturn (lua_State * L, size_t count)
{
	lua_pushinteger(L, count);	// ..., count

	return 1;
}

template<typename T> void AppendElements (T * vec, ByteReader & reader)
{
	unsigned char * pbytes = (unsigned char *)reader.mBytes;

	vec->insert(vec->end(), pbytes, pbytes + reader.mCount);
}

template<typename T> void EraseElements (T * vec, int i1, int i2)
{
	vec->erase(vec->begin() + i1, vec->begin() + i2 + 1);
}

template<typename T> void InsertElements (T * vec, int pos, ByteReader & reader)
{
	unsigned char * pbytes = (unsigned char *)reader.mBytes;

	vec->insert(vec->begin() + pos, pbytes, pbytes + reader.mCount);
}

static int Append (lua_State * L, BlobPropViewer & bpv, ByteReader & reader)
{
	if (!bpv.mProps->mResizable) return AddBytesReturn(L, 0U);	// blob, ..., 0

	#define ALIGNED(n) AppendElements(BlobXS::GetVectorN<n>(L, 1), reader)
	#define UNALIGNED() AppendElements((ucvec *)BlobXS::GetVector(L, 1), reader)

	switch (bpv.mProps->mAlign)
	{
		WITH_VECTOR();
	}

	#undef ALIGNED
	#undef UNALIGNED

	return AddBytesReturn(L, reader.mCount);// blob, ..., count
}

static void Resize (lua_State * L, size_t align, int arg, size_t size)
{
	#define ALIGNED(n) BlobXS::GetVectorN<n>(L, arg)->resize(size)
	#define UNALIGNED() ((ucvec *)BlobXS::GetVector(L, arg))->resize(size)

	switch (align)
	{
		WITH_VECTOR();
	}
		
	#undef ALIGNED
	#undef UNALIGNED
}

#ifdef __ANDROID__
namespace ns_align = std;
#else
namespace ns_align = std::tr1;
#endif

void BlobXS::NewBlob (lua_State * L, size_t size, const CreateOpts * opts)
{
	size_t align = 0, extra = 0;
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

    else if (align > ns_align::alignment_of<double>::value)	// Lua gives back double-aligned memory
	{
		size_t cushioned = size + align - 1;

		void * ud = lua_newuserdata(L, cushioned);	// ..., ud

		extra = cushioned;

		ALIGN(align, size, ud, cushioned);

		extra -= cushioned;

		luaL_argcheck(L, ud, -1, "Unable to align blob");
	}

	else lua_newuserdata(L, size);	// ..., ud

	AttachMethods(L, type, [](lua_State * L)
	{
		luaL_Reg blob_methods[] = {
			{
				"Append", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);
					ByteReader reader(L, 2);

					if (!bpv.mProps || IsLocked(L, 1)) return AddBytesReturn(L, 0U);// blob, bytes, 0

					return Append(L, bpv, reader);	// blob, bytes, count
				}
			}, {
				"Clone", [](lua_State * L)
				{
					lua_pushboolean(L, 0);	// blob, false

					BlobPropViewer bpv(L, 1);
					CreateOpts copts;

					if (bpv.mProps)
					{
						copts.mAlignment = bpv.mProps->mAlign;
						copts.mResizable = bpv.mProps->mResizable;
					}

					copts.mType = NULL;

					lua_getmetatable(L, 1);	// blob, false[, props, prop], mt

					for (lua_pushnil(L); lua_next(L, LUA_REGISTRYINDEX); lua_pop(L, 1)) // blob, false[, props, prop], mt[, key, value]
					{
						if (lua_equal(L, -3, -1) && lua_type(L, -2) == LUA_TSTRING)
						{
							copts.mType = lua_tostring(L, -2);

							break;
						}
					}

					size_t size = GetSize(L, 1);

					NewBlob(L, size, &copts);	// blob, false[, props, prop], mt[, key, value], new_blob

					memcpy(GetData(L, -1), GetData(L, 1), size);

					lua_replace(L, bpv.mTop);	// blob, new_blob[, props, prop], mt[, key, value]

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
				"GetBytes", [](lua_State * L)
				{
					unsigned char * data = GetData(L, 1);
					size_t count = GetSize(L, 1);
					int i1 = luaL_optinteger(L, 2, 1);
					int i2 = luaL_optinteger(L, 3, count);

					if (i1 < 0) i1 += count + 1;
					if (i2 < 0) i2 += count + 1;

					if (i1 <= 0 || i1 > i2 || size_t(i2) > count) lua_pushliteral(L, "");	// blob, ""

					else lua_pushlstring(L, ((const char *)data) + i1 - 1, size_t(i2 - i1 + 1));// blob, data

					return 1;
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
				"Insert", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);
					ByteReader reader(L, 3);

					int pos = luaL_checkint(L, 2);
					size_t size = GetSize(L, 1);

					if (pos < 0) pos += size;

					else --pos;

					if (!reader.mBytes || reader.mCount == 0) return AddBytesReturn(L, 0U);	// blob, pos, bytes, 0
					if (pos < 0 || size_t(pos) > size) return AddBytesReturn(L, 0U);// ditto
					if (!bpv.mProps || IsLocked(L, 1)) return AddBytesReturn(L, 0U);// ditto

					if (size_t(pos) < size)
					{
						size_t count = reader.mCount;
						
						if (bpv.mProps->mResizable)
						{
							#define ALIGNED(n) InsertElements(GetVectorN<n>(L, 1), pos, reader)
							#define UNALIGNED() InsertElements(static_cast<ucvec *>(GetVector(L, 1)), pos, reader)

							switch (bpv.mProps->mAlign)
							{
								WITH_VECTOR();
							}
						
							#undef ALIGNED
							#undef UNALIGNED
						}

						else
						{
							unsigned char * data = GetData(L, 1) + pos;
							size_t available = size - size_t(pos);

							count = (std::min)(count, available);

							if (available > count) memmove(data + count, data, available - count);

							memcpy(data, reader.mBytes, count);
						}

						return AddBytesReturn(L, count);// blob, pos, bytes, count
					}

					else return Append(L, bpv, reader);	// blob, pos, bytes, count
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
				"Remove", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);
					size_t count = GetSize(L, 1);
					int i1 = luaL_optinteger(L, 2, 1);
					int i2 = luaL_optinteger(L, 3, count);

					if (i1 < 0) i1 += count;

					else --i1;

					if (i2 < 0) i2 += count;

					else --i2;

					if (i1 < 0 || i1 > i2 || size_t(i2) >= count) return AddBytesReturn(L, 0U);	// blob[, i1[, i2]], ""
					if (!bpv.mProps || IsLocked(L, 1)) return AddBytesReturn(L, 0U);// ditto

					if (bpv.mProps->mResizable)
					{
						#define ALIGNED(n) EraseElements(BlobXS::GetVectorN<n>(L, 1), i1, i2)
						#define UNALIGNED() EraseElements(static_cast<ucvec *>(GetVector(L, 1)), i1, i2)

						switch (bpv.mProps->mAlign)
						{
							WITH_VECTOR();
						}
						
						#undef ALIGNED
						#undef UNALIGNED
					}

					else
					{
						unsigned char * data = GetData(L, 1);

						memmove(data + i1, data + i2 + 1, count - size_t(i2) - 1);
					}

					return AddBytesReturn(L, size_t(i2 - i1) + 1);	// blob[, i1[, i2]], count
				}
			}, {
				"Submit", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);

					if (IsStoragePrepared() && bpv.mProps && !(bpv.mProps->mResizable && IsLocked(L, 1)))
					{
						size_t hash = Submit(L, 1);

						lua_pushlstring(L, reinterpret_cast<const char *>(&hash), sizeof(hash));// blob, hash
					}

					else lua_pushnil(L);// blob, nil

					return 1;
				}
			}, {
				"Sync", [](lua_State * L)
				{
					size_t hash;

					bool bSynced = IsStoragePrepared() && !IsLocked(L, 1) && IsHash(L, 2, hash) && Sync(L, 1, hash);

					lua_pushboolean(L, bSynced ? 1 : 0);// blob, synced

					return 1;
				}
			}, {
				"Write", [](lua_State * L)
				{
					BlobPropViewer bpv(L, 1);
					ByteReader reader(L, 3);

					int pos = luaL_checkint(L, 2);
					size_t size = GetSize(L, 1);

					if (pos < 0) pos += size;

					else --pos;

					if (!reader.mBytes || reader.mCount == 0) return AddBytesReturn(L, 0U);	// blob, pos, bytes, 0
					if (pos < 0 || size_t(pos) > size) return AddBytesReturn(L, 0U);// ditto
					if (!bpv.mProps || IsLocked(L, 1)) return AddBytesReturn(L, 0U);// ditto
					
					if (size_t(pos) < size)
					{
						size_t count = reader.mCount;
						
						if (!bpv.mProps->mResizable) count = (std::min)(count, size - size_t(pos));

						else if (count + pos > size) Resize(L, bpv.mProps->mAlign, 1, count + pos);

						unsigned char * data = GetData(L, 1) + pos; // do here in case resize changes address

						memcpy(data, reader.mBytes, count);

						return AddBytesReturn(L, count);// blob, pos, bytes, count
					}

					else return Append(L, bpv, reader);	// blob, pos, bytes, count
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
	props->mShift = extra;
	props->mResizable = bCanResize;

	lua_rawset(L, -3);	// ..., ud, props = { ..., [ud] = prop }
	lua_pop(L, 1);	// ..., ud

	// If requested, give resizable blobs an initial size (done after properties are bound).
	if (bCanResize && size > 0U) Resize(L, align, -1, size);
}

// Blob storage API
#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>

namespace BlobXS {
	// Vector with reference count, to allow periodic cleaning
	template<typename T> struct RefCountedVector {
		T mV;	// Vector
		int mRefCount;	// Frames to persist

		RefCountedVector (void) : mV(), mRefCount(5) {}
	};

	// Referenced counted hash map of vectors
	template<int N> struct HashMap { typedef std::map<size_t, RefCountedVector<typename VectorType<N>::type>> type; };
	template<> struct HashMap<0> { typedef std::map<size_t, RefCountedVector<ucvec>> type; };
};

static void * GetVectorStore (size_t align)
{
	void * store = NULL;

	#define ALIGNED(n) { static BlobXS::HashMap<n>::type BlobStore_##n; store = &BlobStore_##n; }
	#define UNALIGNED() { static BlobXS::HashMap<0>::type BlobStore_u; store = &BlobStore_u; }

	switch (align)
	{
		WITH_VECTOR();
	}

	#undef ALIGNED
	#undef UNALIGNED

	return store;
}

static std::mutex sMutex;
static int sCounter;

#define ALIGNMENTS_LOOP() for (size_t align = 0, next = 4U; align <= 1024U; align = next, next *= 4U)
#define WITH_MUTEX() std::lock_guard<std::mutex> lock(sMutex)

void BlobXS::CleanStorage (std::vector<size_t> & removed)
{
	ALIGNMENTS_LOOP()
	{
		WITH_MUTEX();

		#define UPDATE_OR_PURGE(n)	{ auto map = *static_cast<HashMap<n>::type *>(GetVectorStore(n));	\
									for (auto iter = map.begin(); iter != map.end(); ) {				\
										if (--(iter->second.mRefCount) == 0) {							\
											removed.push_back(iter->first); iter = map.erase(iter);		\
										} else ++iter; } }
		#define ALIGNED(n) UPDATE_OR_PURGE(n)
		#define UNALIGNED()	UPDATE_OR_PURGE(0)

		switch (align)
		{
			WITH_VECTOR();
		}

		#undef UPDATE_OR_PURGE
		#undef ALIGNED
		#undef UNALIGNED
	}
}

//
static std::atomic_bool sBlobStoragePrepared;

void BlobXS::PrepareStorage (void)
{
	sBlobStoragePrepared = true;
}

size_t BlobXS::Submit (lua_State * L, int arg)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps) return NULL;

	void * store = GetVectorStore(bpv.mProps->mAlign);
	size_t hash = std::hash<int>{}(sCounter);

	WITH_MUTEX();

	++sCounter;

	#define ALIGNED(n)	{ auto map = *static_cast<HashMap<n>::type *>(store);					\
						map[hash] = RefCountedVector<typename VectorType<n>::type>();			\
						if (bpv.mProps->mResizable) GetVectorN<n>(L, arg)->swap(map[hash].mV);	\
						else { unsigned char * data = GetData(L, arg);							\
							map[hash].mV.assign(data, data + GetSize(L, arg)); } }
	#define UNALIGNED()	{ auto map = *static_cast<HashMap<0>::type *>(store);									\
						map[hash] = RefCountedVector<ucvec>();													\
						if (bpv.mProps->mResizable) static_cast<ucvec *>(GetVector(L, arg))->swap(map[hash].mV);\
						else { unsigned char * data = GetData(L, arg);											\
							map[hash].mV.assign(data, data + GetSize(L, arg)); } }

	switch (bpv.mProps->mAlign)
	{
		WITH_VECTOR();
	}

	#undef ALIGNED
	#undef UNALIGNED

	return hash;
}

bool BlobXS::Exists (size_t hash)
{
	if (!IsStoragePrepared()) return false;

	WITH_MUTEX();

	#define ALIGNED(n)	{ auto map = static_cast<HashMap<n>::type *>(GetVectorStore(n));\
						if (map->find(hash) != map->end()) return true; }
	#define UNALIGNED()	ALIGNED(0)

	ALIGNMENTS_LOOP()
	{
		switch (align)
		{
			WITH_VECTOR();
		}
	}

	#undef ALIGNED
	#undef UNALIGNED

	return false;
}

bool BlobXS::IsHash (lua_State * L, int arg, size_t & hash)
{
	bool bIsHash = lua_type(L, arg) == LUA_TSTRING && lua_objlen(L, arg) == sizeof(size_t);

	if (bIsHash) memcpy(&hash, lua_tostring(L, 1), sizeof(size_t));

	return bIsHash;
}

bool BlobXS::IsStoragePrepared (void)
{
	return sBlobStoragePrepared;
}

static bool FastSwap (lua_State * L, int arg, size_t align, size_t hash)
{
	void * store = GetVectorStore(align);
	
	#define ALIGNED(n)	{ auto map = static_cast<BlobXS::HashMap<n>::type *>(store);			\
						auto iter = map->find(hash);											\
						if (iter == map->end()) return false;									\
						BlobXS::GetVectorN<n>(L, arg)->swap(iter->second.mV);					\
						map->erase(iter); }
	#define UNALIGNED()	{ auto map = static_cast<BlobXS::HashMap<0>::type *>(store);			\
						auto iter = map->find(hash);											\
						if (iter == map->end()) return false;									\
						static_cast<ucvec *>(BlobXS::GetVector(L, arg))->swap(iter->second.mV);	\
						map->erase(iter); }

	switch (align)
	{
		WITH_VECTOR();
	}

	#undef ALIGNED
	#undef UNALIGNED

	return true;
}

bool BlobXS::Sync (lua_State * L, int arg, size_t hash)
{
	BlobPropViewer bpv(L, arg);

	if (!bpv.mProps) return false;

	WITH_MUTEX();

	if (bpv.mProps->mResizable && FastSwap(L, arg, bpv.mProps->mAlign, hash)) return true;

	else
	{
		#define ALIGNED(n)	{ auto map = static_cast<HashMap<n>::type *>(GetVectorStore(n));\
							auto iter = map->find(hash);									\
							if (iter != map->end()) {										\
								from = iter->second.mV.data();								\
								size = iter->second.mV.size();								\
								break; } }
		#define UNALIGNED() ALIGNED(0)

		unsigned char * from = NULL;
		size_t size;

		ALIGNMENTS_LOOP()
		{
			switch (align)
			{
				WITH_VECTOR();
			}
		}

		#undef ALIGNED
		#undef UNALIGNED

		if (from)
		{
			if (bpv.mProps->mResizable)
			{
				#define ALIGNED(n)	GetVectorN<n>(L, arg)->assign(from, from + size)
				#define UNALIGNED() static_cast<ucvec *>(GetVector(L, arg))->assign(from, from + size)

				switch (bpv.mProps->mAlign)
				{
					WITH_VECTOR();
				}

				#undef ALIGNED
				#undef UNALIGNED
			}

			else memcpy(GetData(L, arg), from, (std::min)(GetSize(L, arg), size));

			return true;
		}
	}

	return false;
}

#undef ALIGN
#undef WITH_SIZE
#undef WITH_VECTOR
#undef ALIGNMENTS_LOOP
#undef WITH_MUTEX
*/