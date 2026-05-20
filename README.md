## EveHorizon
A high-performance, C++20 library for the on-the-fly linearization, verification, and configuration analysis of concurrent event systems.
Designed specifically for massive event volumes and robust integration into broader model checkers, simulation environments, and formal verification pipelines.
------------------------------
## 🌌 The Philosophy & Core Pillars
Distributed and concurrent systems often generate vast state spaces where the order and legitimacy of execution are difficult to verify. EveHorizon is built around a singular metaphor: the Horizon represents the precise boundary of events sitting on the threshold of execution. By tracking and managing this local wavefront, EveHorizon explores complex event structures without drowning in global state graphs.
The design philosophy of EveHorizon is built upon four fundamental architectural choices:
## 1. Bounded Spaces: Traversal via Kahn's Algorithm
For exhaustive state-space searches inside bounded, finite event structures, EveHorizon leverages a generalized, backtracking variant of Kahn's Algorithm.

* The Mechanism: Instead of searching the global network at every branch, the engine relies on stateful trackers that use disruptive satisfaction counters (such as in-degree and conflict boundaries). Every state mutation is an incremental $O(1)$ push or pop operation.
* The Finite Limitation: While Kahn's algorithm yields blazing-fast execution speeds for finite graphs, it relies on complete, bounded structural definitions. Passing an infinite unfolding to a Kahn tracker results in infinite loop traps or unbounded initialization loops, making it structurally incompatible with infinite concurrency traces.

## 2. Unbounded Spaces: Lazy Unfolding via Breadth-First Search (BFS)
To safely reason about infinite models, preemption, or loops, EveHorizon shifts to a stateless, discovery-driven Breadth-First Search (BFS) engine.

* Why BFS? A Depth-First Search (DFS) can easily get trapped down a single infinite execution path, never returning to evaluate alternative branches. BFS evaluates all traces of length $N$ before moving to $N+1$. This guarantees structural fairness—every possible finite prefix of your concurrent system will eventually cross the horizon.
* On-The-Fly Discovery: Rather than storing a global registry, the infinite engines discover adjacent events dynamically by looking downstream from the latest fired action, making it entirely safe for infinite systems, but less performant.

## 3. Structural Extensibility: Decoupling Trackers from Search Engines
EveHorizon completely separates how an event structure evaluates rules from how the search space is navigated.

* The Trackers: Trackers serve as pure, encapsulated logic backends. They handle the mathematical constraints of various event structure types—whether it's the strict precedence of Prime Structures, the alternative enabling histories of Stable Structures, the complex AND-of-ORs constraints of Bundle Structures, or the non-inherited local restrictions of Flow Event Structures. For each type of event structures, there is a tracker implemented that represents that kind of structure.
* The Explorers: The explorers are universal, algorithmic drivers. They consume trackers via compile-time interfaces, remaining entirely oblivious to whether they are traversing an Extended Bundle system or a standard Petri Net unfolding.
* Graph Representation: To abstract from the way an event structure is represented, ie how the graphs of causality and conflict are represented (eg through a 2D array, a map, etc), getters are used. For instance, causality is represented through a function that given an event, it returns the list of predecessors in case of a prime event structure, or the list of enabling bundles in case of a bundle event structure, and so on. The same applies to the conflict relation.


       [ Custom Event Logic Functors / Getters ]
                         │
                         ▼
        [ Stateless / Stateful Trackers ] ◄── Encapsulated Rules
                         │
                         ▼  (Verified by C++20 Concepts)
       [ Universal Trace Generation Engines ] ◄── Reusable Algorithms
                         │
                         ▼
       [ Validated Traces / Deadlock Reports ] ◄── The application

## 4. High-Performance Engineering: Zero-Allocation and Modern C++
EveHorizon is engineered for modern hardware, optimizing execution speed and minimizing memory overhead through standard C++ semantics:

* Asynchronous Coroutines: Search loops use lazy coroutine TraceGenerator wrappers. Computation pauses at every co_yield suspension boundary, generating data only when requested. This give the control to the caller to decide when to stop generating traces.
* Move Semantics & Lifetime Isolation: Traces are yielded via rvalue move semantics (std::move), passing internal allocation memory directly to the consumer without redundant copying.
* Reference-Qualified Engines: Member execution boundaries use reference qualifiers (eg explore() & vs explore() && = delete). The compiler actively forbids launching executions on rvalue temporaries, catching dangling-pointer vulnerabilities at compile time.
* Contiguous Cache Alignment: Wherever possible, scattered tree lookups are replaced with binary searches over sorted std::vector chunks.

------------------------------
## 📈 Integration & Verification Use Case
EveHorizon is explicitly structured to act as an underlying validation framework. Higher-level analysis routines, such as configuration checkers and deadlock detectors, can consume this module with minimal integration overhead.

