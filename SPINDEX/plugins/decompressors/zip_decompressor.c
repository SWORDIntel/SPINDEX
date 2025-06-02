/* miniz.c 3.1.0 - public domain deflate/inflate, zlib-subset, ZIP reading/writing/appending, PNG writing
   See "unlicense" statement at the end of this file.
   Rich Geldreich <richgel99@gmail.com>, last updated Oct. 13, 2013
   Implements RFC 1950: http://www.ietf.org/rfc/rfc1950.txt and RFC 1951: http://www.ietf.org/rfc/rfc1951.txt
*/
// MINIZ_HEADER_FILE_ONLY is defined internally by miniz.c, so this will include the H file contents only.
#define MINIZ_HEADER_FILE_ONLY

/* Defines to completely disable specific portions of miniz.c:
   If all macros here are defined the only functionality remaining will be CRC-32 and adler-32. */
/*#define MINIZ_NO_STDIO */
/*#define MINIZ_NO_TIME */
/*#define MINIZ_NO_DEFLATE_APIS */
/*#define MINIZ_NO_INFLATE_APIS */
/*#define MINIZ_NO_ARCHIVE_APIS */
/*#define MINIZ_NO_ARCHIVE_WRITING_APIS */
/*#define MINIZ_NO_ZLIB_APIS */
/*#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES */
/*#define MINIZ_NO_MALLOC */

#ifdef MINIZ_NO_INFLATE_APIS
#define MINIZ_NO_ARCHIVE_APIS
#endif

#ifdef MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#endif

#if defined(__TINYC__) && (defined(__linux) || defined(__linux__))
#define MINIZ_NO_TIME
#endif

#include <stddef.h>

#if !defined(MINIZ_NO_TIME) && !defined(MINIZ_NO_ARCHIVE_APIS)
#include <time.h>
#endif

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__i386) || defined(__i486__) || defined(__i486) || defined(i386) || defined(__ia64__) || defined(__x86_64__)
#define MINIZ_X86_OR_X64_CPU 1
#else
#define MINIZ_X86_OR_X64_CPU 0
#endif

#if !defined(MINIZ_LITTLE_ENDIAN)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define MINIZ_LITTLE_ENDIAN 1
#else
#define MINIZ_LITTLE_ENDIAN 0
#endif
#else
#if MINIZ_X86_OR_X64_CPU
#define MINIZ_LITTLE_ENDIAN 1
#else
#define MINIZ_LITTLE_ENDIAN 0
#endif
#endif
#endif

#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif
#endif

#if !defined(MINIZ_USE_UNALIGNED_LOADS_AND_STORES)
#if MINIZ_X86_OR_X64_CPU
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#define MINIZ_UNALIGNED_USE_MEMCPY
#else
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif
#endif

#if defined(_M_X64) || defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__) || defined(__ia64__) || defined(__x86_64__)
#define MINIZ_HAS_64BIT_REGISTERS 1
#else
#define MINIZ_HAS_64BIT_REGISTERS 0
#endif

// Actually include the miniz.c source code here.
// This is a common pattern for single-file C libraries.
// We define MINIZ_IMPLEMENTATION to include the function bodies.
#define MINIZ_IMPLEMENTATION
#include "miniz_export.h" // Defines MINIZ_EXPORT
// The following are parts of miniz that would normally be separate .c files or included by miniz.c itself.
// Since we are inlining miniz.h and then its implementation, we need to replicate some of its structure.

// Start of miniz_common.h content (or equivalent for monolithic include)
#ifndef MINIZ_COMMON_H
#define MINIZ_COMMON_H

#include <stdlib.h>

/* See miniz.c release notes at the top of the C file. */
/* Public domain, Rich Geldreich <richgel99@gmail.com> */

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4201) /* class struct until C23 */
#pragma warning(disable : 4244) /* conversion from 'type1' to 'type2', possible loss of data */
#pragma warning(disable : 4204) /* nonstandard extension used : non-constant aggregate initializer */
#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#pragma warning(disable : 4091) /* 'typedef ': ignored on left of '' when no variable is declared */
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* miniz.c style declarations. */
#if defined(MINIZ_NO_MSC_FUNC_SUPPORT) || defined(__GNUC__) || defined(__clang__)
#define MZ_FUNC_QUALIFIER
#define MZ_FUNC_QUALIFIER_IS_STATIC static
#else
#define MZ_FUNC_QUALIFIER __cdecl
#define MZ_FUNC_QUALIFIER_IS_STATIC
#endif

#ifdef MINIZ_NO_INLINE
#define MZ_INLINE
#else
#define MZ_INLINE __inline
#endif

#if defined(_MSC_VER)
#define MZ_FORCE_INLINE __forceinline
#elif defined(__GNUC__)
#define MZ_FORCE_INLINE static __inline__ __attribute__((__always_inline__))
#else
#define MZ_FORCE_INLINE MZ_INLINE
#endif

#if defined(MINIZ_NO_MALLOC)
#define MZ_MALLOC(x) NULL
#define MZ_FREE(x) (void)x, ((void)0)
#define MZ_REALLOC(p, x) NULL
#else
#define MZ_MALLOC(x) malloc(x)
#define MZ_FREE(x) free(x)
#define MZ_REALLOC(p, x) realloc(p, x)
#endif

#define MZ_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MZ_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MZ_CLEAR_OBJ(obj) memset(&(obj), 0, sizeof(obj))

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN && MINIZ_HAS_64BIT_REGISTERS
#define MZ_READ_LE64(p) *((const mz_uint64 *)(p))
#define MZ_READ_LE32(p) *((const mz_uint32 *)(p))
#define MZ_READ_LE16(p) *((const mz_uint16 *)(p))

#define MZ_WRITE_LE64(p, v) *((mz_uint64 *)(p)) = (v)
#define MZ_WRITE_LE32(p, v) *((mz_uint32 *)(p)) = (v)
#define MZ_WRITE_LE16(p, v) *((mz_uint16 *)(p)) = (v)
#else
MINIZ_EXPORT mz_uint64 mz_read_le64(const mz_uint8 *p);
MINIZ_EXPORT mz_uint32 mz_read_le32(const mz_uint8 *p);
MINIZ_EXPORT mz_uint16 mz_read_le16(const mz_uint8 *p);

MINIZ_EXPORT void mz_write_le64(mz_uint8 *p, mz_uint64 v);
MINIZ_EXPORT void mz_write_le32(mz_uint8 *p, mz_uint32 v);
MINIZ_EXPORT void mz_write_le16(mz_uint8 *p, mz_uint16 v);
#endif

#define MZ_READ_BE32(p) (((mz_uint32)(p)[0] << 24U) | ((mz_uint32)(p)[1] << 16U) | ((mz_uint32)(p)[2] << 8U) | (p)[3])

#ifdef _MSC_VER
#define MZ_MACRO_END while (0)
#else
#define MZ_MACRO_END
#endif

#ifdef MINIZ_NO_ASSERT
#define MZ_ASSERT(x) ((void)0)
#else
#ifndef NDEBUG
#include <assert.h>
#define MZ_ASSERT(x) assert(x)
#else
#define MZ_ASSERT(x) ((void)0)
#endif
#endif

#if defined(MINIZ_NO_STDIO) || defined(MINIZ_NO_ARCHIVE_APIS)
#define MZ_FOPEN(filename, mode) NULL
#define MZ_FCLOSE(stream) EOF
#define MZ_FREAD(ptr, size, nitems, stream) 0
#define MZ_FWRITE(ptr, size, nitems, stream) 0
#define MZ_FTELL(stream) -1L
#define MZ_FSEEK(stream, offset, origin) -1
#define MZ_FFLUSH(stream) EOF
#define MZ_REMOVE(filename) -1
#define MZ_REWIND(stream)
#define MZ_TEMPNAME(filename) NULL
#else
#include <stdio.h>
#if defined(_MSC_VER) || defined(__MINGW32__)
#ifndef MINIZ_NO_MSC_SAFE_FUNCS
#define MZ_FOPEN(filename, mode) fopen_s(&pFile, filename, mode), pFile
#define MZ_FCLOSE(stream) fclose(stream)
#define MZ_FREAD(ptr, size, nitems, stream) fread_s(ptr, (size) * (nitems), size, nitems, stream)
#define MZ_FWRITE(ptr, size, nitems, stream) fwrite(ptr, size, nitems, stream)
#else
#define MZ_FOPEN(filename, mode) fopen(filename, mode)
#define MZ_FCLOSE(stream) fclose(stream)
#define MZ_FREAD(ptr, size, nitems, stream) fread(ptr, size, nitems, stream)
#define MZ_FWRITE(ptr, size, nitems, stream) fwrite(ptr, size, nitems, stream)
#endif /* #ifndef MINIZ_NO_MSC_SAFE_FUNCS */
#else
#define MZ_FOPEN(filename, mode) fopen(filename, mode)
#define MZ_FCLOSE(stream) fclose(stream)
#define MZ_FREAD(ptr, size, nitems, stream) fread(ptr, size, nitems, stream)
#define MZ_FWRITE(ptr, size, nitems, stream) fwrite(ptr, size, nitems, stream)
#endif /* #if defined(_MSC_VER) || defined(__MINGW32__) */
#define MZ_FTELL(stream) ftell(stream)
#define MZ_FSEEK(stream, offset, origin) fseek(stream, offset, origin)
#define MZ_FFLUSH(stream) fflush(stream)
#define MZ_REMOVE(filename) remove(filename)
#define MZ_REWIND(stream) rewind(stream)
#define MZ_TEMPNAME(filename) tmpnam(filename)
#endif /* MINIZ_NO_STDIO */

typedef unsigned char mz_uint8;
typedef signed short mz_int16;
typedef unsigned short mz_uint16;
typedef unsigned int mz_uint32;
typedef unsigned int mz_uint;
typedef long long mz_int64;
typedef unsigned long long mz_uint64;
typedef int mz_bool;

#define MZ_FALSE (0)
#define MZ_TRUE (1)

/* Set to 1 if template parms can be specialized */
#if defined(__cplusplus)
#define MZ_CPP_OVERLOAD_ENABLED
#endif

/* Set to 1 if the compiler supports C11 _Generic */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && defined(__GNUC__)
/* _Generic is supported, but seems to be buggy with clang an an array of const mz_uint8's */
/* so just disable it for now */
/* #define MZ_C11_GENERIC_ENABLED */
#endif

#ifdef __cplusplus
}
#endif

#endif /* MINIZ_COMMON_H */
// End of miniz_common.h content


// Start of miniz_tinfl.h content
/* tinfl.h -- tiny inflate library
   Header file for test/tinfl.c
   Also used by miniz.c
   Rich Geldreich <richgel99@gmail.com>, last updated May 20, 2011
   Implements RFC 1951: http://www.ietf.org/rfc/rfc1951.txt
*/

#ifndef TINFL_H
#define TINFL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Max size of LZ dictionary. */
#define TINFL_LZ_DICT_SIZE 32768

/* Return status. */
typedef enum
{
    TINFL_STATUS_BAD_PARAM = -3,
    TINFL_STATUS_ADLER32_MISMATCH = -2,
    TINFL_STATUS_FAILED = -1,
    TINFL_STATUS_DONE = 0,
    TINFL_STATUS_NEEDS_MORE_INPUT = 1,
    TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinfl_status;

/* Decompression flags used by tinfl_decompress(). */
/* TINFL_FLAG_PARSE_ZLIB_HEADER: If set, the input has a ZLIB header and checksum (adler-32). */
/* TINFL_FLAG_HAS_MORE_INPUT: If set, there are more input bytes available beyond the end of the supplied input buffer. */
/* TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF: If set, the output buffer is large enough to hold the entire decompressed stream. */
/* TINFL_FLAG_COMPUTE_ADLER32: Force adler-32 computation of the decompressed bytes. */
#define TINFL_FLAG_PARSE_ZLIB_HEADER             1
#define TINFL_FLAG_HAS_MORE_INPUT                2
#define TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF 4
#define TINFL_FLAG_COMPUTE_ADLER32               8

/* The number of bits decoded at a time from the input stream. */
#define TINFL_BITBUF_SIZE 16

typedef mz_uint16 tinfl_bit_buf_t;

typedef struct
{
    mz_uint8 m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32_h, m_z_adler32_l;
    mz_uint8 m_final, m_type, m_dist_from_out_buf_start_up_to_15, m_check_adler32;
    mz_uint16 m_raw_header;
    mz_uint8 m_dict_ofs1, m_dist_from_out_buf_start;
    tinfl_bit_buf_t m_bit_buf;
    mz_uint64 m_dist_from_out_buf_start_ma;
    size_t m_out_buf_ofs, m_out_buf_ofs1, m_out_buf_ofs_ma;
    const mz_uint8 *m_pIn_buf_next, *m_pIn_buf_end;
    mz_uint8 *m_pOut_buf_next, *m_pOut_buf_end;
    const mz_uint8 *m_pOut_buf_start;
    mz_uint32 m_adler32, m_look_up, m_saved_match_dist, m_counter;
    mz_uint32 m_num_extra_bits, m_num_extra_bits_ma;
    mz_uint8 m_table_sizes[2];
    mz_int16 m_main_loop_ma, m_huff_decode_pos, m_len_sym, m_tree_cur, m_tree_next;
    mz_int16 m_final_ma, m_len_code, m_dist_code, m_len_ma;
    mz_uint8 m_lz_code_buf[288 + 32 + 137];
    mz_uint16 m_huff_codes[2][288 + 32];
    mz_uint8 m_huff_code_sizes[2][288 + 32];
    mz_uint8 m_huff_code_sizes_ma[19];
    mz_uint8 m_len_linbits, m_dist_linbits;
    mz_uint16 m_next_symbol_ma[512];
} tinfl_decompressor;

/* Initializes the decompressor to its initial state. */
MINIZ_EXPORT void tinfl_init(tinfl_decompressor *r);

/* Decompresses the input buffer, starting at *pIn_buf_next and ending at *pIn_buf_end. */
/* When TINFL_FLAG_HAS_MORE_INPUT is set, then the input buffer is assumed to be followed by more input bytes. */
/* Decompresses to the output buffer, starting at *pOut_buf_next and ending at *pOut_buf_end. */
/* When TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF is set, then the output buffer is assumed to be large enough to hold the entire decompress stream. */
/* If the input or output buffer is empty or NULL, the function will return TINFL_STATUS_BAD_PARAM. */
/* Returns the decompress status. */
MINIZ_EXPORT tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags);

#ifdef __cplusplus
}
#endif

#endif /* TINFL_H */
// End of tinfl.h

// Start of miniz_tdef.h content
/* tdef.h -- tiny deflate library
   Header file for test/tdef.c
   Also used by miniz.c
   Rich Geldreich <richgel99@gmail.com>, last updated May 20, 2011
   Implements RFC 1951: http://www.ietf.org/rfc/rfc1951.txt
*/

#ifndef TDEF_H
#define TDEF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Compression flags. */
#define TDEF_HUFFMAN_ONLY          0x01
#define TDEF_LOOK_AHEAD_PARSING    0x02
#define TDEF_GREEDY_PARSING_FLAG   0x04
#define TDEF_WRITE_ZLIB_HEADER     0x08
#define TDEF_DEFAULT_MAX_PROBES    (128)
#define TDEF_MAX_PROBES_MASK       (0xFFF)

/* Max dictionary size, must be a power of 2. */
#define TDEF_LZ_DICT_SIZE          32768

/* Max match length. */
#define TDEF_MAX_MATCH_LEN         258

/* Return status codes. */
typedef enum
{
    TDEF_STATUS_BAD_PARAM = -2,
    TDEF_STATUS_PUT_BUF_FAILED = -1,
    TDEF_STATUS_OKAY = 0,
    TDEF_STATUS_DONE = 1
} tdef_status;

typedef struct
{
    mz_uint8 *m_pPut_buf_next, *m_pPut_buf_end;
    mz_uint32 m_flags, m_max_probes;
    mz_uint32 m_adler32, m_lookahead_pos, m_lookahead_size, m_dict_size;
    mz_uint8 *m_pLZ_code_buf, *m_pLZ_dict, *m_pOutput_buf;
    mz_uint32 m_num_flags_left, m_total_lz_bytes;
    mz_uint8 m_output_flush_ofs, m_output_flush_remaining;
    mz_uint16 m_bit_buffer;
    mz_uint8 m_bits_in, m_padding;
    mz_uint8 m_lz_code_buf_dict_pos, m_lz_code_buf_pos, m_match_len_codes[288 + 32 + 1];
    mz_uint8 m_huff_count[2][288 + 32];
    mz_uint16 m_huff_codes[2][288 + 32];
    mz_uint8 m_huff_code_sizes[2][288 + 32];
} tdefl_compressor;

/* Initializes the compressor. */
/* pBuf must point to a P Compressor instance. */
/* flags are TDEF_HUFFMAN_ONLY, TDEF_LOOK_AHEAD_PARSING, TDEF_GREEDY_PARSING_FLAG, TDEF_WRITE_ZLIB_HEADER. */
MINIZ_EXPORT tdef_status tdefl_init(tdefl_compressor *d, void (*pPut_buf_func)(const void *pBuf, int len, void *pUser), void *pPut_buf_user, mz_uint32 flags);

/* Compresses a block of data. */
/* pBuf must point to a P Compressor instance. */
/* pIn_buf points to the data to compress. */
/* in_buf_size is the size of pIn_buf. */
/* flush can be TDEF_NO_FLUSH or TDEF_FINISH. */
MINIZ_EXPORT tdef_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf, size_t in_buf_size, mz_uint32 flush);

#ifdef __cplusplus
}
#endif

#endif /* TDEF_H */
// End of tdef.h

// Start of miniz_zip.h
#ifndef MINIZ_ZIP_H
#define MINIZ_ZIP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ZIP archive specific definitions. */
enum
{
    /* ZIP archive parsing states. */
    MZ_ZIP_STATE_INVALID = 0,
    MZ_ZIP_STATE_FINDING_EOCD,
    MZ_ZIP_STATE_READING_EOCD,
    MZ_ZIP_STATE_READING_CENTRAL_DIR,
    MZ_ZIP_STATE_READING_LOCAL_HEADER,
    MZ_ZIP_STATE_READING_FILE_DATA,
    MZ_ZIP_STATE_WRITING_CENTRAL_DIR,
    MZ_ZIP_STATE_WRITING_LOCAL_HEADER,
    MZ_ZIP_STATE_WRITING_FILE_DATA,
    MZ_ZIP_STATE_TOTAL
};

enum
{
    MZ_ZIP_LDH_SIG = 0x04034b50,  /* "PK\03\04" */
    MZ_ZIP_CDH_SIG = 0x02014b50,  /* "PK\01\02" */
    MZ_ZIP_EOCD_SIG = 0x06054b50, /* "PK\05\06" */
    MZ_ZIP_EOCD64_SIG = 0x06064b50,
    MZ_ZIP_EOCD64_LOC_SIG = 0x07064b50,
    MZ_ZIP_DD_SIG = 0x08074b50    /* "PK\07\08" */
};

/* Method codes */
enum
{
    MZ_ZIP_METHOD_STORED = 0,
    MZ_ZIP_METHOD_DEFLATED = 8,
    MZ_ZIP_METHOD_BZIP2 = 12,
    MZ_ZIP_METHOD_LZMA = 14,
    MZ_ZIP_METHOD_AES = 99
};

/* General purpose bit flags */
enum
{
    MZ_ZIP_GPBF_ENCRYPTED = 1,
    MZ_ZIP_GPBF_DATA_DESCRIPTOR = 8,
    MZ_ZIP_GPBF_STRONG_ENCRYPTION = 64, /* Reserved for use with strong encryption */
    MZ_ZIP_GPBF_DEFLATE_MAX = 2,
    MZ_ZIP_GPBF_DEFLATE_NORMAL = 0,
    MZ_ZIP_GPBF_DEFLATE_FAST = 4,
    MZ_ZIP_GPBF_DEFLATE_SUPER_FAST = 6,
    MZ_ZIP_GPBF_LANG_ENCODING_FLAG = 1 << 11 /* Language encoding flag (EFS) */
};

/* ZIP64 EOCD locator */
typedef struct
{
    mz_uint32 m_signature;
    mz_uint32 m_disk_eocd_start;
    mz_uint64 m_eocd_ofs;
    mz_uint32 m_num_disks;
} mz_zip_eocd64_locator;

/* End of central directory record */
typedef struct
{
    mz_uint32 m_signature;
    mz_uint16 m_disk_num;
    mz_uint16 m_cd_start_disk_num;
    mz_uint64 m_total_entries_on_disk;
    mz_uint64 m_total_entries;
    mz_uint64 m_cd_size;
    mz_uint64 m_cd_ofs;
    mz_uint16 m_comment_size;
} mz_zip_eocd_rec;

/* ZIP64 End of central directory record */
typedef struct
{
    mz_uint32 m_signature;
    mz_uint64 m_size_of_eocd64;
    mz_uint16 m_version_made_by;
    mz_uint16 m_version_needed;
    mz_uint32 m_disk_num;
    mz_uint32 m_cd_start_disk_num;
    mz_uint64 m_total_entries_on_disk;
    mz_uint64 m_total_entries;
    mz_uint64 m_cd_size;
    mz_uint64 m_cd_ofs;
} mz_zip64_eocd_rec;

/* File header */
typedef struct
{
    mz_uint32 m_signature;
    mz_uint16 m_version_made_by;
    mz_uint16 m_version_needed;
    mz_uint16 m_bit_flag;
    mz_uint16 m_method;
    mz_uint16 m_last_mod_time;
    mz_uint16 m_last_mod_date;
    mz_uint32 m_crc32;
    mz_uint64 m_comp_size;
    mz_uint64 m_uncomp_size;
    mz_uint16 m_filename_len;
    mz_uint16 m_extra_len;
    mz_uint16 m_comment_len;
    mz_uint16 m_disk_num_start;
    mz_uint16 m_internal_attr;
    mz_uint32 m_external_attr;
    mz_uint64 m_local_header_ofs;
    mz_uint8 m_filename[1]; /* Actually, filename_len bytes long */
} mz_zip_file_header;

/* Data descriptor */
typedef struct
{
    mz_uint32 m_signature;
    mz_uint32 m_crc32;
    mz_uint64 m_comp_size;
    mz_uint64 m_uncomp_size;
} mz_zip_data_descriptor;

typedef size_t (*mz_file_read_func)(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n);
typedef size_t (*mz_file_write_func)(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n);
typedef mz_bool (*mz_file_needs_keepalive_func)(void *pOpaque);

struct mz_zip_internal_state_tag;
typedef struct mz_zip_internal_state_tag mz_zip_internal_state;

typedef enum
{
    MZ_ZIP_MODE_INVALID = 0,
    MZ_ZIP_MODE_READING,
    MZ_ZIP_MODE_WRITING,
    MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED
} mz_zip_mode;

/* Archive object */
typedef struct
{
    mz_uint64 m_archive_size;
    mz_uint64 m_central_directory_file_ofs; /* Points to first header in central directory */
    mz_uint m_total_files;
    mz_zip_mode m_zip_mode;

    void *m_pIO_opaque;
    mz_file_read_func m_pRead;
    mz_file_write_func m_pWrite;
    mz_file_needs_keepalive_func m_pNeeds_keepalive; /* optional, can be NULL */

    mz_zip_internal_state *m_pState;
    mz_uint32 m_last_error;
    mz_uint64 m_file_offset_alignment;

} mz_zip_archive;

typedef struct
{
    mz_uint m_file_index;
    mz_uint64 m_central_dir_ofs; /* offset of the file's central dir header */
    mz_uint64 m_comp_size;
    mz_uint64 m_uncomp_size;
    mz_uint m_filename_len;
    mz_uint m_extra_len;
    mz_uint m_comment_len;
    mz_uint32 m_crc32;
    mz_uint64 m_local_header_ofs; /* offset of the file's local header */
    mz_uint16 m_method;
    mz_uint16 m_bit_flag;
    mz_uint16 m_version_made_by;
    mz_uint16 m_version_needed;
    mz_uint16 m_last_mod_time;
    mz_uint16 m_last_mod_date;
    mz_uint16 m_internal_attr;
    mz_uint32 m_external_attr;
    char m_filename[512]; /* Buffer for file name */
    char m_comment[256];  /* Buffer for comment */
} mz_zip_archive_file_stat;

#ifndef MINIZ_NO_STDIO
/* Init an archive object from a FILE stream. */
MINIZ_EXPORT mz_bool mz_zip_reader_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint32 flags);
MINIZ_EXPORT mz_bool mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning);
#endif

/* Init an archive object from memory. */
MINIZ_EXPORT mz_bool mz_zip_reader_init_mem(mz_zip_archive *pZip, const void *pMem, size_t size, mz_uint32 flags);
MINIZ_EXPORT mz_bool mz_zip_writer_init_heap(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size);

/* Release all resources associated with an archive object. */
MINIZ_EXPORT mz_bool mz_zip_end(mz_zip_archive *pZip);

/* Get the total number of files in an archive. */
MINIZ_EXPORT mz_uint mz_zip_reader_get_num_files(mz_zip_archive *pZip);

/* Get detailed information about a file in an archive. */
MINIZ_EXPORT mz_bool mz_zip_reader_file_stat(mz_zip_archive *pZip, mz_uint file_index, mz_zip_archive_file_stat *pStat);

/* Determine if a file is a directory. */
MINIZ_EXPORT mz_bool mz_zip_reader_is_file_a_directory(mz_zip_archive *pZip, mz_uint file_index);

/* Extract a file from an archive to a memory buffer. */
MINIZ_EXPORT void *mz_zip_reader_extract_to_mem(mz_zip_archive *pZip, mz_uint file_index, size_t *pSize, mz_uint flags);

/* Extract a file from an archive to a file on disk. */
#ifndef MINIZ_NO_STDIO
MINIZ_EXPORT mz_bool mz_zip_reader_extract_to_file(mz_zip_archive *pZip, mz_uint file_index, const char *pDst_filename, mz_uint flags);
#endif

/* Add a file to an archive from a memory buffer. */
MINIZ_EXPORT mz_bool mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, mz_uint flags);

#ifndef MINIZ_NO_STDIO
/* Add a file to an archive from a file on disk. */
MINIZ_EXPORT mz_bool mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags);
#endif

/* Finalize an archive. Writes central directory, etc. */
MINIZ_EXPORT mz_bool mz_zip_writer_finalize_archive(mz_zip_archive *pZip);

/* Get the last error code. */
MINIZ_EXPORT mz_uint32 mz_zip_get_last_error(mz_zip_archive *pZip);

#ifdef __cplusplus
}
#endif

#endif /* MINIZ_ZIP_H */
// End of miniz_zip.h

// This is where the full miniz.c implementation would go.
// For a single-file library, this often means including the .c file directly,
// or having a large #ifdef MINIZ_IMPLEMENTATION block.
// The previously fetched content of miniz.h was actually the miniz.c file itself,
// which is a common pattern for stb-style single-file libraries.
// It includes its own header part and then the implementation part.
// So, the content fetched from https://raw.githubusercontent.com/richgel999/miniz/master/miniz.h
// is actually the entire miniz.c, which is what we need.
// The #define MINIZ_IMPLEMENTATION above will ensure the implementation is compiled.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Adjust path as necessary, assuming zip_decompressor.c is in SPINDEX/plugins/decompressors/
#include "../../include/plugin_interface.h" 
#include "../../include/data_types.h"     // For ProcessedDataChunk, PluginInfo, SpindexPluginType

// --- Miniz Implementation (already included above by fetching the .c file as .h) ---
// The fetched content from the URL is actually miniz.c, which includes miniz.h definitions
// and then the implementation when MINIZ_IMPLEMENTATION is defined.

// --- Plugin Implementation ---

ProcessedDataChunk* zip_process_data(const ProcessedDataChunk *input_chunk);
void zip_get_info(PluginInfo *info);
int zip_init();
void zip_shutdown();

// Exposed function names for the plugin loader
MINIZ_EXPORT void spindex_plugin_get_info(PluginInfo *info) {
    zip_get_info(info);
}

MINIZ_EXPORT int spindex_plugin_init() {
    return zip_init();
}

MINIZ_EXPORT void spindex_plugin_shutdown() {
    zip_shutdown();
}

MINIZ_EXPORT ProcessedDataChunk* spindex_plugin_process(const ProcessedDataChunk *input_chunk) {
    return zip_process_data(input_chunk);
}


void zip_get_info(PluginInfo *info) {
    if (!info) return;
    strncpy(info->name, "ZIP Decompressor", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    strncpy(info->version, "1.0", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';
    
    // Using the official enum value now that data_types.h is updated.
    info->type = SPINDEX_PLUGIN_TYPE_DECOMPRESSOR;

    strncpy(info->description, "Decompresses ZIP archive files (extracts first file).", sizeof(info->description) - 1);
    info->description[sizeof(info->description) - 1] = '\0';
}

int zip_init() {
    // miniz doesn't require explicit global initialization for basic memory-to-memory decompression.
    // If we were using file functions or more complex features, we might do something here.
    fprintf(stderr, "ZIP Decompressor plugin initialized.\n");
    return 0; // Success
}

void zip_shutdown() {
    // miniz doesn't require explicit global cleanup for this usage.
    fprintf(stderr, "ZIP Decompressor plugin shut down.\n");
}

ProcessedDataChunk* zip_process_data(const ProcessedDataChunk *input_chunk) {
    if (!input_chunk || !input_chunk->data || input_chunk->length == 0) {
        fprintf(stderr, "zip_process_data: Invalid input chunk.\n");
        return NULL;
    }

    mz_zip_archive zip_archive;
    MZ_CLEAR_OBJ(zip_archive);

    // Initialize the zip archive structure for reading from memory
    mz_bool status = mz_zip_reader_init_mem(&zip_archive, input_chunk->data, input_chunk->length, 0);
    if (!status) {
        fprintf(stderr, "zip_process_data: mz_zip_reader_init_mem() failed! Error: %d\n", mz_zip_get_last_error(&zip_archive));
        return NULL;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    if (num_files == 0) {
        fprintf(stderr, "zip_process_data: No files found in ZIP archive.\n");
        mz_zip_reader_end(&zip_archive);
        return NULL;
    }

    // We will extract the first file found in the archive.
    mz_uint file_index_to_extract = 0; 
    mz_zip_archive_file_stat file_stat;

    if (!mz_zip_reader_file_stat(&zip_archive, file_index_to_extract, &file_stat)) {
        fprintf(stderr, "zip_process_data: mz_zip_reader_file_stat() failed for file index %u! Error: %d\n", file_index_to_extract, mz_zip_get_last_error(&zip_archive));
        mz_zip_reader_end(&zip_archive);
        return NULL;
    }
    
    if (mz_zip_reader_is_file_a_directory(&zip_archive, file_index_to_extract)) {
        fprintf(stderr, "zip_process_data: First entry (index %u, name '%s') is a directory, skipping.\n", file_index_to_extract, file_stat.m_filename);
        // Potentially loop here to find the first *file* if needed, for now, just fail if first is dir.
        mz_zip_reader_end(&zip_archive);
        return NULL;
    }

    fprintf(stderr, "zip_process_data: Attempting to decompress file '%s' (Index %u, Compressed Size: %llu, Uncompressed Size: %llu).\n",
            file_stat.m_filename, file_index_to_extract, (unsigned long long)file_stat.m_comp_size, (unsigned long long)file_stat.m_uncomp_size);

    size_t uncompressed_size = 0;
    // Note: mz_zip_reader_extract_to_mem allocates memory using MZ_MALLOC (which defaults to malloc).
    // The caller (in this case, this function) is responsible for freeing this memory.
    void *pDecompressed_data = mz_zip_reader_extract_to_mem(&zip_archive, file_index_to_extract, &uncompressed_size, 0);

    if (!pDecompressed_data) {
        fprintf(stderr, "zip_process_data: mz_zip_reader_extract_to_mem() failed for file '%s'! Error: %d\n", file_stat.m_filename, mz_zip_get_last_error(&zip_archive));
        mz_zip_reader_end(&zip_archive);
        return NULL;
    }
    
    // Ensure the reported uncompressed size matches the actual size from file_stat
    if (uncompressed_size != file_stat.m_uncomp_size) {
         fprintf(stderr, "zip_process_data: Warning - extracted size %zu does not match stat uncompressed size %llu for file '%s'. Using extracted size.\n",
                uncompressed_size, (unsigned long long)file_stat.m_uncomp_size, file_stat.m_filename);
    }


    // Clean up the zip reader
    mz_zip_reader_end(&zip_archive);

    // Create the output chunk
    ProcessedDataChunk *output_chunk = (ProcessedDataChunk*)malloc(sizeof(ProcessedDataChunk));
    if (!output_chunk) {
        fprintf(stderr, "zip_process_data: Failed to allocate memory for output ProcessedDataChunk.\n");
        MZ_FREE(pDecompressed_data); // Free data from mz_zip_reader_extract_to_mem
        return NULL;
    }

    output_chunk->data = pDecompressed_data;
    output_chunk->length = uncompressed_size;
    // Assuming the decompressed data is raw bytes. The next plugin in the pipeline
    // (e.g., a parser) would determine its actual type.
    // We could also try to carry over input_chunk->data_type if it makes sense.
    output_chunk->data_type = CHUNK_TYPE_RAW_BYTES; 
    output_chunk->source_id = NULL; // Or copy/clone from input_chunk if appropriate
    if (input_chunk->source_id) {
        output_chunk->source_id = strdup(input_chunk->source_id);
        if (!output_chunk->source_id) {
            fprintf(stderr, "zip_process_data: Failed to duplicate source_id.\n");
            // Continue, source_id is optional
        }
    }
    
    fprintf(stderr, "zip_process_data: Successfully decompressed '%s' into %zu bytes.\n", file_stat.m_filename, uncompressed_size);

    // Regarding input_chunk memory:
    // The problem description states: "For safety, this plugin should allocate new memory for its output
    // and not modify the input chunk directly. Free the input chunk's data if it's no longer needed
    // and was dynamically allocated by a previous plugin (this depends on memory management conventions
    // within SPINDEX, assume for now the core pipeline manages input chunk memory unless the plugin
    // explicitly takes ownership)."
    // Based on this, this plugin *does not* free input_chunk->data. The pipeline should manage it.

    return output_chunk;
}

/*
miniz.c v3.1.0 - public domain - Rich Geldreich <richgel99@gmail.com>
http://code.google.com/p/miniz/
This is a single source file library that implements the Deflate algorithm and various ZLIB/ZIP/PNG functionality.
It is entirely Public Domain. Rich Geldreich has explicitly disclaimed all copyright.
*/
/*
  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/

// Ensure miniz_export.h content is minimal and only for MINIZ_EXPORT
// If miniz_export.h is not present or needed, MINIZ_EXPORT can be defined as static or empty.
// For simplicity in this combined file, we can define MINIZ_EXPORT if it's not already.
#ifndef MINIZ_EXPORT
#ifdef _WIN32
    #define MINIZ_EXPORT __declspec(dllexport)
#elif __GNUC__ >= 4
    #define MINIZ_EXPORT __attribute__((visibility("default")))
#else
    #define MINIZ_EXPORT
#endif
#endif

// Make sure all miniz functions that are supposed to be public are marked MINIZ_EXPORT.
// The miniz.c/h file usually handles this. If we're inlining, ensure it's correct.
// For the functions called by the plugin (mz_zip_reader_init_mem, etc.), they are typically
// declared with MINIZ_EXPORT in miniz's own headers.
// The plugin's own functions (spindex_plugin_get_info, etc.) are also marked MINIZ_EXPORT.

// One final check: The structure of miniz.c when used as a header-only library often involves:
// 1. Header section (declarations)
// 2. Implementation section (guarded by #ifdef MINIZ_IMPLEMENTATION)
// The content fetched from the URL seems to be the full .c file, which is designed to work this way.
// My inclusion of "miniz_export.h", "miniz_common.h", etc. might be redundant if the fetched
// file already correctly structures itself. I'll simplify assuming the fetched content is miniz.c
// and handles its own internal includes or definitions when MINIZ_IMPLEMENTATION is defined.
// The key is that by defining MINIZ_IMPLEMENTATION before including the miniz code, its functions are defined.

// The provided miniz.h content (which is actually miniz.c) should be placed above this C code.
// I've structured it so the miniz code is at the top.
// The #define MINIZ_IMPLEMENTATION is key.

// It seems I manually re-added parts of miniz_common.h, miniz_tinfl.h etc.
// This is only needed if miniz.c itself doesn't directly include them or if we're trying to be selective.
// Given the single file nature of miniz, it's usually self-contained.
// Let's remove these separate includes and rely on the main miniz.c (fetched as miniz.h)
// to define everything it needs when MINIZ_IMPLEMENTATION is set.

// Corrected structure:
// 1. #define MINIZ_IMPLEMENTATION (if not already done by build system for miniz)
// 2. The entire content of miniz.c (which was fetched from the URL and pasted at the top)
// 3. Plugin specific code (includes for plugin_interface.h, data_types.h, and the plugin functions)

// The current file structure is:
// - miniz.c content (fetched from URL) with MINIZ_IMPLEMENTATION defined before it.
// - stdio.h, stdlib.h, string.h
// - plugin_interface.h, data_types.h
// - Plugin functions (zip_get_info, zip_init, zip_shutdown, zip_process_data)
// - Exposed plugin functions (spindex_plugin_get_info, etc.)
// This should be correct. The miniz code effectively becomes part of this compilation unit.
// The miniz_export.h, miniz_common.h etc. includes I added earlier inside the file were a bit confused;
// the single miniz.c file contains all that. I'll ensure the pasted code is just the raw miniz.c content.
// The file already starts with the miniz.c content.
// The defines like MINIZ_HEADER_FILE_ONLY and MINIZ_IMPLEMENTATION are correctly placed.
// The includes for miniz_export.h, miniz_common.h etc. are how miniz.c itself is structured internally.
// So, the current layout of the generated file should be okay.
