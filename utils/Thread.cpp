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

#include "utils/Thread.h"

#ifdef __APPLE__
	#include "TargetCondtionals.h"
#endif

#include <atomic>
#include <limits>
#include <map>
#include <pthread.h>

namespace ThreadXS {
	//
	const size_t kInvalidID = (std::numeric_limits<size_t>::max)();

	// 
	static std::atomic<size_t> sID(0U);

	//
	static pthread_key_t tls_key;

	//
	using map_type = std::map<size_t, var_storage>;

	//
	Slot::Slot (size_t size) : mData(size, 0), mIndex(kInvalidID)
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
	}

	void Slot::GetVar (void * var)
	{
		map_type * tls = static_cast<map_type *>(pthread_getspecific(tls_key));

		if (tls)
		{
			auto iter = tls->find(mIndex);

			if (iter != tls->end()) memcpy(var, iter->second.data(), mData.size());

			else memcpy(var, mData.data(), mData.size());
		}
		
		else memcpy(var, mData.data(), mData.size());
	}

	void Slot::SetVar (const void * var)
	{
		auto bytes = static_cast<const unsigned char *>(var);

		if (mIndex != kInvalidID)
		{
			map_type * tls = static_cast<map_type *>(pthread_getspecific(tls_key));

			if (!tls)
			{
				tls = new map_type;

				pthread_setspecific(tls_key, tls);
			}

			(*tls)[mIndex].assign(bytes, bytes + mData.size());
		}

		else mData.assign(bytes, bytes + mData.size());
	}

	void Slot::Sync (void)
	{
		mIndex = sID++;
	}
}