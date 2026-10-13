#pragma once

#include "pathexperiment/Search.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pathx {

struct AlgorithmDescriptor {
    std::string id;
    std::string name;
    std::string description;
    std::string priorityFormula;
    bool guaranteesLowestCost{};
    std::function<std::unique_ptr<ISearchSession>(Map, bool)> create;
};

class AlgorithmRegistry {
public:
    void add(AlgorithmDescriptor descriptor);
    [[nodiscard]] const std::vector<AlgorithmDescriptor>& algorithms() const noexcept;
    [[nodiscard]] const AlgorithmDescriptor& get(std::string_view id) const;

private:
    std::vector<AlgorithmDescriptor> algorithms_;
};

[[nodiscard]] AlgorithmRegistry makeDefaultRegistry();
void registerCustomAlgorithms(AlgorithmRegistry& registry);

} // namespace pathx
