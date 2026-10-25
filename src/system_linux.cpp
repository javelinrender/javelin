// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "system.h"

#include <pthread.h>

#include <cstring>

namespace javelin::system {

void setThreadName(const char* threadName) {
    const std::size_t size = std::strlen(threadName);
    // pthread_setname_np() only allows names up to 16 characters, including the
    // null terminator.
    if (size <= 15) {
        ::pthread_setname_np(pthread_self(), threadName);
    } else {
        char buffer[16];
        std::memcpy(buffer, threadName, 15);
        buffer[15] = '\0';
        ::pthread_setname_np(pthread_self(), buffer);
    }
}

} // namespace javelin::system
