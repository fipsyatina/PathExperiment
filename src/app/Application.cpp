#include "Application.hpp"
#include "Theme.hpp"
#include <SFML/OpenGL.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

namespace pathx {
namespace {
constexpr sf::Vector2f logicalSize{1280.f, 820.f};
constexpr float maximumVisualSpeed = 60.f;

std::filesystem::path createCaptureDirectory(const std::filesystem::path& requested) {
    auto output = requested;
    if (std::filesystem::exists(output)) {
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        output += "-" + std::to_string(stamp);
        for (int suffix = 1; std::filesystem::exists(output); ++suffix)
            output = requested.string() + "-" + std::to_string(stamp) + "-" + std::to_string(suffix);
    }
    std::filesystem::create_directories(output);
    return output;
}

std::filesystem::path userSaveDirectory(const std::filesystem::path& resources) {
#ifdef _WIN32
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (length > 0) {
        std::wstring value(length, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), length);
        if (written > 0 && written < length) {
            value.resize(written);
            return std::filesystem::path(value) / "PathExperiment" / "maps";
        }
    }
#else
    if (const auto* value = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(value) / "PathExperiment" / "maps";
    }
    if (const auto* value = std::getenv("HOME")) {
        return std::filesystem::path(value) / ".local/share/PathExperiment/maps";
    }
#endif
    return resources / "saved-maps";
}

std::filesystem::path uniqueMapPath(const std::filesystem::path& directory) {
    const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto path = directory / ("map-" + std::to_string(stamp) + ".pemap");
    for (int suffix = 1; std::filesystem::exists(path); ++suffix) {
        path = directory / ("map-" + std::to_string(stamp) + "-" + std::to_string(suffix) + ".pemap");
    }
    return path;
}
} // namespace

const std::array<std::string, 6>& sampleFiles() {
    static const std::array<std::string, 6> files{
        "open.pemap", "maze.pemap", "weighted-detour.pemap", "blocked-goal.pemap", "narrow-passage.pemap", "large-weighted.pemap"};
    return files;
}
const std::array<std::string, 6>& sampleNames() {
    static const std::array<std::string, 6> names{
        "Open field", "Maze", "The cheaper detour", "Blocked goal", "Narrow passage", "Weighted field"};
    return names;
}

std::filesystem::path findResources(const std::filesystem::path& executable) {
    const std::array candidates{executable.parent_path(), std::filesystem::current_path()};
    for (const auto& directory : candidates) {
        if (std::filesystem::exists(directory / "assets/fonts/NotoSans.ttf") &&
            std::filesystem::exists(directory / "maps/open.pemap")) {
            return directory;
        }
    }
    throw std::runtime_error("Resources missing. Keep assets/ and maps/ beside PathExperiment.");
}

bool MapEditor::paint(Map& map, Point point, Tool tool, bool erase) {
    if (!map.grid.contains(point)) return false;
    if (erase) tool = Tool::Plain;
    if (tool == Tool::Start || tool == Tool::Goal) {
        auto& endpoint = tool == Tool::Start ? map.start : map.goal;
        const bool blocked = !passable(map.grid.at(point));
        if (endpoint == point && !blocked) return false;
        if (blocked) map.grid.set(point, Terrain::Plain);
        endpoint = point;
        return true;
    }
    const std::array terrains{Terrain::Road, Terrain::Plain, Terrain::Swamp, Terrain::Wall};
    const auto terrain = terrains.at(static_cast<std::size_t>(tool));
    if (terrain == Terrain::Wall && (point == map.start || point == map.goal)) return false;
    if (map.grid.at(point) == terrain) return false;
    map.grid.set(point, terrain);
    return true;
}

Application::Application(std::filesystem::path resources)
    : renderer_(resources),
      window_(sf::VideoMode({1280u, 820u}), "PathExperiment") {
    state_.resources = std::move(resources);
    state_.saveDirectory = userSaveDirectory(state_.resources);
    state_.comparison.resize(state_.registry.algorithms().size());
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    window_.setView(sf::View(sf::FloatRect({0.f, 0.f}, logicalSize)));
}

int Application::run() {
    sf::Clock clock;
    while (window_.isOpen()) {
        while (const auto event = window_.pollEvent()) handleEvent(*event);
        if (!window_.isOpen()) break;
        update(std::min(clock.restart().asSeconds(), 0.1f));
        render();
    }
    return 0;
}

void Application::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) window_.close();
    else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        // Letterbox a fixed logical canvas; hit tests use the same view transformation.
        const float width = static_cast<float>(resized->size.x);
        const float height = static_cast<float>(resized->size.y);
        if (width <= 0.f || height <= 0.f) return;
        const float scale = std::min(width / logicalSize.x, height / logicalSize.y);
        sf::View view(sf::FloatRect({0.f, 0.f}, logicalSize));
        const sf::Vector2f ratio{logicalSize.x * scale / width, logicalSize.y * scale / height};
        view.setViewport({{(1.f - ratio.x) / 2.f, (1.f - ratio.y) / 2.f}, ratio});
        window_.setView(view);
    } else if (event.is<sf::Event::FocusLost>()) {
        state_.dragging = false;
        state_.speedDragging = false;
    } else if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        pointerMove(window_.mapPixelToCoords(moved->position));
    } else if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>(); wheel && state_.algorithmMenuOpen) {
        const auto count = state_.registry.algorithms().size();
        if (wheel->delta < 0 && state_.algorithmMenuOffset + 6 < count) ++state_.algorithmMenuOffset;
        else if (wheel->delta > 0 && state_.algorithmMenuOffset > 0) --state_.algorithmMenuOffset;
    } else if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (pressed->button == sf::Mouse::Button::Left || pressed->button == sf::Mouse::Button::Right) {
            const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
            const bool control = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
            const std::optional<Tool> endpoint = shift ? std::optional(Tool::Start) : control ? std::optional(Tool::Goal) : std::nullopt;
            pointerDown(window_.mapPixelToCoords(pressed->position), pressed->button == sf::Mouse::Button::Right, endpoint);
        }
    } else if (event.is<sf::Event::MouseButtonReleased>()) {
        state_.dragging = false;
        state_.speedDragging = false;
    } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using Key = sf::Keyboard::Key;
        if (state_.algorithmMenuOpen) {
            const auto count = state_.registry.algorithms().size();
            if (key->code == Key::Escape) { state_.algorithmMenuOpen = false; return; }
            if (key->code == Key::Enter) { selectAlgorithm(state_.algorithmMenuCursor); return; }
            if (key->code == Key::Down || key->code == Key::Up) {
                state_.algorithmMenuCursor = (state_.algorithmMenuCursor + (key->code == Key::Down ? 1 : count - 1)) % count;
                if (state_.algorithmMenuCursor < state_.algorithmMenuOffset) state_.algorithmMenuOffset = state_.algorithmMenuCursor;
                if (state_.algorithmMenuCursor >= state_.algorithmMenuOffset + 6) state_.algorithmMenuOffset = state_.algorithmMenuCursor - 5;
                return;
            }
        }
        if (key->control && key->code == Key::S) action(Action::Save);
        else if (key->control && key->code == Key::O) action(Action::Load);
        else if (key->code == Key::Space) action(Action::RunPause);
        else if (key->code == Key::N) action(Action::Step);
        else if (key->code == Key::R) action(Action::Reset);
        else if (key->code == Key::C) action(Action::Clear);
        else if (key->code == Key::B) action(Action::Compare);
        else if (key->code == Key::Tab) selectAlgorithm((state_.algorithmIndex + 1) % state_.registry.algorithms().size());
        else {
            const std::array keys{Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5, Key::Num6};
            for (std::size_t index = 0; index < keys.size(); ++index)
                if (key->code == keys[index]) state_.tool = static_cast<Tool>(index);
        }
    }
}

void Application::pointerDown(sf::Vector2f position, bool erase, std::optional<Tool> endpoint) {
    state_.mouse = position;
    if (state_.algorithmMenuOpen) {
        const auto visible = std::min<std::size_t>(6, state_.registry.algorithms().size() - state_.algorithmMenuOffset);
        for (std::size_t row = 0; row < visible; ++row) {
            if (Renderer::algorithmOptionBounds(row).contains(position)) {
                selectAlgorithm(state_.algorithmMenuOffset + row);
                return;
            }
        }
        state_.algorithmMenuOpen = false;
        return;
    }
    if (!erase) {
        for (int tool = 0; tool < 6; ++tool) {
            if (Renderer::toolBounds(tool).contains(position)) {
                state_.tool = static_cast<Tool>(tool);
                return;
            }
        }
        for (const auto& button : Renderer::buttons(state_)) {
            if (button.bounds.contains(position)) { action(button.action); return; }
        }
        if (Renderer::speedBounds().contains(position)) {
            state_.speedDragging = true;
            setSpeed(position.x);
            return;
        }
    }
    state_.hover = Renderer::cellAt(position, state_.map.grid);
    if (!erase && endpoint && state_.hover) {
        const auto tool = state_.tool;
        state_.tool = *endpoint;
        state_.erasing = false;
        paint();
        state_.tool = tool;
        state_.dragging = false;
        return;
    }
    state_.dragging = state_.hover.has_value();
    state_.erasing = erase;
    if (state_.dragging) paint();
}

void Application::pointerMove(sf::Vector2f position) {
    const auto previous = state_.hover;
    state_.mouse = position;
    state_.hover = Renderer::cellAt(position, state_.map.grid);
    if (state_.algorithmMenuOpen) {
        const auto visible = std::min<std::size_t>(6, state_.registry.algorithms().size() - state_.algorithmMenuOffset);
        for (std::size_t row = 0; row < visible; ++row)
            if (Renderer::algorithmOptionBounds(row).contains(position)) state_.algorithmMenuCursor = state_.algorithmMenuOffset + row;
        return;
    }
    if (state_.speedDragging) { setSpeed(position.x); return; }
    if (!state_.dragging || !state_.hover) return;
    if (previous && state_.tool != Tool::Start && state_.tool != Tool::Goal) {
        const Point target = *state_.hover;
        Point point = *previous;
        const int dx = std::abs(target.x - point.x), dy = -std::abs(target.y - point.y);
        const int sx = point.x < target.x ? 1 : -1, sy = point.y < target.y ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            state_.hover = point;
            paint();
            if (point == target) break;
            const int doubled = 2 * error;
            if (doubled >= dy) { error += dy; point.x += sx; }
            if (doubled <= dx) { error += dx; point.y += sy; }
        }
        state_.hover = target;
    } else paint();
}

void Application::paint() {
    if (state_.hover && MapEditor::paint(state_.map, *state_.hover, state_.tool, state_.erasing)) {
        resetSearch();
        state_.comparison.assign(state_.registry.algorithms().size(), std::nullopt);
        state_.mapName = "Edited map";
        state_.message = "Map changed. The next search uses the new terrain.";
    }
}

void Application::setSpeed(float x) {
    const auto bounds = Renderer::speedBounds();
    const float ratio = std::clamp((x - bounds.position.x) / bounds.size.x, 0.f, 1.f);
    // Logarithmic slider: readable single steps through fast exploration.
    state_.stepsPerSecond = std::pow(maximumVisualSpeed, ratio);
}

void Application::resetSearch() {
    state_.search.reset();
    state_.running = false;
    state_.stepAge = 1.f;
    accumulator_ = 0.f;
}

void Application::selectAlgorithm(std::size_t index) {
    if (index >= state_.registry.algorithms().size()) return;
    if (index != state_.algorithmIndex) resetSearch();
    state_.algorithmIndex = index;
    state_.algorithmMenuCursor = index;
    state_.algorithmMenuOpen = false;
    const auto& descriptor = state_.registry.algorithms()[index];
    state_.message = descriptor.name + ": priority " + descriptor.priorityFormula + ". " + descriptor.description;
}

void Application::startSearch() {
    auto session = state_.registry.algorithms().at(state_.algorithmIndex).create(state_.map, true);
    if (!session) throw std::runtime_error("Algorithm factory returned no session.");
    state_.search = std::move(session);
    accumulator_ = 0.f;
}

void Application::action(Action requested) {
    try {
        if (requested != Action::AlgorithmMenu) state_.algorithmMenuOpen = false;
        state_.dragging = false;
        state_.speedDragging = false;
        switch (requested) {
        case Action::AlgorithmMenu: {
            state_.algorithmMenuOpen = !state_.algorithmMenuOpen;
            state_.algorithmMenuCursor = state_.algorithmIndex;
            state_.algorithmMenuOffset = state_.algorithmIndex >= 6 ? state_.algorithmIndex - 5 : 0;
            break;
        }
        case Action::RunPause:
            if (!state_.search || state_.search->result().status != SearchStatus::Searching) {
                startSearch();
                state_.running = state_.search->result().status == SearchStatus::Searching;
            } else state_.running = !state_.running;
            state_.message = state_.running ? "Searching... blue branches are explored; orange is the current candidate." : "Paused. Press N to expand exactly one cell.";
            break;
        case Action::Step:
            state_.running = false;
            if (!state_.search || state_.search->result().status != SearchStatus::Searching)
                startSearch();
            advance();
            break;
        case Action::Reset:
            resetSearch();
            state_.message = "Search cleared. Your map is ready for another experiment.";
            break;
        case Action::Finish:
            state_.running = false;
            if (!state_.search || state_.search->result().status != SearchStatus::Searching) startSearch();
            while (state_.search->result().status == SearchStatus::Searching) advance();
            break;
        case Action::Clear:
            resetSearch();
            state_.comparison.assign(state_.registry.algorithms().size(), std::nullopt);
            state_.map = Map{};
            state_.mapName = "Untitled map";
            state_.hover.reset();
            state_.message = "New 120 x 80 map. Draw terrain; Shift-click to place start, Ctrl-click to place goal.";
            break;
        case Action::Compare:
            state_.running = false;
            for (std::size_t index = 0; index < state_.registry.algorithms().size(); ++index)
                state_.comparison[index] = benchmark(state_.map, state_.registry.algorithms()[index]);
            state_.message = "Compared on the same map: median of 31 runs, with no rendering or delays.";
            break;
        case Action::PreviousSample: state_.sampleIndex = (state_.sampleIndex + 5) % 6; break;
        case Action::NextSample: state_.sampleIndex = (state_.sampleIndex + 1) % 6; break;
        case Action::LoadSample: loadSample(state_.sampleIndex); break;
        case Action::Save: save(); break;
        case Action::Load: load(); break;
        }
    } catch (const std::exception& error) {
        state_.message = std::string("Could not complete action: ") + error.what();
    }
}

void Application::advance() {
    if (!state_.search || state_.search->result().status != SearchStatus::Searching) return;
    state_.search->step();
    state_.stepAge = 0.f;
    const auto& result = state_.search->result();
    if (result.status == SearchStatus::Found) {
        state_.running = false;
        state_.message = state_.registry.algorithms()[state_.algorithmIndex].guaranteesLowestCost
            ? "Minimum-cost route found. Green: route; blue: explored branches."
            : "Route found. This algorithm does not guarantee minimum weighted cost.";
    } else if (result.status == SearchStatus::NoPath) {
        state_.running = false;
        state_.message = "No route. Every reachable cell was explored; the goal cannot be reached.";
    }
}

void Application::update(float seconds) {
    state_.stepAge += seconds;
    if (!state_.running) return;
    accumulator_ += seconds;
    const float interval = 1.f / state_.stepsPerSecond;
    // Every expansion is rendered. Slow frames reduce visual speed rather than
    // skipping accepted/rejected neighbor evaluations. Benchmarking is separate.
    if (accumulator_ >= interval && state_.running) {
        accumulator_ = std::min(accumulator_ - interval, interval);
        advance();
    }
}

void Application::loadSample(int index) {
    auto map = loadMap(state_.resources / "maps" / sampleFiles().at(static_cast<std::size_t>(index)));
    resetSearch();
    state_.comparison.assign(state_.registry.algorithms().size(), std::nullopt);
    state_.map = std::move(map);
    state_.hover.reset();
    state_.sampleIndex = index;
    state_.mapName = sampleNames().at(static_cast<std::size_t>(index));
    state_.message = "Example loaded. Select an algorithm and run, pause or step through its decisions.";
}

void Application::save() {
    std::filesystem::create_directories(state_.saveDirectory);
    const auto path = uniqueMapPath(state_.saveDirectory);
    saveMap(path, state_.map);
    state_.message = "Saved: " + path.string();
}

void Application::load() {
    std::filesystem::path chosen;
#ifdef _WIN32
    std::filesystem::create_directories(state_.saveDirectory);
    std::array<wchar_t, 32768> filename{};
    const auto directory = state_.saveDirectory.wstring();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_.getNativeHandle();
    dialog.lpstrFilter = L"PathExperiment maps (*.pemap)\0*.pemap\0All files\0*.*\0";
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.lpstrInitialDir = directory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) {
        if (CommDlgExtendedError() != 0) throw std::runtime_error("The file picker could not open.");
        return;
    }
    chosen = filename.data();
#else
    if (std::filesystem::exists(state_.saveDirectory)) {
        for (const auto& item : std::filesystem::directory_iterator(state_.saveDirectory)) {
            if (item.is_regular_file() && item.path().extension() == ".pemap" &&
                (chosen.empty() || item.last_write_time() > std::filesystem::last_write_time(chosen))) chosen = item.path();
        }
    }
    if (chosen.empty()) throw std::runtime_error("No saved maps yet. Save a map first.");
#endif
    auto map = loadMap(chosen); // Parse first; a bad file never discards the current experiment.
    resetSearch();
    state_.comparison.assign(state_.registry.algorithms().size(), std::nullopt);
    state_.map = std::move(map);
    state_.hover.reset();
    state_.mapName = chosen.stem().string();
    state_.message = "Loaded: " + chosen.string();
}

void Application::render() {
    window_.clear(theme::background);
    renderer_.draw(window_, state_);
    window_.display();
}

void Application::capture(const std::filesystem::path& path) {
    // Capture the back buffer before display swaps it. Reading after display
    // can return the previous frame on double-buffered Windows drivers.
    window_.clear(theme::background);
    renderer_.draw(window_, state_);
    // Direct framebuffer readback also works on software OpenGL where maximum
    // texture size can be 1024, smaller than the desktop window (CI runners).
    const auto size = window_.getSize();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.x) * size.y * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y), GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
    sf::Image pixels(size, bytes.data());
    pixels.flipVertically();
    if (!pixels.saveToFile(path)) throw std::runtime_error("Could not save capture.");
    window_.display();
}

void Application::expect(bool condition, const std::string& message) const {
    if (!condition) throw std::runtime_error("GUI smoke: " + message);
}

int Application::smokeTest(const std::filesystem::path& requestedOutput) {
    const auto output = createCaptureDirectory(requestedOutput);
    state_.saveDirectory = output / "saved-maps";
    capture(output / "empty.png");
    const auto pointPixel = [&](Point point) { return Renderer::cellCenter(point, state_.map.grid); };
    const auto release = [&] { state_.dragging = false; state_.speedDragging = false; };
    handleEvent(sf::Event::KeyPressed{.code = sf::Keyboard::Key::Num4});
    pointerDown(pointPixel({4, 2}), false);
    release();
    expect(state_.map.grid.at({4, 2}) == Terrain::Wall, "wall brush did not paint");
    pointerDown(pointPixel({4, 2}), true);
    release();
    expect(state_.map.grid.at({4, 2}) == Terrain::Plain, "right erase failed");
    state_.tool = Tool::Road;
    pointerDown(pointPixel({4, 2}), false);
    pointerMove(pointPixel({8, 2}));
    release();
    for (int x = 4; x <= 8; ++x) expect(state_.map.grid.at({x, 2}) == Terrain::Road, "drag brush left a gap");
    pointerDown(pointPixel({5, 3}), true);
    release();
    state_.map.grid.set({119, 79}, Terrain::Wall);
    pointerDown(pointPixel({119, 79}), false, Tool::Start);
    expect(state_.map.start == Point{119, 79} && state_.map.grid.at({119, 79}) == Terrain::Plain, "arbitrary start placement on a wall failed");
    pointerDown(pointPixel({0, 0}), false, Tool::Goal);
    expect(state_.map.goal == Point{0, 0}, "arbitrary goal placement failed");
    expect(state_.tool == Tool::Road, "endpoint shortcut changed the brush");
    capture(output / "endpoints.png");
    pointerDown(pointPixel({0, 0}), false, Tool::Start);
    action(Action::Step);
    expect(state_.search->result().status == SearchStatus::Found && state_.search->result().cost == 0, "shared endpoint failed in the UI");
    capture(output / "shared-endpoint.png");
    pointerDown({1100.f, 140.f}, false);
    expect(state_.algorithmMenuOpen, "algorithm selector did not open");
    capture(output / "algorithm-menu.png");
    pointerDown({1100.f, 240.f}, false);
    expect(state_.algorithmIndex == 2 && !state_.algorithmMenuOpen, "BFS selection failed");
    handleEvent(sf::Event::KeyPressed{.code = sf::Keyboard::Key::Tab});
    expect(state_.algorithmIndex == 3, "Tab did not select Greedy");
    action(Action::AlgorithmMenu);
    handleEvent(sf::Event::KeyPressed{.code = sf::Keyboard::Key::Up});
    handleEvent(sf::Event::KeyPressed{.code = sf::Keyboard::Key::Enter});
    expect(state_.algorithmIndex == 2, "keyboard menu selection failed");
    selectAlgorithm(1);
    pointerDown({1250.f, 386.f}, false);
    pointerMove({1256.f, 386.f});
    release();
    expect(state_.stepsPerSecond > 59.f, "speed slider failed");
    state_.stepsPerSecond = 24.f;
    action(Action::Clear);
    expect(state_.map.grid.size() == 9600, "New did not create 120 x 80");
    action(Action::Step);
    expect(state_.search && state_.search->result().expanded == 1, "single step must expand one cell");
    expect(!state_.running, "step must stay paused");
    const auto expanded = state_.search->result().expanded;
    update(.5f);
    expect(state_.search->result().expanded == expanded, "paused search advanced");
    capture(output / "single-step.png");
    action(Action::RunPause);
    update(1.f);
    expect(state_.search->result().expanded == expanded + 1, "visual mode skipped expansions after a stall");
    action(Action::RunPause);
    loadSample(2);
    pointerDown({1060.f, 279.f}, false);
    expect(state_.running, "Run button did not start");
    update(.1f);
    expect(state_.search->result().expanded > 0, "Run did not advance");
    pointerDown({1060.f, 279.f}, false);
    expect(!state_.running, "Pause button did not pause");
    for (int count = 0; count < 80 && state_.search->result().status == SearchStatus::Searching; ++count) advance();
    capture(output / "searching.png");
    pointerDown({1190.f, 319.f}, false);
    expect(state_.search->result().status == SearchStatus::Found, "Finish did not complete");
    action(Action::Compare);
    expect(state_.comparison.size() == state_.registry.algorithms().size(), "comparison omitted algorithms");
    for (const auto& measured : state_.comparison) expect(measured && measured->runs == 31, "algorithm benchmark missing");
    expect(state_.comparison[0]->result.cost == state_.comparison[1]->result.cost, "optimal algorithm costs differ");
    capture(output / "comparison.png");
    save();
    int savedCount{};
    for (const auto& item : std::filesystem::directory_iterator(state_.saveDirectory)) {
        if (item.path().extension() != ".pemap") continue;
        const auto restored = loadMap(item.path());
        expect(restored.start == state_.map.start && restored.goal == state_.map.goal, "saved endpoints differ");
        expect(solve(restored, state_.registry.get("astar")).cost == state_.search->result().cost, "saved terrain differs");
        ++savedCount;
    }
    expect(savedCount > 0, "save produced no map");
    state_.tool = Tool::Wall;
    pointerDown(pointPixel({12, 12}), false);
    release();
    expect(!state_.search && !state_.comparison[0] && !state_.running, "editing retained stale results");
    for (int index = 0; index < static_cast<int>(sampleFiles().size()); ++index) {
        loadSample(index);
        const auto first = solve(state_.map, state_.registry.get("dijkstra"));
        const auto second = solve(state_.map, state_.registry.get("astar"));
        expect(first.status == second.status && first.cost == second.cost, "sample optimal algorithms disagree");
        expect(first.status == (index == 3 ? SearchStatus::NoPath : SearchStatus::Found), "unexpected sample reachability");
    }
    loadSample(2);
    for (std::size_t index = 0; index < state_.registry.algorithms().size(); ++index) {
        selectAlgorithm(index);
        action(Action::Step);
        expect(state_.search->result().expanded == 1, "registered algorithm cannot single-step");
        action(Action::Finish);
        expect(state_.search->result().status == SearchStatus::Found, "registered algorithm cannot finish");
        capture(output / (state_.registry.algorithms()[index].id + ".png"));
    }
    selectAlgorithm(1);
    loadSample(3);
    action(Action::Finish);
    expect(!state_.running && state_.search->result().status == SearchStatus::NoPath, "no-path did not stop");
    capture(output / "no-path.png");
    std::ofstream report(output / "smoke-report.txt", std::ios::app);
    report << "PASS: native light workbench; 120x80 field; wall/erase/drag; arbitrary endpoints and wall clearing; "
              "algorithm dropdown/mouse/keyboard; all registry sessions and benchmarks; step/pause/stall/Finish; "
              "save/reload; stale invalidation; six examples; no-path; framebuffer capture.\n";
    window_.close();
    return 0;
}

int Application::recordDemo(const std::filesystem::path& requestedOutput) {
    const auto output = createCaptureDirectory(requestedOutput);
    loadSample(5);
    selectAlgorithm(1);
    state_.stepsPerSecond = 12.f;
    const auto work = solve(state_.map, state_.registry.get("astar")).expanded;
    const auto stride = std::max<std::size_t>(1, work / 120);
    action(Action::RunPause);
    int frame{};
    const auto framePath = [&] {
        std::ostringstream name;
        name << "frame-" << std::setw(4) << std::setfill('0') << frame++ << ".png";
        return output / name.str();
    };
    capture(framePath());
    while (state_.search->result().status == SearchStatus::Searching) {
        for (std::size_t step = 0; step < stride && state_.search->result().status == SearchStatus::Searching; ++step) advance();
        capture(framePath());
        while (const auto event = window_.pollEvent()) if (event->is<sf::Event::Closed>()) { window_.close(); return 1; }
    }
    action(Action::Compare);
    capture(output / "comparison.png");
    for (int hold = 0; hold < 16; ++hold) capture(framePath());
    window_.close();
    return 0;
}

int Application::recordAlgorithms(const std::filesystem::path& requestedOutput) {
    const auto output = createCaptureDirectory(requestedOutput);
    loadSample(5);
    for (std::size_t index = 0; index < state_.registry.algorithms().size(); ++index) {
        selectAlgorithm(index);
        action(Action::RunPause);
        const auto work = solve(state_.map, state_.registry.algorithms()[index]).expanded;
        const auto preview = std::min<std::size_t>(work > 1 ? work - 1 : 1, std::max<std::size_t>(20, work / 3));
        for (std::size_t step = 0; step < preview && state_.search->result().status == SearchStatus::Searching; ++step) advance();
        state_.running = false;
        capture(output / (state_.registry.algorithms()[index].id + "-search.png"));
        action(Action::Finish);
        action(Action::Compare);
        capture(output / (state_.registry.algorithms()[index].id + "-result.png"));
    }
    window_.close();
    return 0;
}

} // namespace pathx
