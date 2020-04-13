/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*
* Compiler-provided Thread-Local Storage
* TLS declarations for various compilers:
* http://en.wikipedia.org/wiki/Thread-local_storage
*/

#ifndef __TLS_H__
#define __TLS_H__

/* Each #ifdef must define
* THREAD_LOCAL
* EXPORT
*/

#ifdef _MSC_VER

#define THREAD_LOCAL __declspec(thread)
#define EXPORT __declspec(dllexport)

#else
/* assume gcc or compatible compiler */
#define THREAD_LOCAL __thread
#define EXPORT

#endif /* _MSC_VER */

#endif /*__TLS_H__*/
