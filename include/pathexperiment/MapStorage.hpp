#pragma once

#include "pathexperiment/Grid.hpp"
#include <filesystem>
#include <iosfwd>

namespace pathx {

// Versioned text: PATHEXPERIMENT 1, width height, sx sy gx gy, then rows (. r ~ #).
// Parsing rejects invalid sizes, terrain, endpoints, missing rows and trailing content.
[[nodiscard]] Map readMap(std::istream& input);
void writeMap(std::ostream& output, const Map& map);
[[nodiscard]] Map loadMap(const std::filesystem::path& path);
void saveMap(const std::filesystem::path& path, const Map& map);

} // namespace pathx
