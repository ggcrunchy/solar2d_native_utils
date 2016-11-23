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

#ifdef _WIN32
	#include <ppl.h>
#elif __APPLE__
	#include <dispatch/dispatch.h>
#elif __ANDROID__
	#include <parallel/algorithm>
#else
	#error "Unsupported target"
#endif

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

		static_assert(std::is_trivially_copyable<T>::value && std::is_trivially_destructible<T>::value, "ThreadXS::TLS only supports types that are trivally copyable and destructible");

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
			std::aligned_union<0U, T> value;

			mSlot.GetVar(&value);

			return *reinterpret_cast<T *>(&value);
		}

#ifdef _WIN32 // workaround for lack of SFINAE on MSVC 2013
		template<bool is_pointer = std::is_pointer<T>::value> typename std::conditional<is_pointer, T, void>::type operator -> (void);

		template<> inline T operator -> <true> (void) { return T(*this); }
		template<> inline void operator -> <false> (void) {}
#else
		template<typename T> std::enable_if<std::is_pointer<T>::value, T>::type operator -> () { return T(*this); }
#endif
	};

	// Adapted from parallel_for_each, which follows
	template<typename F> inline void parallel_for (size_t a, size_t b, F && f)
	{
	#ifdef _WIN32
		Concurrency::parallel_for(a, b, std::forward<F>(f));
	#elif __APPLE__
		using data_t = std::pair<size_t, F>;
		data_t helper = data_t(a, std::forward<F>(f));

		dispatch_apply_f(b - a, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), &helper, [](void * ctx, size_t cnt)
		{
			data_t * d = static_cast<data_t *>(ctx);

			(*d).second(d->first + cnt);
		});
	#elif __ANDROID__
		__gnu_parallel::generate_n(a, b - a, std::forward<F>(f));
	#endif
	}

	// https://xenakios.wordpress.com/2014/09/29/concurrency-in-c-the-cross-platform-way/
	template<typename It, typename F> inline void parallel_for_each (It a, It b, F && f)
	{
	#ifdef _WIN32
		Concurrency::parallel_for_each(a, b, std::forward<F>(f));
	#elif __APPLE__
		size_t count = std::distance(a, b);
		using data_t = std::pair<It, F>;
		data_t helper = data_t(a, std::forward<F>(f));

		dispatch_apply_f(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), &helper, [](void * ctx, size_t cnt)
		{
			data_t * d = static_cast<data_t *>(ctx);
			auto elem_it = std::next(d->first, cnt);

			(*d).second(*(elem_it));
		});
	#elif __ANDROID__
		__gnu_parallel::for_each(a, b, std::forward<F>(f));
	#endif
	}
}