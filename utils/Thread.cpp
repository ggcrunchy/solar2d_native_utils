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
	using map_type = std::map<size_t, POD>;

	//
	Slot::Slot (void) : mIndex(kInvalidID)
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

		memset(&mData, 0, sizeof(POD));
	}

	void Slot::GetItem (POD & pod)
	{
		map_type * tls = static_cast<map_type *>(pthread_getspecific(tls_key));

		if (tls)
		{
			auto iter = tls->find(mIndex);

			if (iter != tls->end()) pod = iter->second;

			else pod = mData;
		}
		
		else pod = mData;
	}

	void Slot::SetItem (const POD & pod)
	{
		if (mIndex != kInvalidID)
		{
			map_type * tls = static_cast<map_type *>(pthread_getspecific(tls_key));

			if (!tls)
			{
				tls = new map_type;

				pthread_setspecific(tls_key, tls);
			}

			(*tls)[mIndex] = pod;
		}

		else mData = pod;
	}

	void Slot::Sync (void)
	{
		mIndex = sID++;
	}
}