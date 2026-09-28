/**********************************************************************************************
*
*   dirent.h stub for ESP-IDF
*
*   Provides minimal stubs for directory functions not available/needed on ESP-IDF
*   This prevents compilation errors when raylib tries to include <dirent.h>
*
**********************************************************************************************/

#ifndef DIRENT_H_ESP_IDF_STUB
#define DIRENT_H_ESP_IDF_STUB

// Stub types - not actually used on ESP-IDF
typedef struct DIR DIR;
struct dirent {
    char d_name[256];
};

// Stub functions - return NULL/error to indicate not supported
static inline DIR *opendir(const char *name) { return NULL; }
static inline struct dirent *readdir(DIR *dirp) { return NULL; }
static inline int closedir(DIR *dirp) { return -1; }

#endif // DIRENT_H_ESP_IDF_STUB
