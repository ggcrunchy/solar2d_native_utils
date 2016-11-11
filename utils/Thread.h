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

#ifdef _WIN32
	#define DECL_TLS(type) __declspec(thread) type
#else
	#ifdef __APPLE__
		#include "TargetCondtionals.h"
	#endif

	#if TARGET_OS_IOS
		#include <type_traits>

		namespace TLSXS {
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

			template<typename T> class TypeWithTLS {
				size_t mSlot;

			public:
				TypeWithTLS (void)
				{
					mSlot = GetSlot();
				}

				TypeWithTLS (const T & value)
				{
					mSlot = GetSlot();

					SetItemInSlot(mSlot, Value<T>(value).mData);
				}

				TypeWithTLS<T> & operator = (const T & value)
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

				T operator -> () // workaround while testing on MSVC
				{
					if (std::is_pointer<T>::value)
					{
						Value<T> value;

						GetItemInSlot(mSlot, value.mData);

						return value.Get();
					}

					else return nullptr;
				}
			};
		}

		#define DECL_TLS(type) static TLSXS::TypeWithTLS<type>
	#else
		#define DECL_TLS(type) __thread type
	#endif
#endif