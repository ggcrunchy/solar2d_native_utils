#include "ComputeUtils.h"
#include "LuaUtils.h"
#include <pthread.h>

#ifdef _WIN32
	#include <windows.h>

	#define COMPUTE_LIB HMODULE
	#define _CUDA_API_
#else
	#include <dlfcn.h>
	#define COMPUTE_LIB void *

	#define _CUDA_API_ CUDAAPI
#endif

// The following functions, along with CUDA support, adapted from: http://stackoverflow.com/a/12854285

//
COMPUTE_LIB LoadLib (const char * name)
{
#ifdef _WIN32
    return LoadLibraryA(name);
#else
    return dlopen (name, RTLD_NOW);
#endif
}

//
void (*LookupProc (COMPUTE_LIB lib, const char * name)) (void)
{
#ifdef _WIN32
    return (void (*)(void)) GetProcAddress(lib, name);
#else
    return (void (*)(void)) dlsym(lib, (const char *)name);
#endif
}

//
int FreeLib (COMPUTE_LIB lib)
{
#ifdef _WIN32
    return FreeLibrary(lib);
#else
    return dlclose(lib);
#endif
}

//
struct ComputeDevices {
#ifdef WANT_CUDA
	COMPUTE_LIB mCUDA;	//
#endif

#ifdef WANT_METAL
//    MTLDevice mMetalDevice;
#endif
};

// Global device info
static ComputeDevices * sDevices;

// Avoid conflicts while launching device queries
static pthread_mutex_t s_DevicesInit = PTHREAD_MUTEX_INITIALIZER;

#ifdef WANT_CUDA

#include <cuda.h>

typedef CUresult _CUDA_API_ (*cuInit_pt)(unsigned int Flags);
typedef CUresult _CUDA_API_ (*cuDeviceGetCount_pt)(int * count);
typedef CUresult _CUDA_API_ (*cuDeviceComputeCapability_pt)(int * major, int * minor, CUdevice dev);

static std::nullptr_t Fail (COMPUTE_LIB lib)
{
	if (lib) FreeLib(lib);

	return nullptr;
}

static COMPUTE_LIB CheckCUDA (ComputeCaps & caps)
{
#ifdef _WIN32
	const char * name = "nvcuda.dll";
#else
    const char * name = "libcuda.so";
#endif

    COMPUTE_LIB cuLib;

    cuInit_pt my_cuInit = NULL;
    cuDeviceGetCount_pt my_cuDeviceGetCount = NULL;
    cuDeviceComputeCapability_pt my_cuDeviceComputeCapability = NULL;

    if ((cuLib = LoadLib(name)) == NULL)
        return NULL; // cuda library is not present in the system

    if ((my_cuInit = (cuInit_pt) LookupProc(cuLib, "cuInit")) == NULL) return Fail(cuLib);
    if ((my_cuDeviceGetCount = (cuDeviceGetCount_pt) LookupProc(cuLib, "cuDeviceGetCount")) == NULL) return Fail(cuLib);
    if ((my_cuDeviceComputeCapability = (cuDeviceComputeCapability_pt) LookupProc(cuLib, "cuDeviceComputeCapability")) == NULL) return Fail(cuLib);

	int count;

    if (CUDA_SUCCESS != my_cuInit(0)) return Fail(cuLib);
    if (CUDA_SUCCESS != my_cuDeviceGetCount(&count)) return Fail(cuLib);

	caps.mDevicesCUDA.resize(size_t(count));

    for (int i = 0; i < count; i++)
    {
        auto dev = caps.mDevicesCUDA[i];

        if (CUDA_SUCCESS != my_cuDeviceComputeCapability(&dev.mMajor, &dev.mMinor, i)) Fail(cuLib);
    }

	return cuLib;
}

#endif

bool CheckComputeSupport (lua_State * L, ComputeCaps & caps)
{
	if (!IsMainState(L)) return false;

	caps.mFlags = 0;

	ComputeDevices * cd = (ComputeDevices *)lua_newuserdata(L, sizeof(ComputeDevices));	// ..., devices

	#ifdef WANT_CUDA
		cd->mCUDA = CheckCUDA(caps);

		if (cd->mCUDA) caps.mFlags |= ComputeCaps::eCUDA;
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
	case ComputeCaps::eCUDA:
	#ifdef WANT_CUDA
		if (sDevices->mCUDA) FreeLib(sDevices->mCUDA);

		sDevices->mCUDA = NULL;
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
	#endif
		break;
	case ComputeCaps::eRenderScript:
	#ifdef WANT_RENDERSCRIPT
		// TODO!
	#endif
		break;
	}
}

#undef COMPUTE_LIB
#undef _CUDA_API_