#include <iostream>
#include <vector>
#include <memory>
#include <zstd.h>
#include <cassert>
#include "UnleashedRecomp/decompressor.h"

// Mock the generated header and data
extern const char g_test_array[44];
const char g_test_array[44] =
	"\x28\xb5\x2f\xfd\x04\x00\xd1\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x20\x5a\x53\x54\x44\x20\x54\x65\x73\x74\x2e\x01\x00\x65\x2e\x04\x13\x13\x71\x84\x30\x91\xd1";

int main() {
    size_t decompressedSize = 25; // "Hello World! ZSTD Test." is 23 chars + null? Let's say 25.
    auto result = decompressZstd(g_test_array, decompressedSize);
    if (result) {
        std::cout << "Decompression successful: " << (char*)result.get() << std::endl;
        return 0;
    } else {
        std::cerr << "Decompression failed!" << std::endl;
        return 1;
    }
}
