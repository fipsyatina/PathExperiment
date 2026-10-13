#pragma once

#include "pathexperiment/MapStorage.hpp"
#include "pathexperiment/Search.hpp"
#include "pathexperiment/AlgorithmRegistry.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <filesystem>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace pathx {

enum class Tool { Road, Plain, Swamp, Wall, Start, Goal };
enum class Action { AlgorithmMenu, RunPause, Step, Reset, Finish, Clear, Compare, PreviousSample, NextSample, LoadSample, Save, Load };

class MapEditor {
public:
    static bool paint(Map& map, Point point, Tool tool, bool erase);
};

struct AppState {
    Map map{};
    AlgorithmRegistry registry{makeDefaultRegistry()};
    std::size_t algorithmIndex{1};
    Tool tool{Tool::Wall};
    std::unique_ptr<ISearchSession> search{};
    std::vector<std::optional<BenchmarkResult>> comparison{};
    bool algorithmMenuOpen{};
    std::size_t algorithmMenuCursor{1};
    std::size_t algorithmMenuOffset{};
    bool running{};
    bool dragging{};
    bool erasing{};
    bool speedDragging{};
    float stepsPerSecond{24.f};
    float stepAge{1.f};
    std::optional<Point> hover{};
    sf::Vector2f mouse{-1.f, -1.f};
    int sampleIndex{5};
    std::string mapName{"Untitled map"};
    std::string message{"Draw terrain, place S and G, then run an experiment."};
    std::filesystem::path resources{};
    std::filesystem::path saveDirectory{};
};

struct ButtonSpec {
    sf::FloatRect bounds;
    std::string label;
    Action action;
    bool selected{};
    bool primary{};
};

class Renderer {
public:
    explicit Renderer(const std::filesystem::path& resources);
    void draw(sf::RenderWindow& window, const AppState& state) const;
    [[nodiscard]] static std::vector<ButtonSpec> buttons(const AppState& state);
    [[nodiscard]] static sf::FloatRect toolBounds(int tool);
    [[nodiscard]] static sf::FloatRect speedBounds();
    [[nodiscard]] static sf::FloatRect algorithmOptionBounds(std::size_t visibleIndex);
    [[nodiscard]] static std::optional<Point> cellAt(sf::Vector2f position, const Grid& grid);
    [[nodiscard]] static sf::Vector2f cellCenter(Point point, const Grid& grid);
private:
    void text(sf::RenderTarget& target, const std::string& value, float x, float y,
              unsigned int size, sf::Color color, bool bold = false) const;
    void box(sf::RenderTarget& target, sf::FloatRect bounds, sf::Color fill,
             sf::Color outline = sf::Color::Transparent, float thickness = 1.f) const;
    void line(sf::RenderTarget& target, sf::Vector2f from, sf::Vector2f to,
              sf::Color color, float thickness = 2.f) const;
    void quad(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d, sf::Color color) const;
    void flush(sf::RenderTarget& target) const;
    void grid(sf::RenderTarget& target, const AppState& state) const;
    void sidebar(sf::RenderTarget& target, const AppState& state) const;
    sf::Font font_;
    mutable std::vector<sf::Vertex> geometry_{};
    // Layout positions identify label slots; only changing text is reshaped.
    mutable std::map<std::tuple<float, float, unsigned int, bool>, sf::Text> labels_{};
};

class Application {
public:
    explicit Application(std::filesystem::path resources);
    int run();
    int smokeTest(const std::filesystem::path& output);
    int recordDemo(const std::filesystem::path& output);
    int recordAlgorithms(const std::filesystem::path& output);
private:
    void action(Action action);
    void advance();
    void resetSearch();
    void update(float seconds);
    void handleEvent(const sf::Event& event);
    void pointerDown(sf::Vector2f position, bool erase, std::optional<Tool> endpoint = {});
    void pointerMove(sf::Vector2f position);
    void paint();
    void setSpeed(float x);
    void selectAlgorithm(std::size_t index);
    void startSearch();
    void capture(const std::filesystem::path& path);
    void loadSample(int index);
    void save();
    void load();
    void render();
    void expect(bool condition, const std::string& message) const;
    AppState state_{};
    Renderer renderer_;
    sf::RenderWindow window_;
    float accumulator_{};
};

[[nodiscard]] std::filesystem::path findResources(const std::filesystem::path& executable);
[[nodiscard]] const std::array<std::string, 6>& sampleFiles();
[[nodiscard]] const std::array<std::string, 6>& sampleNames();

} // namespace pathx
