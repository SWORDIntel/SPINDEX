/**
 * @file mmap_reader.h
 * @brief Memory-mapped file reader for high-performance I/O
 *
 * Provides fast file reading using mmap() instead of read() syscalls.
 * Significantly faster for large files and sequential access patterns.
 */

#ifndef SPINDEX_MMAP_READER_H
#define SPINDEX_MMAP_READER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory-mapped file handle
 */
typedef struct {
    void* data;           // Mapped memory region
    size_t size;          // File size in bytes
    int fd;               // File descriptor
    const char* filepath; // Path to file (for logging)
    bool is_mapped;       // Whether mmap succeeded
} MmapFile;

/**
 * @brief Open and memory-map a file for reading
 *
 * @param filepath Path to file to map
 * @param file Output MmapFile structure
 * @return true on success, false on error
 *
 * @note Call mmap_file_close() when done to unmap and close
 */
bool mmap_file_open(const char* filepath, MmapFile* file);

/**
 * @brief Close and unmap a memory-mapped file
 *
 * @param file MmapFile to close
 */
void mmap_file_close(MmapFile* file);

/**
 * @brief Get pointer to mapped data
 *
 * @param file MmapFile handle
 * @return Pointer to mapped data, or NULL if not mapped
 */
const void* mmap_file_data(const MmapFile* file);

/**
 * @brief Get size of mapped file
 *
 * @param file MmapFile handle
 * @return Size in bytes, or 0 if not mapped
 */
size_t mmap_file_size(const MmapFile* file);

/**
 * @brief Check if file is successfully mapped
 *
 * @param file MmapFile handle
 * @return true if mapped, false otherwise
 */
bool mmap_file_is_mapped(const MmapFile* file);

/**
 * @brief Advise kernel about access pattern
 *
 * @param file MmapFile handle
 * @param sequential true for sequential access, false for random
 * @return true on success, false on error
 */
bool mmap_file_advise(MmapFile* file, bool sequential);

#ifdef __cplusplus
}
#endif

#endif // SPINDEX_MMAP_READER_H
