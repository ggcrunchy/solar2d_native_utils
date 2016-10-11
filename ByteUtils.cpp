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

#ifndef PATHUTILS_NOWRITEDATA
#include "PathUtils.h"
#endif

BlobXS::State::State (lua_State * L, int arg, const char * key, bool bLeave) : mW(0), mH(0), mBPP(0), mStride(0), mLength(0), mBlob(NULL), mData(NULL)
{
	if (key && lua_istable(L, arg))
	{
		lua_getfield(L, arg, key);	// ..., arg, ..., blob?

		Bind(L, -1);

		if (!bLeave) lua_pop(L, 1);	// ...
	}

	else Bind(L, arg);
}

bool BlobXS::State::Fit (int x, int y, int w, int h)
{
	if (!mBlob || mStride == 0) return false;
	if (x < 0 || y < 0 || w <= 0 || h <= 0) return false;
	if (x + w > mW || y + h > mH) return false;

	mData = mBlob + y * mStride + x * mBPP;

	return true;
}

bool BlobXS::State::InterpretAs (int w, int h, int bpp, int stride)
{
	if (!mBlob || w <= 0 || h <= 0 || bpp <= 0) return false;

	int wlen = w * bpp;

	if (!stride) stride = wlen;

	else if (stride < wlen) return false;

	if (size_t(h * stride) > mLength) return false;

	mW = w;
	mH = h;
	mBPP = bpp;
	mStride = stride;

	return true;
}

int BlobXS::State::GetOffset (lua_State * L, int t, const char * key)
{
	int offset = 0;

	if (mBlob && lua_istable(L, t))
	{
		lua_getfield(L, t, key);// ..., t, ..., offset

		offset = luaL_optint(L, -1, 0);

		lua_pop(L, 1);	// ...
	}

	return offset;
}

void BlobXS::State::Bind (lua_State * L, int arg)
{
	mBlob = !lua_isnoneornil(L, arg) ? (unsigned char *)luaL_checkudata(L, arg, "xs.blob") : NULL;
	mLength = lua_objlen(L, arg);
}

void BlobXS::State::Instantiate (lua_State * L, size_t size, const char * name)
{
	void * ud = lua_newuserdata(L, size);	// ..., ud

	memset(ud, 0, size);

	AddBytesMetatable(L, name);
}

unsigned char * BlobXS::State::PointToData (lua_State * L, int opts, int w, int h, int stride, int & was_blob, bool bZero, int bpp)
{
	size_t size = GetSizeWithStride(L, w, h, stride, bpp);

	State blob(L, opts, "blob");// ...[, blob]

    was_blob = blob.Bound() ? 1 : 0;
    
	if (was_blob)
	{
		if (blob.InterpretAs(w, h, bpp, stride))
		{
			int x = blob.GetOffset(L, opts, "xoff"), y = blob.GetOffset(L, opts, "yoff");

			if (blob.Fit(x, y, w, h)) return blob;
		}

		lua_pop(L, 1); // ...
	}

	unsigned char * out = (unsigned char *)lua_newuserdata(L, size);// ..., ud

	if (bZero) memset(out, 0, size);

	return out;
}

int BlobXS::State::PushData (lua_State * L, unsigned char * out, const char * btype, int was_blob, bool bAsUserdata)
{
	if (!was_blob)
	{
		if (bAsUserdata) AddBytesMetatable(L, btype);

		else lua_pushlstring(L, (const char *)out, lua_objlen(L, -1));
	}
	
	return 1;
}

const char * FitData (lua_State * L, const ByteReader & reader, int barg, size_t n, size_t size)
{
	barg = CoronaLuaNormalize(L, barg);

	//
	size_t len = reader.mCount / size;

	const char * data = (const char *)reader.mBytes;

	//
	if (n > len)
	{
		luaL_Buffer buff;

		luaL_buffinit(L, &buff);
		luaL_addlstring(&buff, data, len * size);

		if (size > 1)
		{
			char pad[2 * sizeof(double)] = { 0 };

			do {
				luaL_addlstring(&buff, pad, size);
			} while (++len < n);
		}
			
		else do {
			luaL_addchar(&buff, '\0');
		} while (++len < n);

		luaL_pushresult(&buff);	// ..., data, ..., ex
		lua_replace(L, barg);	// ..., ex, ...

		data = lua_tostring(L, barg);
	}

	return data;
}

float * PointToFloats (lua_State * L, int arg, size_t nfloats, bool as_bytes, int * count)
{
	float * pfloats;

	if (!lua_istable(L, arg))
	{
		ByteReader reader(L, arg);

		if (!reader.mBytes) lua_error(L);

		if (as_bytes)
		{
			auto bytes = (unsigned char *)reader.mBytes;

			pfloats = (float *)lua_newuserdata(L, nfloats * sizeof(float));	// ..., float_bytes, ..., floats

			for (size_t i = 0; i < (std::min)(reader.mCount, nfloats); ++i) pfloats[i] = float(bytes[i]) / 255.0f;
			for (size_t i = reader.mCount; i < nfloats; ++i) pfloats[i] = 0.0f;

			lua_replace(L, arg);// ..., floats, ...
		}

		else pfloats = (float *)FitData(L, reader, arg, nfloats, sizeof(float));
	}

	else
	{
		size_t n = lua_objlen(L, arg);
		
		pfloats = (float *)lua_newuserdata(L, nfloats * sizeof(float));	// ..., float_table, ..., floats

		for (size_t i = 0; i < (std::min)(n, nfloats); ++i)
		{
			lua_rawgeti(L, arg, i + 1);	// ..., float_table, ..., floats, v

			pfloats[i] = (float)luaL_checknumber(L, -1);

			lua_pop(L, 1);	// ..., float_table, ..., floats
		}

		for (size_t i = n; i < nfloats; ++i) pfloats[i] = 0.0f;

		lua_replace(L, arg);// ..., floats, ...
	}

	if (count) *count = int(nfloats);

	return pfloats;
}

void AddBytesMetatable (lua_State * L, const char * type, const BytesMetatableOpts * opts)
{
	luaL_argcheck(L, lua_isuserdata(L, -1), -1, "Non-userdata on top of stack");

	if (luaL_newmetatable(L, type))	// ..., ud, mt
	{
		lua_pushboolean(L, 1);	// ..., ud, mt, true
		lua_setfield(L, -2, "__bytes");	// ..., ud, mt = { __bytes = true }
		lua_pushcfunction(L, [](lua_State * L)
		{
			lua_pushinteger(L, lua_objlen(L, 1));	// bytes, len

			return 1;
		});	// ..., ud, mt, len
		lua_setfield(L, -2, "__len");	// ..., ud, mt = { __bytes, __len = len }

		const char * name = "bytes_mt";

		if (opts && opts->mMetatableName) name = opts->mMetatableName;

		lua_pushstring(L, name);// ..., ud, mt, name
		lua_setfield(L, -2, "__metatable");	// ..., ud, mt = { __bytes, __len, __metatable = name }

		if (opts && opts->mMore) opts->mMore(L);
	}

	lua_setmetatable(L, -2);// ..., ud
}


#ifndef PATHUTILS_NOWRITEDATA

WriteAux::WriteAux (lua_State * L, int w, int h, PathData * pd) : mFilename(NULL), mW(w), mH(h)
{
	if (pd) mFilename = pd->Canonicalize(L, false);
}

const char * WriteAux::GetBytes (lua_State * L, const ByteReader & reader, size_t w, size_t size, int barg) const
{
	return FitData(L, reader, barg, w, size_t(mH) * size);
}

WriteAuxReader::WriteAuxReader (lua_State * L, int w, int h, int barg, PathData * pd) : WriteAux(L, w, h, pd), mReader(L, barg), mBArg(barg)
{
	if (!mReader.mBytes) lua_error(L);
}

const char * WriteAuxReader::GetBytes (lua_State * L, size_t w, size_t size)
{
	return WriteAux::GetBytes(L, mReader, w, size, mBArg);
}

#endif