#include "pathexperiment/AlgorithmRegistry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pathx {
namespace {

AlgorithmDescriptor builtinDescriptor(std::string id,
                                      std::string name,
                                      std::string description,
                                      std::string priorityFormula,
                                      const bool guaranteesLowestCost,
                                      const Algorithm algorithm) {
    return AlgorithmDescriptor{
        std::move(id),
        std::move(name),
        std::move(description),
        std::move(priorityFormula),
        guaranteesLowestCost,
        [algorithm](Map map, const bool recordEvents) {
            return std::make_unique<SearchSession>(std::move(map), algorithm, recordEvents);
        },
    };
}

} // namespace

void AlgorithmRegistry::add(AlgorithmDescriptor descriptor) {
    if (descriptor.id.empty()) {
        throw std::invalid_argument{"algorithm ID must not be empty"};
    }
    if (!descriptor.create) {
        throw std::invalid_argument{"algorithm session factory must not be empty"};
    }
    const auto duplicate{std::ranges::find(algorithms_, descriptor.id,
                                           &AlgorithmDescriptor::id)};
    if (duplicate != algorithms_.end()) {
        throw std::invalid_argument{"duplicate algorithm ID: " + descriptor.id};
    }
    algorithms_.push_back(std::move(descriptor));
}

const std::vector<AlgorithmDescriptor>& AlgorithmRegistry::algorithms() const noexcept {
    return algorithms_;
}

const AlgorithmDescriptor& AlgorithmRegistry::get(const std::string_view id) const {
    const auto found{std::ranges::find(algorithms_, id, &AlgorithmDescriptor::id)};
    if (found == algorithms_.end()) {
        throw std::out_of_range{"unknown algorithm ID: " + std::string{id}};
    }
    return *found;
}

AlgorithmRegistry makeDefaultRegistry() {
    AlgorithmRegistry registry;
    registry.add(builtinDescriptor("dijkstra",
                                   "Dijkstra",
                                   "Finds a route with the lowest weighted movement cost.",
                                   "g",
                                   true,
                                   Algorithm::Dijkstra));
    registry.add(builtinDescriptor("astar",
                                   "A*",
                                   "Uses accumulated cost and Manhattan distance.",
                                   "g + h",
                                   true,
                                   Algorithm::AStar));
    registry.add(builtinDescriptor("bfs",
                                   "Breadth-first",
                                   "Finds a route with the fewest steps; terrain costs do not guide it.",
                                   "depth",
                                   false,
                                   Algorithm::BreadthFirst));
    registry.add(builtinDescriptor("greedy",
                                   "Greedy best-first",
                                   "Follows Manhattan distance toward the goal without an optimality guarantee.",
                                   "h",
                                   false,
                                   Algorithm::GreedyBestFirst));
    registerCustomAlgorithms(registry);
    return registry;
}

} // namespace pathx
