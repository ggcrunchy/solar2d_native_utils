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

#include "utils/Compute.h"
#include "utils/Lua.h"
#include "utils/Path.h"
#include <pthread.h>

//
struct ComputeDevices {
#ifdef WANT_AMP
	LibLoader mLibAMP;	// C++ AMP dynamic library
	// best double-accelerator (if any)
	// best all-around (most dedicated memory, etc.)
#endif

#ifdef WANT_CUDA
	LibLoader mLibCUDA;	// CUDA dynamic library
#endif

#ifdef WANT_METAL
//    MTLDevice mMetalDevice;
#endif
	ComputeCaps mCaps;	// Various capabilities
};

// Global device info
static ComputeDevices * sDevices;

// Avoid conflicts while launching device queries
static pthread_mutex_t s_DevicesInit = PTHREAD_MUTEX_INITIALIZER;

#ifdef WANT_AMP

static bool CheckAMP (LibLoader & amp_lib, ComputeCaps & caps)
{
	amp_lib.Load("vcamp120.dll");

	if (amp_lib.LateLoad())
	{
		auto accs = concurrency::accelerator::get_all();

		accs.erase(std::remove_if(begin(accs), end(accs),
			[](concurrency::accelerator acc) { return acc.is_debug || acc.is_emulated; }
		), end(accs));

		for (auto acc : accs)
		{
		//	if (acc.has_display) wprintf(L"OH? %s\n", acc.description);
		}

		caps.mAccelerators = accs;

		if (!caps.mAccelerators.empty()) return true; // Probably too liberal (e.g. emulated stuff unlikely to be much good)
	}

	amp_lib.Close();

	return false;
}

#endif

#ifdef WANT_CUDA

#include <cuda.h>
//#include <cuda_runtime_api.h>

static bool CheckCUDA (LibLoader & cuda_lib, ComputeCaps & caps)
{
	cuda_lib.Load(
	#ifdef _WIN32
		"nvcuda.dll"
	#elif __APPLE__ // see ComputeUtils.h, which currently filters out iPhone and tvOS
		"libcuda.dylib"
	#else
		"libcuda.so"
	#endif
	);

	if (!cuda_lib.IsLoaded()) return false;

	#define CUDA_BIND(name, ...) CUresult (CUDAAPI *name)(__VA_ARGS__); LIB_BIND(cuda_lib, cu, name)
	#define CUDA_CALL(name, ...) if (!name || CUDA_SUCCESS != name(__VA_ARGS__)) return false

	CUDA_BIND(Init, unsigned int);
	CUDA_BIND(DeviceGetCount, int *);
	CUDA_BIND(DeviceComputeCapability, int *, int *, CUdevice);

	int count;

    CUDA_CALL(Init, 0);
    CUDA_CALL(DeviceGetCount, &count);

	caps.mDevicesCUDA.resize(size_t(count));

    for (int i = 0; i < count; i++)
    {
        ComputeCaps::DeviceInfoCUDA & dev = caps.mDevicesCUDA[i];
/*
struct cudaDeviceProp deviceProp;
if(cudaGetDeviceProperties(&deviceProp,i) == CUDA_SUCCESS)

  {
	  int m,n;
	 cuDeviceComputeCapability(&m, &n, i);

	 unsigned long memory = (unsigned long)deviceProp.totalGlobalMem;

	 memory = memory / 1024;

	 memory = memory / 1024;

	 printf("Device Name: %s", deviceProp.name);

	 printf("Device Computer Capability: %d.%d", m, n);

	 printf("\nMem Size: %lu \n", memory);

	 printf("Max Threads: %d\n", deviceProp.maxThreadsPerBlock);

	 printf("Max Threads Dim[x?]: %d\n", deviceProp.maxThreadsDim[0]);

	 printf("Max Threads Dim[y?]: %d\n", deviceProp.maxThreadsDim[1]);

	 printf("Max Threads Dim[z?]: %d\n", deviceProp.maxThreadsDim[2]);

	 printf("Max Grid Size[x?]: %d\n", deviceProp.maxGridSize[0]);

	 printf("Max Grid Size[y?]: %d\n", deviceProp.maxGridSize[1]);

	 printf("Max Grid Size[z?]: %d\n", deviceProp.maxGridSize[2]);

  }
  */
        CUDA_CALL(DeviceComputeCapability, &dev.mMajor, &dev.mMinor, i);
printf("%i, %i, %i!\n", i, dev.mMajor, dev.mMinor);
		// ^^ TODO: Detect minimum acceptable version... (test on better device!)

    }

	#undef CUDA_BIND
	#undef CUDA_CALL

	return true;
}

#endif

bool CheckComputeSupport (lua_State * L)
{
	if (!IsMainState(L)) return false;

	ComputeDevices * cd = (ComputeDevices *)lua_newuserdata(L, sizeof(ComputeDevices));	// ..., devices
	
	new (cd) ComputeDevices;

#ifdef WANT_AMP
	if (CheckAMP(cd->mLibAMP, cd->mCaps)) cd->mCaps.mSupported |= ComputeCaps::eAMP;
#endif

#ifdef WANT_CUDA
	if (CheckCUDA(cd->mLibCUDA, cd->mCaps)) cd->mCaps.mSupported |= ComputeCaps::eCUDA;
#endif

#ifdef WANT_METAL
/*
	cd->mMetalDevice = MTLCreateSystemDefaultDevice();

	if (cd->mMetalDevice) caps.mFlags |= ComputeCaps::eMetal;
*/
#endif

	cd->mCaps.mEnabled = cd->mCaps.mSupported;

	AttachGC(L, "compute_devices", [](lua_State * L)
	{
		ShutDownBackend(ComputeCaps::eAMP);
		ShutDownBackend(ComputeCaps::eCUDA);
		ShutDownBackend(ComputeCaps::eMetal);
		ShutDownBackend(ComputeCaps::eOpenCL);
		ShutDownBackend(ComputeCaps::eRenderScript);
		ShutDownBackend(ComputeCaps::eVULKAN);

		return 0;
	});

	pthread_mutex_lock(&s_DevicesInit);

	sDevices = cd;

	pthread_mutex_unlock(&s_DevicesInit);

	return true;
}


bool GetComputeCaps (lua_State * L, ComputeCaps & caps)
{
	pthread_mutex_lock(&s_DevicesInit);	// wait for any initialization
	pthread_mutex_unlock(&s_DevicesInit);

	if (!sDevices) return false;

	caps = sDevices->mCaps;

	return true;
}

void ShutDownBackend (ComputeCaps::Flag flag)
{
	pthread_mutex_lock(&s_DevicesInit);	// wait for any initialization
	pthread_mutex_unlock(&s_DevicesInit);

	if (!sDevices) return;

	// TODO: This is not even remotely safe (can it be?), both thread-wise and just "pulling out the rug"-wise
	// Maybe all the WANT_* stuff should be read out of the preferences and thus start-up only?

	switch (flag)
	{
	case ComputeCaps::eAMP:
	#ifdef WANT_AMP
		sDevices->mLibAMP.Close();
	#endif
		break;
	case ComputeCaps::eCUDA:
	#ifdef WANT_CUDA
		sDevices->mLibCUDA.Close();
	#endif
		break;
	case ComputeCaps::eMetal:
	#ifdef WANT_METAL
	/*	if (mMetalDevice) ;

		mMetalDevice = NULL;*/
	#endif
		break;
	case ComputeCaps::eOpenCL:
	#ifdef WANT_OPENCL
		// TODO!
		// http://arrayfire.com/getting-started-with-opencl-on-android/
		// http://arrayfire.com/opencl-on-mobile-devices/
		// https://stackoverflow.com/questions/26795921/does-android-support-opencl
		// https://streamcomputing.eu/blog/2014-06-30/opencl-support-recent-android-smartphones/
	#endif
		break;
	case ComputeCaps::eRenderScript:
	#ifdef WANT_RENDERSCRIPT
		// TODO!
		// https://possiblemobile.com/2013/10/renderscript-for-all/
	#endif
		break;
	case ComputeCaps::eVULKAN:
	#ifdef WANT_VULKAN
		// TODO!
	#endif
		break;
	default:	// ????
		return;
	}

	sDevices->mCaps.mEnabled &= ~(1 << flag);
}