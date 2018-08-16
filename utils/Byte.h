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
#include "ByteReader.h"
#include "utils/LuaEx.h"
#include "utils/Namespace.h"
#include "external/aligned_allocator.h"
#include <vector>

namespace CEU_NAMESPACE(ByteXS) {
    //
    struct ByteWriter {
        luaL_Buffer mB;	// Buffer, when not writing to a blob
        unsigned char * mLine;	// Current line, when writing to a blob
        size_t mOffset{0U}, mStride;// Offset into line; stride to next line
        
        ByteWriter (lua_State * L, unsigned char * out = nullptr, size_t stride = 0U);
        ~ByteWriter (void);
        
        void AddBytes (const void * bytes, size_t n);
        void NextLine (void);
        void ZeroPad (size_t n);
    };

	//
	template<typename T> size_t GetCount (lua_State * L, int arg)
	{
		int top = lua_gettop(L);

		ByteReader reader{L, arg, false}; // ..., reader args

		lua_settop(L, top);	// ...

		if (reader.mBytes) return reader.mCount / sizeof(T);

		return lua_objlen(L, arg);
	}

	//
	template<typename T = unsigned char> const T * EnsureN (lua_State * L, const ByteReader & reader, size_t n, size_t size = sizeof(T))
	{
		if (size < sizeof(T)) return nullptr;

		//
		size_t len = reader.mCount / size;
		const void * data = reader.mBytes;

		//
		if (n > len)
		{
			{
				ByteWriter writer{L};

				writer.AddBytes(data, len * size);
				writer.ZeroPad((n - len) * size);
			}
			
			// ~ByteWriter(): ..., data, ..., ex

			data = lua_tostring(L, -1);

			lua_replace(L, reader.mPos);// ..., ex, ...
		}

		return static_cast<const T *>(data);
	}

	const float * EnsureFloatsN (lua_State * L, int arg, size_t nfloats, bool as_bytes);
	const float * EnsureFloatsN (lua_State * L, int arg, size_t nfloats, float * afloats, size_t na, bool as_bytes);

	struct BytesMetatableOpts {
		const char * mMetatableName{nullptr};
		void (*mMore)(lua_State * L){nullptr};
	};

	void AddBytesMetatable (lua_State * L, const char * type, const BytesMetatableOpts * opts = nullptr);

	template<typename T = unsigned char> size_t GetSizeWithStride (lua_State * L, int w, int h, int stride, int nchannels = 1)
	{
		int wlen = w * nchannels * sizeof(T);

		if (!stride) stride = wlen;

		else if (stride < wlen) luaL_error(L, "Stride too short: %d vs. w * nchannels * size = %d\n", stride, wlen);

		return size_t(stride * h);
	}

	// Point to the start of a given element (stride unknown)
	template<typename T> T * PointToData (T * start, int x, int y, int w, int bpp, int * stride)
	{
		int istride = w * bpp;
 
		if (stride) *stride = istride;
	
		return start + y * istride + x * bpp;
	}

	// Point to the start of a given element (stride known)
	template<typename T> T * PointToData (T * start, int x, int y, int bpp, int stride)
	{
		return start + y * stride + x * bpp;
	}

	//
	template<int kPos = 1, typename F, typename T> int WithByteReader (lua_State * L, F func, T falsy)
	{
		std::function<int (lua_State *)> wrapped = [func](lua_State * L) {
			ByteReader reader{L, kPos};

			if (!reader.mBytes) lua_error(L);

			return func(L, reader);
		};

		return LuaXS::PCallWithStackThenReturn(L, wrapped, falsy);
	}
}

using namespace CEU_NAMESPACE(ByteXS);