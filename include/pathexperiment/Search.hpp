#pragma once

#include "pathexperiment/Grid.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

namespace pathx {

inline constexpr int infinity = std::numeric_limits<int>::max() / 4;
enum class Algorithm { Dijkstra, AStar, BreadthFirst, GreedyBestFirst };
enum class SearchStatus { Searching, Found, NoPath, InvalidMap };
enum class NodeState { Unseen, Open, Closed };
enum class EvaluationKind { Discovered, Improved, Blocked, Closed, NoImprovement };

struct NodeData {
    int g{infinity};
    int h{};
    int depth{infinity};
    int priority{infinity};
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

class ISearchSession {
public:
    virtual ~ISearchSession() = default;
    virtual const SearchStep& step() = 0;
    [[nodiscard]] virtual const Map& map() const noexcept = 0;
    [[nodiscard]] virtual const std::vector<NodeData>& nodes() const noexcept = 0;
    [[nodiscard]] virtual const SearchStep& lastStep() const noexcept = 0;
    [[nodiscard]] virtual const SearchResult& result() const noexcept = 0;
    [[nodiscard]] virtual std::vector<Point> currentPath() const = 0;
};

class SearchSession final : public ISearchSession {
public:
    explicit SearchSession(Map map, Algorithm algorithm, bool recordEvents = true);
    // Discards stale queue entries, then expands at most one live cell and its neighbors.
    const SearchStep& step() override;
    [[nodiscard]] const Map& map() const noexcept override;
    [[nodiscard]] Algorithm algorithm() const noexcept;
    [[nodiscard]] const std::vector<NodeData>& nodes() const noexcept override;
    [[nodiscard]] const SearchStep& lastStep() const noexcept override;
    [[nodiscard]] const SearchResult& result() const noexcept override;
    [[nodiscard]] std::vector<Point> currentPath() const override;
private:
    struct QueueEntry {
        int priority{};
        int g{};
        int depth{};
        std::uint64_t order{};
        int index{};
    };
    struct QueueCompare { bool operator()(const QueueEntry& left, const QueueEntry& right) const noexcept; };
    [[nodiscard]] int heuristic(Point point) const noexcept;
    [[nodiscard]] int priority(int g, int h, int depth) const noexcept;
    [[nodiscard]] bool acceptsImprovement(const NodeData& node, int candidateG) const noexcept;
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

struct AlgorithmDescriptor;
[[nodiscard]] SearchResult solve(const Map& map, Algorithm algorithm);
[[nodiscard]] SearchResult solve(const Map& map, const AlgorithmDescriptor& descriptor);
struct BenchmarkResult {
    SearchResult result{};
    double medianMicroseconds{};
    int runs{};
};
// One warm-up, then independent timed runs including initialization and reconstruction.
[[nodiscard]] BenchmarkResult benchmark(const Map& map, Algorithm algorithm, int runs = 31);
[[nodiscard]] BenchmarkResult benchmark(const Map& map,
                                        const AlgorithmDescriptor& descriptor,
                                        int runs = 31);

} // namespace pathx
