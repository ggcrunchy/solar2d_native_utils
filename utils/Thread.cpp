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

#include "utils/Compat.h"
#include "utils/Thread.h"
#include <map>
#include <pthread.h>

#if TARGET_OS_IOS
    static pthread_mutex_t sMutexID = PTHREAD_MUTEX_INITIALIZER;
#endif

namespace ThreadXS {
	//
	using map_type = std::map<size_t, Slot::storage_type>;

	//
	static pthread_key_t tls_key;

	//
	void Slot::Init (void)
	{
		static struct KeyLifetime {
			KeyLifetime (void)
			{
				pthread_key_create(&tls_key, [](void * data)
				{
					if (data) delete static_cast<map_type *>(data);
				});
			}

			~KeyLifetime (void)
			{
				pthread_key_delete(tls_key);
			}
		} sKeyLifetime;

		//
    #if 0 //!TARGET_OS_IOS
		static CompatXS::atomic<size_t> sID{0U};
    #else
        static size_t sID = 0U;

        pthread_mutex_lock(&sMutexID);
    #endif
		mIndex = sID++;
    #if TARGET_OS_IOS
        pthread_mutex_unlock(&sMutexID);
    #endif
	}

	//
	Slot::Slot (size_t size) : mData(size, 0)
	{
		Init();
	}

	//
	static void Assign (Slot::storage_type & storage, const void * var, size_t size)
	{
		auto bytes = static_cast<const unsigned char *>(var);

		storage.assign(bytes, bytes + size);
	}

	//
	Slot::Slot (size_t size, const void * var) : mData(size)
	{
		Init();
		Assign(mData, var, size);
	}

	//
	void Slot::GetVar (void * var)
	{
		map_type * tls = static_cast<map_type *>(pthread_getspecific(tls_key));
		auto source = &mData;

		if (tls)
		{
			auto iter = tls->find(mIndex);

			if (iter != tls->end()) source = &iter->second;
		}
		
		memcpy(var, source->data(), mData.size());
	}

	//
	void Slot::SetVar (const void * var)
	{
		map_type * tls = static_cast<map_type *>(pthread_getspecific(tls_key));

		if (!tls)
		{
			tls = new map_type;

			pthread_setspecific(tls_key, tls);
		}

		Assign((*tls)[mIndex], var, mData.size());
	}
}