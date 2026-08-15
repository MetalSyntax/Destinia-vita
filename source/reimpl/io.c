/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <psp2/kernel/threadmgr.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/utils.h"

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

#include <psp2/io/stat.h>

void ensure_destinia_dirs(void) {
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/destinia", 0777);
    sceIoMkdir("ux0:data/destinia/saves", 0777);
    sceIoMkdir("ux0:data/destinia/assets", 0777);
    sceIoMkdir("ux0:data/destinia/sound", 0777);
    sceIoMkdir("ux0:data/destinia/logs", 0777);
}

static const char *destinia_redirect_path(const char *path, char *buf, size_t buf_size) {
    if (!path) return path;
    const char *prefix = "/data/data/game.destiniaeng/files/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(path, prefix, prefix_len) == 0) {
        snprintf(buf, buf_size, DATA_PATH "saves/%s", path + prefix_len);
        return buf;
    }
    return path;
}

FILE * fopen_soloader(const char * filename, const char * mode) {
    char rpath[512];
    const char *real_file = destinia_redirect_path(filename, rpath, sizeof(rpath));

    if (strcmp(real_file, "/proc/cpuinfo") == 0) {
        return fopen_soloader("app0:/cpuinfo", mode);
    } else if (strcmp(real_file, "/proc/meminfo") == 0) {
        return fopen_soloader("app0:/meminfo", mode);
    }

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(real_file, mode);
#else
    FILE* ret = fopen(real_file, mode);
#endif

    if (ret)
        l_debug("fopen(%s -> %s, %s): %p", filename, real_file, mode, ret);
    else
        l_warn("fopen(%s -> %s, %s): %p", filename, real_file, mode, ret);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    char rpath[512];
    const char *real_path = destinia_redirect_path(path, rpath, sizeof(rpath));

    if (strcmp(real_path, "/proc/cpuinfo") == 0) {
        return open_soloader("app0:/cpuinfo", oflag);
    } else if (strcmp(real_path, "/proc/meminfo") == 0) {
        return open_soloader("app0:/meminfo", oflag);
    }

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(real_path, oflag, mode);
    if (ret >= 0)
        l_debug("open(%s -> %s, %x): %i", path, real_path, oflag, ret);
    else
        l_warn("open(%s -> %s, %x): %i", path, real_path, oflag, ret);
    return ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("fstat(%i): %i", fd, res);
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    char rpath[512];
    const char *real_path = destinia_redirect_path(path, rpath, sizeof(rpath));

    struct stat st;
    int res = stat(real_path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s -> %s): %i", path, real_path, res);
    return res;
}

int remove_soloader(const char * path) {
    char rpath[512];
    const char *real_path = destinia_redirect_path(path, rpath, sizeof(rpath));
    int ret = remove(real_path);
    l_debug("remove(%s -> %s): %i", path, real_path, ret);
    return ret;
}

int fclose_soloader(FILE * f) {
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

int close_soloader(int fd) {
    int ret = close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    DIR* ret = opendir(_pathname);
    l_debug("opendir(\"%s\"): %p", _pathname, ret);
    return ret;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;

    struct dirent* ret = readdir(dir);
    l_debug("readdir(%p): %p", dir, ret);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_warn("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}
