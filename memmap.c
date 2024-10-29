// from: https://github.com/alex-s168/allib/blob/main/kallok/virtual.c

#include "internal.h"

#ifdef _WIN32
VirtAlloc virtualMap(int *err, const char *file) {
    VirtAlloc alloc;

    alloc.hFile = CreateFileA(file, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, 0);
    if (alloc.hFile == INVALID_HANDLE_VALUE) {
        *err = 1;
        return alloc;
    }

    LARGE_INTEGER liFileSize;

    if (!GetFileSizeEx(alloc.hFile, &liFileSize)) {
        CloseHandle(alloc.hFile);
        *err = 2;
        return alloc;
    }

    if (liFileSize.QuadPart == 0) {
        CloseHandle(alloc.hFile);
        *err = 3;
        return alloc;
    }

    alloc.hMap = CreateFileMapping(
        alloc.hFile,
        NULL,
        PAGE_READWRITE,
        0, 0,
        NULL);
    if (alloc.hMap == 0) {
        CloseHandle(alloc.hFile);
        *err = 4;
        return alloc;
    }

    alloc.data = MapViewOfFile(
        alloc.hMap,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        0);
    if (alloc.data == NULL) {
        CloseHandle(alloc.hMap);
        CloseHandle(alloc.hFile);
        *err = 5;
        return alloc;
    }
    
    alloc.size = liFileSize.QuadPart;

    *err = 0;
    return alloc;
}

void virtualUnmap(VirtAlloc alloc) {
    UnmapViewOfFile(alloc.data);
    CloseHandle(alloc.hMap);
    CloseHandle(alloc.hFile);
}

#else
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

VirtAlloc virtualMap(int *err, const char *file) {
    VirtAlloc alloc;
    struct stat sb;
    alloc.fd = open(file, O_RDWR);
    fstat(alloc.fd, &sb);
    alloc.size = sb.st_size;

    alloc.data = mmap(NULL, alloc.size, PROT_READ|PROT_WRITE,
                      MAP_SHARED, alloc.fd, 0);

    *err = (alloc.data == MAP_FAILED);

    return alloc;
}

void virtualUnmap(VirtAlloc alloc) {
    munmap(alloc.data, alloc.size);
}

#endif

#include <stdio.h>
#include <unistd.h>

VirtAlloc virtualMapInit(int *err, const char *path, size_t initLen, void* initData)
{
    if (access(path, F_OK)) {
        FILE* f = fopen(path, "w");
        if (f == NULL) {
            *err = 1;
            return (VirtAlloc) {0};
        }
        fwrite(initData, initLen, 1, f);
        fclose(f);
    }

    return virtualMap(err, path);
}
