#include "Application.hpp"
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

const std::array<std::string, 5>& sampleFiles() {
    static const std::array<std::string, 5> files{
        "open.pemap", "maze.pemap", "weighted-detour.pemap", "blocked-goal.pemap", "narrow-passage.pemap"};
    return files;
}
const std::array<std::string, 5>& sampleNames() {
    static const std::array<std::string, 5> names{
        "Open field", "Maze", "The cheaper detour", "Blocked goal", "Narrow passage"};
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
        if (!passable(map.grid.at(point)) || endpoint == point) return false;
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
      window_(sf::VideoMode({1280u, 820u}), "PathExperiment | Pathfinding Lab") {
    state_.resources = std::move(resources);
    state_.saveDirectory = userSaveDirectory(state_.resources);
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
    } else if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (pressed->button == sf::Mouse::Button::Left || pressed->button == sf::Mouse::Button::Right)
            pointerDown(window_.mapPixelToCoords(pressed->position), pressed->button == sf::Mouse::Button::Right);
    } else if (event.is<sf::Event::MouseButtonReleased>()) {
        state_.dragging = false;
        state_.speedDragging = false;
    } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using Key = sf::Keyboard::Key;
        if (key->control && key->code == Key::S) action(Action::Save);
        else if (key->control && key->code == Key::O) action(Action::Load);
        else if (key->code == Key::Space) action(Action::RunPause);
        else if (key->code == Key::N) action(Action::Step);
        else if (key->code == Key::R) action(Action::Reset);
        else if (key->code == Key::C) action(Action::Clear);
        else if (key->code == Key::B) action(Action::Compare);
        else if (key->code == Key::Tab)
            action(state_.algorithm == Algorithm::AStar ? Action::Dijkstra : Action::AStar);
        else {
            const std::array keys{Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5, Key::Num6};
            for (std::size_t index = 0; index < keys.size(); ++index)
                if (key->code == keys[index]) state_.tool = static_cast<Tool>(index);
        }
    }
}

void Application::pointerDown(sf::Vector2f position, bool erase) {
    state_.mouse = position;
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
    state_.dragging = state_.hover.has_value();
    state_.erasing = erase;
    if (state_.dragging) paint();
}

void Application::pointerMove(sf::Vector2f position) {
    const auto previous = state_.hover;
    state_.mouse = position;
    state_.hover = Renderer::cellAt(position, state_.map.grid);
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
        state_.comparison = {};
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

void Application::action(Action requested) {
    try {
        switch (requested) {
        case Action::Dijkstra:
        case Action::AStar: {
            const auto algorithm = requested == Action::Dijkstra ? Algorithm::Dijkstra : Algorithm::AStar;
            if (state_.algorithm != algorithm) { resetSearch(); state_.algorithm = algorithm; }
            state_.message = algorithm == Algorithm::AStar ? "A*: next cell minimizes f = g + h." : "Dijkstra: next cell minimizes accumulated cost g.";
            break;
        }
        case Action::RunPause:
            if (!state_.search || state_.search->result().status != SearchStatus::Searching) {
                state_.search = std::make_unique<SearchSession>(state_.map, state_.algorithm);
                accumulator_ = 0.f;
                state_.running = state_.search->result().status == SearchStatus::Searching;
            } else state_.running = !state_.running;
            state_.message = state_.running ? "Searching... blue branches are explored; orange is the current candidate." : "Paused. Press N to expand exactly one cell.";
            break;
        case Action::Step:
            state_.running = false;
            if (!state_.search || state_.search->result().status != SearchStatus::Searching)
                state_.search = std::make_unique<SearchSession>(state_.map, state_.algorithm);
            advance();
            break;
        case Action::Reset:
            resetSearch();
            state_.message = "Search cleared. Your map is ready for another experiment.";
            break;
        case Action::Clear:
            resetSearch();
            state_.comparison = {};
            state_.map = Map{};
            state_.mapName = "Untitled map";
            state_.hover.reset();
            state_.message = "Empty 40 x 25 map. Choose a brush and draw.";
            break;
        case Action::Compare:
            state_.running = false;
            state_.comparison[0] = benchmark(state_.map, Algorithm::Dijkstra);
            state_.comparison[1] = benchmark(state_.map, Algorithm::AStar);
            state_.message = "Compared on the same map: median of 31 runs, with no rendering or delays.";
            break;
        case Action::PreviousSample: state_.sampleIndex = (state_.sampleIndex + 4) % 5; break;
        case Action::NextSample: state_.sampleIndex = (state_.sampleIndex + 1) % 5; break;
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
        state_.message = "Route found. Green = optimal route; blue = explored branches outside the route.";
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
    state_.comparison = {};
    state_.map = std::move(map);
    state_.hover.reset();
    state_.sampleIndex = index;
    state_.mapName = sampleNames().at(static_cast<std::size_t>(index));
    state_.message = "Example loaded. Run both algorithms on these exact conditions.";
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
    state_.comparison = {};
    state_.map = std::move(map);
    state_.hover.reset();
    state_.mapName = chosen.stem().string();
    state_.message = "Loaded: " + chosen.string();
}

void Application::render() {
    window_.clear(sf::Color(12, 18, 28));
    renderer_.draw(window_, state_);
    window_.display();
}

void Application::capture(const std::filesystem::path& path) {
    const auto started = std::chrono::steady_clock::now();
    // Capture the back buffer before display swaps it. Reading after display
    // can return the previous frame on double-buffered Windows drivers.
    window_.clear(sf::Color(12, 18, 28));
    renderer_.draw(window_, state_);
    const auto drawn = std::chrono::steady_clock::now();
    sf::Texture texture(window_.getSize());
    texture.update(window_);
    const auto pixels = texture.copyToImage();
    const auto copied = std::chrono::steady_clock::now();
    if (!pixels.saveToFile(path)) throw std::runtime_error("Could not save capture.");
    const auto saved = std::chrono::steady_clock::now();
    window_.display();
    const auto displayed = std::chrono::steady_clock::now();
    const auto milliseconds = [](auto from, auto to) { return std::chrono::duration<double, std::milli>(to - from).count(); };
    std::ofstream profile(path.parent_path() / "capture-profile.csv", std::ios::app);
    profile << path.filename().string() << ',' << milliseconds(started, drawn) << ',' << milliseconds(drawn, copied)
            << ',' << milliseconds(copied, saved) << ',' << milliseconds(saved, displayed) << '\n';
}

void Application::expect(bool condition, const std::string& message) const {
    if (!condition) throw std::runtime_error("GUI smoke: " + message);
}

int Application::smokeTest(const std::filesystem::path& requestedOutput) {
    const auto output = createCaptureDirectory(requestedOutput);
    state_.saveDirectory = output / "saved-maps";
    capture(output / "empty.png");
    // Drive the same pointer/key event handlers as normal interaction.
    handleEvent(sf::Event::KeyPressed{.code = sf::Keyboard::Key::Num4});
    handleEvent(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left, {144, 248}});
    handleEvent(sf::Event::MouseButtonReleased{sf::Mouse::Button::Left, {144, 248}});
    expect(state_.map.grid.at({4, 2}) == Terrain::Wall, "wall brush did not paint");
    handleEvent(sf::Event::MouseButtonPressed{sf::Mouse::Button::Right, {144, 248}});
    handleEvent(sf::Event::MouseButtonReleased{sf::Mouse::Button::Right, {144, 248}});
    expect(state_.map.grid.at({4, 2}) == Terrain::Plain, "right erase did not restore plain");
    state_.tool = Tool::Road;
    pointerDown({144.f, 248.f}, false);
    pointerMove({224.f, 248.f});
    state_.dragging = false;
    for (int x = 4; x <= 8; ++x) expect(state_.map.grid.at({x, 2}) == Terrain::Road, "drag brush left a gap");
    state_.tool = Tool::Start;
    pointerDown({144.f, 248.f}, false);
    state_.dragging = false;
    expect(state_.map.start == Point{4, 2}, "start placement failed");
    state_.tool = Tool::Goal;
    pointerDown({224.f, 248.f}, false);
    state_.dragging = false;
    expect(state_.map.goal == Point{8, 2}, "goal placement failed");
    pointerDown({1230.f, 328.f}, false);
    pointerMove({1234.f, 328.f});
    state_.speedDragging = false;
    expect(state_.stepsPerSecond > 59.f, "speed slider failed");
    state_.stepsPerSecond = 24.f;
    action(Action::Clear);
    action(Action::Step);
    expect(state_.search && state_.search->result().expanded == 1, "single step did not expand exactly one cell");
    expect(!state_.running, "step must stay paused");
    const auto expanded = state_.search->result().expanded;
    update(0.5f);
    expect(state_.search->result().expanded == expanded, "paused search advanced");
    capture(output / "single-step.png");
    action(Action::RunPause);
    const auto beforeStall = state_.search->result().expanded;
    update(1.f);
    expect(state_.search->result().expanded == beforeStall + 1, "visual mode skipped steps during a stall");
    action(Action::RunPause);
    loadSample(2);
    pointerDown({1000.f, 209.f}, false); // Run button.
    expect(state_.running, "Run button did not start visual search");
    update(0.1f);
    expect(state_.search->result().expanded > 0, "Run did not advance");
    pointerDown({1000.f, 209.f}, false); // Pause button.
    expect(!state_.running, "Pause button did not pause");
    for (int count = 0; count < 120 && state_.search->result().status == SearchStatus::Searching; ++count) advance();
    capture(output / "searching.png");
    while (state_.search->result().status == SearchStatus::Searching) advance();
    expect(state_.search->result().status == SearchStatus::Found, "weighted example should have route");
    action(Action::Compare);
    expect(state_.comparison[0] && state_.comparison[1], "comparison is missing");
    expect(state_.comparison[0]->result.cost == state_.comparison[1]->result.cost, "algorithm costs differ");
    capture(output / "comparison.png");
    state_.hover = state_.map.start;
    capture(output / "hover.png");
    state_.hover.reset();
    save();
    int savedCount{};
    for (const auto& item : std::filesystem::directory_iterator(state_.saveDirectory)) {
        if (item.path().extension() != ".pemap") continue;
        const auto restored = loadMap(item.path());
        expect(restored.start == state_.map.start && restored.goal == state_.map.goal, "saved endpoints differ");
        expect(solve(restored, Algorithm::AStar).cost == state_.search->result().cost, "saved terrain differs");
        ++savedCount;
    }
    expect(savedCount > 0, "save did not produce a map");
    state_.hover = Point{12, 12};
    state_.tool = Tool::Wall;
    paint();
    expect(!state_.search && !state_.comparison[0] && !state_.running, "editing failed to invalidate old results");
    for (int index = 0; index < 5; ++index) {
        loadSample(index);
        const auto first = solve(state_.map, Algorithm::Dijkstra);
        const auto second = solve(state_.map, Algorithm::AStar);
        expect(first.status == second.status && first.cost == second.cost, "sample algorithms disagree");
        expect(first.status == (index == 3 ? SearchStatus::NoPath : SearchStatus::Found), "unexpected sample reachability");
    }
    loadSample(3);
    action(Action::RunPause);
    while (state_.search->result().status == SearchStatus::Searching) advance();
    expect(!state_.running && state_.search->result().status == SearchStatus::NoPath, "no-path did not stop");
    capture(output / "no-path.png");
    std::ofstream report(output / "smoke-report.txt", std::ios::app);
    report << "PASS: native SFML rendering; wall/erase input events; drag brush; start/goal placement; speed slider; single-step; pause; Run/Pause clicks; "
              "weighted route; both benchmarks; timestamp save/reload; stale-result invalidation; all five examples; no-path.\n";
    window_.close();
    return 0;
}

int Application::recordDemo(const std::filesystem::path& requestedOutput) {
    const auto output = createCaptureDirectory(requestedOutput);
    loadSample(2);
    state_.algorithm = Algorithm::AStar;
    state_.stepsPerSecond = 12.f;
    action(Action::RunPause);
    int frame{};
    const auto framePath = [&] {
        std::ostringstream name;
        name << "frame-" << std::setw(4) << std::setfill('0') << frame++ << ".png";
        return output / name.str();
    };
    capture(framePath());
    while (state_.search->result().status == SearchStatus::Searching) {
        advance();
        capture(framePath());
    }
    action(Action::Compare);
    capture(output / "comparison.png");
    for (int hold = 0; hold < 16; ++hold) capture(framePath());
    window_.close();
    return 0;
}

} // namespace pathx
