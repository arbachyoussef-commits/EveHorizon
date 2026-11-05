## EveHorizon
A high-performance, C++20 library for the on-the-fly linearization, verification, and configuration analysis of concurrent event systems.
Designed specifically for massive event volumes and robust integration into broader model checkers, simulation environments, and formal verification pipelines.
------------------------------
## 🌌 The Philosophy & Core Pillars
Distributed and concurrent systems often generate vast state spaces where the order and legitimacy of execution are difficult to verify. EveHorizon is built around a singular metaphor: the Horizon represents the precise boundary of events sitting on the threshold of execution. By tracking and managing this local wavefront, EveHorizon explores complex event structures without drowning in global state graphs.
The design philosophy of EveHorizon is built upon four fundamental architectural choices:
## 1. Three Trace-Generation Models
EveHorizon provides three coroutine-based generators. Each yields trace prefixes on demand, but they differ in the structure they expect, traversal order, and how they maintain search state.

| Generator | Intended input | Traversal order | Search state | Useful properties and trade-offs |
| --- | --- | --- | --- | --- |
| **Finite ES: Kahn-style DFS** (`Include/FiniteES/DFSTraceGenerator.h`) | A supplied, complete finite event range | Depth first; emits a prefix before exploring its extensions | One tracker is updated with `push`/`pop`; enabled candidates are selected from the supplied events at each prefix | `TraceResult` reports whether each prefix is maximal. Complete finite input lets the tracker use precomputed structure and incremental updates. It does not avoid the potentially exponential number of traces. |
| **Infinite ES: hybrid BFS** (`Include/InfiniteES/BFS/BFSTraceGenerator.h`) | Initial roots and a BFS tracker that discovers successors as events fire | Breadth first by trace length | Queue of branch states; each queued branch carries a copied tracker and its local frontier | Visits shorter prefixes before longer ones. With finite branching, it will not spend all its time down one infinite branch. Memory can grow with the breadth of the explored levels and the size of copied tracker states. |
| **Infinite ES: generative DFS** (`Include/InfiniteES/DFS/DFSTraceGenerator.h`) | Initial roots, a generative DFS tracker, and a trace-limit predicate | Depth first; explores a branch before its siblings | One tracker is advanced and rolled back with `push`/`pop`; successors are discovered as the search proceeds | Uses a stack and branch-local frontier segments rather than a queue of all waiting branches. A matching limit predicate prunes that prefix and its descendants. `NeverPruneTrace{}` is available for explicitly unbounded traversal; DFS may follow an infinite branch indefinitely, so it does not provide BFS's fairness. |

The finite and infinite names describe the intended model/input, not a guarantee that exploration terminates. A finite event structure can still have an exponentially large set of traces. The finite generator receives the complete event range and can report maximality; the two infinite generators start from roots and obtain newly enabled events incrementally, and yield paths without a maximality flag.

Choose **finite Kahn-style DFS** when the event set is known and finite and maximal-trace information is useful. Choose **infinite BFS** when reaching prefixes in increasing length matters and the frontier can fit in memory. Choose **infinite DFS** when exploring deeply with lower queue pressure is preferable and the model or a trace limiter gives adequate control over long or infinite branches. Performance depends on the event structure, tracker, and branching pattern; these trade-offs should be measured on representative workloads.

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

