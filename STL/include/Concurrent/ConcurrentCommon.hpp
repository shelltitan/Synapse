#pragma once
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
#endif


namespace Synapse::STL::Concurrent {
    inline auto SpinLoopPause() noexcept -> void {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)

#if (defined(__ARM_ARCH_6K__) ||  \
     defined(__ARM_ARCH_6Z__) ||  \
     defined(__ARM_ARCH_6ZK__) || \
     defined(__ARM_ARCH_6T2__) || \
     defined(__ARM_ARCH_7__) ||   \
     defined(__ARM_ARCH_7A__) ||  \
     defined(__ARM_ARCH_7R__) ||  \
     defined(__ARM_ARCH_7M__) ||  \
     defined(__ARM_ARCH_7S__) ||  \
     defined(__ARM_ARCH_8A__) ||  \
     defined(__aarch64__))
        asm volatile("yield" ::: "memory");
#elif defined(_M_ARM64)
        __yield();
#else
        asm volatile("nop" ::: "memory");
#endif

#else
#warning "Unknown CPU architecture. No spinloop pause instruction."
#endif
    }
}
