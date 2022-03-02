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

#include "utils/Namespace.h"

#ifdef __APPLE__
	#include "TargetConditionals.h"

    #if TARGET_OS_MAC
        #if !defined(__x86_64__)
            #define IS_APPLE_SILICON
        #endif
    #endif
#endif

CEU_BEGIN_NAMESPACE(PlatformXS) {
	#ifdef __ANDROID__
        const bool is_android = true;
    
        #if defined(__ARM_NEON)
            const bool neon_defined = true;
        #else
            const bool neon_defined = false;
        #endif
    #else
		const bool is_android = false;
        const bool neon_defined = false;
    #endif

	#ifdef __APPLE__
		const bool is_apple = true;
	#else
		const bool is_apple = false;
	#endif

	#if TARGET_OS_IPHONE
		const bool is_iphone = true;

		#if TARGET_OS_IOS
			const bool is_ios = true;
		#elif TARGET_OS_TV
			const bool is_ios = false;
		#else
			#error "Unsupported iPhone target"
		#endif

		const bool is_tvos = !is_ios;

		#if TARGET_OS_SIMULATOR
			const bool is_iphone_simulator = true;
		#else
			const bool is_iphone_simulator = false;
		#endif
	#else
		const bool is_iphone = false;
		const bool is_ios = false;
		const bool is_tvos = false;
		const bool is_iphone_simulator = false;
	#endif

	#ifdef _WIN32
		const bool is_win32 = true;
	#else
		const bool is_win32 = false;
	#endif
	#if TARGET_CPU_ARM64 && TARGET_OS_MAC
		const bool is_apple_silicon = true;
	#else
		const bool is_apple_silicon = false;
	#endif

    const bool has_accelerate = is_apple;// && !is_ios;
	const bool has_neon = (is_iphone && !is_iphone_simulator) || is_apple_silicon;
    const bool might_have_neon = (is_android && neon_defined) || has_neon;
CEU_END_NAMESPACE(PlatformXS)

//
#if defined(__APPLE__) && !TARGET_OS_IOS
    static_assert(PlatformXS::has_accelerate, "Broken Accelerate test");

    #include <Accelerate/Accelerate.h>

    #define HAS_ACCELERATE
#endif

//
#if (defined(__ANDROID__) && defined(__ARM_NEON)) || (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) || (TARGET_CPU_ARM64 && TARGET_OS_MAC)
    static_assert(PlatformXS::might_have_neon, "Broken NEON test");

    #include <arm_neon.h>

    #define MIGHT_HAVE_NEON
#endif
