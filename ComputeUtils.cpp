#include "ComputeUtils.h"
#include "LuaUtils.h"
#include <pthread.h>

#ifdef _WIN32
	#include <Windows.h>
	#include <delayimp.h>

	bool TryToLoad (const char * name)
	{
		return !FAILED(__HrLoadAllImportsForDll(name));
	}
#else
	#include <dlfcn.h>

	bool TryToLoad (const char * name);
#endif

//
struct ComputeDevices {
#ifdef WANT_AMP
	// best double-accelerator (if any)
	// best all-around (most dedicated memory, etc.)
#endif

#ifdef WANT_CUDA
//	COMPUTE_LIB mCUDA;	//
#endif

#ifdef WANT_METAL
//    MTLDevice mMetalDevice;
#endif
};

//
static bool Exists (const char * name)
{
#ifdef _WIN32
	HMODULE lib = LoadLibraryA(name);
#else
	void * lib = dlopen(name, RTLD_NOW);
#endif

	if (lib != NULL)
	{
#ifdef _WIN32
		FreeLibrary(lib);
#else
		dlclose(lib);
#endif
	}

	return lib != NULL;
}

// Global device info
static ComputeDevices * sDevices;

// Avoid conflicts while launching device queries
static pthread_mutex_t s_DevicesInit = PTHREAD_MUTEX_INITIALIZER;

#ifdef WANT_CUDA

#include <cuda.h>
//#include <cuda_runtime_api.h>

static bool CheckCUDA (ComputeCaps & caps)
{
	if (!TryToLoad(
#ifdef _WIN32
		"nvcuda.dll"
#else
		"libcuda.so"
#endif
	)) return false;

	int count;

    if (CUDA_SUCCESS != cuInit(0)) return false;
    if (CUDA_SUCCESS != cuDeviceGetCount(&count)) return false;

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
        if (CUDA_SUCCESS != cuDeviceComputeCapability(&dev.mMajor, &dev.mMinor, i)) return false;
printf("%i, %i, %i!\n", i, dev.mMajor, dev.mMinor);
		// ^^ TODO: Detect minimum acceptable version... (test on better device!)
    }

	return true;
}

#endif
//#include <iostream>
bool CheckComputeSupport (lua_State * L, ComputeCaps & caps)
{
	if (!IsMainState(L)) return false;

	caps.mFlags = 0;

	ComputeDevices * cd = (ComputeDevices *)lua_newuserdata(L, sizeof(ComputeDevices));	// ..., devices
	
	//
	#ifdef WANT_AMP
	printf("YES?\n");
		if (TryToLoad("vcamp120.dll"))
		{
			printf("AM\n");
			auto accs = concurrency::accelerator::get_all();

			accs.erase(std::remove_if(begin(accs), end(accs),
				[](concurrency::accelerator acc) { return acc.is_debug || acc.is_emulated; }
			), end(accs));

			for (auto acc : accs)
			{
			//	if (acc.has_display) wprintf(L"OH? %s\n", acc.description);
			}

			caps.mAccelerators = accs;

			if (!caps.mAccelerators.empty()) caps.mFlags |= ComputeCaps::eAMP; // Probably too liberal (e.g. emulated stuff unlikely to be much good)
		}
		else printf("NO :(\n");
	#endif

	#ifdef WANT_CUDA
		if (CheckCUDA(caps)) caps.mFlags |= ComputeCaps::eCUDA;
	#endif

	#ifdef WANT_METAL
    /*
		cd->mMetalDevice = MTLCreateSystemDefaultDevice();

		if (cd->mMetalDevice) caps.mFlags |= ComputeCaps::eMetal;
    */
	#endif

	AttachGC(L, "compute_devices", [](lua_State * L)
	{
		ShutDownBackend(ComputeCaps::eCUDA);
		ShutDownBackend(ComputeCaps::eMetal);
		ShutDownBackend(ComputeCaps::eOpenCL);
		ShutDownBackend(ComputeCaps::eRenderScript);

		return 0;
	});

	pthread_mutex_lock(&s_DevicesInit);

	sDevices = cd;

	pthread_mutex_unlock(&s_DevicesInit);

	return true;
}

void ShutDownBackend (ComputeCaps::Flag flag)
{
	pthread_mutex_lock(&s_DevicesInit);	// wait for any initialization
	pthread_mutex_unlock(&s_DevicesInit);

	if (!sDevices) return;

	switch (flag)
	{
	case ComputeCaps::eAMP:
	#ifdef WANT_AMP
		// TODO!
	#endif
		break;
	case ComputeCaps::eCUDA:
	#ifdef WANT_CUDA
		// TODO!
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
	}
}

#undef COMPUTE_LIB
#undef COMPUTE_PROC
#undef _CUDA_API_