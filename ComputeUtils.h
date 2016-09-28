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

#ifndef COMPUTE_UTILS_H
#define COMPUTE_UTILS_H

#include "CoronaLua.h"
#include <vector>

#ifdef _WIN32
	#ifndef IGNORE_AMP
		#include <amp.h>

		#define WANT_AMP
	#endif

	#ifndef IGNORE_CUDA
		#define WANT_CUDA
	#endif

	#ifndef IGNORE_OPENCL
		#define WANT_OPENCL
	#endif
#elif __ANDROID__
	// TODO: CUDA, OpenCL, RenderScript
	// VULKAN?
#elif __APPLE__
	#include "TargetConditionals.h"

	#ifndef IGNORE_METAL
		#define WANT_METAL
	#endif

	#if !TARGET_OS_IPHONE && !TARGET_OS_TV
		#ifndef IGNORE_CUDA
			#define WANT_CUDA
		#endif

		#ifndef IGNORE_OPENCL
			#define WANT_OPENCL
		#endif
	#endif
#endif

struct ComputeCaps {
	typedef enum { eAMP, eCUDA, eMetal, eOpenCL, eRenderScript, eVULKAN } Flag;

	unsigned int mSupported;// Which backends are supported?
	unsigned int mEnabled;	// Which backends are enabled? (i.e. supported and not shut down)

#ifdef WANT_AMP
	std::vector<concurrency::accelerator> mAccelerators;// List of AMP accelerators
#endif

#ifdef WANT_CUDA
	struct DeviceInfoCUDA {
		int mMajor, mMinor;
	};

	std::vector<DeviceInfoCUDA> mDevicesCUDA;	// List of devices (index = device id) along with CUDA driver info
#endif

	ComputeCaps (void) : mSupported(0), mEnabled(0) {}
};

bool CheckComputeSupport (lua_State * L);
bool GetComputeCaps (lua_State * L, ComputeCaps & caps);
void ShutDownBackend (ComputeCaps::Flag flag);

#ifdef WANT_CUDA
#include <cuda.h>
#endif

#ifdef WANT_METAL
//#include <metal.h> // apparently want to do this from a .mm file?

//id<MTLDevice> GetMetalDevice ();
#endif

#ifdef WANT_OPENCL
// TODO: OpenCL!
#endif

#ifdef WANT_RENDERSCRIPT
	// TODO!
#endif

#ifdef WANT_VULKAN
	// TODO!
#endif

#endif