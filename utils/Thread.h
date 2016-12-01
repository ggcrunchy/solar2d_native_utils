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

#include "utils/Compat.h"
#include <algorithm>
#include <vector>

#ifdef _WIN32
	#include <ppl.h>
#elif __APPLE__
	#include <dispatch/dispatch.h>
	#include "TargetConditionals.h"
#elif __ANDROID__
	#include <parallel/algorithm>
#else
	#error "Unsupported target"
#endif

namespace ThreadXS {
	// Macro to declare type with thread-local duration, preferring compiler support when available.
	// Using `thread_local` would be even better, but seems to be pretty hit-and-miss.
	#ifdef _WIN32
		#define THREAD_LOCAL(type) __declspec(thread) type
	#elif !TARGET_OS_IOS
		#define THREAD_LOCAL(type) __thread type
	#else
		#define THREAD_LOCAL(type) ThreadXS::TLS<type>
	#endif

	//
	class Slot {
	public:
		using storage_type = std::vector<unsigned char>;

	private:
		storage_type mData;
		size_t mIndex;

		void Init (void);

	public:
		Slot (size_t size);
		Slot (size_t size, const void * var);

		void GetVar (void * var);
		void SetVar (const void * var);
	};

	//
	template<typename T> class TLS {
		Slot mSlot;

		static_assert(CompatXS::TrivialTraits<T>::is_copyable::value && CompatXS::TrivialTraits<T>::is_destructible::value, "ThreadXS::TLS only supports types that are trivally copyable and destructible");

	public:
		TLS (void) : mSlot{sizeof(T)}
		{
		}

		TLS (const T & value) : mSlot{sizeof(T), &value}
		{
		}

		TLS & operator = (const T & value)
		{
			mSlot.SetVar(&value);

			return *this;
		}

		operator T ()
		{
			std::aligned_union<0U, T> value;// Use POD data so that T need not be trivially constructible

			mSlot.GetVar(&value);

			return *reinterpret_cast<T *>(&value);
		}

	#ifdef _WIN32 // workaround for lack of SFINAE on MSVC 2013
		template<bool is_pointer = std::is_pointer<T>::value> typename std::conditional<is_pointer, T, void>::type operator -> (void);

		template<> inline T operator -> <true> (void) { return T{*this}; }
		template<> inline void operator -> <false> (void) {} // T is not a pointer, so cut off this operator
	#else
		template<typename U = T> typename std::enable_if<std::is_pointer<U>::value, U>::type operator -> () { return U{*this}; }
	#endif
	};

	// Adapted from parallel_for_each, which follows
	template<typename F> inline void parallel_for (size_t a, size_t b, F && f)
	{
	#ifdef _WIN32
		Concurrency::parallel_for(a, b, std::forward<F>(f));
	#elif __APPLE__
		using data_t = std::pair<size_t, F>;

		data_t helper = data_t{a, CompatXS::forward<F>(f)};

		dispatch_apply_f(b - a, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), &helper, [](void * ctx, size_t cnt)
		{
			data_t * d = static_cast<data_t *>(ctx);

			(*d).second(d->first + cnt);
		});
	#elif __ANDROID__
		__gnu_parallel::generate_n(a, b - a, std::forward<F>(f));
	#endif
	}

	template<typename F> inline void parallel_for (size_t a, size_t b, F && f, bool bParallel)
	{
		if (bParallel) parallel_for(a, b, CompatXS::forward<F>(f));

		else std::generate_n(a, b - a, CompatXS::forward<F>(f));
	}

	// https://xenakios.wordpress.com/2014/09/29/concurrency-in-c-the-cross-platform-way/
	template<typename It, typename F> inline void parallel_for_each (It a, It b, F && f)
	{
	#ifdef _WIN32
		Concurrency::parallel_for_each(a, b, std::forward<F>(f));
	#elif __APPLE__
		using data_t = std::pair<It, F>;

		size_t count = std::distance(a, b);
		data_t helper = data_t{a, CompatXS::forward<F>(f)};

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

	template<typename It, typename F> inline void parallel_for_each (It a, It b, F && f, bool bParallel)
	{
		if (bParallel) parallel_for_each(CompatXS::forward<It>(a), CompatXS::forward<It>(b), CompatXS::forward<F>(f))

		else std::for_each(CompatXS::forward<It>(a), CompatXS::forward<It>(b), CompatXS::forward<F>(f));
	}

	// parallel_reduce: http://www.idryman.org/blog/2012/08/05/grand-central-dispatch-vs-openmp/ and https://gist.github.com/m0wfo/1101546
	// __gnu_parallel:: transform, then accumulate
	// parallel_scan: GCD?
	// __gnu_parallel:: prefix_sum
	// See also thrust, etc.
}