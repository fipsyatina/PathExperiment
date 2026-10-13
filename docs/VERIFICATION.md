# Verification

Checked locally on Windows x64 with MSVC, SFML 3.1.0 and a Release build.

- 40 core checks cover weighted route correctness, unreachable and shared endpoints,
  deterministic steps, serialization, registry validation and custom factories.
- Dijkstra and A* are checked against an independent Bellman–Ford oracle on
  80 small maps generated with a fixed seed.
- The supplied DFS extension example is compiled by the test build and was
  exercised through the registry during development.
- The native smoke test covers the 120 × 80 field, terrain painting and erasing,
  continuous strokes, arbitrary endpoint placement including walls and borders,
  dropdown and keyboard algorithm selection, Run/Pause/Step/Finish, the speed
  slider, saving and reloading, stale-result invalidation, all six examples and
  completion of searches through every registered factory.
- The screenshot and GIF are native SFML captures. The GIF samples a long
  search; normal visual execution renders every expansion separately.

## Example costs

| Map | Dijkstra / A* minimum cost |
| --- | ---: |
| Open field | 70 |
| Maze | 126 |
| Cheaper detour | 49 |
| Blocked goal | no route |
| Narrow passage | 35 |
| Large weighted field | 155 |

On the large weighted field, BFS and Greedy return a legal route with cost 266.
These algorithms do not promise minimum weighted cost.

## Package and CI

The Windows executable links SFML and the C++ runtime statically. Its checked
PE imports contain only Windows system DLLs. Installed-package smoke testing
also runs from an unrelated working directory to check resource discovery.

GitHub Actions builds the core on Linux in Debug and Release, and builds/tests
the application on Windows in Release. See the repository’s Actions tab for
current run results. Native tests use framebuffer readback so software OpenGL
runners do not need textures larger than 1024 pixels.

A native smoke test is not a Linux/macOS GUI test or a manual test of the
Windows file-selection dialog.
