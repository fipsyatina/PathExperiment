#pragma once
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace pathx::theme {
inline const sf::Color background{245, 246, 247};
inline const sf::Color surface{255, 255, 255};
inline const sf::Color toolbar{239, 241, 243};
inline const sf::Color border{204, 210, 216};
inline const sf::Color ink{33, 39, 46};
inline const sf::Color muted{87, 98, 110};
inline const sf::Color accent{36, 91, 137};
inline const sf::Color selection{224, 235, 245};
inline const sf::Color frontier{107, 165, 210};
inline const sf::Color explored{218, 231, 240};
inline const sf::Color current{209, 149, 40};
inline const sf::Color candidate{244, 217, 158};
inline const sf::Color route{29, 130, 88};
inline const sf::Color rejected{178, 70, 75};
inline constexpr sf::Vector2f canvasOrigin{16.f, 112.f};
inline constexpr sf::Vector2f canvasSize{960.f, 640.f};
inline constexpr float maximumCellSize = 8.f;
} // namespace pathx::theme
