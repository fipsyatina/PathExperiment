# PathExperiment

[English](README.md) · [Polski](README.pl.md) · [Українська](README.uk.md)

![Animated view of PathExperiment expanding cells, rejecting transitions, and completing a route](docs/media/search.gif)

The GIF samples a long search. Live Step mode expands one cell at a time.

![PathExperiment comparison panel with results for the available algorithms](docs/media/comparison.png)

PathExperiment is a C++20 desktop workbench for drawing weighted grid maps, watching pathfinding one expansion at a time, and comparing algorithms under identical conditions. Its search core is independent of SFML, so the project is both a visual learning tool and a practical prototype for future unit navigation in Ribterra.

[Download the Windows build](https://github.com/fipsyatina/PathExperiment/releases/latest), extract the ZIP, and run `PathExperiment.exe`. The application interface is in English.

## What is implemented

- [x] Large 120 × 80 default field with approximately 8 px cells
- [x] Road, plain, swamp, wall, start, and goal editing tools
- [x] Arbitrary passable start and goal cells
- [x] Dijkstra, A*, breadth-first search, and greedy best-first search
- [x] Live open, closed, current, rejected-neighbor, tentative-path, and final-path states
- [x] Run, pause, one-step, finish, and 1–60 expansions-per-second controls
- [x] Route steps, route cost, expanded cells, status, and computation time
- [x] Fair benchmark mode with one warm-up and 31 independent measured runs
- [x] Versioned `.pemap` save/load format, five 40 × 25 examples, and one 120 × 80 example
- [x] Headless core tests and an optional native SFML smoke test
- [x] Source-level custom algorithm registry

## The workbench

The toolbar contains the terrain and file tools. The inspector beside the map contains the algorithm selector, execution controls, result rows and comparison table. Hover a cell to inspect its search data. Start and goal are marked `S` and `G`.

Five examples use 40 × 25 grids for short experiments; the weighted field uses the full 120 × 80 grid. The loader accepts maps from 1 × 1 through 512 × 512.

## Controls

Left-click paints with the selected tool. Right-click restores plain terrain. Dragging paints a continuous stroke.

Placing an endpoint on a wall converts that cell to plain terrain. Start and goal may overlap; searching then returns a zero-step, zero-cost route.

| Input | Action |
| --- | --- |
| `1` | Road, entry cost 1 |
| `2` | Plain terrain, entry cost 2 |
| `3` | Swamp, entry cost 5 |
| `4` | Impassable wall |
| `5` | Place start |
| `6` | Place goal |
| `Shift` + left-click | Place start directly |
| `Ctrl` + left-click | Place goal directly |
| `Space` | Run or pause |
| `N` | Expand exactly one live cell and inspect its neighbors |
| `Finish` button | Complete the current search without animation |
| `R` | Reset search visualization; keep the map |
| `C` | Create a new plain 120 × 80 map |
| `Tab` | Select the next registered algorithm |
| `B` | Benchmark all registered algorithms on the current map |
| `Ctrl+S` | Save to a uniquely timestamped `.pemap` file |
| `Ctrl+O` | Open a native `.pemap` picker on Windows; load the newest save on other systems |

The speed slider covers 1–60 expansions per second. The visual loop expands at most one cell per rendered frame. If rendering slows down, the search animation slows with it instead of hiding intermediate states.

Saved maps go to `%LOCALAPPDATA%\PathExperiment\maps` on Windows. On Linux they go to `$XDG_DATA_HOME/PathExperiment/maps`, or `~/.local/share/PathExperiment/maps` when `XDG_DATA_HOME` is unset.

## Grid and cost model

Treat the map as a graph $G=(V,E)$. Every passable cell is a vertex in $V$. An edge in $E$ connects two passable orthogonal neighbors; diagonal movement is unavailable. The degree is at most four.

| Terrain | Entry cost |
| --- | ---: |
| Road | 1 |
| Plain | 2 |
| Swamp | 5 |
| Wall | impassable |

For a route $p=(v_0,v_1,\ldots,v_k)$, its length is $k$ steps and its weighted cost is

$$
C(p)=\sum_{i=1}^{k} w(v_i).
$$

The start cell $v_0$ contributes zero. Entering the goal contributes the goal cell's terrain cost. Therefore a longer route can be cheaper: the diagrams use an illustrative ten-step swamp route with cost $10\cdot5=50$ and a fourteen-step road detour with cost $14\cdot1=14$. These numbers explain the model; they are not measured application statistics.

When Dijkstra or A* examines an edge $(u,v)$, it performs relaxation:

$$
g_{candidate}(v)=g(u)+w(v).
$$

If this value improves the recorded value for $v$, the search updates `g`, stores `u` as the parent, and pushes a new frontier entry. Old entries stay in the priority queue and are skipped as stale when popped. This lazy update avoids needing a decrease-key operation.

## Algorithms

| Algorithm | Frontier priority | Guarantee in PathExperiment | Main lesson |
| --- | --- | --- | --- |
| Dijkstra | $g(n)$ | minimum route cost | real accumulated cost is enough |
| A* | $f(n)=g(n)+h(n)$ | minimum route cost | a safe estimate guides an optimal search |
| Breadth-first | depth | minimum number of steps | fewest steps can cost more on weighted terrain |
| Greedy best-first | $h(n)$ | no minimum-cost guarantee | strong goal direction can be shortsighted |

All four use deterministic four-neighbor generation and stable FIFO tie-breaking through a monotonically increasing insertion order. Repeating the same algorithm on the same map therefore produces the same trace.

### Dijkstra

Dijkstra always expands the live frontier entry with the smallest accumulated cost $g$. With positive entry costs, the first time the goal is popped its cost is minimal. The algorithm expands broadly because it has no estimate of the goal direction. This is the weighted-grid form of uniform-cost search described in the [UC Berkeley CS188 uninformed-search notes](https://inst.eecs.berkeley.edu/~cs188/textbook/search/uninformed.html).

![Dijkstra flow: pop the smallest g, relax four neighbors, and choose the cost-14 detour over the cost-50 direct route](docs/media/algorithm-dijkstra.svg)

[View a search state from the application](docs/media/dijkstra-search.png)

### A*

A* orders the frontier by

$$
f(n)=g(n)+h(n), \qquad
h(n)=|x_n-x_G|+|y_n-y_G|.
$$

Manhattan distance counts the fewest orthogonal moves on an obstacle-free grid. Every move costs at least 1, so $h(n)$ never exceeds the real remaining cost. It is also consistent: for adjacent $n,n'$, Manhattan distance changes by at most 1, hence

$$
h(n)\le 1+h(n')\le w(n')+h(n').
$$

Consistency means a cheaper route to an already closed cell is unnecessary under this policy. A* retains Dijkstra's minimum-cost guarantee while usually focusing expansion toward the goal. The role of admissible heuristics and $f=g+h$ follows the [UC Berkeley CS188 informed-search notes](https://inst.eecs.berkeley.edu/~cs188/textbook/search/informed.html).

![A-star flow: combine accumulated cost g with a consistent Manhattan estimate h](docs/media/algorithm-astar.svg)

[View a search state from the application](docs/media/astar-search.png)

### Breadth-first search

Breadth-first search prioritizes depth, so it expands all routes with $d$ steps before routes with $d+1$ steps. It is optimal when every edge has the same cost. PathExperiment intentionally keeps the terrain weights active, so BFS reports the fewest-step route and also calculates its real cost, but that cost may be higher than Dijkstra's or A*'s result.

![Breadth-first flow: expand by depth and prefer a ten-step cost-50 route over a fourteen-step cost-14 route](docs/media/algorithm-bfs.svg)

[View a search state from the application](docs/media/bfs-search.png)

### Greedy best-first search

Greedy best-first search prioritizes only $h(n)$. It often moves sharply toward the goal and can expand few cells, but $g(n)$ does not influence the choice. Walls or expensive terrain can therefore lure it into a costly route. The [CS188 informed-search discussion](https://inst.eecs.berkeley.edu/~cs188/textbook/search/informed.html) likewise distinguishes greedy priority $h$ from A* priority $g+h$.

![Greedy best-first flow: follow the smallest Manhattan h even when the direct route costs more](docs/media/algorithm-greedy.svg)

[View a search state from the application](docs/media/greedy-search.png)

## What the animation shows

One visual step pops the next live frontier cell and evaluates its four neighbors. The canvas distinguishes unseen cells, open cells waiting in the frontier, closed cells already expanded, the current cell, the tentative parent chain, and the final route. Brief rejected-neighbor marks explain walls, already closed cells, and candidates that did not improve the stored value.

Hovering a cell exposes its coordinates, terrain cost, state, `g`, `h`, priority, depth, and parent when those values exist. A rejection is evidence about a particular transition, not an algorithm failure.

## Results and fair comparison

- **Status** — searching, route found, no route, or invalid map.
- **Steps** — number of transitions in the returned route.
- **Cost** — sum of entered-cell costs, excluding the start.
- **Expanded** — live cells removed from the frontier and processed, including the goal when popped.
- **Median** — median computation time in microseconds.

`B` benchmarks every registered algorithm on the same immutable map snapshot. Each row uses one untimed warm-up followed by 31 independent timed sessions. Initialization, search, and route reconstruction are included; rendering and animation delays are excluded. Use a Release build for performance comparisons. Timing on small maps still varies with the machine and operating system, so compare medians and treat them as local measurements.

## Complexity

Let $|V|$ be the number of passable cells and $|E|$ the orthogonal connections. On this grid, $|E|=O(|V|)$.

| Algorithm | Textbook frontier | Textbook time | This implementation |
| --- | --- | --- | --- |
| Dijkstra | binary heap | $O((V+E)\log V)$ | same bound, with lazy stale entries |
| A* | binary heap | $O((V+E)\log V)$ for this finite graph-search implementation | same queue mechanics; explored count depends on the heuristic |
| BFS | FIFO queue | $O(V+E)$ | unified priority queue by depth: $O((V+E)\log V)$ |
| Greedy | priority queue | $O((V+E)\log V)$ | same bound; no optimal-cost guarantee |

Node data uses $O(V)$ memory. Lazy frontier entries can add $O(E)$ queue storage in the weighted searches.

## Example maps and file format

| File | Scenario |
| --- | --- |
| `open.pemap` | open 40 × 25 field |
| `maze.pemap` | alternating maze barriers |
| `weighted-detour.pemap` | expensive direct corridor, cheap road detour, and dead ends |
| `blocked-goal.pemap` | unreachable goal |
| `narrow-passage.pemap` | one narrow connection between large barriers |
| `large-weighted.pemap` | 120 × 80 weighted field for a full-size experiment |

`.pemap` is a strict, versioned text format:

```text
PATHEXPERIMENT 1
40 25
2 12 37 12
........................................
...
```

The second line is `width height`; the third contains zero-based `start_x start_y goal_x goal_y`; exactly `height` rows follow. Symbols are `r` road, `.` plain, `~` swamp, and `#` wall. Endpoint cells must be passable.

`PathExperiment --record-demo captures` records real native search frames and comparison output. It uses an adaptive capture stride on large maps to keep the GIF bounded; normal interactive visual mode still renders every expansion as its own frame. Convert the frames with `python tools/make_demo.py captures docs/media/search.gif` after installing Pillow, and use fresh output paths to preserve earlier captures.

## Build, test, and install

Requirements: CMake 3.28 or newer, a C++20 compiler, and network access during the first configure so CMake can fetch SFML 3.1.0. The setup follows the same CMake integration approach as the official [SFML CMake project template](https://github.com/SFML/cmake-sfml-project). CMake's `-S`, `-B`, `--build`, `--install`, and `ctest` workflow is documented in the official [CMake command-line manual](https://cmake.org/cmake/help/latest/manual/cmake.1.html).

On Windows, install Visual Studio's **Desktop development with C++** workload. On Ubuntu, the GUI build needs:

```sh
sudo apt update
sudo apt install cmake g++ libxrandr-dev libxcursor-dev libxi-dev libudev-dev libgl1-mesa-dev libegl1-mesa-dev
```

Configure, build, and test:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The GUI executable is `build/bin/PathExperiment.exe` on Windows or `build/bin/PathExperiment` on Linux. Create a portable install tree with:

```sh
cmake --install build --config Release --prefix dist/PathExperiment
```

Build only the SFML-independent core and tests with:

```sh
cmake -S . -B build-core -DPATHEXPERIMENT_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## Add an algorithm

Algorithms are source extensions. Implement `ISearchSession`, create an `AlgorithmDescriptor` factory, and add it in `registerCustomAlgorithms()` in `src/algorithms/CustomAlgorithms.cpp`. Rebuild the application. The registry then supplies the selector and benchmark table automatically.

The extension boundary is intentionally C++ rather than runtime DLL discovery or command scripting: a custom session must expose the exact map, node, step-event, result, and tentative-path data needed by the visualizer. See [Adding algorithms](docs/ADDING_ALGORITHMS.md) for the API contract and a compile-ready tutorial.

## Architecture and project scope

`pathx_core` owns the grid, search sessions, registry, results, benchmarking, and storage without including SFML headers. The application layer owns editing, input, timing, and rendering. This boundary allows headless correctness tests and lets a future Ribterra adapter reuse the model without modifying the Ribterra repository today.

Read [Architecture](docs/ARCHITECTURE.md), [Adding algorithms](docs/ADDING_ALGORITHMS.md), and [Verification](docs/VERIFICATION.md) for the detailed contracts and current evidence. The CI workflow builds the headless core on Linux and the application on Windows.

## Sources

- [UC Berkeley CS188: Uninformed Search](https://inst.eecs.berkeley.edu/~cs188/textbook/search/uninformed.html)
- [UC Berkeley CS188: Informed Search](https://inst.eecs.berkeley.edu/~cs188/textbook/search/informed.html)
- [SFML CMake project template](https://github.com/SFML/cmake-sfml-project)
- [CMake command-line manual](https://cmake.org/cmake/help/latest/manual/cmake.1.html)

## License

PathExperiment source code is available under the MIT License. Third-party notices are collected in `assets/`.
