/*
 * Automatically generated C config: don't edit
 */
#if !defined __FEATURES_H && !defined __need_uClibc_config_h
#error Never include <bits/uClibc_config.h> directly; use <features.h> instead
#endif

/*
 * Version Number
 */
#define __UCLIBC_MAJOR__ 0
#define __UCLIBC_MINOR__ 9
#define __UCLIBC_SUBLEVEL__ 28
#undef __TARGET_alpha__
#undef __TARGET_arm__
#undef __TARGET_bfin__
#undef __TARGET_cris__
#undef __TARGET_e1__
#undef __TARGET_frv__
#undef __TARGET_h8300__
#undef __TARGET_i386__
#undef __TARGET_i960__
#undef __TARGET_m68k__
#undef __TARGET_microblaze__
#define __TARGET_mips__ 1
#undef __TARGET_nios__
#undef __TARGET_nios2__
#undef __TARGET_powerpc__
#undef __TARGET_sh__
#undef __TARGET_sh64__
#undef __TARGET_sparc__
#undef __TARGET_v850__
#undef __TARGET_x86_64__

/*
 * Target Architecture Features and Options
 */
#define __HAVE_ELF__ 1
#define __ARCH_SUPPORTS_LITTLE_ENDIAN__ 1
#define __TARGET_ARCH__ "mips"
#define __ARCH_CFLAGS__ "-mno-split-addresses"
#define __ARCH_SUPPORTS_BIG_ENDIAN__ 1
#undef __CONFIG_MIPS_ISA_1__
#define __CONFIG_MIPS_ISA_2__ 1
#undef __CONFIG_MIPS_ISA_3__
#undef __CONFIG_MIPS_ISA_4__
#undef __CONFIG_MIPS_ISA_MIPS32__
#undef __CONFIG_MIPS_ISA_MIPS64__
#define __ARCH_LITTLE_ENDIAN__ 1
#undef __ARCH_BIG_ENDIAN__
#undef __ARCH_HAS_NO_MMU__
#define __ARCH_HAS_MMU__ 1
#define __UCLIBC_HAS_FLOATS__ 1
#define __HAS_FPU__ 1
#define __DO_C99_MATH__ 1
#define __KERNEL_SOURCE__ "/home/winfred/RT288x_SDK/toolchain/buildroot/toolchain_build_mipsel/linux"
#define __C_SYMBOL_PREFIX__ ""
#define __HAVE_DOT_CONFIG__ 1

/*
 * General Library Settings
 */
#undef __HAVE_NO_PIC__
#define __DOPIC__ 1
#undef __HAVE_NO_SHARED__
#define __HAVE_SHARED__ 1
#undef __ARCH_HAS_NO_LDSO__
#define __BUILD_UCLIBC_LDSO__ 1
#undef __FORCE_SHAREABLE_TEXT_SEGMENTS__
#define __LDSO_LDD_SUPPORT__ 1
#define __LDSO_CACHE_SUPPORT__ 1
#undef __LDSO_PRELOAD_FILE_SUPPORT__
#define __LDSO_BASE_FILENAME__ "ld.so"
#undef __LDSO_RUNPATH__
#undef __DL_FINI_CRT_COMPAT__
#define __UCLIBC_CTOR_DTOR__ 1
#undef __HAS_NO_THREADS__
#define __UCLIBC_HAS_THREADS__ 1
#define __PTHREADS_DEBUG_SUPPORT__ 1
#define __UCLIBC_HAS_LFS__ 1
#undef __UCLIBC_STATIC_LDCONFIG__
#undef __MALLOC__
#undef __MALLOC_SIMPLE__
#define __MALLOC_STANDARD__ 1
#define __MALLOC_GLIBC_COMPAT__ 1
#define __UCLIBC_DYNAMIC_ATEXIT__ 1
#define __HAS_SHADOW__ 1
#undef __UNIX98PTY_ONLY__
#define __ASSUME_DEVPTS__ 1
#define __UCLIBC_HAS_TM_EXTENSIONS__ 1
#define __UCLIBC_HAS_TZ_CACHING__ 1
#define __UCLIBC_HAS_TZ_FILE__ 1
#define __UCLIBC_HAS_TZ_FILE_READ_MANY__ 1
#define __UCLIBC_TZ_FILE_PATH__ "/etc/TZ"

/*
 * Networking Support
 */
#define __UCLIBC_HAS_IPV6__ 1
#define __UCLIBC_HAS_RPC__ 1
#define __UCLIBC_HAS_FULL_RPC__ 1

/*
 * String and Stdio Support
 */
#define __UCLIBC_HAS_STRING_GENERIC_OPT__ 1
#define __UCLIBC_HAS_STRING_ARCH_OPT__ 1
#define __UCLIBC_HAS_CTYPE_TABLES__ 1
#define __UCLIBC_HAS_CTYPE_SIGNED__ 1
#undef __UCLIBC_HAS_CTYPE_UNSAFE__
#define __UCLIBC_HAS_CTYPE_CHECKED__ 1
#undef __UCLIBC_HAS_CTYPE_ENFORCED__
#define __UCLIBC_HAS_WCHAR__ 1
#undef __UCLIBC_HAS_LOCALE__
#define __UCLIBC_HAS_HEXADECIMAL_FLOATS__ 1
#define __UCLIBC_HAS_GLIBC_CUSTOM_PRINTF__ 1
#define __UCLIBC_PRINTF_SCANF_POSITIONAL_ARGS__ 9
#define __UCLIBC_HAS_SCANF_GLIBC_A_FLAG__ 1
#undef __UCLIBC_HAS_STDIO_BUFSIZ_NONE__
#undef __UCLIBC_HAS_STDIO_BUFSIZ_256__
#undef __UCLIBC_HAS_STDIO_BUFSIZ_512__
#undef __UCLIBC_HAS_STDIO_BUFSIZ_1024__
#undef __UCLIBC_HAS_STDIO_BUFSIZ_2048__
#define __UCLIBC_HAS_STDIO_BUFSIZ_4096__ 1
#undef __UCLIBC_HAS_STDIO_BUFSIZ_8192__
#define __UCLIBC_HAS_STDIO_BUILTIN_BUFFER_NONE__ 1
#undef __UCLIBC_HAS_STDIO_BUILTIN_BUFFER_4__
#undef __UCLIBC_HAS_STDIO_BUILTIN_BUFFER_8__
#undef __UCLIBC_HAS_STDIO_SHUTDOWN_ON_ABORT__
#define __UCLIBC_HAS_STDIO_GETC_MACRO__ 1
#define __UCLIBC_HAS_STDIO_PUTC_MACRO__ 1
#define __UCLIBC_HAS_STDIO_AUTO_RW_TRANSITION__ 1
#undef __UCLIBC_HAS_FOPEN_LARGEFILE_MODE__
#define __UCLIBC_HAS_FOPEN_EXCLUSIVE_MODE__ 1
#define __UCLIBC_HAS_GLIBC_CUSTOM_STREAMS__ 1
#define __UCLIBC_HAS_PRINTF_M_SPEC__ 1
#define __UCLIBC_HAS_ERRNO_MESSAGES__ 1
#undef __UCLIBC_HAS_SYS_ERRLIST__
#define __UCLIBC_HAS_SIGNUM_MESSAGES__ 1
#undef __UCLIBC_HAS_SYS_SIGLIST__
#define __UCLIBC_HAS_GNU_GETOPT__ 1

/*
 * Big and Tall
 */
#define __UCLIBC_HAS_REGEX__ 1
#undef __UCLIBC_HAS_WORDEXP__
#define __UCLIBC_HAS_FTW__ 1
#define __UCLIBC_HAS_GLOB__ 1

/*
 * Library Installation Options
 */
#define __SHARED_LIB_LOADER_PREFIX__ "/lib"
#define __RUNTIME_PREFIX__ "/"
#define __DEVEL_PREFIX__ "/usr/"

/*
 * uClibc security related options
 */
#undef __UCLIBC_SECURITY__

/*
 * uClibc development/debugging options
 */
#define __CROSS_COMPILER_PREFIX__ "/home/winfred/RT288x_SDK/toolchain/buildroot-gdb/bin/mipsel-linux-uclibc-"
#undef __DODEBUG__
#undef __DODEBUG_PT__
#undef __DOASSERTS__
#undef __SUPPORT_LD_DEBUG__
#undef __SUPPORT_LD_DEBUG_EARLY__
#define __WARNINGS__ "-Wall"
#undef __UCLIBC_MJN3_ONLY__
