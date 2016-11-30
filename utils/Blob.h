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
#include "external/aligned_allocator.h"
#include <vector>

namespace BlobXS {
	//
	template<size_t N, typename T = unsigned char> struct VectorType {
		typedef std::vector<T, simdpp::SIMDPP_ARCH_NAMESPACE::aligned_allocator<T, N>> type;
	};

	template<> struct VectorType<0U> {
		typedef std::vector<unsigned char> type;
	};

	//
	class State {
	public:
		struct Pimpl {
			// Methods
			virtual bool CanBind (lua_State * L, int, int, int, int) { return false; } // Other, options? (sets reason if false)
			virtual void Commit (lua_State * L) {} // ...if true, applies previous step
			// ^^^ TO DO: Fumbling toward a solution, but these should subsume the next three and possibly the later methods

			virtual bool Bound (void) const { return false; }
			virtual bool Fit (lua_State *, int, int, int, int) { return false; }
			virtual bool InterpretAs (lua_State *, int, int, int, int) { return false; }
			virtual void CopyTo (void * ptr) {}
			virtual void LoadFrom (void * ptr) {}
			virtual void Zero (void) {}
			virtual operator unsigned char * (void) { return nullptr; }
			virtual const char * GetReason (void) const { return nullptr; }

			// Lifetime
			virtual void Initialize (lua_State *, int, const char *, bool) {}
			virtual ~Pimpl (void) {}
		};

	private:
		Pimpl * mPimpl;	// Implementation details

	public:
		bool Bound (void) const { return mPimpl->Bound(); }
		bool Fit (lua_State * L, int x, int y, int w, int h) { return mPimpl->Fit(L, x, y, w, h); }
		bool InterpretAs (lua_State * L, int w, int h, int bpp, int stride = 0) { return mPimpl->InterpretAs(L, w, h, bpp, stride); }
		void CopyTo (void * ptr) { mPimpl->CopyTo(ptr); }
		void LoadFrom (void * ptr) { mPimpl->LoadFrom(ptr); }
		void Zero (void) { mPimpl->Zero(); }
		operator unsigned char * (void) { return mPimpl->operator unsigned char *(); }
		const char * GetReason (void) const { return mPimpl->GetReason(); }

		State (lua_State * L, int arg, const char * key = nullptr, bool bLeave = true);
		~State (void) { delete mPimpl; }

		static void Instantiate (lua_State * L, size_t size, const char * type = "xs.blob");

		unsigned char * PointToDataIfBound (lua_State * L, int x, int y, int w, int h, int stride, int bpp = 1);
		unsigned char * PointToData (lua_State * L, int x, int y, int w, int h, int stride, bool bZero = true, int bpp = 1);
		int PushData (lua_State * L, unsigned char * out, const char * btype, bool bAsUserdata);
	};

	// Check for blobs in an options table. If present, the blob is pushed onto the stack and
	// the members of this structure are filled in; otherwise, pushes nil.
	struct Options {
		// Lookup keys: a blob may be in one or the other of `mBlob` or `mOpt`, favoring the
		// former. If a blob is found in `mBlob`, it must be used, or else we have an error; a
		// blob in `mOpt`, on the other hand, says "use me if I'm satisfactory, otherwise seek
		// alternatives". Blob-related information may be found in the table in `mInfo`.
		struct Keys {
			const char * mBlob, * mOpt, * mInfo;// Defaults = "blob", "opt_blob", "blob_info"

			Keys (void);
		} mKeys;

		// An info table might look like so:
		// { x = 7, y = 2, stride = 100, no_resize = false, discard_ok = true }
		int mX, mY;	// Offsets into blob, >= 0; both default to 0
		int mStride;// Stride of blob data; defaults to 0, meaning it will be calculated as width * bytes-per-component
		bool mNoResize;	// Request to not resize blob even if possible; defaults to false
		bool mDiscardOK;// Allow destructive resizing, e.g. along the x-axis; defaults to false
		bool mMustUseBlob;	// If true, blob must be used

		Options (lua_State * L, int arg, const Keys & keys = Keys());
	};

	//
	struct CreateOpts {
		size_t mAlignment;	// Multiple of 4 detailing what sort of memory alignment to assume (with 0 meaning none)
		bool mResizable;// If true, the userdata holds a vector that contains the blob; otherwise, it is the blob
		const char * mType;	// User-defined blob type (if unspecified, "xs.blob")

		CreateOpts (void) : mAlignment{0}, mType{nullptr}, mResizable{false}
		{
		}
	};

	//
	struct BlobPimpl {
		typedef unsigned long long storage_id;

		// Helper functions
		static storage_id BadID (void);

		// Methods
		virtual bool IsBlob (lua_State *, int, const char *) { return false; }
		virtual bool IsLocked (lua_State *, int, void *) { return false; }
		virtual bool IsResizable (lua_State *, int) { return false; }
		virtual bool Lock (lua_State *, int, void *) { return false; }
		virtual bool Unlock (lua_State *, int, void *) { return false; }
		virtual bool Resize (lua_State *, int, size_t, void *) { return false; }
		virtual size_t GetAlignment (lua_State *, int) { return 0U; }
		virtual size_t GetSize (lua_State *, int, bool) { return 0U; }
		virtual unsigned char * GetData (lua_State *, int) { return nullptr; }
		virtual void * GetVector (lua_State *, int) { return nullptr; }

		virtual void NewBlob (lua_State *, size_t, const CreateOpts *) {}

		virtual storage_id Submit (lua_State *, int, void *) { return BadID(); }
		virtual storage_id GetID (lua_State *, int) { return BadID(); }
		virtual bool Exists (storage_id) { return false; }
		virtual bool Sync (lua_State *, int, storage_id, void *) { return false; }

		// Lifetime
		virtual ~BlobPimpl (void) {}
	};

	// Blob implementations, once blob plugin is loaded in thread
	struct Pimpls {
		BlobPimpl * mBlobImpl;	// Blob implementation
		void (*mInstantiate)(lua_State *, size_t, const char *);// Factory for blobs
		State::Pimpl * (*mMake)(void);	// Factory for state implementation
	};	

	// Interface
	void PushImplKey (lua_State * L);
	Pimpls * GetImplementations (lua_State * L);
	BlobPimpl & UsingPimpl (lua_State * L);

	inline bool IsBlob (lua_State * L, int arg, const char * type = nullptr) { return UsingPimpl(L).IsBlob(L, arg, type); }
	inline bool IsLocked (lua_State * L, int arg, void * key = nullptr) { return UsingPimpl(L).IsLocked(L, arg, key); }
	inline bool IsResizable (lua_State * L, int arg) { return UsingPimpl(L).IsResizable(L, arg); }
	inline bool Lock (lua_State * L, int arg, void * key) { return UsingPimpl(L).Lock(L, arg, key); }
	inline bool Unlock (lua_State * L, int arg, void * key) { return UsingPimpl(L).Unlock(L, arg, key); }
	inline bool Resize (lua_State * L, int arg, size_t size, void * key = nullptr) { return UsingPimpl(L).Resize(L, arg, size, key); }
	inline size_t GetAlignment (lua_State * L, int arg) { return UsingPimpl(L).GetAlignment(L, arg); }
	inline size_t GetSize (lua_State * L, int arg, bool bNSized = false) { return UsingPimpl(L).GetSize(L, arg, bNSized); }
	inline unsigned char * GetData (lua_State * L, int arg) { return UsingPimpl(L).GetData(L, arg); }
	inline void * GetVector (lua_State * L, int arg) { return UsingPimpl(L).GetVector(L, arg); }

	template<size_t N> typename VectorType<N>::type * GetVectorN (void * data)
	{
		return static_cast<typename VectorType<N>::type *>(data);
	}

	template<size_t N> typename VectorType<N>::type * GetVectorN (lua_State * L, int arg)
	{
		return GetVectorN<N>(GetVector(L, arg));
	}

	inline void NewBlob (lua_State * L, size_t size, const CreateOpts * opts) { UsingPimpl(L).NewBlob(L, size, opts); }

	inline BlobPimpl::storage_id Submit (lua_State * L, int arg, void * key = nullptr) { return UsingPimpl(L).Submit(L, arg, key); }
	inline BlobPimpl::storage_id GetID (lua_State * L, int arg) { return UsingPimpl(L).GetID(L, arg); }
	inline bool Exists (lua_State * L, BlobPimpl::storage_id id) { return UsingPimpl(L).Exists(id); }
	inline bool Sync (lua_State * L, int arg, BlobPimpl::storage_id id, void * key = nullptr) { return UsingPimpl(L).Sync(L, arg, id, key); }
}