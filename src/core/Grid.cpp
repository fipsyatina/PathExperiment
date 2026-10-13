#include "pathexperiment/Grid.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace pathx {
namespace {

constexpr int min_dimension{1};
constexpr int max_dimension{512};

void validateDimensions(const int width, const int height) {
    if (width < min_dimension || width > max_dimension ||
        height < min_dimension || height > max_dimension) {
        throw std::invalid_argument{"grid dimensions must be between 1 and 512"};
    }
}

} // namespace

int entryCost(const Terrain terrain) noexcept {
    switch (terrain) {
    case Terrain::Road:
        return 1;
    case Terrain::Plain:
        return 2;
    case Terrain::Swamp:
        return 5;
    case Terrain::Wall:
        return 0;
    }
    return 0;
}

bool passable(const Terrain terrain) noexcept {
    return terrain != Terrain::Wall;
}

Grid::Grid(const int width, const int height, const Terrain fill)
    : width_{width}, height_{height} {
    validateDimensions(width, height);
    cells_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), fill);
}

int Grid::width() const noexcept {
    return width_;
}

int Grid::height() const noexcept {
    return height_;
}

std::size_t Grid::size() const noexcept {
    return cells_.size();
}

bool Grid::contains(const Point point) const noexcept {
    return point.x >= 0 && point.x < width_ && point.y >= 0 && point.y < height_;
}

int Grid::index(const Point point) const {
    if (!contains(point)) {
        throw std::out_of_range{"grid point is outside the map"};
    }
    return point.y * width_ + point.x;
}

Point Grid::point(const int index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= cells_.size()) {
        throw std::out_of_range{"grid index is outside the map"};
    }
    return Point{index % width_, index / width_};
}

Terrain Grid::at(const Point point) const {
    return cells_.at(static_cast<std::size_t>(index(point)));
}

void Grid::set(const Point point, const Terrain terrain) {
    cells_.at(static_cast<std::size_t>(index(point))) = terrain;
}

void Grid::fill(const Terrain terrain) {
    std::fill(cells_.begin(), cells_.end(), terrain);
}

std::vector<Point> Grid::neighbors(const Point point) const {
    if (!contains(point)) {
        throw std::out_of_range{"grid point is outside the map"};
    }

    constexpr std::array<Point, 4> offsets{{
        Point{0, -1},
        Point{0, 1},
        Point{-1, 0},
        Point{1, 0},
    }};

    std::vector<Point> result;
    result.reserve(offsets.size());
    for (const Point offset : offsets) {
        const Point candidate{point.x + offset.x, point.y + offset.y};
        if (contains(candidate)) {
            result.push_back(candidate);
        }
    }
    return result;
}

} // namespace pathx
