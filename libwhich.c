#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define PSAPI_VERSION 2 // for EnumProcessModulesEx
#include <windows.h>
#include <psapi.h> // for EnumProcessModulesEx
#define T(str) L##str
#define main wmain // avoid magic behavior for the name "main" in gcc
int main(int argc, const WCHAR **argv);
typedef struct {
    HANDLE fd;
    BOOL isconsole;
} FILE;
static FILE _stdout = { INVALID_HANDLE_VALUE };
static FILE _stderr = { INVALID_HANDLE_VALUE };
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;
int _fwrite(const WCHAR *str, size_t nchars, FILE *out) {
    DWORD written;
    if (nchars == 0)
        return 0;
    if (out->isconsole) {
        if (WriteConsoleW(out->fd, str, nchars, &written, NULL))
            return written;
    } else {
        size_t u8len = WideCharToMultiByte(CP_UTF8, 0, str, nchars, NULL, 0, NULL, NULL);
        char *u8str = (char*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, u8len);
        if (!WideCharToMultiByte(CP_UTF8, 0, str, nchars, u8str, u8len, NULL, NULL))
            return 0;
        if (WriteFile(out->fd, u8str, u8len, &written, NULL)) {
            HeapFree(GetProcessHeap(), 0, u8str);
            return nchars;
        }
        HeapFree(GetProcessHeap(), 0, u8str);
    }
    return -1;
}
size_t wcslen(const WCHAR *str)
{
    size_t len = 0;
    while (*str++)
        len++;
    return len;
}
int fputs(const WCHAR *str, FILE *out)
{
    return _fwrite(str, wcslen(str), out);
}
int putc(WCHAR c, FILE *out)
{
    return _fwrite(&c, 1, out) == -1 ? -1 :  c;
}
int putchar(WCHAR c)
{
    return putc(c, stdout);
}
void abort(void) {
    ExitProcess(128 + 6);
}
LPWSTR *CommandLineToArgv(LPWSTR lpCmdLine, int *pNumArgs)
{
    LPWSTR out = lpCmdLine;
    LPWSTR cmd = out;
    unsigned MaxEntries = 4;
    unsigned backslashes = 0;
    int in_quotes = 0;
    int empty = 1;
    LPWSTR *cmds;
    *pNumArgs = 0;
    cmds = (LPWSTR*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(LPWSTR) * MaxEntries);
    while (1) {
        WCHAR c = *lpCmdLine++;
        switch (c) {
        case 0:
            if (!empty) {
                *out++ = '\0';
                cmds[(*pNumArgs)++] = cmd;
            }
            cmds[*pNumArgs] = NULL;
            return cmds;
        default:
            *out++ = c;
            empty = 0;
            break;
        case '"':
            out -= backslashes / 2; // remove half of the backslashes
            if (backslashes % 2)
                *(out - 1) = '"'; // replace \ with "
            else
                in_quotes = !in_quotes; // treat as quote delimater
            empty = 0;
            break;
        case '\t':
        case ' ':
            if (in_quotes) {
                *out++ = c;
            } else if (!empty) {
                *out++ = '\0';
                cmds[(*pNumArgs)++] = cmd;
                cmd = out;
                empty = 1;
                if (*pNumArgs >= MaxEntries - 1) {
                    MaxEntries *= 2;
                    cmds = (LPWSTR*)HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, cmds, sizeof(LPWSTR) * MaxEntries);
                }
            }
        }
        if (c == '\\')
            backslashes++;
        else
            backslashes = 0;
    }
}
int mainCRTStartup(void) // actually is WINAPI without mangling (but ABI is the same as cdecl)
{
    int argc;
    LPWSTR *argv;
    int retcode;
    DWORD mode;
    // setup
    _stdout.fd = GetStdHandle(STD_OUTPUT_HANDLE);
    _stdout.isconsole = GetConsoleMode(_stdout.fd, &mode);
    _stderr.fd = GetStdHandle(STD_ERROR_HANDLE);
    _stderr.isconsole = GetConsoleMode(_stderr.fd, &mode);
    argv = CommandLineToArgv(GetCommandLine(), &argc);
    // main
    retcode = main(argc, (const WCHAR **)argv);
    ExitProcess(retcode);
}

#else
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
typedef char WCHAR;
#define T(str) str

#if defined(__APPLE__)
#define LIBWHICH_LD_LIBRARY_PATH "DYLD_LIBRARY_PATH"
#else
#define LIBWHICH_LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#endif

#endif

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

typedef const WCHAR* STR;

void int2str(WCHAR *out, int d)
{
    WCHAR *end = out;
    int neg = (d < 0);
    do {
    int c = d % 10;
    if (neg)
            c = -c;
        *end++ = '0' + c;
        d /= 10;
    } while (d != 0);
    if (neg)
        *end++ = '-';
    *end-- = '\0';
    while (end > out) {
        WCHAR c = *out;
        *out++ = *end;
        *end-- = c;
    }
}

int streq(STR a, STR b) {
    if (a == b)
        return 1;
    while (*a != '\0') {
        if (*a++ != *b++)
            return 0;
    }
    return (*b == '\0');
}

struct vector_t {
    STR *data;
    size_t length;
};

#if defined(__APPLE__)
#include <mach-o/dyld.h>

struct vector_t dllist(void)
{
    struct vector_t dynamic_libraries;
    dynamic_libraries.length = _dyld_image_count();
    dynamic_libraries.data = (const char**)malloc(sizeof(char*) * dynamic_libraries.length);
    for (size_t i = 0; i < dynamic_libraries.length; i++) {
        const char *name = _dyld_get_image_name(i);
        dynamic_libraries.data[i] = name;
    }
    return dynamic_libraries;
}
const char *dlpath(void *handle, struct vector_t name)
{
    for (size_t i = 0; i < name.length; i++) {
        void *h2 = dlopen(name.data[i], RTLD_LAZY | RTLD_NOLOAD);
        if (h2)
            dlclose(h2);
        // If the handle is the same as what was passed in (modulo mode bits), return this image name
        if (((intptr_t)handle & (-4)) == ((intptr_t)h2 & (-4)))
            return name.data[i];
    }
    return NULL;
}

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__ELF__)
#include <link.h>

int get_names(struct dl_phdr_info *info, size_t size, void *data)
{
    struct vector_t *dynamic_libraries = (struct vector_t*)data;
    size_t i = dynamic_libraries->length++;
    dynamic_libraries->data = (const char**)realloc(dynamic_libraries->data, sizeof(char*) * dynamic_libraries->length);
    dynamic_libraries->data[i] = info->dlpi_name;
    return 0;
}
struct vector_t dllist(void)
{
    struct vector_t dynamic_libraries = {NULL, 0};
    dl_iterate_phdr(get_names, (void*)&dynamic_libraries);
    return dynamic_libraries;
}

#if (defined(__linux__) || defined(__FreeBSD__)) && !defined(ANDROID) && !defined(__ANDROID__) // Use `dlinfo` API, when supported
const char *dlpath(void *handle, struct vector_t name)
{
    struct link_map *map;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    if (map)
        return map->l_name;
    return NULL;
}
#else
const char *dlpath(void *handle, struct vector_t name)
{
    for (size_t i = 0; i < name.length; i++) {
        void *h2 = dlopen(name.data[i], RTLD_LAZY);
        if (h2)
            dlclose(h2);
        // If the handle is the same as what was passed in, return this image name
        if (handle == h2)
            return name.data[i];
    }
    return NULL;
}
#endif

#elif defined(_WIN32)

const STR dlerror(void) {
    STR errmsg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(),
        0, (LPWSTR) &errmsg, 0, NULL);
    return errmsg;
}

HMODULE dlopen(const STR path, int flags)
{
    return LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

const STR dlpath(HMODULE handle, struct vector_t name)
{
    DWORD AllocSize = 64;
    while (1) {
        WCHAR *hFilename;
        DWORD Size;
        hFilename = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(WCHAR) * AllocSize);
        Size = GetModuleFileName(handle, hFilename, AllocSize);
        if (Size < AllocSize)
            return hFilename;
        HeapFree(GetProcessHeap(), 0, hFilename);
        AllocSize *= 2;
    }
}

struct vector_t dllist(void)
{
    struct vector_t dynamic_libraries = {NULL, 0};
    HMODULE _hModules[32];
    HMODULE *hModules = _hModules;
    DWORD Needed;
    if (!EnumProcessModulesEx(GetCurrentProcess(), hModules, sizeof(_hModules), &Needed, LIST_MODULES_ALL))
        return dynamic_libraries;
    if (Needed > sizeof(_hModules)) {
        hModules = (HMODULE*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, Needed);
        if (!EnumProcessModulesEx(GetCurrentProcess(), hModules, Needed, &Needed, LIST_MODULES_ALL)) {
            HeapFree(GetProcessHeap(), 0, hModules);
            return dynamic_libraries;
        }
    }
    dynamic_libraries.length = Needed / sizeof(hModules[0]);
    dynamic_libraries.data = (STR*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(STR) * dynamic_libraries.length);
    for (size_t i = 0; i < dynamic_libraries.length; i++) {
        dynamic_libraries.data[i] = dlpath(hModules[i], dynamic_libraries);
    }
    if (hModules != _hModules)
        HeapFree(GetProcessHeap(), 0, hModules);
    return dynamic_libraries;
}

#else
#error "dllist not defined for platform"
#endif

int main(int argc, STR *argv)
{
    char opt = 0;
    STR libname;
    if (argc == 2) {
        libname = argv[1];
    } else if (argc == 3) {
        libname = argv[2];
        if (argv[1][0] == '-')
            opt = argv[1][1];
        if (opt == '\0' || argv[1][2] != '\0' ||
            !(opt == 'p' || opt == 'a' || opt == 'd')) {
            fputs(T("expected first argument to specify an output option:\n")
                  T("  -p  library path\n")
                  T("  -a  all dependencies\n")
                  T("  -d  direct dependencies\n"),
                  stdout);
            return 1;
        }
    } else {
        WCHAR digits[33];
        fputs(T("expected 1 argument specifying library to open (got "), stdout);
        int2str(digits, argc - 1);
        fputs(digits, stdout);
        fputs(T(")\n"), stdout);
        return 1;
    }
#ifndef _WIN32
    // If LIBWHICH_LIBRARY_PATH is set, re-run the program with LD_LIBRARY_PATH
    // or DYLD_LIBRARY_PATH set instead, so dlopen picks up these library paths.
    const char *libwhich_path = getenv("LIBWHICH_LIBRARY_PATH");
    if (libwhich_path) {
        setenv(LIBWHICH_LD_LIBRARY_PATH, libwhich_path, 1);
        unsetenv("LIBWHICH_LIBRARY_PATH");
        if (execvp(argv[0], (char **)argv) == -1) {
            perror("libwhich: execvp");
            return 1;
        }
    }
#endif
    struct vector_t before = dllist();
    void *lib = dlopen(libname, RTLD_LAZY);
    if (!lib) {
        fputs(T("failed to open library: "), stdout);
#if defined(_WIN32)
        fputs(T("LoadLibrary("), stdout);
        fputs(libname, stdout);
        fputs(T("): "), stdout);
#elif defined(__OpenBSD__)
        fputs(libname, stdout);
        fputs(" ", stdout);
#endif
        fputs(dlerror(), stdout);
        putchar('\n');
        return 1;
    }
    struct vector_t after = dllist();
    const STR name = dlpath(lib, after);
    switch ((char)opt) {
    case 0:
        fputs(T("library:\n  "), stdout);
        fputs(name ? name : T(""), stdout);
        fputs(T("\n\n"), stdout);
        fputs(T("dependencies:\n"), stdout);
        for (size_t i = 1; i < after.length; i++) {
            const STR depname = after.data[i];
            if (depname[0] == '\0')
                continue;
            int new = 1;
            for (size_t j = 0; new && j < before.length; j++) {
                if (streq(before.data[j], depname))
                    new = 0;
            }
            putchar(new ? '+' : ' ');
            putchar(' ');
            fputs(depname, stdout);
            putchar('\n');
        }
        break;
    case 'p':
        if (name)
            fputs(name, stdout);
        break;
    case 'a':
    case 'd':
        for (size_t i = 1; i < after.length; i++) {
            const STR depname = after.data[i];
            if (depname[0] == '\0')
                continue;
            int new = !streq(depname, name);
            for (size_t j = 0; new && j < before.length; j++) {
                if (streq(before.data[j], depname))
                    new = 0;
            }
            if (opt == 'a' || new) {
                fputs(depname, stdout);
                putchar('\0');
            }
        }
        break;
    default:
        abort(); // unreachable
    }
    return 0;
}
