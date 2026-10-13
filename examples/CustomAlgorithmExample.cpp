#include "pathexperiment/AlgorithmRegistry.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace pathx::examples {

// A complete depth-first search example. It demonstrates the extension API and
// intentionally makes no shortest-path or lowest-cost guarantee.
class DepthFirstSession final : public ISearchSession {
public:
    explicit DepthFirstSession(Map map, const bool recordEvents)
        : map_{std::move(map)},
          recordEvents_{recordEvents},
          nodes_(map_.grid.size()) {
        if (!map_.grid.contains(map_.start) || !map_.grid.contains(map_.goal) ||
            !passable(map_.grid.at(map_.start)) || !passable(map_.grid.at(map_.goal))) {
            result_.status = SearchStatus::InvalidMap;
            return;
        }

        const int startIndex{map_.grid.index(map_.start)};
        NodeData& start{nodes_.at(static_cast<std::size_t>(startIndex))};
        start.g = 0;
        start.depth = 0;
        start.priority = 0;
        start.state = NodeState::Open;
        stack_.push_back(startIndex);
    }

    const SearchStep& step() override {
        lastStep_ = {};
        if (result_.status != SearchStatus::Searching) {
            return lastStep_;
        }

        while (!stack_.empty()) {
            const int currentIndex{stack_.back()};
            stack_.pop_back();
            NodeData& currentNode{nodes_.at(static_cast<std::size_t>(currentIndex))};
            if (currentNode.state != NodeState::Open) {
                continue;
            }

            const Point current{map_.grid.point(currentIndex)};
            currentNode.state = NodeState::Closed;
            ++result_.expanded;
            if (recordEvents_) {
                lastStep_.current = current;
            }

            if (current == map_.goal) {
                result_.status = SearchStatus::Found;
                result_.path = pathTo(currentIndex);
                result_.cost = currentNode.g;
                return lastStep_;
            }

            std::vector<int> discovered;
            for (const Point neighbor : map_.grid.neighbors(current)) {
                const int neighborIndex{map_.grid.index(neighbor)};
                NodeData& neighborNode{nodes_.at(static_cast<std::size_t>(neighborIndex))};
                EvaluationKind kind{EvaluationKind::NoImprovement};
                if (!passable(map_.grid.at(neighbor))) {
                    kind = EvaluationKind::Blocked;
                } else if (neighborNode.state == NodeState::Closed) {
                    kind = EvaluationKind::Closed;
                } else if (neighborNode.state == NodeState::Unseen) {
                    kind = EvaluationKind::Discovered;
                    neighborNode.g = currentNode.g + entryCost(map_.grid.at(neighbor));
                    neighborNode.depth = currentNode.depth + 1;
                    neighborNode.priority = neighborNode.depth;
                    neighborNode.parent = currentIndex;
                    neighborNode.state = NodeState::Open;
                    discovered.push_back(neighborIndex);
                }

                if (recordEvents_) {
                    lastStep_.evaluations.push_back({neighbor, kind});
                }
            }

            // Reverse insertion preserves Grid::neighbors() order when the
            // LIFO stack chooses the next cell.
            for (auto iterator = discovered.rbegin(); iterator != discovered.rend(); ++iterator) {
                stack_.push_back(*iterator);
            }
            return lastStep_;
        }

        result_.status = SearchStatus::NoPath;
        return lastStep_;
    }

    [[nodiscard]] const Map& map() const noexcept override {
        return map_;
    }

    [[nodiscard]] const std::vector<NodeData>& nodes() const noexcept override {
        return nodes_;
    }

    [[nodiscard]] const SearchStep& lastStep() const noexcept override {
        return lastStep_;
    }

    [[nodiscard]] const SearchResult& result() const noexcept override {
        return result_;
    }

    [[nodiscard]] std::vector<Point> currentPath() const override {
        if (result_.status == SearchStatus::Found) {
            return result_.path;
        }
        return lastStep_.current.has_value()
                   ? pathTo(map_.grid.index(*lastStep_.current))
                   : std::vector<Point>{};
    }

private:
    [[nodiscard]] std::vector<Point> pathTo(int index) const {
        std::vector<Point> path;
        path.reserve(nodes_.size());
        while (true) {
            path.push_back(map_.grid.point(index));
            const std::optional<int>& parent{nodes_.at(static_cast<std::size_t>(index)).parent};
            if (!parent.has_value()) {
                break;
            }
            index = *parent;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    Map map_;
    bool recordEvents_;
    std::vector<NodeData> nodes_;
    std::vector<int> stack_;
    SearchStep lastStep_;
    SearchResult result_;
};

void registerDepthFirstExample(AlgorithmRegistry& registry) {
    registry.add(AlgorithmDescriptor{
        "depth-first-example",
        "Depth-first (example)",
        "Explores one branch at a time using a LIFO stack; routes are not optimal.",
        "LIFO stack",
        false,
        [](Map map, const bool recordEvents) {
            return std::make_unique<DepthFirstSession>(std::move(map), recordEvents);
        },
    });
}

} // namespace pathx::examples
