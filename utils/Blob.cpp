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
#include <functional>
#include <limits>

BlobXS::State::State (lua_State * L, int arg, const char * key, bool bLeave) : mPimpl(nullptr)
{
	auto impl = GetImplementations(L);

	mPimpl = impl ? impl->mMake() : new BlobXS::State::Pimpl();

	mPimpl->Initialize(L, arg, key, bLeave);
}

void BlobXS::State::Instantiate (lua_State * L, size_t size, const char * name)
{
	auto impl = GetImplementations(L);

	if (impl) impl->mInstantiate(L, size, name);
}

unsigned char * BlobXS::State::PointToDataIfBound (lua_State * L, int x, int y, int w, int h, int stride, int bpp)
{
	if (Bound() && InterpretAs(L, w, h, bpp, stride) && Fit(L, x, y, w, h)) return *this;

	else return nullptr;
}

unsigned char * BlobXS::State::PointToData (lua_State * L, int x, int y, int w, int h, int stride, bool bZero, int bpp)
{
	if (Bound())
	{
		if (InterpretAs(L, w, h, bpp, stride) && Fit(L, x, y, w, h))
		{
			if (bZero) Zero();

			return *this;
		}

		lua_pop(L, 1); // ...
	}

	size_t size = GetSizeWithStride(L, w, h, stride, bpp);

	unsigned char * out = static_cast<unsigned char *>(lua_newuserdata(L, size));	// ..., ud

	if (bZero) memset(out, 0, size);

	return out;
}

int BlobXS::State::PushData (lua_State * L, unsigned char * out, const char * btype, bool bAsUserdata)
{
	if (Bound()) CopyTo(out);

	else
	{
		if (bAsUserdata) AddBytesMetatable(L, btype);

		else lua_pushlstring(L, (const char *)out, lua_objlen(L, -1));
	}
	
	return 1;
}

//
size_t BlobXS::BlobPimpl::ComputeHash (unsigned long long value)
{
	return std::hash<unsigned long long>{}(value);
}

//
size_t BlobXS::BlobPimpl::BadHash (void)
{
	return ComputeHash(std::numeric_limits<unsigned long long>::max());
}

//
void BlobXS::PushImplKey (lua_State * L)
{
	lua_pushliteral(L, "BlobXS::ImplKey");	// ..., key
}

//
BlobXS::Pimpls * BlobXS::GetImplementations (lua_State * L)
{
	PushImplKey(L);	// ..., key

	lua_rawget(L, LUA_REGISTRYINDEX);	// ..., pimpls?

	BlobXS::Pimpls * impl = nullptr;

	if (!lua_isnil(L, -1)) impl = static_cast<BlobXS::Pimpls *>(lua_touserdata(L, -1));

	lua_pop(L, 1);	// ...

	return impl;
}

//
BlobXS::BlobPimpl & BlobXS::UsingPimpl (lua_State * L)
{
	static BlobPimpl def;

	auto impl = GetImplementations(L);

	if (impl) return *impl->mBlobImpl;

	else return def;
}