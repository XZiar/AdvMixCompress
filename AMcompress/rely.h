#pragma once

#define DEBUG false
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS
#define ifAVX2 true
//#define  NUM_THREAD 4


//#include <omp.h> 
//#include <io.h>
//#include <fcntl.h>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <iostream>
#include <string>
#include <cstring>
#include <corecrt_wstring.h>
#include <vector>
#include <time.h>
#include <Windows.h>

#if defined(__GNUC__)
	#include <x86intrin.h>
	#define _MM_ALIGN16 _CRT_ALIGN(16)
	#define malloc_align(size, align) memalign((align), (size))
	#define free_align(ptr) free(ptr)
#else
	#include <intrin.h>
	#define malloc_align(size, align) _aligned_malloc((size), (align))
	#define free_align(ptr) _aligned_free(ptr)
#endif

#if DEBUG
	#define DEBUG_Thr false
	#define DEBUG_Com true
#else
	#define DEBUG_Thr false
	#define DEBUG_Com false
#endif

using namespace std;

