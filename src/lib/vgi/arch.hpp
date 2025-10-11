///! Helper header that detects the CPU architecture for which the code is being compiled for
#pragma once

/* ===================== x86 family ===================== */
/* 64-bit (amd64 / x86_64) */
#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || \
        defined(_M_X64) || defined(_M_AMD64)
#define VGI_ARCH_FAMILY_X86 1
#define VGI_ARCH_X86_64 1
/* x32 ABI: 32-bit pointers on x86_64 */
#if defined(__ILP32__)
#define VGI_ARCH_X86_X32 1
#endif
/* 32-bit x86 */
#elif defined(__i386__) || defined(__i386) || defined(i386) || defined(__X86__) || \
        defined(_X86_) || defined(_M_IX86) /* note: Watcom also defines this in 16-bit mode */
#define VGI_ARCH_FAMILY_X86 1
/* Try to distinguish 16-bit vs 32-bit */
#if defined(_M_I86) || defined(__I86__)
#define VGI_ARCH_X86_16 1
#else
#define VGI_ARCH_X86_32 1
#endif
/* 16-bit x86 (explicit) */
#elif defined(_M_I86) || defined(__I86__)
#define VGI_ARCH_FAMILY_X86 1
#define VGI_ARCH_X86_16 1
#endif

/* ===================== ARM / AArch64 ===================== */
#if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC) || defined(__AARCH64EL__) || \
        defined(__AARCH64EB__)
#define VGI_ARCH_FAMILY_ARM 1
#define VGI_ARCH_AARCH64 1
#elif defined(__arm__) || defined(_M_ARM) || defined(__thumb__) || defined(_M_ARMT)
#define VGI_ARCH_FAMILY_ARM 1
#define VGI_ARCH_ARM 1
#endif

/* ===================== MIPS ===================== */
#if defined(__mips64) || defined(__mips64__) /* common compilers */ \
        || (defined(__mips__) && defined(__LP64__))
#define VGI_ARCH_FAMILY_MIPS 1
#define VGI_ARCH_MIPS64 1
#elif defined(__mips__) || defined(mips) /* GNU C */
#define VGI_ARCH_FAMILY_MIPS 1
#define VGI_ARCH_MIPS32 1
#endif

/* ===================== PowerPC ===================== */
#if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
#define VGI_ARCH_FAMILY_PPC 1
#define VGI_ARCH_PPC64 1
#elif defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || \
        defined(_M_PPC) || defined(_ARCH_PPC)
#define VGI_ARCH_FAMILY_PPC 1
#define VGI_ARCH_PPC32 1
#endif

/* ===================== SPARC ===================== */
#if defined(__sparc__) || defined(__sparc)
#define VGI_ARCH_FAMILY_SPARC 1
#if defined(__sparc_v9__) || defined(__sparcv9)
#define VGI_ARCH_SPARC64 1
#else
#define VGI_ARCH_SPARC32 1
#endif
#endif

/* ===================== System/370, /390, z/Architecture ===================== */
#if defined(__s390x__) || defined(__zarch__)
#define VGI_ARCH_FAMILY_SYSTEMZ 1
#define VGI_ARCH_S390X 1
#elif defined(__s390__) || defined(__370__) || defined(__THW_370__)
#define VGI_ARCH_FAMILY_SYSTEMZ 1
#define VGI_ARCH_S390 1
#endif

/* ===================== IA-64 (Itanium) ===================== */
#if defined(__ia64__) || defined(__ia64) || defined(_M_IA64) || defined(__itanium__)
#define VGI_ARCH_IA64 1
#endif

/* ===================== Alpha ===================== */
#if defined(__alpha__) || defined(__alpha) || defined(_M_ALPHA)
#define VGI_ARCH_ALPHA 1
#endif

/* ===================== PA-RISC (HPPA) ===================== */
#if defined(__hppa__) || defined(__HPPA__) || defined(__hppa)
#define VGI_ARCH_HPPA 1
#endif

/* ===================== SuperH ===================== */
#if defined(__sh__) || defined(__SH__)
#define VGI_ARCH_SH 1
#endif

/* ===================== Motorola 68k ===================== */
#if defined(__m68k__) || defined(__MC68K__)
#define VGI_ARCH_M68K 1
#endif

/* ===================== RISC-V ===================== */
#if defined(__riscv)
#define VGI_ARCH_FAMILY_RISCV 1
#if defined(__riscv_xlen) && (__riscv_xlen == 64)
#define VGI_ARCH_RISCV64 1
#else
#define VGI_ARCH_RISCV32 1
#endif
#endif

/* ===================== LoongArch ===================== */
#if defined(__loongarch__)
#define VGI_ARCH_FAMILY_LOONGARCH 1
#if defined(__loongarch_grlen) && (__loongarch_grlen == 64)
#define VGI_ARCH_LOONGARCH64 1
#else
#define VGI_ARCH_LOONGARCH32 1
#endif
#endif

/* ===================== WebAssembly (not in Predef page, widely used) ===================== */
#if defined(__wasm64__)
#define VGI_ARCH_FAMILY_WASM 1
#define VGI_ARCH_WASM64 1
#elif defined(__wasm32__) || defined(__wasm__)
#define VGI_ARCH_FAMILY_WASM 1
#define VGI_ARCH_WASM32 1
#endif
