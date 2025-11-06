/**
 * @file mmap_reader.c
 * @brief Memory-mapped file reader implementation
 */

#include "mmap_reader.h"
#include "utils.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

bool mmap_file_open(const char* filepath, MmapFile* file) {
    if (!filepath || !file) {
        LOG_ERROR("mmap_file_open: NULL filepath or file structure");
        return false;
    }

    // Initialize structure
    memset(file, 0, sizeof(MmapFile));
    file->fd = -1;
    file->filepath = filepath;

    // Open file
    file->fd = open(filepath, O_RDONLY);
    if (file->fd < 0) {
        LOG_ERROR("mmap_file_open: Failed to open '%s': %s", filepath, strerror(errno));
        return false;
    }

    // Get file size
    struct stat st;
    if (fstat(file->fd, &st) < 0) {
        LOG_ERROR("mmap_file_open: Failed to stat '%s': %s", filepath, strerror(errno));
        close(file->fd);
        file->fd = -1;
        return false;
    }

    file->size = st.st_size;

    // Handle empty files
    if (file->size == 0) {
        LOG_WARN("mmap_file_open: File '%s' is empty", filepath);
        file->data = NULL;
        file->is_mapped = false;
        return true; // Not an error, but no mapping
    }

    // Memory-map the file
    file->data = mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, file->fd, 0);
    if (file->data == MAP_FAILED) {
        LOG_ERROR("mmap_file_open: Failed to mmap '%s' (%zu bytes): %s",
                  filepath, file->size, strerror(errno));
        file->data = NULL;
        close(file->fd);
        file->fd = -1;
        return false;
    }

    file->is_mapped = true;

    LOG_DEBUG("mmap_file_open: Successfully mapped '%s' (%zu bytes)", filepath, file->size);
    return true;
}

void mmap_file_close(MmapFile* file) {
    if (!file) return;

    // Unmap if mapped
    if (file->is_mapped && file->data && file->data != MAP_FAILED) {
        if (munmap(file->data, file->size) < 0) {
            LOG_WARN("mmap_file_close: Failed to munmap '%s': %s",
                     file->filepath ? file->filepath : "unknown",
                     strerror(errno));
        } else {
            LOG_DEBUG("mmap_file_close: Unmapped '%s'",
                      file->filepath ? file->filepath : "unknown");
        }
    }

    // Close file descriptor
    if (file->fd >= 0) {
        close(file->fd);
    }

    // Clear structure
    memset(file, 0, sizeof(MmapFile));
    file->fd = -1;
}

const void* mmap_file_data(const MmapFile* file) {
    if (!file || !file->is_mapped) return NULL;
    return file->data;
}

size_t mmap_file_size(const MmapFile* file) {
    if (!file) return 0;
    return file->size;
}

bool mmap_file_is_mapped(const MmapFile* file) {
    if (!file) return false;
    return file->is_mapped;
}

bool mmap_file_advise(MmapFile* file, bool sequential) {
    if (!file || !file->is_mapped || !file->data) {
        return false;
    }

    int advice = sequential ? MADV_SEQUENTIAL : MADV_RANDOM;

    if (madvise(file->data, file->size, advice) < 0) {
        LOG_WARN("mmap_file_advise: Failed to set advice for '%s': %s",
                 file->filepath ? file->filepath : "unknown",
                 strerror(errno));
        return false;
    }

    // Also hint that we'll need the data soon
    if (sequential) {
        madvise(file->data, file->size, MADV_WILLNEED);
    }

    LOG_DEBUG("mmap_file_advise: Set %s access pattern for '%s'",
              sequential ? "sequential" : "random",
              file->filepath ? file->filepath : "unknown");

    return true;
}
