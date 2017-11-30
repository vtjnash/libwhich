#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define PSAPI_VERSION 2 // for EnumProcessModulesEx
#include <windows.h>
#include <psapi.h>
#define T(str) L##str
#define main wmain
int main(int argc, const WCHAR **argv);
typedef struct {
    HANDLE fd;
} FILE;
static FILE _stdout = { INVALID_HANDLE_VALUE };
static FILE _stderr = { INVALID_HANDLE_VALUE };
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;
int fputs(const WCHAR *str, FILE *out)
{
    DWORD written;
    if (WriteFile(out->fd, str, sizeof(WCHAR) * wcslen(str), &written, NULL))
        return written;
    return -1;
}
int putchar(WCHAR c)
{
    DWORD written;
    if (WriteFile(stdout->fd, &c, sizeof(WCHAR), &written, NULL))
        return c;
    return -1;
}
void abort() {
    ExitProcess(128 + 6);
}
LPWSTR *CommandLineToArgvW(LPWSTR lpCmdLine, int *pNumArgs)
{
    LPWSTR cmd = lpCmdLine;
    unsigned MaxEntries = 4;
    LPWSTR *cmds;
    *pNumArgs = 0;
    cmds = (LPWSTR*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(LPWSTR) * MaxEntries);
    while (1) {
        switch (*lpCmdLine) {
        case 0:
            cmds[(*pNumArgs)++] = cmd;
            cmds[*pNumArgs] = NULL;
            return cmds;
        case '\\':
        case '"':
        case '\'':
        default:
            break;
        case ' ':
            *lpCmdLine = '\0';
            cmds[(*pNumArgs)++] = cmd;
            cmd = lpCmdLine + 1;
            if (*pNumArgs >= MaxEntries - 1) {
                MaxEntries *= 2;
                cmds = (LPWSTR*)HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, cmds, sizeof(LPWSTR) * MaxEntries);
            }
        }
        lpCmdLine++;
    }
}
int WINAPI mainCRTStartup(void)
{
    int argc;
    LPWSTR *argv;
    int retcode;
    // setup
    _stdout.fd = GetStdHandle(STD_OUTPUT_HANDLE);
    _stderr.fd = GetStdHandle(STD_ERROR_HANDLE);
    argv = CommandLineToArgvW(GetCommandLine(), &argc);
    // main
    retcode = main(argc, (const WCHAR **)argv);
    ExitProcess(retcode);
}

#else
#include <dlfcn.h>
#include <stdio.h>
typedef char WCHAR;
#define T(str) str
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
    while (*a) {
        if (*a++ != *b++)
            return 0;
    }
    return 1;
}

struct vector_t {
    STR *data;
    size_t length;
};

#if defined(__APPLE__)
#include <mach-o/dyld.h>

struct vector_t dllist()
{
    struct vector_t dynamic_libraries;
    dynamic_libraries.length = _dyld_image_count() - 1;
    dynamic_libraries.data = (const char**)malloc(sizeof(char*) * dynamic_libraries.length);
    for (size_t i = 0; i < dynamic_libraries.length; i++) {
        // start at 1 instead of 0 to skip self
        const char *name = _dyld_get_image_name(i + 1);
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

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__ELF__)
#include <link.h>

int get_names(struct dl_phdr_info *info, size_t size, void *data)
{
    struct vector_t *dynamic_libraries = (struct vector_t*)data;
    if (info->dlpi_name[0]) {
        size_t i = (++dynamic_libraries->length);
        dynamic_libraries->data = (const char**)realloc(dynamic_libraries->data, sizeof(char*) * i);
        dynamic_libraries->data[i - 1] = info->dlpi_name;
    }
    return 0;
}
struct vector_t dllist()
{
    struct vector_t dynamic_libraries = {NULL, 0};
    dl_iterate_phdr(get_names, (void*)&dynamic_libraries);
    return dynamic_libraries;
}

const char *dlpath(void *handle, struct vector_t name)
{
    struct link_map *map;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);
    if (map)
        return map->l_name;
    return NULL;
}

#elif defined(_WIN32)

const STR dlerror() {
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

struct vector_t dllist()
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
    dynamic_libraries.length = Needed / sizeof(hModules[0]) - 1;
    dynamic_libraries.data = (STR*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(STR) * dynamic_libraries.length);
    for (size_t i = 0; i < dynamic_libraries.length; i++) {
        // start at 1 instead of 0 to skip self
        dynamic_libraries.data[i] = dlpath(hModules[i + 1], dynamic_libraries);
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
    struct vector_t before = dllist();
    void *lib = dlopen(libname, RTLD_LAZY);
    if (!lib) {
        fputs(T("failed to open library: "), stdout);
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
        for (size_t i = 0; i < after.length; i++) {
            const STR depname = after.data[i];
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
        for (size_t i = 0; i < after.length; i++) {
            const STR depname = after.data[i];
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
