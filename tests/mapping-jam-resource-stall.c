// SPDX-License-Identifier: GPL-2.0-or-later
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void stall_receipt(const char *path, const char *mode)
{
    const char *marker = getenv("RB_QA_STALL_MARKER");
    if (!marker || !strchr(mode,'w') || !strstr(path,"/packs/") || !strstr(path,"/.verified"))
        return;
    const int once = open(marker,O_CREAT | O_EXCL | O_WRONLY,0600);
    if (once < 0) return;
    close(once);
    fprintf(stderr,"RB_QA simulated unresponsive receipt write\n");
    sleep(95);
}

FILE *fopen(const char *path, const char *mode)
{
    FILE *(*real_open)(const char *, const char *) = dlsym(RTLD_NEXT,"fopen");
    stall_receipt(path,mode);
    return real_open(path,mode);
}

FILE *fopen64(const char *path, const char *mode)
{
    FILE *(*real_open)(const char *, const char *) = dlsym(RTLD_NEXT,"fopen64");
    stall_receipt(path,mode);
    return real_open(path,mode);
}
