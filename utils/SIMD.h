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

#include "external/aligned_allocator.h"
#include <utility>
#include <vector>


namespace SimdXS {
	bool CanUseNeon (void);

	#ifdef _MSC_VER
		#define ALIGNED16 __declspec(align(16))
	#else
		#define ALIGNED16 __attribute__((aligned(16)))
	#endif

	//
	template<typename T> struct AlignedVector16 : std::vector<T, simdpp::SIMDPP_ARCH_NAMESPACE::aligned_allocator<T, 16U>>
	{
		template<typename ... Args> AlignedVector16 (Args && ... args) : std::vector<T, allocator_type>(std::forward<Args>(args)...)
		{
		}
	};

	void * Align (size_t bound, size_t size, void *& ptr, size_t * space = nullptr);

	void FloatsToUnorm8s (const float * pfloats, unsigned char * u8, size_t n);
	void Unorm8sToFloats (const unsigned char * u8, float * pfloats, size_t n);
}