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

namespace ThreadXS {
	union POD {
		void * mP;
		double mD;
	};

	template<typename T> struct Value {
		POD mData;

		T Get (void)
		{
			T value;

			memcpy(&value, &mData, sizeof(T));

			return value;
		}

		void Set (const T & value)
		{
			memcpy(&mData, &value, sizeof(T));
		}

		Value (void) {}

		Value (const T & value)
		{
			Set(value);
		}
	};

	size_t GetSlot (void);
	void GetItemInSlot (size_t slot, POD & pod);
	void SetItemInSlot (size_t slot, const POD & pod);
	void Sync (void);

	template<typename T> class TLS {
		size_t mSlot;

	public:
		TLS (void)
		{
			mSlot = GetSlot();
		}

		TLS (const T & value)
		{
			mSlot = GetSlot();

			SetItemInSlot(mSlot, Value<T>(value).mData);
		}

		TLS & operator = (const T & value)
		{
			SetItemInSlot(mSlot, Value<T>(value).mData);

			return *this;
		}

		operator T ()
		{
			Value<T> value;

			GetItemInSlot(mSlot, value.mData);

			return value.Get();
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
				Value<T> value;

				GetItemInSlot(mSlot, value.mData);

				return value.Get();
			}
#ifdef _WIN32
			else return nullptr;
#endif
		}
	};
}