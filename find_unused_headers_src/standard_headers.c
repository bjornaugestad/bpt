/* We add standard headers from here instead of from the config file. Makes it 
 * easier to use the program without write access to e.g. /etc. */

#include "file.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static struct {
    const char *names[5];
    const char *symbols[2000];
} std[] = { 
    { 
        { "assert.h", "cassert", NULL }, 
        { "NDEBUG", "assert", "static_assert" }
    },
    {
        { "complex.h", NULL },
        { 
            "complex", "imaginary", "_Complex_I", "_Imaginary_I",
            "CX_LIMITED_RANGE",
            "cacos", "cacosf", "cacosl",
            "casin", "casinf", "casinl",
            "catan", "catanf", "catanl",
            "ccos", "ccosf", "ccosl",
            "csin", "csinf", "csinl",
            "ctan", "ctanf", "ctanl",
            "cacosh", "cacoshl", "cacoshf",
            "casinh", "casinhl", "casinhf",
            "catanh", "catanhf", "catanhl",
            "ccosh", "ccoshf", "ccoshl",
            "csinh", "csinhf", "csinhl",
            "ctanh", "ctanhf", "ctanhl",
            "cexp", "cexpf", "cexpl",
            "clog", "clogf", "clogl",
            "cabs", "cabsf", "cabsl",
            "cpow", "cpowf", "cpowl",
            "csqrt", "csqrtf", "csqrtl",
            "carg", "cargf", "cargl",
            "cimag", "cimagf", "cimagl",
            "conj", "conjf", "conjl",
            "cproj", "cprojf", "cprojl",
            "creal", "crealf", "creall",
        }
    },

    {
        { "ctype.h", NULL },
        {
            "isalnum", "isalpha", "isascii", "isblank", "iscntrl", "isdigit", "isgraph", "islower", 
            "isprint", "ispunct", "isspace", "isupper", "isxdigit", "tolower", "toupper"
        }
    },

    {
        { "errno.h", "cerrno", NULL },
        {
            "errno",
            "E2BIG", "EACCES", "EADDRINUSE", "EADDRNOTAVAIL", "EADV", "EAFNOSUPPORT",
            "EAGAIN", "EALREADY", "EBADE", "EBADF", "EBADFD", "EBADMSG", "EBADR",
            "EBADRQC", "EBADSLT", "EBFONT", "EBUSY", "ECANCELED", "ECHILD", "ECHRNG",
            "ECOMM", "ECONNABORTED", "ECONNREFUSED", "ECONNRESET", "EDEADLK", "EDEADLOCK",
            "EDESTADDRREQ", "EDOM", "EDOTDOT", "EDQUOT", "EEXIST", "EFAULT", "EFBIG",
            "EHOSTDOWN", "EHOSTUNREACH", "EHWPOISON", "EIDRM", "EILSEQ", "EINPROGRESS",
            "EINTR", "EINVAL", "EIO", "EISCONN", "EISDIR", "EISNAM", "EKEYEXPIRED",
            "EKEYREJECTED", "EKEYREVOKED", "EL2HLT", "EL2NSYNC", "EL3HLT", "EL3RST",
            "ELIBACC", "ELIBBAD", "ELIBEXEC", "ELIBMAX", "ELIBSCN", "ELNRNG", "ELOOP",
            "EMEDIUMTYPE", "EMFILE", "EMLINK", "EMSGSIZE", "EMULTIHOP", "ENAMETOOLONG",
            "ENAVAIL", "ENETDOWN", "ENETRESET", "ENETUNREACH", "ENFILE", "ENOANO",
            "ENOBUFS", "ENOCSI", "ENODATA", "ENODEV", "ENOENT", "ENOEXEC", "ENOKEY",
            "ENOLCK", "ENOLINK", "ENOMEDIUM", "ENOMEM", "ENOMSG", "ENONET", "ENOPKG",
            "ENOPROTOOPT", "ENOSPC", "ENOSR", "ENOSTR", "ENOSYS", "ENOTBLK", "ENOTCONN",
            "ENOTDIR", "ENOTEMPTY", "ENOTNAM", "ENOTRECOVERABLE", "ENOTSOCK", "ENOTSUP",
            "ENOTTY", "ENOTUNIQ", "ENXIO", "EOPNOTSUPP", "EOVERFLOW", "EOWNERDEAD",
            "EPERM", "EPFNOSUPPORT", "EPIPE", "EPROTO", "EPROTONOSUPPORT", "EPROTOTYPE",
            "ERANGE", "EREMCHG", "EREMOTE", "EREMOTEIO", "ERESTART", "ERFKILL", "EROFS",
            "ESHUTDOWN", "ESOCKTNOSUPPORT", "ESPIPE", "ESRCH", "ESRMNT", "ESTALE",
            "ESTRPIPE", "ETIME", "ETIMEDOUT", "ETOOMANYREFS", "ETXTBSY", "EUCLEAN",
            "EUNATCH", "EUSERS", "EWOULDBLOCK", "EXDEV", "EXFULL",
        }
    },

    {
        { "fenv.h", NULL },
        {
            "fenv_t", "fexpect_t", "FE_DIVBYZERO", "FE_INEXACT", "FE_INVALID",
            "FE_OVERFLOW", "FE_UNDERFLOW", "FE_ALL_EXCEPT", "FW_DOWNWARD",
            "FE_TONEAREST", "FE_TOWARDZERO", "FE_UPWARD", "FE_DFL_ENV",
            "FENV_ACCESS", "feclearexcept", "fegetexceptflag", "feraiseexcept",
            "fesetexceptflag", "fetestexcept", "fegetround", "fesetround",
            "fegetenv", "feholdexcept", "fesetenv", "feupdateenv",
        }
    },

    {
        { "float.h", NULL },
        {
            
            "FLT_ROUNDS", "FLT_EVAL_METHOD", "FLT_RADIX", "FLT_MANT_DIG",
            "DBL_MANT_DIG", "LDBL_MANT_DIG", "DECIMAL_DIG", "FLT_DIG",
            "DBL_DIG", "LDBL_DIG", "FLT_MIN_EXP", "DBL_MIN_EXP", "LDBL_MIN_EXP",
            "FLT_MIN_10_EXP", "DBL_MIN_10_EXP", "LDBL_MIN_10_EXP", "FLT_MAX_EXP",
            "DBL_MAX_EXP", "LDBL_MAX_EXP", "FLT_MAX_10_EXP", "DBL_MAX_10_EXP",
            "LDBL_MAX_10_EXP", "FLT_MAX", "DBL_MAX", "LDBL_MAX", "FLT_EPSILON",
            "DBL_EPSILON", "LDBL_EPSILON", "FLT_MIN", "DBL_MIN", "LDBL_MIN",
        }
    },

    {
        { "inttypes.h", NULL },
        {
            "PRIX16", "PRIX32", "PRIX64", "PRIX8",
            "PRIXFAST16", "PRIXFAST32", "PRIXFAST64", "PRIXFAST8",
            "PRIXLEAST16", "PRIXLEAST32", "PRIXLEAST64", "PRIXLEAST8",
            "PRIXMAX", "PRIXPTR",
            "PRId16", "PRId32", "PRId64", "PRId8",
            "PRIdFAST16", "PRIdFAST32", "PRIdFAST64", "PRIdFAST8",
            "PRIdLEAST16", "PRIdLEAST32", "PRIdLEAST64", "PRIdLEAST8",
            "PRIdMAX", "PRIdPTR",
            "PRIi16", "PRIi32", "PRIi64", "PRIi8",
            "PRIiFAST16", "PRIiFAST32", "PRIiFAST64", "PRIiFAST8",
            "PRIiLEAST16", "PRIiLEAST32", "PRIiLEAST64", "PRIiLEAST8",
            "PRIiMAX", "PRIiPTR",
            "PRIo16", "PRIo32", "PRIo64", "PRIo8",
            "PRIoFAST16", "PRIoFAST32", "PRIoFAST64", "PRIoFAST8",
            "PRIoLEAST16", "PRIoLEAST32", "PRIoLEAST64", "PRIoLEAST8",
            "PRIoMAX", "PRIoPTR",
            "PRIu16", "PRIu32", "PRIu64", "PRIu8",
            "PRIuFAST16", "PRIuFAST32", "PRIuFAST64", "PRIuFAST8",
            "PRIuLEAST16", "PRIuLEAST32", "PRIuLEAST64", "PRIuLEAST8",
            "PRIuMAX", "PRIuPTR",
            "PRIx16", "PRIx32", "PRIx64", "PRIx8",
            "PRIxFAST16", "PRIxFAST32", "PRIxFAST64", "PRIxFAST8",
            "PRIxLEAST16", "PRIxLEAST32", "PRIxLEAST64", "PRIxLEAST8",
            "PRIxMAX", "PRIxPTR",
            "SCNd16", "SCNd32", "SCNd64", "SCNd8",
            "SCNdFAST16", "SCNdFAST32", "SCNdFAST64", "SCNdFAST8",
            "SCNdLEAST16", "SCNdLEAST32", "SCNdLEAST64", "SCNdLEAST8",
            "SCNdMAX", "SCNdPTR",
            "SCNi16", "SCNi32", "SCNi64", "SCNi8",
            "SCNiFAST16", "SCNiFAST32", "SCNiFAST64", "SCNiFAST8",
            "SCNiLEAST16", "SCNiLEAST32", "SCNiLEAST64", "SCNiLEAST8",
            "SCNiMAX", "SCNiPTR",
            "SCNo16", "SCNo32", "SCNo64", "SCNo8",
            "SCNoFAST16", "SCNoFAST32", "SCNoFAST64", "SCNoFAST8",
            "SCNoLEAST16", "SCNoLEAST32", "SCNoLEAST64", "SCNoLEAST8",
            "SCNoMAX", "SCNoPTR",
            "SCNu16", "SCNu32", "SCNu64", "SCNu8",
            "SCNuFAST16", "SCNuFAST32", "SCNuFAST64", "SCNuFAST8",
            "SCNuLEAST16", "SCNuLEAST32", "SCNuLEAST64", "SCNuLEAST8",
            "SCNuMAX", "SCNuPTR",
            "SCNx16", "SCNx32", "SCNx64", "SCNx8",
            "SCNxFAST16", "SCNxFAST32", "SCNxFAST64", "SCNxFAST8",
            "SCNxLEAST16", "SCNxLEAST32", "SCNxLEAST64", "SCNxLEAST8",
            "SCNxMAX", "SCNxPTR",
            "imaxabs", "imaxdiv", "strtoimax", ", strtoumax", "wcstoimax", "wcstoumax",
        }
    },

#if 0

/* exclude this file as it reports too many false positives in comments et al. */

    {
        { "iso646.h", NULL },
        {
            "and", "and_eq", "bitand", "bitor", "compl", "not", "not_eq", 
            "or", "or_eq", "xor", "xor_eq"
        }
    },
#endif

    {
        { "limits.h", "climits", NULL },
        {
            "CHAR_BIT", "SCHAR_MIN", "SCHAR_MAX", "UCHAR_MAX", "CHAR_MIN",
            "CHAR_MAX", "MB_LEN_MAX", "SHRT_MIN", "SHRT_MAX", "USHRT_MAX",
            "INT_MIN", "INT_MAX", "UINT_MAX", "LONG_MIN", "LONG_MAX",
            "ULONG_MAX", "LLONG_MIN", "LLONG_MAX", "ULLONG_MAX",

            /* Some non-C99 stuff */
            "NR_OPEN",
            "NGROUPS_MAX",
            "ARG_MAX",
            "LINK_MAX",
            "MAX_CANON",
            "MAX_INPUT",
            "NAME_MAX",
            "PATH_MAX",
            "PIPE_BUF",
            "XATTR_NAME_MAX",
            "XATTR_SIZE_MAX",
            "XATTR_LIST_MAX",

        }
    },

    {
        { "locale.h", NULL },
        {
            "lconv", "LC_ALL", "LC_COLLATE", "LC_CTYPE", "LC_MONETARY", 
            "LC_NUMERIC", "LC_TIME", "setlocale", "localeconv",
            /* Some non-C99 stuff */
            "newlocale", /* XOPEN 2K8 */
        }
    },

    {
        { "setjmp.h", NULL },
        { "jmp_buf", "setjmp", "longjmp" },
    },

    { 
        { "signal.h", NULL },
        { 
            "sig_atomic_t", 
            "signal", 
            "raise", 
            "sighandler_t",  /* GNU Extension */
            /* from bits/signum.h */
            "SIGABRT", "SIGALRM", "SIGBUS", "SIGCHLD", "SIGCLD",
            "SIGCONT", "SIGFPE", "SIGHUP", "SIGILL", "SIGINT",
            "SIGIO", "SIGIOT", "SIGKILL", "SIGPIPE", "SIGPOLL",
            "SIGPROF", "SIGPWR", "SIGQUIT", "SIGRTMAX", "SIGRTMIN",
            "SIGSEGV", "SIGSTKFLT", "SIGSTOP", "SIGSYS", "SIGTERM",
            "SIGTRAP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGUNUSED",
            "SIGURG", "SIGUSR1", "SIGUSR2", "SIGVTALRM", "SIGWINCH",
            "SIGXCPU", "SIGXFSZ", "SIG_DFL", "SIG_ERR", "SIG_HOLD",
            "SIG_IGN", "_NSIG", "__SIGRTMAX", "__SIGRTMIN",
        }
    },

    {
        { "stdbool.h", NULL },
        { "true", "false", "bool", "__bool_true_false_are_defined" },
    },

    {
        { "stdio.h", "cstdio", NULL },
        {
            "BUFSIZ", "EOF", "FILENAME_MAX", "FILE", "FOPEN_MAX", "L_tmpnam", 
            "SEEK_CUR", "SEEK_END", "SEEK_SET", "TMP_MAX", "_IOFBF", "_IOLBF", "clearerr", 
            "fclose", "feof", "ferror", "fflush", "fgetc", "fgetpos", "fgets", "fopen", 
            "fpos_t", "fprintf", "fputc", "fputs", "fread", "freopen", "fscanf", "fseek", 
            "fsetpos", "ftell", "fwrite", "getc", "getchar", "gets", "perror", "printf", 
            "putc", "putchar", "puts", "remove", "rename", "rewind", "scanf", "setbuf", 
            "setvbuf", "snprintf", "vsnprintf", "sprinf", "sprintf", "sscanf", "stderr", 
            "stdin", "stdout", "svnprintf", "tmpfile", "tmpnam", "ungetc", "vfprintf", 
            "vfscanf", "vprintf", "vscanf", "vsprintf", "vsscanf",
            "vasprintf", "dprintf", "vdprintf", /* GNU extension */
        }
    },

    {
        { "stdarg.h", "cstdarg", NULL }, 
        { "va_list", "va_arg", "va_copy", "va_end", "va_start" },
    },

    {
        { "stddef.h", "cstddef", NULL },
        { "size_t", "ptrdiff_t", "NULL", "nullptr", "offsetof", "wchar_t" }
    },

    {
        { "stdint.h", NULL },
        {
            "uint8_t", "int8_t", "uint16_t", "int16_t", "uint32_t", "int32_t", "uint64_t",
            "int64_t", "uint_least8_t", "int_least8_t", "uint_least16_t", "int_least16_t",
            "uint_least32_t", "int_least32_t", "uint_least64_t", "int_least64_t", "uint_fast8_t",
            "int_fast8_t", "uint_fast16_t", "int_fast16_t", "uint_fast32_t", "int_fast32_t",
            "uint_fast64_t", "int_fast64_t", "intptr_t", "uintptr_t", "intmax_t", "uintmax_t",
            "INT8_MIN", "INT8_MAX", "UINT8_MAX", "INT16_MIN", "INT16_MAX", "UINT16_MAX",
            "INT32_MIN", "INT32_MAX", "UINT32_MAX", "INT64_MIN", "INT64_MAX", "UINT64_MAX",
            "INT_LEAST8_MIN", "INT_LEAST8_MAX", "UINT_LEAST8_MAX", "INT_LEAST16_MIN",
            "INT_LEAST16_MAX", "UINT_LEAST16_MAX", "INT_LEAST32_MIN", "INT_LEAST32_MAX",
            "UINT_LEAST32_MAX", "INT_LEAST64_MIN", "INT_LEAST64_MAX", "UINT_LEAST64_MAX",
            "INT_FAST8_MIN", "INT_FAST8_MAX", "UINT_FAST8_MAX", "INT_FAST16_MIN",
            "INT_FAST16_MAX", "UINT_FAST16_MAX", "INT_FAST32_MIN", "INT_FAST32_MAX",
            "UINT_FAST32_MAX", "INT_FAST64_MIN", "INT_FAST64_MAX", "UINT_FAST64_MAX",
            "INTPTR_MIN", "INTPTR_MAX", "UINTPTR_MAX", "PTRDIFF_MIN", "PTRDIFF_MAX",
            "SIG_ATOMIC_MIN", "SIG_ATOMIC_MAX", "SIZE_MAX", "WCHAR_MIN",
            "WCHAR_MAX", "WINT_MIN", "WINT_MAX",
        }
    },

    {
        { "stdlib.h", "cstdlib", NULL },
        { 
            "div_t", "ldiv_t", "lldiv_t", "EXIT_FAILURE", "EXIT_SUCCESS", "RAND_MAX",
            "MB_CUR_MAX", "atof", "atoi", "atol", "atoll", "strtod", "strtof",
            "strtold", "strtol", "strtoll", "strtoul", "strtoull", "rand",
            "srand", "srandom", "calloc", "free", "malloc", "memalign", "realloc", "abort",
            "valloc", "atexit", "exit", "_Exit", "getenv", "system", "bsearch", "qsort",
            "abs", "labs", "llabs", "div", "ldiv", "lldiv", "mblen", "mbtowc",
            "wctomb", "mbstowcs", "wcstombs" ,
            "mkstemp", "mkostemp", "mkstemps", "mkostemps",  // Misc GNU/BSD
        }
    },

    {
        { "string.h", "cstring", NULL },
        {
            "memcpy", "memmove", "strcpy", "strncpy", "strcat", "strncat", 
            "memcmp", "strcmp", "strcoll", "strncmp", "strxfrm",
            "memchr", "strchr", "strcspn", "strlen", "strpbrk", "strrchr",
            "strspn", "strstr", "strtok", "memset", "strerror",
            "strdup", "strcasecmp", "strncasecmp", "strsignal", /* POSIX */
            "basename", /* GNU */
            "strerror_r", /* POSIX/GNU */
            "strsep", /* BSD */
        }
    }, 

    {
        { "time.h", "ctime", NULL },
        { "CLOCKS_PER_SEC", "clock_t", "time_t", "tm", "clock", "difftime", "mktime",
            "time", "strftime", "asctime", "ctime", "gmtime", "localtime", 
            "timespec", /* GNU */
        }
    },
    {
        { "memory", NULL },
        { "allocator", "auto_ptr", "auto_ptr_ref", "raw_storage_iterator",
          "get_temporary_buffer", "return_temporary_buffer", "shared_ptr", 
          "weak_ptr", "unique_ptr", "default_delete", "make_shared", "make_unique", 
          "allocate_shared", "static_pointer_cast", "dynamic_pointer_cast",
          "const_pointer_cast", "get_deleter", "owner_less", "enable_shared_from_this",
          "uninitialized_copy", "uninitialized_fill", "uninitialized_fill_n"
        }
    },
    {
        { "string", NULL },
        { "std::string", "std::basic_string" },
    },
    {
        { "map", NULL },
        { "std::map", "std::multimap" },
    },
    {
        { "set", NULL },
        { "std::set", "std::multiset" },
    },
    {
        { "queue", NULL },
        { "std::queue", "std::priority_queue" },
    },


    {
        { "atomic", NULL },
            {
            "std::atomic", "std::atomic_bool", "std::atomic_char", "std::atomic_schar",
            "std::atomic_uchar", "std::atomic_short", "std::atomic_ushort", "std::atomic_int",
            "std::atomic_uint", "std::atomic_long", "std::atomic_ulong", "std::atomic_llong",
            "std::atomic_ullong", "std::atomic_wchar_t", "std::atomic_char16_t", "std::atomic_char32_t",
            "std::atomic_int8_t", "std::atomic_uint8_t", "std::atomic_int16_t", "std::atomic_uint16_t",
            "std::atomic_int32_t", "std::atomic_uint32_t", "std::atomic_int64_t", "std::atomic_uint64_t",
            "std::atomic_int_least8_t", "std::atomic_uint_least8_t", "std::atomic_int_least16_t",
            "std::atomic_uint_least16_t", "std::atomic_int_least32_t", "std::atomic_uint_least32_t",
            "std::atomic_int_least64_t", "std::atomic_uint_least64_t", "std::atomic_int_fast8_t",
            "std::atomic_uint_fast8_t", "std::atomic_int_fast16_t", "std::atomic_uint_fast16_t",
            "std::atomic_int_fast32_t", "std::atomic_uint_fast32_t", "std::atomic_int_fast64_t",
            "std::atomic_uint_fast64_t", "std::atomic_intptr_t", "std::atomic_uintptr_t",
            "std::atomic_size_t", "std::atomic_ptrdiff_t", "std::atomic_intmax_t", "std::atomic_uintmax_t",
            },
    },



    {
        { "iomanip", NULL },
        {
            "setiosflags", "resetiosflags", "setbase", "setfill", "setprecision",
            "setw", "get_money", "put_money", "get_time", "put_time",
        },
    },



    {
        { "utility", NULL },
        { 
            // Functions
            "std::swap",
            "std::make_pair",
            "std::forward",
            "std::move",
            "std::move_if_noexcept",
            "std::declval",

            // types
            "std::pair",
            "std::piecewise_construct_t",

            // constants
            "std::piecewise_construct",

        },
    },
    {
        { "vector", NULL },
        { "std::vector" },
    },
    {
        { "condition_variable", NULL },
        { "std::condition_variable", "std::condition_variable_any", "std::cv_status", "std::notify_all_at_thread_exit" },
    },
    {
        { "sstream", NULL },
        { "std::stringstream", "std::istringstream", "std::ostringstream", "std::stringbuf" },
    },
    {
        { "boost/bind.hpp", NULL },
        { "boost::bind", "boost::_bi::bind_t" }
    },
    {
        { "boost/shared_ptr.hpp", NULL },
        { "boost::shared_ptr" }
    },
    {
        { "boost/scoped_ptr.hpp", NULL },
        { "boost::scoped_ptr" }
    },
    {
        { "boost/weak_ptr.hpp", NULL },
        { "boost::weak_ptr" }
    },
    {
        { "boost/scoped_array.hpp", NULL },
        { "boost::scoped_array" }
    },

    {
        { "boost/regex.hpp", NULL },
        { 
            "boost::regex",
            "boost::regex_match", "boost::regex_search", "boost::regex_iterator", 
            "boost::regex_token_iterator", "boost::regex_grep", "boost::regex_replace",
            "boost::regex_merge", "boost::regex_split"
        }
    },

    {
        { "boost/make_shared.hpp", NULL },
        { "boost::make_shared", "boost::allocate_shared" }
    },
    {
        { "boost/foreach.hpp", NULL },
        { "BOOST_FOREACH" }
    },
    {
        { "boost/function.hpp", NULL },
        { 
            "boost::function", 
            "boost::function0",
            "boost::function1" 
            "boost::function2" 
            "boost::function3" 
            "boost::function4" 
            "boost::function5" 
            "boost::function6" 
            "boost::function7" 
        }
    },
    {
        { "boost/noncopyable.hpp", NULL },
        { "boost::noncopyable" }
    },
    {
        { "boost/tuple.hpp", NULL },
        { "boost::tuple" }
    },
    {
        { "boost/signals2.hpp", NULL },
        { "boost::signals2" }
    },

    {
        { "iostream", NULL },
        {
            "cin", "cout", "cerr", "clog", "wcin", "wcout", "wcerr", "wclog" ,
            "std::istream", "std::ostream",
        },
    },
    {
        { "iomanip", NULL },
        {
            "setiosflags", "resetiosflags", "setbase", "setfill", "setprecision", 
            "setw", "get_money", "put_money", "get_time", "put_time", 
        },
    },

    {
        { "mutex", NULL },
        {
            "std::lock_guard",
            "std::mutex",
            "std::recursive_mutex",
            "std::recursive_timed_mutex",
            "std::timed_mutex",
            "std::unique_lock",
            "std::try_lock",
            "std::scoped_lock",
            "std::lock",
            "std::once_flag",
            "std::call_once",
        },
    },
    {
        { "thread", NULL },
        {
            "std::thread",
            "std::mutex",
            "this_thread",
            "this_thread::sleep_for",
            "this_thread::sleep_until",
            "this_thread::get_id",
            "this_thread::yield",
        },
    },
};

void add_standard_headers(struct file *head)
{
    struct file *p;
    size_t i, j, k, nfiles = sizeof std / sizeof *std;

    assert(head != NULL);

    for(i = 0; i < nfiles; i++) {
        for(j = 0; std[i].names[j] != NULL; j++) {
            if( (p = file_new_tail(head, std[i].names[j])) == NULL)
                abort();
            for(k = 0; std[i].symbols[k] != NULL; k++)
                file_add_symbol(p, std[i].symbols[k]);
        }
    }
}

