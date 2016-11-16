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
#include "aligned_allocator.h"
#include <vector>

namespace ByteXS {
	const unsigned char * FitData (lua_State * L, const ByteReader & reader, int barg, size_t n, size_t size = 1U);
	const float * PointToFloats (lua_State * L, int arg, size_t nfloats, bool as_bytes, int * count = nullptr);

	template<typename T> const T * FitDataTyped (lua_State * L, const ByteReader & reader, int barg, size_t n)
	{
		const void * data = FitData(L, reader, barg, n, sizeof(T));

		return static_cast<const T *>(data);
	}

	struct ByteWriter {
		luaL_Buffer mB;	// Buffer, when not writing to a blob
		unsigned char * mLine;	// Current line, when writing to a blob
		size_t mOffset, mStride;// Offset into line; stride to next line

		ByteWriter (lua_State * L, unsigned char * out = nullptr, size_t stride = 0U);
		~ByteWriter (void);

		void AddBytes (const unsigned char * bytes, size_t n);
		void NextLine (void);
	};

	struct BytesMetatableOpts {
		const char * mMetatableName;
		void (*mMore)(lua_State * L);

		BytesMetatableOpts (void) : mMetatableName(nullptr), mMore(nullptr) {}
	};

	void AddBytesMetatable (lua_State * L, const char * type, const BytesMetatableOpts * opts = nullptr);

	template<typename T = unsigned char> size_t GetSizeWithStride (lua_State * L, int w, int h, int stride, int nchannels = 1)
	{
		int wlen = w * nchannels * sizeof(T);

		if (!stride) stride = wlen;

		else if (stride < wlen) luaL_error(L, "Stride too short: %i vs. w * nchannels * size = %i\n", stride, wlen);

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
}