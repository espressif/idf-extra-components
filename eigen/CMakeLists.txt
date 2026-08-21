# Eigen is a header-only C++ template library.
#
# No build step is required -- Eigen headers are consumed directly from the source tree.
#
# The upstream include layout is used directly (`#include <Eigen/...>`),
# so the include directory points to the submodule root.
#
idf_component_register(
    INCLUDE_DIRS "${COMPONENT_DIR}/eigen"
)

# Eigen 5 requires C++14 or newer.
target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_14)
