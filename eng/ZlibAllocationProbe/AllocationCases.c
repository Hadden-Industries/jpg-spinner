#include <limits.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "zutil.h"

int main(void)
{
    /* A normal allocation must retain the complete requested writable extent. */
    voidpf ordinary = zcalloc(Z_NULL, 7, 11);
    if (ordinary == Z_NULL)
        return 2;
    memset(ordinary, 0x5A, 77);
    zcfree(Z_NULL, ordinary);

    /* Exercise the public codec too: allocator changes must preserve ordinary
       binary compression/decompression, including embedded zero bytes. */
    const Bytef input[] = {0, 0x17, 0xFF, 0, 0x80, 1, 2, 3};
    Bytef compressed[128];
    Bytef decoded[sizeof(input)];
    uLongf compressed_length = sizeof(compressed);
    uLongf decoded_length = sizeof(decoded);
    if (compress2(compressed, &compressed_length, input, sizeof(input), Z_BEST_COMPRESSION) != Z_OK ||
        uncompress(decoded, &decoded_length, compressed, compressed_length) != Z_OK ||
        decoded_length != sizeof(input) || memcmp(input, decoded, sizeof(input)) != 0)
        return 3;
    puts("PASS: ordinary native allocation and binary codec round trip.");

    /* These real requests exceed the process's maximum addressable range. They cannot
       legitimately allocate host memory. The defective unsigned multiplication
       wraps them to one/four bytes, so reproducing it does not stress the PC. */
    const unsigned counts[] = {UINT_MAX, UINT_MAX - 1};
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    for (size_t index = 0; index < sizeof(counts) / sizeof(counts[0]); ++index)
    {
        const uint64_t requested = (uint64_t)counts[index] * counts[index];
        if (requested <= (uintptr_t)system_info.lpMaximumApplicationAddress)
            return 4; /* Refuse an unsupported host rather than allocate GBs. */
        voidpf allocation = zcalloc(Z_NULL, counts[index], counts[index]);
        if (allocation != Z_NULL)
        {
            zcfree(Z_NULL, allocation);
            fputs("FAIL: native zcalloc accepted an impossible request after narrow multiplication.\n", stderr);
            return 1;
        }
    }
    puts("PASS: both impossible native allocation requests are refused.");
    return 0;
}
