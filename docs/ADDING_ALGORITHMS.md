# Adding a Pathfinding Algorithm

PathExperiment discovers algorithms through `AlgorithmRegistry`. An algorithm
supplies metadata for the interface and a factory that creates an independent
`ISearchSession`. The core model and search interfaces do not depend on SFML.

The complete depth-first search implementation in
[`examples/CustomAlgorithmExample.cpp`](https://github.com/fipsyatina/PathExperiment/blob/main/examples/CustomAlgorithmExample.cpp)
is the reference extension. It is an actual search implementation rather than
a renamed wrapper around a built-in algorithm. Depth-first search is useful as
an extension example because its LIFO frontier differs clearly from the four
built-in priority policies. It intentionally guarantees neither the fewest
steps nor the lowest weighted cost.

## 1. Implement `ISearchSession`

Create a final class derived from `pathx::ISearchSession` and implement every
virtual method:

```cpp
class MySearchSession final : public pathx::ISearchSession {
public:
    MySearchSession(pathx::Map map, bool recordEvents);

    const pathx::SearchStep& step() override;
    const pathx::Map& map() const noexcept override;
    const std::vector<pathx::NodeData>& nodes() const noexcept override;
    const pathx::SearchStep& lastStep() const noexcept override;
    const pathx::SearchResult& result() const noexcept override;
    std::vector<pathx::Point> currentPath() const override;
};
```

The session owns its `Map`. This gives every run an isolated snapshot even if
the editor changes later. Its `nodes()` vector must contain exactly one
`NodeData` per grid cell, addressed by `Grid::index()`.

Maintain these `NodeData` fields for rendering and inspection:

- `g`: actual accumulated entry cost from the start; the start has cost zero.
- `h`: heuristic value, or zero when the algorithm has no heuristic.
- `depth`: number of moves from the start.
- `priority`: the value or rank used to select the node.
- `parent`: previous grid index for route reconstruction.
- `state`: `Unseen`, `Open`, or `Closed`.

Use `pathx::infinity` for unknown integer values. It leaves enough headroom for
safe additions on the supported maximum grid size.

## 2. Preserve the incremental-search contract

Each call to `step()` must discard stale frontier entries and expand at most
one live cell. Expanding a cell means marking it closed, incrementing
`SearchResult::expanded`, and examining its four `Grid::neighbors()`.

When `recordEvents` is true, set `SearchStep::current` and report each examined
neighbor as one of:

- `Discovered`: first insertion into the frontier.
- `Improved`: a better route replaced an existing open route.
- `Blocked`: wall terrain.
- `Closed`: already expanded.
- `NoImprovement`: passable, but the candidate route was not accepted.

When event recording is false, leave the event list empty. `solve()` and
`benchmark()` use this mode so measurement excludes animation work.

Set the final status to `Found`, `NoPath`, or `InvalidMap`. A found path includes
both endpoints, while its weighted cost excludes the starting cell. If start
equals goal, the result is a one-cell path with zero steps and zero cost.

## 3. Register metadata and a factory

Construct an `AlgorithmDescriptor` with a stable, unique ID:

```cpp
registry.add(pathx::AlgorithmDescriptor{
    "my-search",
    "My search",
    "A concise explanation shown in the application.",
    "priority expression",
    false,
    [](pathx::Map map, bool recordEvents) {
        return std::make_unique<MySearchSession>(std::move(map), recordEvents);
    },
});
```

Set `guaranteesLowestCost` to true only when the algorithm is proven to return
the minimum weighted entry cost for this grid model. Breadth-first search, for
example, minimizes steps but not weighted cost, so its value is false.

The registry rejects empty IDs, duplicate IDs, and empty factories. Its
`get(id)` method throws when an ID is unknown. Factories must return a non-null
session.

## 4. Connect the implementation

For the included example:

1. Add `examples/CustomAlgorithmExample.cpp` to the `pathx_core` source list,
   or copy it into `src/algorithms/` and add that source instead.
2. Declare its registration function where the custom hook can call it:

   ```cpp
   namespace pathx::examples {
   void registerDepthFirstExample(pathx::AlgorithmRegistry& registry);
   }
   ```

3. In `src/algorithms/CustomAlgorithms.cpp`, replace the placeholder body with:

   ```cpp
   void pathx::registerCustomAlgorithms(AlgorithmRegistry& registry) {
       examples::registerDepthFirstExample(registry);
   }
   ```

`makeDefaultRegistry()` first registers Dijkstra, A*, breadth-first, and greedy
best-first, then invokes this hook. The application can therefore populate its
selector from `registry.algorithms()` without adding another hard-coded UI
branch.

## 5. Verify correctness

Compile the example independently before enabling it:

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude \
    -c examples/CustomAlgorithmExample.cpp
```

Then add focused tests for invalid endpoints, no path, start equal to goal,
route adjacency, weighted result cost, deterministic repeated runs, and the
one-live-expansion-per-step rule. Compare an optimal custom algorithm against
an independent shortest-path oracle before setting `guaranteesLowestCost`.

Run the repository validation after registering the source:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Use a new build or capture directory when preserving earlier validation
artifacts. Do not judge algorithm speed from visual animation; use the
descriptor overload of `benchmark()` so setup, search, and route reconstruction
are included in each independent timed run.
