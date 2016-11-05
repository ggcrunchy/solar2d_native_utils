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

#include "BlobUtils.h"
#include "ByteUtils.h"

BlobXS::State::State (lua_State * L, int arg, const char * key, bool bLeave) : mPimpl(nullptr)
{
	auto impl = GetImplementations(L);

	mPimpl = impl ? impl->mMake() : new BlobXS::State::Pimpl();
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