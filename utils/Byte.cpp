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

#include "utils/Blob.h"
#include "utils/Byte.h"
#include "utils/LuaEx.h"
#include "utils/SIMD.h"

namespace ByteXS {
	//
	template<typename F> float * LoadFloats (lua_State * L, int arg, F func, size_t n, size_t size, float * afloats, size_t na)
	{
		float * pfloats;

		size_t count = (std::max)(n, size);

		if (afloats && na >= count) pfloats = afloats;
		else pfloats = LuaXS::NewArray<float>(L, count);// ..., float_bytes, ..., floats

		for (size_t i = 0; i < n; ++i) func(pfloats, n);

		if (size > n) memset(&pfloats[n], 0, (size - n) * sizeof(float));

		if (!afloats || na < count) lua_replace(L, arg);// ..., floats, ...

		return pfloats;
	}

	//
	const float * EnsureFloatsN (lua_State * L, int arg, size_t nfloats, bool as_bytes)
	{
		return EnsureFloatsN(L, arg, nfloats, nullptr, 0U, as_bytes);
	}

	const float * EnsureFloatsN (lua_State * L, int arg, size_t nfloats, float * afloats, size_t na, bool as_bytes)
	{
		if (!lua_istable(L, arg))
		{
			ByteReader reader{L, arg};

			if (!reader.mBytes) lua_error(L);

			if (as_bytes) return LoadFloats(L, arg, [=](float * pfloats, size_t n) {
				SimdXS::Unorm8sToFloats(static_cast<const unsigned char *>(reader.mBytes), pfloats, n);
			}, reader.mCount, nfloats, afloats, na);

			else return EnsureN<float>(L, reader, nfloats);
		}

		else return LoadFloats(L, arg, [=](float * pfloats, size_t n) {
			LuaXS::ForEachI(L, arg, n, [=](lua_State * L, size_t i)
			{
				pfloats[i - 1] = LuaXS::Float(L, -1);
			});
		}, lua_objlen(L, arg), nfloats, afloats, na);
	}

	ByteWriter::ByteWriter (lua_State * L, unsigned char * out, size_t stride) : mLine{out}, mStride{stride}
	{
		if (!mLine) luaL_buffinit(L, &mB);

		else luaL_argcheck(L, BlobXS::IsBlob(L, -1), -1, "ByteWriter expects blob at top of stack"); 
	}

	ByteWriter::~ByteWriter (void)
	{
		if (!mLine) luaL_pushresult(&mB);	// ...[, bytes]
	}

	void ByteWriter::AddBytes (const void * bytes, size_t n)
	{
		if (!mLine) luaL_addlstring(&mB, static_cast<const char *>(bytes), n);

		else
		{
			memcpy(&mLine[mOffset], bytes, n);

			mOffset += n;
		}
	}

	void ByteWriter::NextLine (void)
	{
		if (mLine)
		{
			mLine += mStride;
			mOffset = 0U;
		}
	}

	void ByteWriter::ZeroPad (size_t n)
	{
		if (mLine)
		{
			memset(&mLine[mOffset], 0, n);

			mOffset += n;
		}

		else
		{
			const size_t kPadBufferSize = sizeof(std::max_align_t);
			static char pad[kPadBufferSize] = { 0 };

			for ( ; n >= kPadBufferSize; n -= kPadBufferSize) luaL_addlstring(&mB, pad, kPadBufferSize);

			if (n > 0U) luaL_addlstring(&mB, pad, n);
		}
	}

	void AddBytesMetatable (lua_State * L, const char * type, const BytesMetatableOpts * opts)
	{
		luaL_argcheck(L, lua_isuserdata(L, -1), -1, "Non-userdata on top of stack");

		if (luaL_newmetatable(L, type))	// ..., ud, mt
		{
			LuaXS::SetField(L, -1, "__bytes", true);// ..., ud, mt = { __bytes = true }
			LuaXS::SetField(L, -1, "__len", [](lua_State * L){
				return LuaXS::PushArgAndReturn(L, lua_objlen(L, 1));	// bytes, len
			});	// ..., ud, mt = { __bytes, __len = len }

			const char * name = "bytes_mt";

			if (opts && opts->mMetatableName) name = opts->mMetatableName;

			LuaXS::SetField(L, -1, "__metatable", name);// ..., ud, mt = { __bytes, __len, __metatable = name }

			if (opts && opts->mMore) opts->mMore(L);
		}

		lua_setmetatable(L, -2);// ..., ud
	}
}