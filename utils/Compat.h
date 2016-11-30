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

#ifdef __APPLE__
    #include "TargetConditionals.h"
#endif

#if !TARGET_OS_IPHONE
    #include <type_traits>
	#include <utility>
#else
    #include <tr1/type_traits>
#endif

#ifdef _WIN32
	#include <yvals.h>
#else
	#define _NOEXCEPT noexcept
#endif

namespace CompatXS {
//
#if !TARGET_OS_IPHONE
	//
	using std::conditional;
	using std::forward;

	//
	template<typename T> struct NoThrowTraits {
		typedef std::is_nothrow_default_constructible<T> is_default_constructible;
	};

	//
	template<typename T> struct TrivialTraits {
		typedef std::is_trivially_copyable<T> is_copyable;
		typedef std::is_trivially_destructible<T> is_destructible;
	};

	//
	namespace ns_compat = std;
#else
	template<typename T> struct NoThrowTraits {
		typedef std::tr1::has_nothrow_default_constructor<T> is_default_constructible;
	};

	//
	template<typename T> struct TrivialTraits {
		typedef std::tr1::has_trivial_copy<T> is_copyable;
		typedef std::tr1::has_trivial_destructor<T> is_destructible;
	};

	template<typename T> T && forward (T value) { return static_cast<T &&>(value); }

	template<bool B, typename T, typename F> struct conditional {
		typedef T type;
	};

	template<typename T, typename F> struct conditional<false, T, F> {
		typedef F type;
	};

	//
	namespace ns_compat = std::tr1;
#endif
}