#pragma once

#include "pathexperiment/Grid.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

namespace pathx {

inline constexpr int infinity = std::numeric_limits<int>::max() / 4;
enum class Algorithm { Dijkstra, AStar };
enum class SearchStatus { Searching, Found, NoPath, InvalidMap };
enum class NodeState { Unseen, Open, Closed };
enum class EvaluationKind { Discovered, Improved, Blocked, Closed, NoImprovement };

struct NodeData {
    int g{infinity};
    int h{};
    std::optional<int> parent{};
    NodeState state{NodeState::Unseen};
};
struct NeighborEvaluation {
    Point point{};
    EvaluationKind kind{};
};
struct SearchStep {
    std::optional<Point> current{};
    std::vector<NeighborEvaluation> evaluations{};
};
struct SearchResult {
    SearchStatus status{SearchStatus::Searching};
    std::vector<Point> path{}; // Includes start and goal.
    int cost{-1}; // -1 when no route has been found.
    std::size_t expanded{}; // Includes the goal when it is popped.
    [[nodiscard]] std::size_t steps() const noexcept { return path.empty() ? 0 : path.size() - 1; }
};

class SearchSession {
public:
    explicit SearchSession(Map map, Algorithm algorithm, bool recordEvents = true);
    // Discards stale queue entries, then expands at most one live cell and its neighbors.
    const SearchStep& step();
    [[nodiscard]] const Map& map() const noexcept;
    [[nodiscard]] Algorithm algorithm() const noexcept;
    [[nodiscard]] const std::vector<NodeData>& nodes() const noexcept;
    [[nodiscard]] const SearchStep& lastStep() const noexcept;
    [[nodiscard]] const SearchResult& result() const noexcept;
    [[nodiscard]] std::vector<Point> currentPath() const;
private:
    struct QueueEntry { int priority{}; int g{}; std::uint64_t order{}; int index{}; };
    struct QueueCompare { bool operator()(const QueueEntry& left, const QueueEntry& right) const noexcept; };
    [[nodiscard]] int heuristic(Point point) const noexcept;
    [[nodiscard]] std::vector<Point> pathTo(int index) const;
    Map map_;
    Algorithm algorithm_;
    bool recordEvents_;
    std::vector<NodeData> nodes_;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueCompare> open_;
    std::uint64_t sequence_{};
    SearchStep lastStep_{};
    SearchResult result_{};
};

[[nodiscard]] SearchResult solve(const Map& map, Algorithm algorithm);
struct BenchmarkResult {
    SearchResult result{};
    double medianMicroseconds{};
    int runs{};
};
// One warm-up, then independent timed runs including initialization and reconstruction.
[[nodiscard]] BenchmarkResult benchmark(const Map& map, Algorithm algorithm, int runs = 31);

} // namespace pathx
