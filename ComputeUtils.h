#ifndef COMPUTE_UTILS_H
#define COMPUTE_UTILS_H

#include "CoronaLua.h"
#include <vector>

#ifdef _WIN32
	#ifndef IGNORE_CUDA
		#define WANT_CUDA
	#endif

	#ifndef IGNORE_OPENCL
		#define WANT_OPENCL
	#endif
#elif __ANDROID__
	// TODO: CUDA, RenderScript
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
	typedef enum { eCUDA, eMetal, eOpenCL, eRenderScript } Flag;

	unsigned int mFlags;// Which backends are supported?

#ifdef WANT_CUDA
	struct DeviceInfoCUDA {
		int mMajor, mMinor;
	};

	std::vector<DeviceInfoCUDA> mDevicesCUDA;	// List of devices (index = device id) along with CUDA driver info
#endif
};

bool CheckComputeSupport (lua_State * L, ComputeCaps & caps);
void ShutDownBackend (ComputeCaps::Flag flag);

#ifdef WANT_CUDA
#include <cuda.h>
#endif

#ifdef WANT_METAL
#include <metal.h> // ?

id<MTLDevice> GetMetalDevice ();
#endif

#ifdef WANT_OPENCL
// TODO: OpenCL!
#endif

#ifdef WANT_RENDERSCRIPT
	// TODO!
#endif

#endif