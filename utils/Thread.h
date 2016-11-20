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

#include <type_traits>
#include <vector>

namespace ThreadXS {
	typedef std::vector<unsigned char> var_storage;

	struct Slot {
		var_storage mData;
		size_t mIndex;

		Slot (size_t size);

		void GetVar (void * var);
		void SetVar (const void * var);
		void Sync (void);
	};

	template<typename T> class TLS {
		Slot mSlot;

		static_assert(std::is_trivial<T>::value && std::is_trivially_destructible<T>::value, "ThreadXS::TLS only supports trivial types with trivial destructors");

	public:
		TLS (void) : mSlot(sizeof(T))
		{
			mSlot.Sync();
		}

		TLS (const T & value) : mSlot(sizeof(T))
		{
			mSlot.SetVar(&value);
			mSlot.Sync();
		}

		TLS & operator = (const T & value)
		{
			mSlot.SetVar(&value);

			return *this;
		}

		operator T ()
		{
			T value;

			mSlot.GetVar(&value);

			return value;
		}

#ifndef _WIN32 // workaround for lack of SFINAE on MSVC
		#error "Add some std::enable_if() construct here!"
#endif
		T operator -> ()
		{
#ifdef _WIN32
			if (std::is_pointer<T>::value)
#endif
			{
				T value;

				mSlot.GetVar(&value);

				return value;
			}
#ifdef _WIN32
			else return nullptr;
#endif
		}
	};
}