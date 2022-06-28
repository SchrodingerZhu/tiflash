#pragma once

#ifdef __linux__
#include <sys/auxv.h>
#ifndef HWCAP2_SVE2
#define HWCAP2_SVE2 (1 << 1)
#endif

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif

#ifndef AT_HWCAP2
#define AT_HWCAP2 26
#endif

#ifndef AT_HWCAP
#define AT_HWCAP 16
#endif

namespace detail
{
static inline bool sve2Supported()
{
    auto hwcaps = getauxval(AT_HWCAP2);
    return (hwcaps & HWCAP2_SVE2) != 0;
}

static inline bool sveSupported()
{
    auto hwcaps = getauxval(AT_HWCAP);
    return (hwcaps & HWCAP_SVE) != 0;
}
} // namespace detail

#endif

#define TMV_STRINGIFY_IMPL(X) #X
#define TMV_STRINGIFY(X) TMV_STRINGIFY_IMPL(X)

#define TIFLASH_MULTIVERSIONED_VECTORIZATION_X86_64(RETURN, NAME, ARG_LIST, ARG_NAMES, BODY)                                       \
    struct NAME##TiFlashMultiVersion                                                                                               \
    {                                                                                                                              \
        __attribute__((always_inline)) static inline RETURN inlinedImplementation ARG_LIST BODY;                                   \
                                                                                                                                   \
        __attribute__((target("default"))) /* x86-64-v2 is ready on default */                                                     \
        static RETURN dispatchedImplementation ARG_LIST                                                                            \
        {                                                                                                                          \
            return inlinedImplementation ARG_NAMES;                                                                                \
        };                                                                                                                         \
                                                                                                                                   \
        __attribute__((target("avx,avx2,fma,bmi,bmi2"))) /* x86-64-v3 feature flags */                                             \
        static RETURN dispatchedImplementation ARG_LIST                                                                            \
        {                                                                                                                          \
            return inlinedImplementation ARG_NAMES;                                                                                \
        };                                                                                                                         \
                                                                                                                                   \
        __attribute__((target("avx512f,avx512vl,avx512bw,avx512cd,avx512dq,avx,avx2,fma,bmi,bmi2"))) /* x86-64-v4 feature flags */ \
        static RETURN dispatchedImplementation ARG_LIST                                                                            \
        {                                                                                                                          \
            return inlinedImplementation ARG_NAMES;                                                                                \
        };                                                                                                                         \
                                                                                                                                   \
        __attribute__((always_inline)) static inline RETURN invoke ARG_LIST                                                        \
        {                                                                                                                          \
            return dispatchedImplementation ARG_NAMES;                                                                             \
        };                                                                                                                         \
    };

#define TIFLASH_MULTIVERSIONED_VECTORIZATION_AARCH64(RETURN, NAME, ARG_LIST, ARG_NAMES, BODY)    \
    struct NAME##TiFlashMultiVersion                                                             \
    {                                                                                            \
        __attribute__((always_inline)) static inline RETURN inlinedImplementation ARG_LIST BODY; \
                                                                                                 \
        static RETURN genericImplementation ARG_LIST                                             \
        {                                                                                        \
            return inlinedImplementation ARG_NAMES;                                              \
        };                                                                                       \
                                                                                                 \
        __attribute__((target("sve"))) static RETURN sveImplementation ARG_LIST                  \
        {                                                                                        \
            return inlinedImplementation ARG_NAMES;                                              \
        };                                                                                       \
                                                                                                 \
        __attribute__((target("sve2"))) static RETURN sve2Implementation ARG_LIST                \
        {                                                                                        \
            return inlinedImplementation ARG_NAMES;                                              \
        };                                                                                       \
                                                                                                 \
        static RETURN dispatchedImplementation ARG_LIST                                          \
            __attribute__((ifunc(TMV_STRINGIFY(__tiflash_mvec_##NAME##_resolver))));             \
                                                                                                 \
        __attribute__((always_inline)) static inline RETURN invoke ARG_LIST                      \
        {                                                                                        \
            return dispatchedImplementation ARG_NAMES;                                           \
        };                                                                                       \
    };                                                                                           \
    extern "C" void * __tiflash_mvec_##NAME##_resolver()                                         \
    {                                                                                            \
        if (::detail::sveSupported())                                                            \
        {                                                                                        \
            return reinterpret_cast<void *>(&NAME##TiFlashMultiVersion::sveImplementation);      \
        }                                                                                        \
        if (::detail::sve2Supported())                                                           \
        {                                                                                        \
            return reinterpret_cast<void *>(&NAME##TiFlashMultiVersion::sve2Implementation);     \
        }                                                                                        \
        return reinterpret_cast<void *>(&NAME##TiFlashMultiVersion::genericImplementation);      \
    }

#if defined(__linux__) && defined(__aarch64__)
#define TIFLASH_MULTIVERSIONED_VECTORIZATION TIFLASH_MULTIVERSIONED_VECTORIZATION_AARCH64
#elif defined(__linux__) && defined(__x86_64__)
#define TIFLASH_MULTIVERSIONED_VECTORIZATION TIFLASH_MULTIVERSIONED_VECTORIZATION_X86_64
#else
#define TIFLASH_MULTIVERSIONED_VECTORIZATION(RETURN, NAME, ARG_LIST, ARG_NAMES, BODY) \
    struct NAME##TiFlashMultiVersion                                                  \
    {                                                                                 \
        __attribute__((always_inline)) static inline RETURN invoke ARG_LIST BODY;     \
    };
#endif
