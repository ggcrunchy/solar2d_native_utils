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

#ifndef BLOB_UTILS_H
#define BLOB_UTILS_H

#include "CoronaLua.h"
#include "aligned_allocator.h"
#include <vector>

namespace BlobXS {
	//
	template<size_t N, typename T = unsigned char> struct VectorType {
		typedef std::vector<T, simdpp::SIMDPP_ARCH_NAMESPACE::aligned_allocator<T, N>> type;
	};

	using namespace simdpp::SIMDPP_ARCH_NAMESPACE;
	using ucvec = std::vector<unsigned char>;

	//
	class State {
	public:
		struct Pimpl {
			// Methods
			virtual bool Bound (void) { return false; }
			virtual bool Fit (int, int, int, int) { return false; }
			virtual bool InterpretAs (int, int, int, int) { return false; }
			virtual int GetOffset (lua_State *, int, const char *) { return 0; }
			virtual operator unsigned char * (void) { return nullptr; }

			// Lifetime
			virtual void Initialize (lua_State *, int, const char *, bool) {}
			virtual ~Pimpl (void) {}
		};

	private:
		Pimpl * mPimpl;	// Implementation details

	public:

		bool Bound (void) { return mPimpl->Bound(); }
		bool Fit (int x, int y, int w, int h) { return mPimpl->Fit(x, y, w, h); }
		bool InterpretAs (int w, int h, int bpp, int stride = 0) { return mPimpl->InterpretAs(w, h, bpp, stride); }
		int GetOffset (lua_State * L, int t, const char * key) { return mPimpl->GetOffset(L, t, key); }
		operator unsigned char * (void) { return mPimpl->operator unsigned char *(); }

		State (lua_State * L, int arg, const char * key = NULL, bool bLeave = true);
		~State (void) { delete mPimpl; }

		static void Instantiate (lua_State * L, size_t size, const char * type = "xs.blob");
		static unsigned char * PointToData (lua_State * L, int opts, int w, int h, int stride, int & was_blob, bool bZero = true, int bpp = 1);
		static int PushData (lua_State * L, unsigned char * out, const char * btype, int was_blob, bool bAsUserdata);
	};

	//
	struct CreateOpts {
		size_t mAlignment;	// Multiple of 4 detailing what sort of memory alignment to assume (with 0 meaning none)
		bool mResizable;// If true, the userdata holds a vector that contains the blob; otherwise, it is the blob
		const char * mType;	// User-defined blob type (if unspecified, "xs.blob")

		CreateOpts (void) : mAlignment(0), mType(NULL), mResizable(false) {}
	};

	//
	struct BlobPimpl {
		// Methods
		virtual bool IsBlob (lua_State *, int, const char *) { return false; }
		virtual bool IsLocked (lua_State *, int, void *) { return false; }
		virtual bool IsResizable (lua_State *, int) { return false; }
		virtual bool Lock (lua_State *, int, void *) { return false; }
		virtual bool Unlock (lua_State *, int, void *) { return false; }
		virtual size_t GetAlignment (lua_State *, int) { return 0U; }
		virtual size_t GetSize (lua_State *, int, bool) { return 0U; }
		virtual unsigned char * GetData (lua_State *, int) { return nullptr; }
		virtual void * GetVector (lua_State *, int) { return nullptr; }

		virtual void NewBlob (lua_State *, size_t, const CreateOpts *) {}

		virtual size_t Submit (lua_State *, int) { return 0U; }
		virtual bool Exists (size_t) { return false; }
		virtual bool IsHash (lua_State *, int, size_t &) { return false; }
		virtual bool Sync (lua_State *, int, size_t) { return false; }

		// Lifetime
		virtual ~BlobPimpl (void) {}
	};

	// Blob implementations, once blob plugin is loaded in thread
	struct Pimpls {
		BlobPimpl * mBlobImpl;	// Blob implementation
		State::Pimpl * (*mMake)(void);	// Factory for state implementation
	};

	// Interface
	Pimpls * GetImplementations (lua_State * L);
	BlobPimpl & UsingPimpl (lua_State * L);

	inline bool IsBlob (lua_State * L, int arg, const char * type = NULL) { return UsingPimpl(L).IsBlob(L, arg, type); }
	inline bool IsLocked (lua_State * L, int arg, void * key = NULL) { return UsingPimpl(L).IsLocked(L, arg, key); }
	inline bool IsResizable (lua_State * L, int arg) { return UsingPimpl(L).IsResizable(L, arg); }
	inline bool Lock (lua_State * L, int arg, void * key) { return UsingPimpl(L).Lock(L, arg, key); }
	inline bool Unlock (lua_State * L, int arg, void * key) { return UsingPimpl(L).Unlock(L, arg, key); }
	inline size_t GetAlignment (lua_State * L, int arg) { return UsingPimpl(L).GetAlignment(L, arg); }
	inline size_t GetSize (lua_State * L, int arg, bool bNSized = false) { return UsingPimpl(L).GetSize(L, arg, bNSized); }
	inline unsigned char * GetData (lua_State * L, int arg) { return UsingPimpl(L).GetData(L, arg); }
	inline void * GetVector (lua_State * L, int arg) { return UsingPimpl(L).GetVector(L, arg); }

	template<size_t N> typename VectorType<N>::type * GetVectorN (lua_State * L, int arg)
	{
		return (typename VectorType<N>::type *)GetVector(L, arg);
	}

	inline void NewBlob (lua_State * L, size_t size, const CreateOpts * opts) { UsingPimpl(L).NewBlob(L, size, opts); }

	inline size_t Submit (lua_State * L, int arg) { return UsingPimpl(L).Submit(L, arg); }
	inline bool Exists (lua_State * L, size_t hash) { return UsingPimpl(L).Exists(hash); }
	inline bool IsHash (lua_State * L, int arg, size_t & hash) { return UsingPimpl(L).IsHash(L, arg, hash); }
	inline bool Sync (lua_State * L, int arg, size_t hash) { return UsingPimpl(L).Sync(L, arg, hash); }
};

#endif