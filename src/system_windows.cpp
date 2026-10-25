// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "system.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdlib>

#include <windows.h>

namespace javelin::system {

void setThreadName(const char* threadName) {
    wchar_t buffer[256];
    size_t numConverted;
    mbstowcs_s(&numConverted, buffer, threadName, 256);
    ::SetThreadDescription(::GetCurrentThread(), buffer);
}

} // namespace javelin::system
