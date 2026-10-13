#include "Application.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace pathx {
namespace {
const sf::Color background{12, 18, 28};
const sf::Color panel{20, 29, 43};
const sf::Color border{42, 56, 75};
const sf::Color ink{233, 240, 249};
const sf::Color muted{137, 155, 180};
const sf::Color accent{250, 179, 83};
const sf::Color green{57, 211, 158};
const sf::Color blue{73, 161, 218};
const sf::Color red{238, 113, 121};
constexpr sf::Vector2f gridOrigin{54.f, 198.f};

float cellSize(const Grid& grid) {
    return std::min(800.f / static_cast<float>(grid.width()), 500.f / static_cast<float>(grid.height()));
}

sf::Color terrainColor(Terrain terrain) {
    switch (terrain) {
    case Terrain::Road: return {110, 100, 69};
    case Terrain::Plain: return {31, 42, 56};
    case Terrain::Swamp: return {64, 69, 101};
    case Terrain::Wall: return {98, 113, 133};
    }
    return sf::Color::Black;
}

std::string number(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(value < 100 ? 1 : 0) << value;
    return stream.str();
}

std::string finite(int value) { return value >= infinity ? "--" : std::to_string(value); }

bool successful(EvaluationKind kind) {
    return kind == EvaluationKind::Discovered || kind == EvaluationKind::Improved;
}
} // namespace

Renderer::Renderer(const std::filesystem::path& resources)
    : font_(resources / "assets/fonts/NotoSans.ttf") {
    geometry_.reserve(24000);
}

sf::FloatRect Renderer::toolBounds(int tool) { return {{24.f + 144.f * static_cast<float>(tool), 102.f}, {136.f, 44.f}}; }
sf::FloatRect Renderer::speedBounds() { return {{936.f, 318.f}, {296.f, 20.f}}; }

std::optional<Point> Renderer::cellAt(sf::Vector2f position, const Grid& grid) {
    const float size = cellSize(grid);
    const sf::Vector2f local = position - gridOrigin;
    if (local.x < 0.f || local.y < 0.f) return std::nullopt;
    const Point point{static_cast<int>(local.x / size), static_cast<int>(local.y / size)};
    return grid.contains(point) ? std::optional(point) : std::nullopt;
}

std::vector<ButtonSpec> Renderer::buttons(const AppState& state) {
    return {
        {{{936.f, 150.f}, {144.f, 32.f}}, "Dijkstra", Action::Dijkstra, state.algorithm == Algorithm::Dijkstra},
        {{{1092.f, 150.f}, {140.f, 32.f}}, "A*", Action::AStar, state.algorithm == Algorithm::AStar},
        {{{936.f, 194.f}, {144.f, 38.f}}, state.running ? "Pause  [Space]" : "Run  [Space]", Action::RunPause, false, true},
        {{{1092.f, 194.f}, {140.f, 38.f}}, "One step  [N]", Action::Step},
        {{{936.f, 244.f}, {144.f, 32.f}}, "Reset search  [R]", Action::Reset},
        {{{1092.f, 244.f}, {140.f, 32.f}}, "Empty map  [C]", Action::Clear},
        {{{936.f, 548.f}, {296.f, 36.f}}, "Compare algorithms  [B]", Action::Compare},
        {{{40.f, 750.f}, {32.f, 32.f}}, "<", Action::PreviousSample},
        {{{286.f, 750.f}, {32.f, 32.f}}, ">", Action::NextSample},
        {{{330.f, 750.f}, {132.f, 32.f}}, "Load example", Action::LoadSample},
        {{{490.f, 750.f}, {170.f, 32.f}}, "Save map  [Ctrl+S]", Action::Save},
        {{{672.f, 750.f}, {194.f, 32.f}}, "Load map  [Ctrl+O]", Action::Load}
    };
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
    auto& label = found->second;
    if (label.getFillColor() != color) label.setFillColor(color);
    target.draw(label);
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

void Renderer::box(sf::RenderTarget&, sf::FloatRect bounds, sf::Color fill,
                   sf::Color outline, float thickness) const {
    const auto rectangle = [&](sf::Vector2f position, sf::Vector2f size, sf::Color color) {
        if (color.a == 0 || size.x <= 0.f || size.y <= 0.f) return;
        quad(position, position + sf::Vector2f(size.x, 0.f), position + size,
             position + sf::Vector2f(0.f, size.y), color);
    };
    rectangle(bounds.position, bounds.size, fill);
    if (outline.a == 0 || thickness <= 0.f) return;
    const auto p = bounds.position;
    const auto s = bounds.size;
    rectangle(p - sf::Vector2f(thickness, thickness), {s.x + 2.f * thickness, thickness}, outline);
    rectangle(p + sf::Vector2f(-thickness, s.y), {s.x + 2.f * thickness, thickness}, outline);
    rectangle(p + sf::Vector2f(-thickness, 0.f), {thickness, s.y}, outline);
    rectangle(p + sf::Vector2f(s.x, 0.f), {thickness, s.y}, outline);
}

void Renderer::line(sf::RenderTarget&, sf::Vector2f from, sf::Vector2f to,
                    sf::Color color, float thickness) const {
    const auto delta = to - from;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length < 0.01f) return;
    const sf::Vector2f normal{-delta.y / length * thickness / 2.f, delta.x / length * thickness / 2.f};
    quad(from - normal, to - normal, to + normal, from + normal, color);
}

void Renderer::grid(sf::RenderTarget& target, const AppState& state) const {
    const auto& map = state.map;
    const float size = cellSize(map.grid);
    const auto position = [size](Point point) {
        return gridOrigin + sf::Vector2f(static_cast<float>(point.x) * size, static_cast<float>(point.y) * size);
    };
    for (int y = 0; y < map.grid.height(); ++y) {
        for (int x = 0; x < map.grid.width(); ++x) {
            const Point point{x, y};
            const auto terrain = map.grid.at(point);
            auto color = terrainColor(terrain);
            if (state.search) {
                const auto& node = state.search->nodes().at(static_cast<std::size_t>(map.grid.index(point)));
                if (node.state == NodeState::Closed) color = {35, 65, 97};
                else if (node.state == NodeState::Open) color = {45, 111, 147};
            }
            const float gap = size >= 5.f ? 1.f : 0.f;
            box(target, {position(point), {size - gap, size - gap}}, color);
            // Terrain remains identifiable beneath search colors.
            if (terrain == Terrain::Road && size >= 8.f)
                box(target, {position(point) + sf::Vector2f(size * .35f, size * .75f), {size * .3f, 2.f}}, accent);
            if (terrain == Terrain::Swamp && size >= 8.f) {
                line(target, position(point) + sf::Vector2f(3.f, size * .7f),
                     position(point) + sf::Vector2f(size - 4.f, size * .7f), {156, 150, 202}, 1.f);
                line(target, position(point) + sf::Vector2f(5.f, size * .85f),
                     position(point) + sf::Vector2f(size - 2.f, size * .85f), {156, 150, 202}, 1.f);
            }
            if (terrain == Terrain::Wall && size >= 10.f) {
                line(target, position(point) + sf::Vector2f(4.f, 4.f), position(point) + sf::Vector2f(size - 5.f, size - 5.f), {125, 141, 162}, 1.f);
                line(target, position(point) + sf::Vector2f(size - 5.f, 4.f), position(point) + sf::Vector2f(4.f, size - 5.f), {125, 141, 162}, 1.f);
            }
        }
    }
    if (state.search) {
        const bool found = state.search->result().status == SearchStatus::Found;
        const auto path = found ? state.search->result().path : state.search->currentPath();
        const auto pathColor = found ? green : accent;
        const float inset = std::min(4.f, size * .2f);
        const float gap = size >= 5.f ? 1.f : 0.f;
        for (const auto point : path) {
            box(target, {position(point) + sf::Vector2f(inset, inset), {size - 2.f * inset - gap, size - 2.f * inset - gap}},
                found ? sf::Color(39, 130, 109) : sf::Color(116, 88, 46));
        }
        for (std::size_t index = 1; index < path.size(); ++index) {
            const sf::Vector2f center{size / 2.f, size / 2.f};
            line(target, position(path[index - 1]) + center, position(path[index]) + center, pathColor, std::min(3.f, size * .2f));
        }
        if (const auto current = state.search->lastStep().current) {
            const float insetCurrent = std::min(1.f, size * .1f);
            box(target, {position(*current) + sf::Vector2f(insetCurrent, insetCurrent), {size - 2.f * insetCurrent - gap, size - 2.f * insetCurrent - gap}}, accent, ink, std::min(1.f, size * .1f));
            if (state.search->result().status == SearchStatus::Searching) {
                for (const auto& evaluation : state.search->lastStep().evaluations) {
                    const auto mark = position(evaluation.point);
                    const bool accepted = successful(evaluation.kind);
                    // Persist while paused; fade on the next animated expansion.
                    const float opacity = state.running ? std::max(.35f, 1.f - state.stepAge) : 1.f;
                    auto color = accepted ? green : red;
                    color.a = static_cast<std::uint8_t>(opacity * 255.f);
                    const float markInset = std::min(2.f, size * .1f);
                    box(target, {mark + sf::Vector2f(markInset, markInset), {size - 2.f * markInset - gap, size - 2.f * markInset - gap}}, sf::Color::Transparent, color, std::min(1.5f, size * .1f));
                    if (!accepted && size >= 10.f) {
                        line(target, mark + sf::Vector2f(6.f, 6.f), mark + sf::Vector2f(size - 7.f, size - 7.f), color);
                        line(target, mark + sf::Vector2f(size - 7.f, 6.f), mark + sf::Vector2f(6.f, size - 7.f), color);
                    }
                }
            }
        }
    }
    const auto endpoint = [&](Point point, sf::Color color, const std::string& label) {
        const auto pos = position(point);
        const float inset = std::min(1.f, size * .1f);
        const float gap = size >= 5.f ? 1.f : 0.f;
        box(target, {pos + sf::Vector2f(inset, inset), {size - 2.f * inset - gap, size - 2.f * inset - gap}}, color, ink, std::min(1.f, size * .1f));
        if (size >= 12.f) {
            const bool shared = label == "S/G";
            text(target, label, pos.x + (shared ? 2.f : size * .2f), pos.y + (shared ? size * .2f : -1.f),
                 static_cast<unsigned int>(std::min(size * (shared ? .42f : .8f), shared ? 10.f : 16.f)), background, true);
        }
    };
    if (map.start == map.goal) endpoint(map.start, green, "S/G");
    else {
        endpoint(map.start, green, "S");
        endpoint(map.goal, {203, 155, 239}, "G");
    }
    if (state.hover && map.grid.contains(*state.hover)) {
        const float gap = size >= 5.f ? 1.f : 0.f;
        box(target, {position(*state.hover), {size - gap, size - gap}}, sf::Color::Transparent, ink, std::min(1.5f, size * .1f));
    }
    const int labelStep = std::max(5, static_cast<int>(std::ceil(50.f / size / 5.f)) * 5);
    for (int x = 0; x < map.grid.width(); x += labelStep)
        text(target, std::to_string(x), gridOrigin.x + static_cast<float>(x) * size + 3.f, 180.f, 10, muted);
    for (int y = 0; y < map.grid.height(); y += labelStep)
        text(target, std::to_string(y), 30.f, gridOrigin.y + static_cast<float>(y) * size + 1.f, 10, muted);
}

void Renderer::sidebar(sf::RenderTarget& target, const AppState& state) const {
    text(target, "EXPERIMENT", 936.f, 115.f, 12, muted, true);
    text(target, "Algorithm", 1160.f, 116.f, 12, muted);
    text(target, "Animation speed", 936.f, 289.f, 12, muted);
    text(target, number(state.stepsPerSecond) + " cells / s", 1126.f, 289.f, 12, ink);
    const auto speed = speedBounds();
    const float ratio = std::log(state.stepsPerSecond) / std::log(60.f);
    box(target, {{speed.position.x, 326.f}, {speed.size.x, 4.f}}, border);
    box(target, {{speed.position.x, 326.f}, {speed.size.x * ratio, 4.f}}, accent);
    sf::CircleShape knob(6.f);
    knob.setPosition({speed.position.x + speed.size.x * ratio - 6.f, 322.f});
    knob.setFillColor(accent);
    flush(target);
    target.draw(knob);
    line(target, {936.f, 354.f}, {1232.f, 354.f}, border, 1.f);
    text(target, "LIVE RESULTS", 936.f, 367.f, 12, muted, true);

    std::string status = "Ready to explore";
    auto statusColor = ink;
    if (state.search) {
        switch (state.search->result().status) {
        case SearchStatus::Searching: status = state.running ? "Searching..." : "Paused"; statusColor = accent; break;
        case SearchStatus::Found: status = "Optimal route found"; statusColor = green; break;
        case SearchStatus::NoPath: status = "No route to the goal"; statusColor = red; break;
        case SearchStatus::InvalidMap: status = "Invalid start or goal"; statusColor = red; break;
        }
    }
    text(target, status, 936.f, 388.f, 18, statusColor, true);
    const bool found = state.search && state.search->result().status == SearchStatus::Found;
    const auto* result = state.search ? &state.search->result() : nullptr;
    text(target, "PATH COST", 936.f, 424.f, 10, muted, true);
    text(target, "STEPS", 1092.f, 424.f, 10, muted, true);
    text(target, found ? std::to_string(result->cost) : "--", 936.f, 438.f, 26, green, true);
    text(target, found ? std::to_string(result->steps()) : "--", 1092.f, 438.f, 26, ink, true);
    text(target, "EXPANDED CELLS", 936.f, 479.f, 10, muted, true);
    text(target, "COMPUTE MEDIAN", 1092.f, 479.f, 10, muted, true);
    text(target, result ? std::to_string(result->expanded) : "0", 936.f, 495.f, 22, blue, true);
    const auto& benchmark = state.comparison[state.algorithm == Algorithm::Dijkstra ? 0 : 1];
    text(target, benchmark ? number(benchmark->medianMicroseconds) + " us" : "--", 1092.f, 495.f, 20, ink, true);
    text(target, "SAME MAP / 31 RUNS / NO ANIMATION", 936.f, 594.f, 10, muted, true);
    text(target, "Algorithm", 936.f, 617.f, 11, muted);
    text(target, "Cost", 1034.f, 617.f, 11, muted);
    text(target, "Cells", 1082.f, 617.f, 11, muted);
    text(target, "Median us", 1154.f, 617.f, 11, muted);
    for (std::size_t index = 0; index < state.comparison.size(); ++index) {
        const float y = 638.f + static_cast<float>(index) * 25.f;
        text(target, index == 0 ? "Dijkstra" : "A*", 936.f, y, 12, ink, true);
        const auto& run = state.comparison[index];
        text(target, run ? (run->result.status == SearchStatus::Found ? std::to_string(run->result.cost) : "none") : "--", 1034.f, y, 12, green);
        text(target, run ? std::to_string(run->result.expanded) : "--", 1082.f, y, 12, blue);
        text(target, run ? number(run->medianMicroseconds) : "--", 1154.f, y, 12, ink);
    }
#ifdef NDEBUG
    text(target, "Release build  |  Includes search setup + route", 936.f, 700.f, 10, muted);
#else
    text(target, "Debug build: use Release to compare timings", 936.f, 700.f, 10, accent);
#endif
}

void Renderer::draw(sf::RenderWindow& window, const AppState& state) const {
    geometry_.clear();
    if (labels_.size() > 1024) labels_.clear();
    // Header mark: an actual route motif.
    line(window, {26.f, 36.f}, {45.f, 36.f}, green, 3.f);
    line(window, {45.f, 36.f}, {45.f, 55.f}, green, 3.f);
    line(window, {45.f, 55.f}, {64.f, 55.f}, accent, 3.f);
    box(window, {{24.f, 32.f}, {7.f, 7.f}}, green);
    box(window, {{61.f, 51.f}, {7.f, 7.f}}, accent);
    text(window, "PathExperiment", 82.f, 19.f, 29, ink, true);
    text(window, "A small lab for better routes.", 84.f, 57.f, 12, muted);
    text(window, "C++20  /  SFML  /  v0.1.0", 1070.f, 28.f, 12, muted);
    text(window, "4-way movement  |  Weighted terrain", 1014.f, 52.f, 12, muted);

    box(window, {{24.f, 164.f}, {864.f, 560.f}}, panel, border);
    box(window, {{912.f, 102.f}, {344.f, 622.f}}, panel, border);
    box(window, {{24.f, 738.f}, {864.f, 58.f}}, panel, border);
    box(window, {{912.f, 738.f}, {344.f, 58.f}}, panel, border);
    text(window, state.mapName, 40.f, 166.f, 12, ink, true);
    text(window, std::to_string(state.map.grid.width()) + " x " + std::to_string(state.map.grid.height()) + " cells", 770.f, 166.f, 11, muted);

    const std::array<std::string, 6> names{"Road", "Plain", "Swamp", "Wall", "Start S", "Goal G"};
    const std::array<std::string, 6> costs{"cost 1", "cost 2", "cost 5", "blocked", "place", "place"};
    const std::array colors{terrainColor(Terrain::Road), terrainColor(Terrain::Plain), terrainColor(Terrain::Swamp), terrainColor(Terrain::Wall), green, sf::Color(203, 155, 239)};
    for (int index = 0; index < 6; ++index) {
        const auto bounds = toolBounds(index);
        const auto tool = static_cast<std::size_t>(index);
        const bool selected = state.tool == static_cast<Tool>(index);
        const bool hover = bounds.contains(state.mouse);
        box(window, bounds, selected ? sf::Color(55, 48, 37) : hover ? sf::Color(33, 45, 62) : panel, selected ? accent : border);
        box(window, {bounds.position + sf::Vector2f(10.f, 13.f), {15.f, 15.f}}, colors[tool], border);
        text(window, names[tool], bounds.position.x + 34.f, bounds.position.y + 4.f, 12, selected ? accent : ink, true);
        text(window, costs[tool] + "  [" + std::to_string(index + 1) + "]", bounds.position.x + 34.f, bounds.position.y + 23.f, 10, muted);
    }

    grid(window, state);
    const std::array<std::pair<std::string, sf::Color>, 6> legend{{{"Frontier", blue}, {"Explored", {35, 65, 97}}, {"Current", accent}, {"Candidate", {116, 88, 46}}, {"Route", green}, {"Rejected", red}}};
    float legendX = 54.f;
    for (const auto& [name, color] : legend) {
        box(window, {{legendX, 708.f}, {9.f, 9.f}}, color);
        text(window, name, legendX + 15.f, 703.f, 11, muted);
        legendX += 128.f;
    }
    sidebar(window, state);

    for (const auto& button : buttons(state)) {
        const bool hover = button.bounds.contains(state.mouse);
        const auto fill = button.primary ? accent : button.selected ? sf::Color(44, 58, 73) : hover ? sf::Color(38, 53, 72) : sf::Color(26, 38, 54);
        box(window, button.bounds, fill, button.selected ? accent : border);
        text(window, button.label, button.bounds.position.x + 10.f, button.bounds.position.y + (button.bounds.size.y - 17.f) / 2.f - 2.f,
             12, button.primary ? background : button.selected ? accent : ink, true);
    }
    text(window, sampleNames()[static_cast<std::size_t>(state.sampleIndex)], 84.f, 757.f, 12, ink);

    if (state.hover) {
        const auto point = *state.hover;
        const auto terrain = state.map.grid.at(point);
        text(window, "CELL " + std::to_string(point.x) + ", " + std::to_string(point.y) + "  |  entry " + (passable(terrain) ? std::to_string(entryCost(terrain)) : "blocked"), 926.f, 745.f, 11, ink, true);
        if (state.search) {
            const auto& node = state.search->nodes().at(static_cast<std::size_t>(state.map.grid.index(point)));
            const auto heuristic = state.algorithm == Algorithm::AStar ? std::abs(point.x - state.map.goal.x) + std::abs(point.y - state.map.goal.y) : 0;
            text(window, "g " + finite(node.g) + "    h " + std::to_string(heuristic) + "    f " + finite(node.g >= infinity ? infinity : node.g + heuristic), 926.f, 765.f, 11, muted);
            if (node.parent) {
                const auto parent = state.map.grid.point(*node.parent);
                text(window, "parent " + std::to_string(parent.x) + "," + std::to_string(parent.y), 1120.f, 765.f, 10, muted);
            }
        } else text(window, "Hover shows g / h / f during a search.", 926.f, 767.f, 10, muted);
    } else if (state.search && state.search->lastStep().current) {
        const auto point = *state.search->lastStep().current;
        const auto& node = state.search->nodes().at(static_cast<std::size_t>(state.map.grid.index(point)));
        text(window, "CURRENT  (" + std::to_string(point.x) + ", " + std::to_string(point.y) + ")", 926.f, 745.f, 11, accent, true);
        text(window, state.algorithm == Algorithm::AStar ? "Chosen by lowest f = " + finite(node.g + node.h) : "Chosen by lowest g = " + finite(node.g), 926.f, 767.f, 11, muted);
    } else {
        text(window, "LEFT: paint   RIGHT: erase", 926.f, 745.f, 11, ink, true);
        text(window, "Hover a cell to inspect its search data.", 926.f, 767.f, 10, muted);
    }

    // Clip long filenames/messages to a single footer line.
    const std::string message = state.message.size() > 178 ? state.message.substr(0, 175) + "..." : state.message;
    text(window, message, 24.f, 802.f, 11, muted);
    flush(window);
}

} // namespace pathx
