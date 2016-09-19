#ifndef BYTE_UTILS_H
#define BYTE_UTILS_H

#include "CoronaLua.h"
#include "ByteReader.h"

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

	static void Instantiate (lua_State * L, size_t size);
	static unsigned char * PointToData (lua_State * L, int opts, int w, int h, int stride, int & was_blob, bool bZero = true, int bpp = 1);
	static int PushData (lua_State * L, unsigned char * out, const char * btype, int was_blob, bool bAsUserdata);
};

const char * FitData (lua_State * L, const ByteReader & reader, int barg, size_t n, size_t size = 1U);
float * PointToFloats (lua_State * L, int arg, size_t nfloats, bool as_bytes, int * count = NULL);
void AddBytesMetatable (lua_State * L, const char * type);

template<typename T = unsigned char> size_t GetSizeWithStride (lua_State * L, int w, int h, int stride, int nchannels = 1)
{
	int wlen = w * nchannels * sizeof(T);

	if (!stride) stride = wlen;

	else if (stride < wlen) luaL_error(L, "Stride too short: %i vs. w * nchannels * size = %i\n", stride, wlen);

	return size_t(stride * h);
}

#endif