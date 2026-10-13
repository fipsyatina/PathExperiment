#include "pathexperiment/Search.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace pathx {

bool SearchSession::QueueCompare::operator()(const QueueEntry& left,
                                              const QueueEntry& right) const noexcept {
    if (left.priority != right.priority) {
        return left.priority > right.priority;
    }
    return left.order > right.order;
}

SearchSession::SearchSession(Map map, const Algorithm algorithm, const bool recordEvents)
    : map_{std::move(map)},
      algorithm_{algorithm},
      recordEvents_{recordEvents},
      nodes_(map_.grid.size()) {
    if (!map_.grid.contains(map_.start) || !map_.grid.contains(map_.goal) ||
        !passable(map_.grid.at(map_.start)) || !passable(map_.grid.at(map_.goal))) {
        result_.status = SearchStatus::InvalidMap;
        return;
    }

    const int startIndex{map_.grid.index(map_.start)};
    NodeData& startNode{nodes_.at(static_cast<std::size_t>(startIndex))};
    startNode.g = 0;
    startNode.h = heuristic(map_.start);
    startNode.state = NodeState::Open;
    open_.push(QueueEntry{startNode.g + startNode.h, startNode.g, sequence_++, startIndex});
}

const SearchStep& SearchSession::step() {
    lastStep_ = {};
    if (result_.status != SearchStatus::Searching) {
        return lastStep_;
    }

    while (!open_.empty()) {
        const QueueEntry entry{open_.top()};
        open_.pop();

        NodeData& currentNode{nodes_.at(static_cast<std::size_t>(entry.index))};
        if (currentNode.state != NodeState::Open || currentNode.g != entry.g) {
            continue;
        }

        const Point current{map_.grid.point(entry.index)};
        currentNode.state = NodeState::Closed;
        ++result_.expanded;
        if (recordEvents_) {
            lastStep_.current = current;
        }

        if (current == map_.goal) {
            result_.status = SearchStatus::Found;
            result_.path = pathTo(entry.index);
            result_.cost = currentNode.g;
            return lastStep_;
        }

        for (const Point neighbor : map_.grid.neighbors(current)) {
            const int neighborIndex{map_.grid.index(neighbor)};
            NodeData& neighborNode{nodes_.at(static_cast<std::size_t>(neighborIndex))};
            EvaluationKind kind{EvaluationKind::NoImprovement};

            if (!passable(map_.grid.at(neighbor))) {
                kind = EvaluationKind::Blocked;
            } else if (neighborNode.state == NodeState::Closed) {
                kind = EvaluationKind::Closed;
            } else {
                const int tentativeG{currentNode.g + entryCost(map_.grid.at(neighbor))};
                if (neighborNode.state == NodeState::Unseen) {
                    kind = EvaluationKind::Discovered;
                } else if (tentativeG < neighborNode.g) {
                    kind = EvaluationKind::Improved;
                }

                if (kind == EvaluationKind::Discovered || kind == EvaluationKind::Improved) {
                    neighborNode.g = tentativeG;
                    neighborNode.h = heuristic(neighbor);
                    neighborNode.parent = entry.index;
                    neighborNode.state = NodeState::Open;
                    open_.push(QueueEntry{neighborNode.g + neighborNode.h,
                                          neighborNode.g,
                                          sequence_++,
                                          neighborIndex});
                }
            }

            if (recordEvents_) {
                lastStep_.evaluations.push_back(NeighborEvaluation{neighbor, kind});
            }
        }
        return lastStep_;
    }

    result_.status = SearchStatus::NoPath;
    return lastStep_;
}

const Map& SearchSession::map() const noexcept {
    return map_;
}

Algorithm SearchSession::algorithm() const noexcept {
    return algorithm_;
}

const std::vector<NodeData>& SearchSession::nodes() const noexcept {
    return nodes_;
}

const SearchStep& SearchSession::lastStep() const noexcept {
    return lastStep_;
}

const SearchResult& SearchSession::result() const noexcept {
    return result_;
}

std::vector<Point> SearchSession::currentPath() const {
    if (result_.status == SearchStatus::Found) {
        return result_.path;
    }
    if (!lastStep_.current.has_value()) {
        return {};
    }
    return pathTo(map_.grid.index(*lastStep_.current));
}

int SearchSession::heuristic(const Point point) const noexcept {
    if (algorithm_ == Algorithm::Dijkstra) {
        return 0;
    }
    return std::abs(point.x - map_.goal.x) + std::abs(point.y - map_.goal.y);
}

std::vector<Point> SearchSession::pathTo(int index) const {
    std::vector<Point> reversePath;
    reversePath.reserve(nodes_.size());

    while (true) {
        reversePath.push_back(map_.grid.point(index));
        const std::optional<int>& parent{nodes_.at(static_cast<std::size_t>(index)).parent};
        if (!parent.has_value()) {
            break;
        }
        index = *parent;
    }

    std::reverse(reversePath.begin(), reversePath.end());
    return reversePath;
}

SearchResult solve(const Map& map, const Algorithm algorithm) {
    SearchSession session{map, algorithm, false};
    while (session.result().status == SearchStatus::Searching) {
        session.step();
    }
    return session.result();
}

BenchmarkResult benchmark(const Map& map, const Algorithm algorithm, const int runs) {
    if (runs <= 0) {
        throw std::invalid_argument{"benchmark run count must be positive"};
    }

    static_cast<void>(solve(map, algorithm));

    std::vector<double> timings;
    timings.reserve(static_cast<std::size_t>(runs));
    SearchResult result;
    for (int run{}; run < runs; ++run) {
        const auto started{std::chrono::steady_clock::now()};
        result = solve(map, algorithm);
        const auto finished{std::chrono::steady_clock::now()};
        timings.push_back(std::chrono::duration<double, std::micro>{finished - started}.count());
    }

    std::sort(timings.begin(), timings.end());
    const std::size_t middle{timings.size() / 2U};
    const double median{timings.size() % 2U == 0U
                            ? (timings.at(middle - 1U) + timings.at(middle)) / 2.0
                            : timings.at(middle)};
    return BenchmarkResult{std::move(result), median, runs};
}

} // namespace pathx
