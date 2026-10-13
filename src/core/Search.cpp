#include "pathexperiment/Search.hpp"
#include "pathexperiment/AlgorithmRegistry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace pathx {
namespace {

bool validAlgorithm(const Algorithm algorithm) noexcept {
    switch (algorithm) {
    case Algorithm::Dijkstra:
    case Algorithm::AStar:
    case Algorithm::BreadthFirst:
    case Algorithm::GreedyBestFirst:
        return true;
    }
    return false;
}

double median(std::vector<double>& timings) {
    std::sort(timings.begin(), timings.end());
    const std::size_t middle{timings.size() / 2U};
    return timings.size() % 2U == 0U
               ? (timings.at(middle - 1U) + timings.at(middle)) / 2.0
               : timings.at(middle);
}

SearchResult finish(ISearchSession& session) {
    while (session.result().status == SearchStatus::Searching) {
        session.step();
    }
    return session.result();
}

template<class AlgorithmType>
BenchmarkResult timedSearch(const Map& map, const AlgorithmType& algorithm, int runs) {
    if (runs <= 0) throw std::invalid_argument{"benchmark run count must be positive"};
    static_cast<void>(solve(map, algorithm));
    std::vector<double> timings;
    timings.reserve(static_cast<std::size_t>(runs));
    SearchResult result;
    for (int run = 0; run < runs; ++run) {
        const auto started = std::chrono::steady_clock::now();
        result = solve(map, algorithm);
        const auto finished = std::chrono::steady_clock::now();
        timings.push_back(std::chrono::duration<double, std::micro>(finished - started).count());
    }
    return {std::move(result), median(timings), runs};
}

} // namespace

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
    if (!validAlgorithm(algorithm_)) {
        throw std::invalid_argument{"unknown search algorithm"};
    }
    if (!map_.grid.contains(map_.start) || !map_.grid.contains(map_.goal) ||
        !passable(map_.grid.at(map_.start)) || !passable(map_.grid.at(map_.goal))) {
        result_.status = SearchStatus::InvalidMap;
        return;
    }

    const int startIndex{map_.grid.index(map_.start)};
    NodeData& startNode{nodes_.at(static_cast<std::size_t>(startIndex))};
    startNode.g = 0;
    startNode.h = heuristic(map_.start);
    startNode.depth = 0;
    startNode.priority = priority(startNode.g, startNode.h, startNode.depth);
    startNode.state = NodeState::Open;
    open_.push(QueueEntry{startNode.priority,
                          startNode.g,
                          startNode.depth,
                          sequence_++,
                          startIndex});
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
        if (currentNode.state != NodeState::Open || currentNode.priority != entry.priority ||
            currentNode.g != entry.g || currentNode.depth != entry.depth) {
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
                const int tentativeDepth{currentNode.depth + 1};
                if (neighborNode.state == NodeState::Unseen) {
                    kind = EvaluationKind::Discovered;
                } else if (acceptsImprovement(neighborNode, tentativeG)) {
                    kind = EvaluationKind::Improved;
                }

                if (kind == EvaluationKind::Discovered || kind == EvaluationKind::Improved) {
                    neighborNode.g = tentativeG;
                    neighborNode.h = heuristic(neighbor);
                    neighborNode.depth = tentativeDepth;
                    neighborNode.priority = priority(neighborNode.g,
                                                     neighborNode.h,
                                                     neighborNode.depth);
                    neighborNode.parent = entry.index;
                    neighborNode.state = NodeState::Open;
                    open_.push(QueueEntry{neighborNode.priority,
                                          neighborNode.g,
                                          neighborNode.depth,
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
    if (algorithm_ != Algorithm::AStar && algorithm_ != Algorithm::GreedyBestFirst) {
        return 0;
    }
    return std::abs(point.x - map_.goal.x) + std::abs(point.y - map_.goal.y);
}

int SearchSession::priority(const int g, const int h, const int depth) const noexcept {
    switch (algorithm_) {
    case Algorithm::Dijkstra:
        return g;
    case Algorithm::AStar:
        return g + h;
    case Algorithm::BreadthFirst:
        return depth;
    case Algorithm::GreedyBestFirst:
        return h;
    }
    return infinity;
}

bool SearchSession::acceptsImprovement(const NodeData& node,
                                       const int candidateG) const noexcept {
    return (algorithm_ == Algorithm::Dijkstra || algorithm_ == Algorithm::AStar) &&
           candidateG < node.g;
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
    return finish(session);
}

SearchResult solve(const Map& map, const AlgorithmDescriptor& descriptor) {
    if (!descriptor.create) {
        throw std::invalid_argument{"algorithm descriptor has no session factory"};
    }
    std::unique_ptr<ISearchSession> session{descriptor.create(map, false)};
    if (!session) {
        throw std::runtime_error{"algorithm session factory returned null"};
    }
    return finish(*session);
}

BenchmarkResult benchmark(const Map& map, const Algorithm algorithm, const int runs) {
    return timedSearch(map, algorithm, runs);
}

BenchmarkResult benchmark(const Map& map,
                          const AlgorithmDescriptor& descriptor,
                          const int runs) {
    return timedSearch(map, descriptor, runs);
}

} // namespace pathx
