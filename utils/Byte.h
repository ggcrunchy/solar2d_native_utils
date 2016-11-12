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

const char * FitData (lua_State * L, const ByteReader & reader, int barg, size_t n, size_t size = 1U);
float * PointToFloats (lua_State * L, int arg, size_t nfloats, bool as_bytes, int * count = NULL);

struct BytesMetatableOpts {
	const char * mMetatableName;
	void (*mMore)(lua_State * L);

	BytesMetatableOpts (void) : mMetatableName(NULL), mMore(NULL) {}
};

void AddBytesMetatable (lua_State * L, const char * type, const BytesMetatableOpts * opts = NULL);

template<typename T = unsigned char> size_t GetSizeWithStride (lua_State * L, int w, int h, int stride, int nchannels = 1)
{
	int wlen = w * nchannels * sizeof(T);

	if (!stride) stride = wlen;

	else if (stride < wlen) luaL_error(L, "Stride too short: %i vs. w * nchannels * size = %i\n", stride, wlen);

	return size_t(stride * h);
}