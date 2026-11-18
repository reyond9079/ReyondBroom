// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int rename(const char *source, const char *target) {
    int (*rename_file)(const char *, const char *) = dlsym(RTLD_NEXT, "rename");
    if (!rename_file) {
        errno = ENOSYS;
        return -1;
    }
    const int result = rename_file(source, target);
    const char *folder = getenv("RB_TEXTURE_TEST_INTERRUPT_FOLDER");
    const char *extension = getenv("RB_TEXTURE_TEST_INTERRUPT_EXTENSION");
    if (result == 0 && folder && extension) {
        const size_t path_length = strlen(target);
        const size_t folder_length = strlen(folder);
        const size_t extension_length = strlen(extension);
        if (path_length > folder_length + extension_length &&
            strncmp(target, folder, folder_length) == 0 &&
            target[folder_length] == '/' &&
            strcmp(target + path_length - extension_length, extension) == 0)
            _exit(91);
    }
    return result;
}
