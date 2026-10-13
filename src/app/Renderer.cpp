#include "Application.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace pathx {
namespace {
float cellSize(const Grid& grid) {
    return std::min({theme::maximumCellSize, theme::canvasSize.x / static_cast<float>(grid.width()),
                     theme::canvasSize.y / static_cast<float>(grid.height())});
}
sf::Vector2f origin(const Grid& grid) {
    const float size = cellSize(grid);
    return theme::canvasOrigin + (theme::canvasSize - sf::Vector2f(static_cast<float>(grid.width()) * size,
                                                                 static_cast<float>(grid.height()) * size)) / 2.f;
}
sf::Color terrainColor(Terrain terrain) {
    switch (terrain) {
    case Terrain::Road: return {238, 224, 178};
    case Terrain::Plain: return {255, 255, 255};
    case Terrain::Swamp: return {185, 202, 180};
    case Terrain::Wall: return {88, 96, 104};
    }
    return sf::Color::Black;
}
std::string number(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(value < 100 ? 1 : 0) << value;
    return stream.str();
}
std::string finite(int value) { return value >= infinity ? "--" : std::to_string(value); }
std::string shorten(const std::string& value, std::size_t size) {
    return value.size() > size ? value.substr(0, size - 3) + "..." : value;
}
std::vector<std::string> wrap(const std::string& value, std::size_t columns) {
    std::istringstream words(value);
    std::vector<std::string> lines;
    std::string word, line;
    while (words >> word) {
        if (!line.empty() && line.size() + word.size() + 1 > columns) { lines.push_back(line); line.clear(); }
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
const AlgorithmDescriptor& selected(const AppState& state) {
    return state.registry.algorithms().at(state.algorithmIndex);
}
bool accepted(EvaluationKind kind) {
    return kind == EvaluationKind::Discovered || kind == EvaluationKind::Improved;
}
} // namespace

Renderer::Renderer(const std::filesystem::path& resources) : font_(resources / "assets/fonts/NotoSans.ttf") {
    geometry_.reserve(80000);
}
sf::FloatRect Renderer::toolBounds(int tool) { return {{16.f + 112.f * static_cast<float>(tool), 48.f}, {104.f, 32.f}}; }
sf::FloatRect Renderer::speedBounds() { return {{1008.f, 376.f}, {244.f, 20.f}}; }
sf::FloatRect Renderer::algorithmOptionBounds(std::size_t index) {
    return {{1008.f, 162.f + 32.f * static_cast<float>(index)}, {244.f, 32.f}};
}
std::optional<Point> Renderer::cellAt(sf::Vector2f position, const Grid& grid) {
    if (!sf::FloatRect(theme::canvasOrigin, theme::canvasSize).contains(position)) return {};
    const auto local = position - origin(grid);
    if (local.x < 0.f || local.y < 0.f) return {};
    const float size = cellSize(grid);
    const Point point{static_cast<int>(local.x / size), static_cast<int>(local.y / size)};
    return grid.contains(point) ? std::optional(point) : std::nullopt;
}
sf::Vector2f Renderer::cellCenter(Point point, const Grid& grid) {
    const float size = cellSize(grid);
    return origin(grid) + sf::Vector2f((static_cast<float>(point.x) + .5f) * size,
                                     (static_cast<float>(point.y) + .5f) * size);
}
std::vector<ButtonSpec> Renderer::buttons(const AppState& state) {
    return {
        {{{16.f, 8.f}, {88.f, 28.f}}, "New [C]", Action::Clear},
        {{{112.f, 8.f}, {68.f, 28.f}}, "Open", Action::Load},
        {{{188.f, 8.f}, {68.f, 28.f}}, "Save", Action::Save},
        {{{276.f, 8.f}, {28.f, 28.f}}, "<", Action::PreviousSample},
        {{{484.f, 8.f}, {28.f, 28.f}}, ">", Action::NextSample},
        {{{520.f, 8.f}, {108.f, 28.f}}, "Load example", Action::LoadSample},
        {{{1008.f, 126.f}, {244.f, 32.f}}, shorten(selected(state).name, 31), Action::AlgorithmMenu, state.algorithmMenuOpen},
        {{{1008.f, 264.f}, {116.f, 32.f}}, state.running ? "Pause [Space]" : "Run [Space]", Action::RunPause, false, true},
        {{{1136.f, 264.f}, {116.f, 32.f}}, "Step [N]", Action::Step},
        {{{1008.f, 304.f}, {116.f, 30.f}}, "Reset [R]", Action::Reset},
        {{{1136.f, 304.f}, {116.f, 30.f}}, "Finish", Action::Finish},
        {{{1008.f, 568.f}, {244.f, 30.f}}, "Compare all algorithms [B]", Action::Compare}
    };
}
void Renderer::quad(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d, sf::Color color) const {
    geometry_.insert(geometry_.end(), {{a, color, {}}, {b, color, {}}, {c, color, {}},
                                      {c, color, {}}, {d, color, {}}, {a, color, {}}});
}
void Renderer::flush(sf::RenderTarget& target) const {
    if (geometry_.empty()) return;
    target.draw(geometry_.data(), geometry_.size(), sf::PrimitiveType::Triangles);
    geometry_.clear();
}
void Renderer::box(sf::RenderTarget&, sf::FloatRect bounds, sf::Color fill, sf::Color outline, float thickness) const {
    const auto rectangle = [&](sf::Vector2f p, sf::Vector2f s, sf::Color color) {
        if (color.a == 0 || s.x <= 0.f || s.y <= 0.f) return;
        quad(p, p + sf::Vector2f(s.x, 0.f), p + s, p + sf::Vector2f(0.f, s.y), color);
    };
    rectangle(bounds.position, bounds.size, fill);
    if (outline.a == 0 || thickness <= 0.f) return;
    const auto p = bounds.position, s = bounds.size;
    rectangle(p - sf::Vector2f(thickness, thickness), {s.x + 2.f * thickness, thickness}, outline);
    rectangle(p + sf::Vector2f(-thickness, s.y), {s.x + 2.f * thickness, thickness}, outline);
    rectangle(p + sf::Vector2f(-thickness, 0.f), {thickness, s.y}, outline);
    rectangle(p + sf::Vector2f(s.x, 0.f), {thickness, s.y}, outline);
}
void Renderer::line(sf::RenderTarget&, sf::Vector2f from, sf::Vector2f to, sf::Color color, float thickness) const {
    const auto delta = to - from;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length < .01f) return;
    const sf::Vector2f normal{-delta.y / length * thickness / 2.f, delta.x / length * thickness / 2.f};
    quad(from - normal, to - normal, to + normal, from + normal, color);
}
void Renderer::text(sf::RenderTarget& target, const std::string& value, float x, float y,
                    unsigned int size, sf::Color color, bool bold) const {
    flush(target);
    const auto key = std::tuple{x, y, size, bold};
    auto found = labels_.find(key);
    const auto string = sf::String::fromUtf8(value.begin(), value.end());
    if (found == labels_.end()) {
        sf::Text label(font_, string, size);
        label.setPosition({x, y});
        if (bold) label.setStyle(sf::Text::Bold);
        found = labels_.emplace(key, std::move(label)).first;
    } else if (found->second.getString() != string) found->second.setString(string);
    if (found->second.getFillColor() != color) found->second.setFillColor(color);
    target.draw(found->second);
}
void Renderer::grid(sf::RenderTarget& target, const AppState& state) const {
    const auto& map = state.map;
    const float size = cellSize(map.grid);
    const auto position = [&](Point point) { return cellCenter(point, map.grid) - sf::Vector2f(size / 2.f, size / 2.f); };
    const float gap = size >= 3.f ? .6f : 0.f;
    box(target, {origin(map.grid), {size * static_cast<float>(map.grid.width()), size * static_cast<float>(map.grid.height())}}, {216, 221, 225}, theme::border);
    for (int y = 0; y < map.grid.height(); ++y) {
        for (int x = 0; x < map.grid.width(); ++x) {
            const Point point{x, y};
            const auto terrain = map.grid.at(point);
            auto color = terrainColor(terrain);
            if (state.search) {
                const auto& node = state.search->nodes().at(static_cast<std::size_t>(map.grid.index(point)));
                if (node.state == NodeState::Closed) color = theme::explored;
                else if (node.state == NodeState::Open) color = theme::frontier;
            }
            box(target, {position(point), {size - gap, size - gap}}, color);
            if (terrain == Terrain::Road && size >= 5.f)
                box(target, {position(point) + sf::Vector2f(size * .3f, size * .7f), {size * .3f, .8f}}, {161, 135, 65});
            else if (terrain == Terrain::Swamp && size >= 5.f)
                line(target, position(point) + sf::Vector2f(1.f, size * .7f), position(point) + sf::Vector2f(size - 2.f, size * .7f), {91, 120, 83}, .8f);
        }
    }
    if (state.search) {
        const bool found = state.search->result().status == SearchStatus::Found;
        const auto path = found ? state.search->result().path : state.search->currentPath();
        const auto color = found ? theme::route : theme::current;
        for (const auto point : path)
            box(target, {position(point) + sf::Vector2f(size * .15f, size * .15f), {size * .7f, size * .7f}}, found ? theme::route : theme::candidate);
        for (std::size_t i = 1; i < path.size(); ++i)
            line(target, cellCenter(path[i - 1], map.grid), cellCenter(path[i], map.grid), color, std::min(2.f, size * .25f));
        if (const auto current = state.search->lastStep().current) {
            box(target, {position(*current), {size - gap, size - gap}}, theme::current, theme::ink, std::min(1.f, size * .1f));
            if (state.search->result().status == SearchStatus::Searching) {
                for (const auto& evaluation : state.search->lastStep().evaluations) {
                    const auto p = position(evaluation.point);
                    const auto mark = accepted(evaluation.kind) ? theme::route : theme::rejected;
                    box(target, {p + sf::Vector2f(1.f, 1.f), {std::max(.1f, size - 2.6f), std::max(.1f, size - 2.6f)}}, sf::Color::Transparent, mark, std::min(1.f, size * .1f));
                    if (!accepted(evaluation.kind) && size >= 5.f) {
                        line(target, p + sf::Vector2f(2.f, 2.f), p + sf::Vector2f(size - 2.f, size - 2.f), mark, .8f);
                        line(target, p + sf::Vector2f(size - 2.f, 2.f), p + sf::Vector2f(2.f, size - 2.f), mark, .8f);
                    }
                }
            }
        }
    }
    const auto endpoint = [&](Point point, const std::string& label, sf::Color color, float offset) {
        const auto center = cellCenter(point, map.grid);
        box(target, {position(point), {size - gap, size - gap}}, color, theme::ink, .7f);
        const sf::Vector2f badge{label == "S/G" ? 25.f : 14.f, 16.f};
        const sf::Vector2f anchor{
            std::clamp(center.x - badge.x / 2.f + offset, theme::canvasOrigin.x + 2.f, theme::canvasOrigin.x + theme::canvasSize.x - badge.x - 2.f),
            std::max(theme::canvasOrigin.y + 2.f, center.y - 22.f)};
        line(target, center, {anchor.x + badge.x / 2.f, anchor.y + badge.y}, color, 1.f);
        box(target, {anchor, badge}, theme::surface, color, 1.f);
        text(target, label, anchor.x + 3.f, anchor.y - 1.f, 11, theme::ink, true);
    };
    if (map.start == map.goal) endpoint(map.start, "S/G", theme::route, 0.f);
    else {
        const bool close = std::abs(map.start.x - map.goal.x) + std::abs(map.start.y - map.goal.y) <= 2;
        endpoint(map.start, "S", theme::route, close ? -8.f : 0.f);
        endpoint(map.goal, "G", {117, 73, 133}, close ? 8.f : 0.f);
    }
    if (state.hover && map.grid.contains(*state.hover))
        box(target, {position(*state.hover), {size - gap, size - gap}}, sf::Color::Transparent, theme::ink, 1.f);
}
void Renderer::sidebar(sf::RenderTarget& target, const AppState& state) const {
    const auto& algorithm = selected(state);
    text(target, "Algorithm", 1008.f, 99.f, 13, theme::ink, true);
    const auto description = wrap(algorithm.description, 37);
    for (std::size_t i = 0; i < std::min<std::size_t>(description.size(), 3); ++i)
        text(target, description[i], 1008.f, 169.f + 15.f * static_cast<float>(i), 11, theme::muted);
    text(target, "Priority: " + algorithm.priorityFormula, 1008.f, 219.f, 12, theme::ink);
    text(target, algorithm.guaranteesLowestCost ? "Minimum weighted cost" : "No minimum-cost guarantee", 1008.f, 239.f, 11,
         algorithm.guaranteesLowestCost ? theme::route : theme::rejected);
    text(target, "Speed", 1008.f, 350.f, 12, theme::ink);
    text(target, number(state.stepsPerSecond) + " cells/s", 1162.f, 350.f, 12, theme::muted);
    const auto speed = speedBounds();
    const float ratio = std::log(state.stepsPerSecond) / std::log(60.f);
    box(target, {{speed.position.x, 384.f}, {speed.size.x, 3.f}}, theme::border);
    box(target, {{speed.position.x, 384.f}, {speed.size.x * ratio, 3.f}}, theme::accent);
    box(target, {{speed.position.x + speed.size.x * ratio - 3.f, 379.f}, {6.f, 13.f}}, theme::accent);
    line(target, {1008.f, 412.f}, {1252.f, 412.f}, theme::border, 1.f);
    text(target, "Result", 1008.f, 424.f, 13, theme::ink, true);
    std::string status = "Ready";
    sf::Color statusColor = theme::muted;
    if (state.search) {
        switch (state.search->result().status) {
        case SearchStatus::Searching: status = state.running ? "Searching" : "Paused"; statusColor = theme::accent; break;
        case SearchStatus::Found: status = "Route found"; statusColor = theme::route; break;
        case SearchStatus::NoPath: status = "No route"; statusColor = theme::rejected; break;
        case SearchStatus::InvalidMap: status = "Invalid endpoints"; statusColor = theme::rejected; break;
        }
    }
    text(target, status, 1160.f, 424.f, 13, statusColor, true);
    const auto* result = state.search ? &state.search->result() : nullptr;
    const bool found = result && result->status == SearchStatus::Found;
    const auto value = [&](const std::string& label, const std::string& data, float y) {
        text(target, label, 1008.f, y, 12, theme::muted);
        text(target, data, 1170.f, y, 12, theme::ink, true);
    };
    value("Path cost", found ? std::to_string(result->cost) : "--", 460.f);
    value("Steps", found ? std::to_string(result->steps()) : "--", 484.f);
    value("Expanded cells", result ? std::to_string(result->expanded) : "0", 508.f);
    const auto& measured = state.comparison.at(state.algorithmIndex);
    value("Compute median", measured ? number(measured->medianMicroseconds) + " us" : "--", 532.f);
    text(target, "31 runs / same map / no animation", 1008.f, 607.f, 10, theme::muted);
    text(target, "Algorithm", 1008.f, 631.f, 10, theme::muted);
    text(target, "Cost", 1114.f, 631.f, 10, theme::muted);
    text(target, "Cells", 1160.f, 631.f, 10, theme::muted);
    text(target, "us", 1230.f, 631.f, 10, theme::muted);
    const auto first = state.algorithmIndex / 4 * 4;
    const auto end = std::min(first + 4, state.registry.algorithms().size());
    for (std::size_t i = first; i < end; ++i) {
        const float y = 653.f + 23.f * static_cast<float>(i - first);
        const auto& run = state.comparison.at(i);
        text(target, shorten(state.registry.algorithms()[i].name, 20), 1008.f, y, 10, theme::ink, i == state.algorithmIndex);
        text(target, run && run->result.status == SearchStatus::Found ? std::to_string(run->result.cost) : "--", 1114.f, y, 10, theme::ink);
        text(target, run ? std::to_string(run->result.expanded) : "--", 1160.f, y, 10, theme::ink);
        text(target, run ? number(run->medianMicroseconds) : "--", 1210.f, y, 10, theme::ink);
    }
#ifdef NDEBUG
    text(target, "Release / includes setup + route recovery", 1008.f, 759.f, 10, theme::muted);
#else
    text(target, "Debug: use Release for timing comparisons", 1008.f, 759.f, 10, theme::rejected);
#endif
    if (state.registry.algorithms().size() > 4)
        text(target, "Rows follow the selected algorithm group", 1008.f, 780.f, 10, theme::muted);
}
void Renderer::draw(sf::RenderWindow& window, const AppState& state) const {
    geometry_.clear();
    if (labels_.size() > 1024) labels_.clear();
    box(window, {{0.f, 0.f}, {1280.f, 84.f}}, theme::toolbar);
    line(window, {0.f, 40.f}, {1280.f, 40.f}, theme::border, 1.f);
    line(window, {0.f, 84.f}, {1280.f, 84.f}, theme::border, 1.f);
    line(window, {992.f, 84.f}, {992.f, 800.f}, theme::border, 1.f);
    box(window, {theme::canvasOrigin, theme::canvasSize}, theme::surface, theme::border);
    text(window, shorten(sampleNames().at(static_cast<std::size_t>(state.sampleIndex)), 24), 314.f, 13.f, 12, theme::ink);
    text(window, "Ctrl+S: save   Ctrl+O: open", 1032.f, 14.f, 11, theme::muted);
    text(window, "Shift-click: start    Ctrl-click: goal", 706.f, 56.f, 12, theme::muted);
    text(window, state.mapName, 16.f, 90.f, 12, theme::ink);
    text(window, std::to_string(state.map.grid.width()) + " x " + std::to_string(state.map.grid.height()) + "  /  " + number(cellSize(state.map.grid)) + " px cells", 746.f, 90.f, 11, theme::muted);
    const std::array<std::string, 6> names{"Road 1", "Plain 2", "Swamp 5", "Wall", "Start [S]", "Goal [G]"};
    const std::array colors{terrainColor(Terrain::Road), terrainColor(Terrain::Plain), terrainColor(Terrain::Swamp), terrainColor(Terrain::Wall), theme::route, sf::Color(117, 73, 133)};
    for (int i = 0; i < 6; ++i) {
        const auto bounds = toolBounds(i);
        const bool active = state.tool == static_cast<Tool>(i);
        box(window, bounds, active ? theme::selection : bounds.contains(state.mouse) ? theme::surface : theme::toolbar,
            active ? theme::accent : theme::border);
        box(window, {bounds.position + sf::Vector2f(8.f, 10.f), {10.f, 10.f}}, colors[static_cast<std::size_t>(i)], theme::border);
        text(window, names[static_cast<std::size_t>(i)], bounds.position.x + 25.f, bounds.position.y + 6.f, 11, theme::ink, active);
        text(window, std::to_string(i + 1), bounds.position.x + 91.f, bounds.position.y + 7.f, 10, theme::muted);
    }
    grid(window, state);
    const std::array<std::pair<std::string, sf::Color>, 6> legend{{{"Frontier", theme::frontier}, {"Explored", theme::explored}, {"Current", theme::current}, {"Candidate", theme::candidate}, {"Route", theme::route}, {"Rejected", theme::rejected}}};
    for (std::size_t i = 0; i < legend.size(); ++i) {
        const float x = 16.f + 130.f * static_cast<float>(i);
        box(window, {{x, 764.f}, {9.f, 9.f}}, legend[i].second, theme::border);
        text(window, legend[i].first, x + 15.f, 758.f, 11, theme::muted);
    }
    text(window, "S (" + std::to_string(state.map.start.x) + "," + std::to_string(state.map.start.y) + ")  G (" + std::to_string(state.map.goal.x) + "," + std::to_string(state.map.goal.y) + ")", 16.f, 783.f, 11, theme::ink);
    if (state.hover) {
        const auto point = *state.hover;
        const auto terrain = state.map.grid.at(point);
        std::string info = "Cell (" + std::to_string(point.x) + "," + std::to_string(point.y) + ")  entry " + (passable(terrain) ? std::to_string(entryCost(terrain)) : "blocked");
        if (state.search) {
            const auto& node = state.search->nodes().at(static_cast<std::size_t>(state.map.grid.index(point)));
            info += "    g " + finite(node.g) + "   h " + (node.state == NodeState::Unseen ? "--" : std::to_string(node.h)) + "   priority " + finite(node.priority);
            if (node.parent) {
                const auto parent = state.map.grid.point(*node.parent);
                info += "   parent (" + std::to_string(parent.x) + "," + std::to_string(parent.y) + ")";
            }
        }
        text(window, info, 270.f, 783.f, 11, theme::muted);
    } else if (state.search && state.search->lastStep().current) {
        const auto point = *state.search->lastStep().current;
        const auto& node = state.search->nodes().at(static_cast<std::size_t>(state.map.grid.index(point)));
        text(window, "Current (" + std::to_string(point.x) + "," + std::to_string(point.y) + ")   priority " + finite(node.priority) + " (" + selected(state).priorityFormula + ")", 270.f, 783.f, 11, theme::muted);
    }
    sidebar(window, state);
    for (const auto& button : buttons(state)) {
        const auto fill = button.primary ? theme::accent : button.selected ? theme::selection : button.bounds.contains(state.mouse) ? theme::surface : theme::toolbar;
        box(window, button.bounds, fill, button.selected ? theme::accent : theme::border);
        text(window, button.label, button.bounds.position.x + 8.f, button.bounds.position.y + 5.f, 11,
             button.primary ? theme::surface : theme::ink, button.primary);
        if (button.action == Action::AlgorithmMenu) {
            line(window, {1234.f, 139.f}, {1238.f, 143.f}, theme::ink, 1.f);
            line(window, {1238.f, 143.f}, {1242.f, 139.f}, theme::ink, 1.f);
        }
    }
    if (state.algorithmMenuOpen) {
        const auto count = std::min<std::size_t>(6, state.registry.algorithms().size() - state.algorithmMenuOffset);
        for (std::size_t row = 0; row < count; ++row) {
            const auto index = state.algorithmMenuOffset + row;
            const auto bounds = algorithmOptionBounds(row);
            const bool focused = index == state.algorithmMenuCursor || bounds.contains(state.mouse);
            box(window, bounds, focused ? theme::selection : theme::surface, theme::border);
            text(window, shorten(state.registry.algorithms()[index].name, 33), bounds.position.x + 8.f, bounds.position.y + 6.f, 12, theme::ink, index == state.algorithmIndex);
        }
        if (state.registry.algorithms().size() > 6)
            text(window, "Scroll or use arrow keys", 1016.f, 163.f + 32.f * static_cast<float>(count), 10, theme::muted);
    }
    line(window, {0.f, 800.f}, {1280.f, 800.f}, theme::border, 1.f);
    text(window, shorten(state.message, 180), 16.f, 803.f, 11, theme::muted);
    flush(window);
}
} // namespace pathx
