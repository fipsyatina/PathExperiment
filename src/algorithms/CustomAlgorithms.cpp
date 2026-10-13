#include "pathexperiment/AlgorithmRegistry.hpp"

namespace pathx {

void registerCustomAlgorithms(AlgorithmRegistry& registry) {
    // Add project-specific AlgorithmDescriptor values here. See
    // docs/ADDING_ALGORITHMS.md and examples/CustomAlgorithmExample.cpp.
    static_cast<void>(registry);
}

} // namespace pathx
