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

#include "utils/SIMD.h"
#include "external/DirectXMath.h"
#include "external/DirectXPackedVector.h"

#ifdef __APPLE__
	#include "TargetConditionals.h"
#endif

#ifdef __ANDROID__
	#include <cpu-features.h>
#endif

#if TARGET_OS_IPHONE
	#include <stdlib.h>
#else
	#include <memory>
#endif

namespace SimdXS {
	bool CanUseNeon (void)
	{
	#ifdef _WIN32
		return false;
	#elif __APPLE__
		#if !TARGET_OS_SIMULATOR && (TARGET_OS_IOS || TARGET_OS_TV)
			return true;
		#else
			return false;
		#endif
	#elif __ANDROID__
		static struct UsingNeon {
			bool mUsing;

			UsingNeon (void)
			{
				mUsing = android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM && (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
			}
		} sNeon;

		return sNeon.mUsing;
	#endif
	}

	void * Align (size_t bound, size_t size, void *& ptr, size_t * space)
	{
		size_t cushion = size + bound - 1U;
		size_t & space_ref = space ? *space : cushion;

	#if TARGET_OS_IPHONE
		uintptr_t p = uintptr_t(ptr);
		uintptr_t q = (p + align - 1) & -align;
		uintptr_t diff = q - p;

		if (space_ref - diff < size) return nullptr;
    
		if (space) *space -= diff;

		ptr = static_cast<unsigned char *>(ptr) + q - p;
    
		return ptr;
	#else
		return std::align(bound, size, ptr, space_ref);
	#endif
	}

	void FloatsToUnorm8s (const float * pfloats, unsigned char * u8, size_t n)
	{
		void * ptr = const_cast<float *>(pfloats);

		Align(16U, n * sizeof(float), ptr);

		DirectX::PackedVector::XMUBYTEN4 out;

		// Peel off any leading floats.
		if (pfloats != ptr)
		{
			size_t extra = static_cast<const float *>(ptr) - pfloats;
			DirectX::XMVECTORF32 padding = { 0 };

			for (size_t i = 0; i < extra; ++i, --n) padding.f[4U - extra + i] = pfloats[i];

			DirectX::PackedVector::XMStoreUByteN4(&out, padding.v);

			memcpy(u8, reinterpret_cast<unsigned char *>(&out) + 4U - extra, extra);

			u8 += extra;
		}

		// Blast through the aligned region.
		auto from = reinterpret_cast<const DirectX::XMVECTOR *>(ptr);

		for (; n >= 4U; u8 += 4U, n-= 4U) DirectX::PackedVector::XMStoreUByteN4(reinterpret_cast<DirectX::PackedVector::XMUBYTEN4 *>(u8), *from++);

		// Peel off any trailing floats.
		if (n)
		{
			DirectX::XMVECTORF32 padding = { 0 };

			memcpy(padding.f, from, n * sizeof(float));

			DirectX::PackedVector::XMStoreUByteN4(&out, padding.v);

			memcpy(u8, &out, n);
		}
	}

	void Unorm8sToFloats (const unsigned char * u8, float * pfloats, size_t n)
	{
		void * ptr = pfloats;

		Align(16U, n * sizeof(float), ptr);

		// Peel off any leading floats.
		if (pfloats != ptr)
		{
			size_t extra = static_cast<float *>(ptr) - pfloats;
			uint8_t padding[4] = { 0 };

			for (size_t i = 0; i < extra; ++i, --n) padding[4U - extra + i] = *u8++;
			
			DirectX::PackedVector::XMUBYTEN4 bytes{padding};

			auto result = DirectX::PackedVector::XMLoadUByteN4(&bytes);

			memcpy(pfloats - extra, reinterpret_cast<float *>(&result) + 4U - extra, extra * sizeof(float));
		}

		// Blast through the aligned region.
		auto to = reinterpret_cast<DirectX::XMVECTOR *>(ptr);

		for (; n >= 4U; u8 += 4U, n-= 4U)
		{
			DirectX::PackedVector::XMUBYTEN4 bytes{u8};

			*to++ = DirectX::PackedVector::XMLoadUByteN4(&bytes);
		}

		// Peel off any trailing floats.
		if (n)
		{
			uint8_t padding[4] = { 0 };

			for (size_t i = 0; i < n; ++i) padding[i] = u8[i];

			DirectX::PackedVector::XMUBYTEN4 bytes{padding};

			auto result = DirectX::PackedVector::XMLoadUByteN4(&bytes);

			memcpy(to, &result, n * sizeof(float));
		}
	}
}