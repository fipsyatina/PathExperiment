#include "pathexperiment/Grid.hpp"
#include "pathexperiment/MapStorage.hpp"
#include "pathexperiment/Search.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using pathx::Algorithm;
using pathx::EvaluationKind;
using pathx::Grid;
using pathx::Map;
using pathx::Point;
using pathx::SearchResult;
using pathx::SearchSession;
using pathx::SearchStatus;
using pathx::Terrain;

[[noreturn]] void fail(std::string_view expression, std::string_view file, int line) {
    throw std::runtime_error(
        std::string(file) + ':' + std::to_string(line) + ": CHECK(" + std::string(expression) + ") failed");
}

#define CHECK(expression) ((expression) ? static_cast<void>(0) : fail(#expression, __FILE__, __LINE__))

template <typename Function>
void checkThrows(Function&& function) {
    bool threw = false;
    try {
        std::forward<Function>(function)();
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

Map roadMap(int width, int height, Point start, Point goal) {
    return Map{Grid(width, height, Terrain::Road), start, goal};
}

int pathCost(const Map& map, const std::vector<Point>& path) {
    int cost = 0;
    for (std::size_t index = 1; index < path.size(); ++index) {
        cost += pathx::entryCost(map.grid.at(path[index]));
    }
    return cost;
}

void validateFoundPath(const Map& map, const SearchResult& result) {
    CHECK(result.status == SearchStatus::Found);
    CHECK(!result.path.empty());
    CHECK(result.path.front() == map.start);
    CHECK(result.path.back() == map.goal);
    CHECK(result.steps() + 1 == result.path.size());
    CHECK(result.cost == pathCost(map, result.path));

    for (std::size_t index = 0; index < result.path.size(); ++index) {
        const Point point = result.path[index];
        CHECK(map.grid.contains(point));
        CHECK(pathx::passable(map.grid.at(point)));
        if (index > 0) {
            const Point previous = result.path[index - 1];
            const int distance = std::abs(point.x - previous.x) + std::abs(point.y - previous.y);
            CHECK(distance == 1);
        }
    }
}

int bellmanFordCost(const Map& map) {
    if (!map.grid.contains(map.start) || !map.grid.contains(map.goal) ||
        !pathx::passable(map.grid.at(map.start)) || !pathx::passable(map.grid.at(map.goal))) {
        return pathx::infinity;
    }

    std::vector<int> distances(map.grid.size(), pathx::infinity);
    distances[static_cast<std::size_t>(map.grid.index(map.start))] = 0;

    for (std::size_t pass = 1; pass < map.grid.size(); ++pass) {
        bool changed = false;
        for (int y = 0; y < map.grid.height(); ++y) {
            for (int x = 0; x < map.grid.width(); ++x) {
                const Point from{x, y};
                const int fromIndex = map.grid.index(from);
                const int fromCost = distances[static_cast<std::size_t>(fromIndex)];
                if (fromCost == pathx::infinity || !pathx::passable(map.grid.at(from))) {
                    continue;
                }
                for (const Point to : map.grid.neighbors(from)) {
                    if (!pathx::passable(map.grid.at(to))) {
                        continue;
                    }
                    int& candidate = distances[static_cast<std::size_t>(map.grid.index(to))];
                    const int newCost = fromCost + pathx::entryCost(map.grid.at(to));
                    if (newCost < candidate) {
                        candidate = newCost;
                        changed = true;
                    }
                }
            }
        }
        if (!changed) {
            break;
        }
    }

    return distances[static_cast<std::size_t>(map.grid.index(map.goal))];
}

void testGridNeighborsExcludeWallsOnlyAtSearchLayer() {
    Grid grid(3, 3, Terrain::Road);
    grid.set({1, 0}, Terrain::Wall);
    const auto neighbors = grid.neighbors({1, 1});
    CHECK(neighbors.size() == 4);
    CHECK(std::find(neighbors.begin(), neighbors.end(), Point{1, 0}) != neighbors.end());
}

void testRouteAvoidsWalls() {
    Map map = roadMap(5, 3, {0, 1}, {4, 1});
    map.grid.set({2, 1}, Terrain::Wall);
    const SearchResult result = pathx::solve(map, Algorithm::Dijkstra);
    validateFoundPath(map, result);
    CHECK(std::find(result.path.begin(), result.path.end(), Point{2, 1}) == result.path.end());
}

void testRouteCostExcludesStartAndIncludesDestination() {
    Map map{Grid(3, 1, Terrain::Plain), {0, 0}, {2, 0}};
    map.grid.set(map.start, Terrain::Swamp);
    map.grid.set(map.goal, Terrain::Road);
    const SearchResult result = pathx::solve(map, Algorithm::Dijkstra);
    CHECK(result.cost == 3);
}

void testCheapestDetourBeatsShorterSwampRoute() {
    Map map = roadMap(5, 3, {0, 1}, {4, 1});
    for (int x = 1; x < 4; ++x) {
        map.grid.set({x, 1}, Terrain::Swamp);
    }
    const SearchResult result = pathx::solve(map, Algorithm::Dijkstra);
    validateFoundPath(map, result);
    CHECK(result.steps() == 6);
    CHECK(result.cost == 6);
}

void testBlockedGoalReportsNoPath() {
    Map map = roadMap(3, 3, {0, 0}, {1, 1});
    for (const Point wall : std::array<Point, 4>{{{1, 0}, {0, 1}, {2, 1}, {1, 2}}}) {
        map.grid.set(wall, Terrain::Wall);
    }
    const SearchResult result = pathx::solve(map, Algorithm::AStar);
    CHECK(result.status == SearchStatus::NoPath);
    CHECK(result.path.empty());
    CHECK(result.cost == -1);
}

void testSameStartAndGoalHasZeroLengthAndCost() {
    const Map map = roadMap(3, 3, {1, 1}, {1, 1});
    const SearchResult result = pathx::solve(map, Algorithm::AStar);
    CHECK(result.status == SearchStatus::Found);
    CHECK(result.steps() == 0);
    CHECK(result.cost == 0);
    CHECK((result.path == std::vector<Point>{{1, 1}}));
}

void testOutOfBoundsStartIsInvalid() {
    const Map map = roadMap(3, 3, {-1, 0}, {2, 2});
    CHECK(pathx::solve(map, Algorithm::Dijkstra).status == SearchStatus::InvalidMap);
}

void testOutOfBoundsGoalIsInvalid() {
    const Map map = roadMap(3, 3, {0, 0}, {3, 2});
    CHECK(pathx::solve(map, Algorithm::Dijkstra).status == SearchStatus::InvalidMap);
}

void testWallStartIsInvalid() {
    Map map = roadMap(3, 3, {0, 0}, {2, 2});
    map.grid.set(map.start, Terrain::Wall);
    CHECK(pathx::solve(map, Algorithm::AStar).status == SearchStatus::InvalidMap);
}

void testWallGoalIsInvalid() {
    Map map = roadMap(3, 3, {0, 0}, {2, 2});
    map.grid.set(map.goal, Terrain::Wall);
    CHECK(pathx::solve(map, Algorithm::AStar).status == SearchStatus::InvalidMap);
}

void testEachStepExpandsAtMostOneCell() {
    SearchSession session(roadMap(8, 6, {0, 0}, {7, 5}), Algorithm::Dijkstra);
    while (session.result().status == SearchStatus::Searching) {
        const std::size_t before = session.result().expanded;
        session.step();
        const std::size_t after = session.result().expanded;
        CHECK(after >= before);
        CHECK(after - before <= 1);
    }
}

void testStepReportsDiscoveredAndBlockedNeighbors() {
    Map map = roadMap(3, 2, {0, 0}, {2, 0});
    map.grid.set({0, 1}, Terrain::Wall);
    SearchSession session(map, Algorithm::Dijkstra, true);
    const auto& step = session.step();
    CHECK((step.current == Point{0, 0}));
    CHECK(step.evaluations.size() == 2);
    CHECK(std::any_of(step.evaluations.begin(), step.evaluations.end(), [](const auto& evaluation) {
        return evaluation.point == Point{1, 0} && evaluation.kind == EvaluationKind::Discovered;
    }));
    CHECK(std::any_of(step.evaluations.begin(), step.evaluations.end(), [](const auto& evaluation) {
        return evaluation.point == Point{0, 1} && evaluation.kind == EvaluationKind::Blocked;
    }));
}

void testStepReportsClosedAndNoImprovementNeighbors() {
    SearchSession session(roadMap(3, 2, {0, 0}, {2, 1}), Algorithm::Dijkstra, true);
    session.step();
    session.step();
    const auto& step = session.step();
    CHECK(std::any_of(step.evaluations.begin(), step.evaluations.end(), [](const auto& evaluation) {
        return evaluation.kind == EvaluationKind::Closed;
    }));
    CHECK(std::any_of(step.evaluations.begin(), step.evaluations.end(), [](const auto& evaluation) {
        return evaluation.kind == EvaluationKind::NoImprovement;
    }));
}

void testDisabledEventRecordingLeavesEvaluationsEmpty() {
    SearchSession session(roadMap(3, 2, {0, 0}, {2, 1}), Algorithm::AStar, false);
    CHECK(session.step().evaluations.empty());
}

void testSearchSessionOwnsAnIsolatedMapSnapshot() {
    Map original = roadMap(4, 1, {0, 0}, {3, 0});
    SearchSession session(original, Algorithm::Dijkstra);
    original.grid.set({1, 0}, Terrain::Wall);
    while (session.result().status == SearchStatus::Searching) {
        session.step();
    }
    CHECK(session.result().status == SearchStatus::Found);
    CHECK(session.map().grid.at({1, 0}) == Terrain::Road);
}

void testRepeatedSolvesAreDeterministic() {
    Map map{Grid(6, 5, Terrain::Plain), {0, 2}, {5, 2}};
    map.grid.set({2, 2}, Terrain::Wall);
    const SearchResult first = pathx::solve(map, Algorithm::AStar);
    for (int repeat = 0; repeat < 5; ++repeat) {
        const SearchResult next = pathx::solve(map, Algorithm::AStar);
        CHECK(next.status == first.status);
        CHECK(next.path == first.path);
        CHECK(next.cost == first.cost);
        CHECK(next.expanded == first.expanded);
    }
}

void testSeededRandomMapsMatchBellmanFordOracle() {
    std::mt19937 random(0x5EED1234U);
    std::uniform_int_distribution<int> terrainRoll(0, 99);
    for (int caseIndex = 0; caseIndex < 80; ++caseIndex) {
        const int width = 2 + caseIndex % 6;
        const int height = 2 + (caseIndex / 6) % 5;
        Map map = roadMap(width, height, {0, 0}, {width - 1, height - 1});
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int roll = terrainRoll(random);
                const Terrain terrain = roll < 20 ? Terrain::Wall
                    : roll < 45 ? Terrain::Road
                    : roll < 80 ? Terrain::Plain
                                : Terrain::Swamp;
                map.grid.set({x, y}, terrain);
            }
        }
        map.grid.set(map.start, Terrain::Road);
        map.grid.set(map.goal, Terrain::Road);

        const int expectedCost = bellmanFordCost(map);
        const SearchResult dijkstra = pathx::solve(map, Algorithm::Dijkstra);
        const SearchResult aStar = pathx::solve(map, Algorithm::AStar);
        CHECK(dijkstra.status == aStar.status);
        CHECK(dijkstra.cost == aStar.cost);
        if (expectedCost == pathx::infinity) {
            CHECK(dijkstra.status == SearchStatus::NoPath);
            CHECK(dijkstra.cost == -1);
        } else {
            CHECK(dijkstra.cost == expectedCost);
            validateFoundPath(map, dijkstra);
            validateFoundPath(map, aStar);
        }
    }
}

void testMapStorageRoundTripsThroughStreams() {
    Map original{Grid(4, 3, Terrain::Plain), {0, 2}, {3, 0}};
    original.grid.set({0, 0}, Terrain::Road);
    original.grid.set({1, 0}, Terrain::Swamp);
    original.grid.set({2, 0}, Terrain::Wall);
    std::stringstream stream;
    pathx::writeMap(stream, original);
    const Map restored = pathx::readMap(stream);
    CHECK(restored.grid.width() == original.grid.width());
    CHECK(restored.grid.height() == original.grid.height());
    CHECK(restored.start == original.start);
    CHECK(restored.goal == original.goal);
    for (int y = 0; y < original.grid.height(); ++y) {
        for (int x = 0; x < original.grid.width(); ++x) {
            CHECK(restored.grid.at({x, y}) == original.grid.at({x, y}));
        }
    }
}

void testMapStorageRejectsUnknownVersion() {
    std::istringstream input("PATHEXPERIMENT 2\n2 1\n0 0 1 0\nrr\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsNonPositiveSize() {
    std::istringstream input("PATHEXPERIMENT 1\n0 2\n0 0 0 1\n\n\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsOutOfBoundsEndpoint() {
    std::istringstream input("PATHEXPERIMENT 1\n2 1\n0 0 2 0\nrr\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsWallEndpoint() {
    std::istringstream input("PATHEXPERIMENT 1\n2 1\n0 0 1 0\n#r\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsUnknownTerrainCharacter() {
    std::istringstream input("PATHEXPERIMENT 1\n2 1\n0 0 1 0\nrx\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsShortRow() {
    std::istringstream input("PATHEXPERIMENT 1\n3 1\n0 0 2 0\nrr\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsMissingRow() {
    std::istringstream input("PATHEXPERIMENT 1\n2 2\n0 0 1 0\nrr\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testMapStorageRejectsTrailingJunk() {
    std::istringstream input("PATHEXPERIMENT 1\n2 1\n0 0 1 0\nrr\nunexpected\n");
    checkThrows([&] { static_cast<void>(pathx::readMap(input)); });
}

void testBenchmarkReportsRequestedRunsAndSearchOutcome() {
    const Map map = roadMap(8, 6, {0, 0}, {7, 5});
    const auto measured = pathx::benchmark(map, Algorithm::AStar, 3);
    CHECK(measured.runs == 3);
    CHECK(measured.result.status == SearchStatus::Found);
    CHECK(measured.result.cost == pathx::solve(map, Algorithm::AStar).cost);
    CHECK(measured.medianMicroseconds >= 0.0);
}

using TestFunction = void (*)();

const std::vector<std::pair<std::string_view, TestFunction>> tests{
    {"grid neighbors include adjacent walls for the search layer to classify", testGridNeighborsExcludeWallsOnlyAtSearchLayer},
    {"route avoids walls", testRouteAvoidsWalls},
    {"route cost excludes start and includes destination", testRouteCostExcludesStartAndIncludesDestination},
    {"cheapest detour beats a shorter swamp route", testCheapestDetourBeatsShorterSwampRoute},
    {"blocked goal reports no path", testBlockedGoalReportsNoPath},
    {"same start and goal has zero length and cost", testSameStartAndGoalHasZeroLengthAndCost},
    {"out-of-bounds start is invalid", testOutOfBoundsStartIsInvalid},
    {"out-of-bounds goal is invalid", testOutOfBoundsGoalIsInvalid},
    {"wall start is invalid", testWallStartIsInvalid},
    {"wall goal is invalid", testWallGoalIsInvalid},
    {"each step expands at most one cell", testEachStepExpandsAtMostOneCell},
    {"step reports discovered and blocked neighbors", testStepReportsDiscoveredAndBlockedNeighbors},
    {"step reports closed and no-improvement neighbors", testStepReportsClosedAndNoImprovementNeighbors},
    {"disabled event recording leaves evaluations empty", testDisabledEventRecordingLeavesEvaluationsEmpty},
    {"search session owns an isolated map snapshot", testSearchSessionOwnsAnIsolatedMapSnapshot},
    {"repeated solves are deterministic", testRepeatedSolvesAreDeterministic},
    {"seeded random maps match an independent Bellman-Ford oracle", testSeededRandomMapsMatchBellmanFordOracle},
    {"map storage round-trips through streams", testMapStorageRoundTripsThroughStreams},
    {"map storage rejects unknown version", testMapStorageRejectsUnknownVersion},
    {"map storage rejects non-positive size", testMapStorageRejectsNonPositiveSize},
    {"map storage rejects out-of-bounds endpoint", testMapStorageRejectsOutOfBoundsEndpoint},
    {"map storage rejects wall endpoint", testMapStorageRejectsWallEndpoint},
    {"map storage rejects unknown terrain character", testMapStorageRejectsUnknownTerrainCharacter},
    {"map storage rejects short row", testMapStorageRejectsShortRow},
    {"map storage rejects missing row", testMapStorageRejectsMissingRow},
    {"map storage rejects trailing junk", testMapStorageRejectsTrailingJunk},
    {"benchmark reports requested runs and search outcome", testBenchmarkReportsRequestedRunsAndSearchOutcome},
};

} // namespace

int main() {
    std::size_t passed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        } catch (...) {
            std::cerr << "[FAIL] " << name << ": unknown exception\n";
        }
    }

    std::cout << passed << '/' << tests.size() << " tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
