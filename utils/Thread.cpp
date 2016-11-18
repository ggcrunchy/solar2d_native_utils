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
#include <vector>
#include <pthread.h>

namespace ThreadXS {
	// 
	static std::atomic<Slot *> sHead;

	//
	static pthread_key_t tls_key;

	//
	Slot::Slot (void) : mNext(nullptr), mIndex((std::numeric_limits<size_t>::max)())
	{
		//
		static pthread_once_t tls_key_once = PTHREAD_ONCE_INIT;

		pthread_once(&tls_key_once, []()
		{
			pthread_key_create(&tls_key, [](void * data)
			{
				if (data) delete static_cast<std::vector<POD> *>(data);
			});

			atexit([]() { pthread_key_delete(tls_key); });
		});

		memset(&mData, 0, sizeof(POD));
	}

	void Slot::GetItem (POD & pod)
	{
		std::vector<POD> * tls = static_cast<std::vector<POD> *>(pthread_getspecific(tls_key));

		if (tls && mIndex < tls->size()) pod = tls->at(mIndex);
		
		else pod = mData;
	}

	void Slot::SetItem (const POD & pod)
	{
		if (mIndex != (std::numeric_limits<size_t>::max)())
		{
			std::vector<POD> * tls = static_cast<std::vector<POD> *>(pthread_getspecific(tls_key));

			if (!tls)
			{
				tls = new std::vector<POD>;

				pthread_setspecific(tls_key, tls);
			}

			size_t old_size = tls->size();

			if (mIndex >= old_size)
			{
				tls->resize(mIndex + 1);

				size_t offset = 1, count = mIndex - old_size;

				for (Slot * slot = mNext; offset < count; slot = slot->mNext, ++offset) tls->at(mIndex - offset) = slot->mData;
			}

			tls->at(mIndex) = pod;
		}

		else mData = pod;
	}

	void Slot::Sync (void)
	{
		// http://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange
		mNext = sHead.load(std::memory_order_relaxed);
 
		while (!sHead.compare_exchange_weak(mNext, this, std::memory_order_release, std::memory_order_relaxed));

		// Count the steps to the node. This will be the TLS position.
		mIndex = 0U;

		for (Slot * slot = mNext; slot; slot = slot->mNext) ++mIndex;
	}

	void Slot::RestoreValues (void)
	{
		std::vector<POD> * tls = static_cast<std::vector<POD> *>(pthread_getspecific(tls_key));

		if (tls)
		{
			delete tls;
			
			pthread_setspecific(tls_key, nullptr);
		}
	}
}