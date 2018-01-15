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

#include <cstddef>

#ifdef __APPLE__
    #include "TargetConditionals.h"
#endif

#if 1//!TARGET_OS_IOS
    #include <array>
	#include <atomic>
    #include <functional>
	#include <future>
    #include <memory>
	#include <mutex>
    #include <type_traits>
    #include <tuple>
	#include <utility>

    #define COMPAT_ATOMIC_FLAG_INIT() ATOMIC_FLAG_INIT
    #define ENABLE_IF_VALUE(t) t::value
#else
    #include <boost/core/enable_if.hpp>
    #include <boost/move/unique_ptr.hpp>
    #include <boost/move/utility.hpp>
    #include <boost/function.hpp>
    #include <boost/type_traits.hpp>
    #include <boost/tuple/tuple.hpp>
    #include <boost/tuple/tuple_comparison.hpp>

    #define BOOST_NO_CXX11_RVALUE_REFERENCES // ????!!!

    #include <boost/thread.hpp>

    #undef BOOST_NO_CXX11_RVALUE_REFERENCES

    #define COMPAT_ATOMIC_FLAG_INIT() BOOST_ATOMIC_FLAG_INIT
    #define ENABLE_IF_VALUE(t) t
#endif

#ifdef _WIN32
	#include <yvals.h>

	#define _RESTRICT __restrict
#else
	#define _NOEXCEPT noexcept
	#define _RESTRICT __restrict__
#endif

namespace CompatXS {

// On most targets we have a fairly full-featured C++11 implementation...
#if 1//!TARGET_OS_IOS
	// Bring these into the namespace...
    using std::max_align_t;

	// ...give these a common name, slimming them down slightly to avoid redundancy from the struct...
	template<typename T> struct NoThrowTraits {
		typedef std::is_nothrow_default_constructible<T> is_default_constructible;
	};

	// ...ditto...
	template<typename T> struct TrivialTraits {
		typedef std::is_trivially_copyable<T> is_copyable;
		typedef std::is_trivially_destructible<T> is_destructible;
	};

	// ...and streamline this namespace.
	namespace ns_compat = std;

// ...whereas on iPhone we must use libstdc++ 6, which is likewise or still has many things in TR1.
#else
    using ::max_align_t;

	// As with other targets, but here we need to bring the alternate names into conformity...
	template<typename T> struct NoThrowTraits {
		typedef boost::has_nothrow_constructor<T> is_default_constructible; // Not quite the trait we want, but see note for forward()
	};

	// ...ditto...
	template<typename T> struct TrivialTraits {
		typedef boost::has_trivial_copy<T> is_copyable;
		typedef boost::has_trivial_destructor<T> is_destructible;
	};

	// ...and streamline this namespace.
	namespace ns_compat = boost;
#endif
}
