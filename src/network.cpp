/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
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

#include "network_impl.h"
#include <cubez/common.h>

#ifdef __COMPILE_AS_WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>

WSADATA wsa_data;
#endif

#include <iostream>


void network_initialize() {
#ifdef __COMPILE_AS_WINDOWS__
  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != NO_ERROR) {
    std::cout << "WSAStartup failed with error:" << result << std::endl;
    return;
  }
#endif
}

void network_shutdown() {
#ifdef __COMPILE_AS_WINDOWS__
  WSACleanup();
#endif
}