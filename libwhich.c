#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <dlfcn.h>

struct vector_t {
    const char **data;
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
#else
#error "dllist not defined for platform"
#endif

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("expected 1 argument specifying library to open (got %d)\n", argc - 1);
        return 1;
    }
    const char *libname = argv[1];
    struct vector_t before = dllist();
    void *lib = dlopen(libname, RTLD_LAZY);
    if (!lib) {
        printf("failed to open library: %s\n", dlerror());
        return 1;
    }
    struct vector_t after = dllist();
    const char *name = dlpath(lib, after);
    printf("library:\n  %s\n\n", name ? name : "");
    printf("dependencies:\n");
    for (size_t i = 0; i < after.length; i++) {
        const char *name = after.data[i];
        int new = 1;
        for (size_t j = 0; new && j < before.length; j++) {
            if (before.data[j] == name)
                new = 0;
        }
        printf("%c %s\n", new ? '+' : ' ', name);
    }
    return 0;
}
