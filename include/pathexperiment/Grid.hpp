#pragma once

#include <cstddef>
#include <vector>

namespace pathx {

struct Point {
    int x{};
    int y{};
    friend bool operator==(Point, Point) = default;
};

enum class Terrain { Road, Plain, Swamp, Wall };
[[nodiscard]] int entryCost(Terrain terrain) noexcept; // Wall returns 0; check passable first.
[[nodiscard]] bool passable(Terrain terrain) noexcept;

class Grid {
public:
    explicit Grid(int width = 40, int height = 25, Terrain fill = Terrain::Plain);
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool contains(Point point) const noexcept;
    [[nodiscard]] int index(Point point) const;
    [[nodiscard]] Point point(int index) const;
    [[nodiscard]] Terrain at(Point point) const;
    void set(Point point, Terrain terrain);
    void fill(Terrain terrain);
    [[nodiscard]] std::vector<Point> neighbors(Point point) const;
private:
    int width_;
    int height_;
    std::vector<Terrain> cells_;
};

struct Map {
    Grid grid{};
    Point start{2, 12};
    Point goal{37, 12};
};

} // namespace pathx
