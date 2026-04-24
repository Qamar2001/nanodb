# NanoDB — Architecture & Query Optimizer

**Course:** CS-4002 Applied Programming — MS-CS Spring 2026
**Instructor:** Bushra Fatima
**Deadline:** 10 May 2026

A mini relational database written from scratch in C++ with **zero STL containers**.
Every data structure (stack, queue, priority queue, hash map, doubly-linked list,
AVL tree, graph) is hand-rolled over raw pointers and arrays.

> GitHub repo: https://github.com/Qamar2001/nanodb

---

## Build & Run

Requires a C++17 compiler (`g++` / `clang++`) and `make`.

```bash
make           # builds ./nanodb and ./test_runner
make run       # fresh run: builds dummy TPC-H data, runs queries.txt, saves to data/
make reload    # runs ./nanodb --reload (loads previously saved tables from disk)
make test      # runs the automated test_runner against queries.txt
make clean     # removes binaries, objects, data/, log
```

### First-time demo sequence

```bash
make
./nanodb                 # Runs all 7 test cases, saves tables to data/
./nanodb --reload        # Verifies persistence: reads tables back from disk
cat nanodb_execution.log # Review LRU evictions, postfix conversions, MST decisions
```

### Scaling the dataset

Default dummy data is small so the 3-table join demo completes quickly.
To stress-test at larger scale:

```bash
NANODB_SCALE=10 ./nanodb   # 10x larger: 5000 customer / 6000 orders / 10000 lineitem
```

---

## Architecture

### Directory layout

```
nanodb/
├── main.cpp              driver; runs all 7 test cases
├── test_runner.cpp       minimal harness: loads tables + executes queries.txt
├── queries.txt           50-query workload file
├── Makefile
├── data/                 binary-serialised tables (created on first save)
├── nanodb_execution.log  detailed runtime log
│
├── logger.{h,cpp}        global Logger -> nanodb_execution.log
├── data_types.{h,cpp}    DataType base + IntType/FloatType/StringType (polymorphism + op-overload)
├── stack.h               Stack<T>       — raw array, geometric grow
├── queue.h               Queue<T> + PriorityQueue<T> (max-heap on int priority)
├── hash_map.{h,cpp}      string->void* chaining, djb2, rehash at load 0.75
├── schema.{h,cpp}        Column / Row / Table + binary serialise
├── lru_cache.{h,cpp}     Doubly-linked list + HashMap index, O(1) get/put/evict
├── pager.{h,cpp}         Page buffer pool on top of LRUCache, dirty write-back
├── avl_tree.{h,cpp}      self-balancing BST (LL/RR/LR/RL rotations) on int keys
├── graph.{h,cpp}         weighted undirected graph + Kruskal MST with union-find
├── parser.{h,cpp}        Tokeniser + Shunting-Yard Infix->Postfix using Stack<int>
└── executor.{h,cpp}      Catalog, AVL index, join MST, postfix evaluator, persistence
```

### Complexity summary (full proofs in the Research Report)

| Component            | Operation            | Complexity   |
|----------------------|----------------------|--------------|
| HashMap (djb2+chain) | insert / get         | Θ(1) avg     |
| LRU Cache            | get / put / evict    | Θ(1) worst   |
| AVL Tree             | insert / find        | Θ(log N)     |
| Shunting-Yard        | infix → postfix      | Θ(N)         |
| Postfix eval         | on a postfix stream  | Θ(N)         |
| Kruskal MST          | on E edges, V nodes  | Θ(E log E) sort + Θ(E α(V)) union-find |
| Buffer Pool          | page fetch on hit    | Θ(1)         |
| Buffer Pool          | page fetch on miss   | Θ(1) + disk  |

---

## Demo Coverage — Test Cases A–G

The `./nanodb` binary runs all seven rubric test cases in one pass:

| Test | What it demonstrates                                                           |
|------|--------------------------------------------------------------------------------|
| A    | Complex WHERE parse `(c_acctbal > 5000 AND c_mktsegment == "BUILDING") OR c_nationkey == 15` → postfix + filtered rows |
| B    | Index vs sequential scan timing: AVL `find` vs linear array scan, prints speed-up |
| C    | 3-table join (customer⋈orders⋈lineitem) — prints Kruskal MST path before executing |
| D    | LRU stress: 50-page pool + 5000 page touches — prints total evictions          |
| E    | Priority queue: 50 normal queries + 1 admin — admin runs first                 |
| F    | Deep nested expression `((o_totalprice*1.5) > 100000 AND (o_custkey%2==0)) OR (o_orderstatus!="O")` |
| G    | Persistence: inserts 5 new customers, saves to `data/`, reload with `--reload` |

---

## SQL Subset Supported

```sql
SELECT <col_list | *> FROM <tbl> [JOIN <tbl> ...] [WHERE <expr>]
INSERT INTO <tbl> VALUES (<val>, <val>, ...)
ADMIN <any of the above>           -- routed through priority queue, runs first
```

**Expressions**: `+ - * / %`, `== != < > <= >=`, `AND`, `OR`, parentheses.
Operator precedence handled by the Shunting-Yard algorithm.

---

## Sample Log Output

```
[LOG] Infix "c_acctbal > 5000 AND c_mktsegment == \"BUILDING\"" converted to Postfix "c_acctbal 5000 > c_mktsegment \"BUILDING\" == AND"
[LOG] Page 42 evicted via LRU, written to disk
[LOG] Multi-table join routed via MST: customer-orders -> customer-lineitem
[LOG] AVL index hit on customer.c_custkey=12345 in 2 hops
```

---

## Memory Hygiene

* Every `new` is paired with a `delete` in the owning destructor.
* `Row` owns its `DataType**` cells and deletes them in `~Row`.
* `Table` owns its `Row**` and deletes each row.
* `HashMap` chain nodes and `LRUNode` list entries are freed in their
  respective destructors.
* Run under Valgrind:

  ```bash
  make
  valgrind --leak-check=full ./nanodb 2>&1 | tee valgrind.log
  ```

---

## Academic Integrity Note

This codebase is my own implementation, written specifically for this course.
Every module can be explained on the spot during the viva: pointer layout,
balance-factor cases, hash collision chain walk, union-find path compression,
shunting-yard operator stack transitions.

