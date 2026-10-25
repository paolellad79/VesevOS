#pragma once
// STUB di miniz.h della ROM (esp_rom/include/miniz.h): solo i nomi usati da vos_drv_deflate.cpp, con le stesse firme.
#include <stdint.h>
#include <stddef.h>
typedef uint8_t mz_uint8;
typedef int mz_bool;
typedef struct { uint32_t dummy[200]; } tinfl_decompressor;      // la forma vera non serve al controllo di sintassi
typedef struct { uint32_t dummy[200]; } tdefl_compressor;
enum { TINFL_FLAG_PARSE_ZLIB_HEADER = 1, TINFL_FLAG_HAS_MORE_INPUT = 2, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4, TINFL_FLAG_COMPUTE_ADLER32 = 8 };
typedef enum { TINFL_STATUS_BAD_PARAM = -3, TINFL_STATUS_ADLER32_MISMATCH = -2, TINFL_STATUS_FAILED = -1, TINFL_STATUS_DONE = 0,
               TINFL_STATUS_NEEDS_MORE_INPUT = 1, TINFL_STATUS_HAS_MORE_OUTPUT = 2 } tinfl_status;
#define TINFL_LZ_DICT_SIZE 32768
#define tinfl_init(r) do { (r)->dummy[0] = 0; } while (0)
tinfl_status tinfl_decompress(tinfl_decompressor* r, const mz_uint8* pIn_buf_next, size_t* pIn_buf_size, mz_uint8* pOut_buf_start,
                              mz_uint8* pOut_buf_next, size_t* pOut_buf_size, const uint32_t decomp_flags);
typedef mz_bool (*tdefl_put_buf_func_ptr)(const void* pBuf, int len, void* pUser);
enum { TDEFL_WRITE_ZLIB_HEADER = 0x01000, TDEFL_COMPUTE_ADLER32 = 0x02000, TDEFL_GREEDY_PARSING_FLAG = 0x04000 };
typedef enum { TDEFL_STATUS_BAD_PARAM = -2, TDEFL_STATUS_PUT_BUF_FAILED = -1, TDEFL_STATUS_OKAY = 0, TDEFL_STATUS_DONE = 1 } tdefl_status;
typedef enum { TDEFL_NO_FLUSH = 0, TDEFL_SYNC_FLUSH = 2, TDEFL_FULL_FLUSH = 3, TDEFL_FINISH = 4 } tdefl_flush;
tdefl_status tdefl_init(tdefl_compressor* d, tdefl_put_buf_func_ptr pPut_buf_func, void* pPut_buf_user, int flags);
tdefl_status tdefl_compress(tdefl_compressor* d, const void* pIn_buf, size_t* pIn_buf_size, void* pOut_buf, size_t* pOut_buf_size, tdefl_flush flush);
tdefl_status tdefl_compress_buffer(tdefl_compressor* d, const void* pIn_buf, size_t in_buf_size, tdefl_flush flush);
