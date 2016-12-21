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

#if !TARGET_OS_IOS
	#include <atomic>
    #include <functional>
	#include <future>
    #include <memory>
	#include <mutex>
    #include <type_traits>
    #include <tuple>
	#include <utility>
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
#if !TARGET_OS_IOS
	// Bring these into the namespace...
    using std::async; // maybe remove (just use libdispatch on iOS)
	using std::atomic;
	using std::atomic_flag;
	using std::conditional;
    using std::enable_if;
	using std::forward;
    using std::function;
    using std::get;
	using std::lock_guard;
    using std::make_shared;
    using std::max_align_t;
    using std::move;
	using std::mutex;
    using std::shared_ptr;
    using std::tuple;
    using std::unique_ptr; // ???? (lots of trouble :/)

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
    using boost::async; // maybe remove
    using boost::atomic;
    using boost::atomic_flag;
    using boost::conditional;
    using boost::enable_if;
    using boost::forward;
    using boost::function;
    using boost::tuples::get;
    using boost::lock_guard;
    using boost::make_shared;
    using ::max_align_t;
    using boost::move;
    using boost::mutex;
    using boost::shared_ptr;
    using boost::tuple;
    using boost::movelib::unique_ptr; // :/

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