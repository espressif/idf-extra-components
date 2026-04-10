set(LZ4_LAYOUT_DIR "lz4/lib")

# ESP-IDF builds each component as a static library. Keep the upstream library
# sources explicit so that CLI programs, platform-specific code, tests,
# installation rules, and package-generation logic from the upstream CMake
# project are not pulled into the ESP-IDF build.
idf_component_register(
    INCLUDE_DIRS "${LZ4_LAYOUT_DIR}"
    SRCS
        "${LZ4_LAYOUT_DIR}/lz4.c"
        "${LZ4_LAYOUT_DIR}/lz4hc.c"
        "${LZ4_LAYOUT_DIR}/lz4frame.c"
        "${LZ4_LAYOUT_DIR}/lz4file.c"
        "${LZ4_LAYOUT_DIR}/xxhash.c"
        "lz4_memory.c"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LZ4_HEAPMODE=1             # Allocate Block compression workspace on the heap.
    LZ4HC_HEAPMODE=1           # Allocate the large HC workspace on the heap.
    LZ4F_HEAPMODE=1            # Allocate Frame compression contexts on the heap.
    LZ4_FORCE_MEMORY_ACCESS=0  # Use portable memcpy-based unaligned accesses.
)

if(CONFIG_LZ4_USE_PSRAM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        LZ4_USER_MEMORY_FUNCTIONS=1  # Use the component's PSRAM-aware allocator.
    )
endif()
