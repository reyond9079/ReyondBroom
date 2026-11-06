// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef _WIN32
#define RB_COMPILE_EXPORT extern "C" __declspec(dllexport)
#else
#define RB_COMPILE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
// C ABI is intentional: Windows host is MinGW, Qt can use the official MSVC kit.
// Request and reply are UTF-8 JSON. Reply storage belongs to the host and remains
// valid until the next request. All calls occur on the editor's main thread.
using ReyondCompileRequest = const char* (*)(const char*);
RB_COMPILE_EXPORT int reyondCompileInitialize(ReyondCompileRequest,const char* resources);
RB_COMPILE_EXPORT void reyondCompileShow(int mode);
RB_COMPILE_EXPORT void reyondCompileTick();
RB_COMPILE_EXPORT int reyondCompileVisible();
RB_COMPILE_EXPORT void reyondCompileShutdown();
