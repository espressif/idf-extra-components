set(LZ4_LAYOUT_DIR "lz4/lib")

# Frame and lz4file are compiled through small wrappers so their default
# allocators share LZ4_USER_MEMORY_FUNCTIONS with Block/HC. That covers
# intra-translation-unit callers such as LZ4F_compressFrame() without
# --wrap and without adding ESP-specific public APIs.
idf_component_register(
    INCLUDE_DIRS "${LZ4_LAYOUT_DIR}"
    SRCS
        "${LZ4_LAYOUT_DIR}/lz4.c"
        "${LZ4_LAYOUT_DIR}/lz4hc.c"
        "${LZ4_LAYOUT_DIR}/xxhash.c"
        "lz4_frame.c"
        "lz4_file.c"
        "lz4_memory.c"
    PRIV_REQUIRES esp_psram
)

if(CONFIG_LZ4_BLOCK_HASH_TABLE_SIZE_1KB)
    set(lz4_hash_exp 10)
elseif(CONFIG_LZ4_BLOCK_HASH_TABLE_SIZE_2KB)
    set(lz4_hash_exp 11)
elseif(CONFIG_LZ4_BLOCK_HASH_TABLE_SIZE_4KB)
    set(lz4_hash_exp 12)
elseif(CONFIG_LZ4_BLOCK_HASH_TABLE_SIZE_8KB)
    set(lz4_hash_exp 13)
else()
    set(lz4_hash_exp 14)   # CONFIG_LZ4_BLOCK_HASH_TABLE_SIZE_16KB
endif()

# LZ4_MEMORY_USAGE shapes LZ4_stream_t in the public header, so dependents
# must see the same value the library was built with.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LZ4_MEMORY_USAGE=${lz4_hash_exp}
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LZ4_HEAPMODE=1               # Allocate Block compression workspace on the heap.
    LZ4HC_HEAPMODE=1             # Allocate the large HC workspace on the heap.
    LZ4F_HEAPMODE=1              # Allocate Frame compression contexts on the heap.
    LZ4_USER_MEMORY_FUNCTIONS=1  # Route library heap traffic through LZ4_malloc.
    LZ4_FORCE_MEMORY_ACCESS=0    # Use portable memcpy-based unaligned accesses.
    XXH_FORCE_MEMORY_ACCESS=0    # Use portable memcpy-based unaligned accesses.
    XXH_NAMESPACE=LZ4_           # Keep bundled xxHash symbols private to LZ4.
)

if(CONFIG_LZ4_FAST_DEC_LOOP)
    # Upstream auto-enables its fast decoder only for x86/aarch64; this option lets ESP targets opt in
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LZ4_FAST_DEC_LOOP=1)
endif()
