#include "pathexperiment/MapStorage.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace pathx {
namespace {

constexpr int format_version{1};

[[noreturn]] void parseError(const std::string& detail) {
    throw std::runtime_error{"invalid PathExperiment map: " + detail};
}

Terrain terrainFromSymbol(const char symbol) {
    switch (symbol) {
    case 'r':
        return Terrain::Road;
    case '.':
        return Terrain::Plain;
    case '~':
        return Terrain::Swamp;
    case '#':
        return Terrain::Wall;
    default:
        parseError("unknown terrain symbol");
    }
}

char symbolFromTerrain(const Terrain terrain) {
    switch (terrain) {
    case Terrain::Road:
        return 'r';
    case Terrain::Plain:
        return '.';
    case Terrain::Swamp:
        return '~';
    case Terrain::Wall:
        return '#';
    }
    throw std::invalid_argument{"map contains an unknown terrain value"};
}

void validateEndpoints(const Map& map) {
    if (!map.grid.contains(map.start) || !map.grid.contains(map.goal)) {
        throw std::invalid_argument{"map endpoints must be inside the grid"};
    }
    if (!passable(map.grid.at(map.start)) || !passable(map.grid.at(map.goal))) {
        throw std::invalid_argument{"map endpoints must be passable"};
    }
}

} // namespace

Map readMap(std::istream& input) {
    std::string signature;
    int version{};
    if (!(input >> signature >> version) || signature != "PATHEXPERIMENT" ||
        version != format_version) {
        parseError("unsupported or missing header");
    }

    int width{};
    int height{};
    if (!(input >> width >> height)) {
        parseError("missing dimensions");
    }

    Map map;
    try {
        map.grid = Grid{width, height};
    } catch (const std::invalid_argument&) {
        parseError("dimensions must be between 1 and 512");
    }

    if (!(input >> map.start.x >> map.start.y >> map.goal.x >> map.goal.y)) {
        parseError("missing endpoints");
    }

    for (int y{}; y < height; ++y) {
        std::string row;
        if (!(input >> row) || row.size() != static_cast<std::size_t>(width)) {
            parseError("missing row or incorrect row width");
        }
        for (int x{}; x < width; ++x) {
            map.grid.set(Point{x, y}, terrainFromSymbol(row.at(static_cast<std::size_t>(x))));
        }
    }

    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        parseError("trailing content");
    }

    try {
        validateEndpoints(map);
    } catch (const std::invalid_argument& error) {
        parseError(error.what());
    }
    return map;
}

void writeMap(std::ostream& output, const Map& map) {
    validateEndpoints(map);
    output << "PATHEXPERIMENT " << format_version << '\n'
           << map.grid.width() << ' ' << map.grid.height() << '\n'
           << map.start.x << ' ' << map.start.y << ' '
           << map.goal.x << ' ' << map.goal.y << '\n';

    for (int y{}; y < map.grid.height(); ++y) {
        for (int x{}; x < map.grid.width(); ++x) {
            output << symbolFromTerrain(map.grid.at(Point{x, y}));
        }
        output << '\n';
    }
    if (!output) {
        throw std::runtime_error{"failed to write PathExperiment map"};
    }
}

Map loadMap(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error{"failed to open map for reading: " + path.string()};
    }
    return readMap(input);
}

void saveMap(const std::filesystem::path& path, const Map& map) {
    validateEndpoints(map);
    std::error_code error;
    const bool alreadyExists{std::filesystem::exists(path, error)};
    if (error) {
        throw std::runtime_error{"failed to inspect map destination: " + error.message()};
    }
    if (alreadyExists) {
        throw std::runtime_error{"refusing to overwrite existing map: " + path.string()};
    }

    std::ofstream output{path};
    if (!output) {
        throw std::runtime_error{"failed to open map for writing: " + path.string()};
    }
    writeMap(output, map);
    output.flush();
    if (!output) {
        throw std::runtime_error{"failed to save PathExperiment map: " + path.string()};
    }
}

} // namespace pathx
