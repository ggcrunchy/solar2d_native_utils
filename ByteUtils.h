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

#ifndef BYTE_UTILS_H
#define BYTE_UTILS_H

#include "CoronaLua.h"
#include "ByteReader.h"
#include "aligned_allocator.h"
#include <vector>

class BlobState {
	int mW, mH, mBPP, mStride;	// Interpreted width, height, bits-per-pixel, and stride
	size_t mLength;	// Length of blob
	unsigned char * mBlob, * mData;	// Pointer to blob proper; pointer into its data

	void Bind (lua_State * L, int arg);

public:
	bool Bound (void) { return mBlob != NULL; }
	bool Fit (int x, int y, int w, int h);
	bool InterpretAs (int w, int h, int bpp, int stride = 0);
	int GetOffset (lua_State * L, int t, const char * key);
	operator unsigned char * (void) { return mData; }

	BlobState (lua_State * L, int arg, const char * key = NULL, bool bLeave = true);

	static void Instantiate (lua_State * L, size_t size, const char * type = "xs.blob");
	static unsigned char * PointToData (lua_State * L, int opts, int w, int h, int stride, int & was_blob, bool bZero = true, int bpp = 1);
	static int PushData (lua_State * L, unsigned char * out, const char * btype, int was_blob, bool bAsUserdata);
};

struct BlobOpts {
	size_t mAlignment;	// Multiple of 4 detailing what sort of memory alignment to assume (with 0 meaning none)
	bool mResizable;// If true, the userdata holds a vector that contains the blob; otherwise, it is the blob
	const char * mType;	// User-defined blob type (if unspecified, "xs.blob")

	BlobOpts (void) : mAlignment(0), mType(NULL), mResizable(false) {}
};

template<size_t N, typename T = unsigned char> struct VectorType {
	typedef std::vector<T, simdpp::SIMDPP_ARCH_NAMESPACE::aligned_allocator<T, N>> type;
};

bool IsBlob (lua_State * L, int arg, const char * type = NULL);
size_t GetBlobAlignment (lua_State * L, int arg);
size_t GetBlobSize (lua_State * L, int arg, bool bNSized = false);
unsigned char * GetBlobData (lua_State * L, int arg);
void * GetBlobVector (lua_State * L, int arg);

template<size_t N> typename VectorType<N>::type * GetBlobVectorN (lua_State * L, int arg)
{
	return (typename VectorType<N>::type *)GetBlobVector(L, arg);
}

void NewBlob (lua_State * L, size_t size, const BlobOpts * opts);

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

#endif