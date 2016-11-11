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

#if TARGET_OS_IOS
	#include <vector>
	#include <pthread.h>

	namespace TLSXS {
		static std::vector<POD> * GetData (pthread_key_t key)
		{
			return static_cast<std::vector<POD> *>(pthread_getspecific(key));
		}

		static pthread_key_t tls_key;

		size_t GetSlot (void)
		{
			static pthread_once_t tls_key_once = PTHREAD_ONCE_INIT;

			pthread_once(&tls_key_once, []()
			{
				pthread_key_create(&tls_key, [](void * data)
				{
					if (data) delete static_cast<std::vector<POD> *>(data);
				});

				atexit([]() { pthread_key_delete(tls_key); });
			});

			auto vec = GetData(tls_key);

			if (!vec)
			{
				vec = new std::vector<POD>;

				pthread_setspecific(tls_key, vec);
			}

			POD pod;

			memset(&pod, 0, sizeof(POD));

			vec->push_back(pod);

			return vec->size() - 1;
		}

		void GetItemInSlot (size_t slot, POD & pod)
		{
			pod = GetData(tls_key)->at(slot);
		}

		void SetItemInSlot (size_t slot, const POD & pod)
		{
			GetData(tls_key)->at(slot) = pod;
		}
	}
#endif