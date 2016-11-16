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

namespace ByteXS {
	const unsigned char * FitData (lua_State * L, const ByteReader & reader, int barg, size_t n, size_t size)
	{
		barg = CoronaLuaNormalize(L, barg);

		//
		size_t len = reader.mCount / size;

		const char * data = static_cast<const char *>(reader.mBytes);

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

		return reinterpret_cast<const unsigned char *>(data);
	}

	const float * PointToFloats (lua_State * L, int arg, size_t nfloats, bool as_bytes, int * count)
	{
		float * pfloats;

		if (count) *count = int(nfloats);

		if (!lua_istable(L, arg))
		{
			ByteReader reader(L, arg);

			if (!reader.mBytes) lua_error(L);

			if (as_bytes)
			{
				auto bytes = static_cast<const unsigned char *>(reader.mBytes);

				pfloats = LuaXS::NewArray<float>(L, nfloats);	// ..., float_bytes, ..., floats

				for (size_t i = 0; i < (std::min)(reader.mCount, nfloats); ++i) pfloats[i] = float(bytes[i]) / 255.0f;
				for (size_t i = reader.mCount; i < nfloats; ++i) pfloats[i] = 0.0f;

				lua_replace(L, arg);// ..., floats, ...
			}

			else return FitDataTyped<float>(L, reader, arg, nfloats);
		}

		else
		{
			size_t n = lua_objlen(L, arg);
		
			pfloats = LuaXS::NewArray<float>(L, nfloats);	// ..., float_table, ..., floats

			for (size_t i = 0; i < (std::min)(n, nfloats); ++i)
			{
				lua_rawgeti(L, arg, i + 1);	// ..., float_table, ..., floats, v

				pfloats[i] = (float)luaL_checknumber(L, -1);

				lua_pop(L, 1);	// ..., float_table, ..., floats
			}

			for (size_t i = n; i < nfloats; ++i) pfloats[i] = 0.0f;

			lua_replace(L, arg);// ..., floats, ...
		}

		return pfloats;
	}

	ByteWriter::ByteWriter (lua_State * L, unsigned char * out, size_t stride) : mLine(out), mOffset(0U), mStride(stride)
	{
		if (!mLine) luaL_buffinit(L, &mB);

		else luaL_argcheck(L, BlobXS::IsBlob(L, -1), -1, "ByteWriter expects blob at top of stack"); 
	}

	ByteWriter::~ByteWriter (void)
	{
		if (!mLine) luaL_pushresult(&mB);	// ...[, bytes]
	}

	void ByteWriter::AddBytes (const unsigned char * bytes, size_t n)
	{
		if (!mLine) luaL_addlstring(&mB, reinterpret_cast<const char *>(bytes), n);

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
}