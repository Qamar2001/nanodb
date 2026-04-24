// main.cpp - NanoDB driver.
// Sets up customer/orders/lineitem schemas, generates sample data (since we
// don't have the real TPC-H files), runs a workload file via the Executor,
// exercises the priority queue, and demonstrates the test cases A-G.
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "avl_tree.h"
#include "data_types.h"
#include "executor.h"
#include "graph.h"
#include "tpch_loader.h"
#include "logger.h"
#include "pager.h"
#include "parser.h"
#include "queue.h"
#include "schema.h"
#include "stack.h"

// ----------- TPC-H sample data generation (light) -----------
static void buildCustomer(Executor *ex, int n) {
  Column cols[4];
  cols[0] = Column("c_custkey", T_INT);
  cols[1] = Column("c_name", T_STRING);
  cols[2] = Column("c_nationkey", T_INT);
  cols[3] = Column("c_acctbal", T_FLOAT);
  // add c_mktsegment too - test case A needs it
  Column cols5[5];
  for (int i = 0; i < 4; i++)
    cols5[i] = cols[i];
  cols5[4] = Column("c_mktsegment", T_STRING);

  Table *t = new Table("customer", cols5, 5);
  const char *segs[5] = {"BUILDING", "AUTOMOBILE", "MACHINERY", "HOUSEHOLD",
                         "FURNITURE"};
  for (int i = 0; i < n; i++) {
    Row *r = new Row(5);
    r->set(0, new IntType(i + 1));
    r->set(1, new StringType(std::string("Customer#") + std::to_string(i + 1)));
    r->set(2, new IntType(i % 25));
    r->set(3, new FloatType(1000.0 + (double)((i * 37) % 9000)));
    r->set(4, new StringType(segs[i % 5]));
    t->insert(r);
  }
  ex->registerTable(t, "data/customer.bin", true);
}

static void buildOrders(Executor *ex, int n) {
  Column cols[5];
  cols[0] = Column("o_orderkey", T_INT);
  cols[1] = Column("o_custkey", T_INT);
  cols[2] = Column("o_totalprice", T_FLOAT);
  cols[3] = Column("o_orderstatus", T_STRING);
  cols[4] = Column("o_orderdate", T_STRING);
  Table *t = new Table("orders", cols, 5);
  const char *st[3] = {"O", "F", "P"};
  for (int i = 0; i < n; i++) {
    Row *r = new Row(5);
    r->set(0, new IntType(i + 1));
    r->set(1, new IntType((i % 10) + 1)); // link to customers 1..10
    r->set(2, new FloatType(500.0 + (double)((i * 71) % 200000)));
    r->set(3, new StringType(st[i % 3]));
    r->set(4, new StringType("1996-01-01"));
    t->insert(r);
  }
  ex->registerTable(t, "data/orders.bin", true);
}

static void buildLineitem(Executor *ex, int n) {
  Column cols[4];
  cols[0] = Column("l_linekey", T_INT);
  cols[1] = Column("l_orderkey", T_INT);
  cols[2] = Column("l_quantity", T_INT);
  cols[3] = Column("l_extprice", T_FLOAT);
  Table *t = new Table("lineitem", cols, 4);
  for (int i = 0; i < n; i++) {
    Row *r = new Row(4);
    r->set(0, new IntType(i + 1));
    r->set(1, new IntType((i % 20) + 1));
    r->set(2, new IntType((i % 50) + 1));
    r->set(3, new FloatType(100.0 + (double)((i * 19) % 5000)));
    t->insert(r);
  }
  ex->registerTable(t, "data/lineitem.bin", true);
}

// ----------- Demo: indexed vs sequential scan timing (Test Case B) -----------
static void demoIndexedVsScan(Executor *ex) {
  std::cout << "\n=== Test Case B: Indexed vs Sequential Scan ===\n";
  Table *c = ex->getTable("customer");
  TableEntry *ce = ex->getEntry("customer");
  if (!c || !ce)
    return;

  int targetKey = c->numRows / 2; // something that exists

  // Sequential scan
  auto t1 = std::chrono::high_resolution_clock::now();
  int foundSeq = -1;
  for (int i = 0; i < c->numRows; i++) {
    DataType *v = c->rows[i]->cells[0];
    if (v && v->getType() == T_INT && ((IntType *)v)->value == targetKey) {
      foundSeq = c->rows[i]->rowId;
      break;
    }
  }
  auto t2 = std::chrono::high_resolution_clock::now();
  long long seqNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

  // AVL index lookup
  int foundIdx = -1;
  auto t3 = std::chrono::high_resolution_clock::now();
  if (ce->primaryIndex)
    foundIdx = ce->primaryIndex->find(targetKey);
  auto t4 = std::chrono::high_resolution_clock::now();
  long long idxNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(t4 - t3).count();

  std::cout << "  target c_custkey = " << targetKey << "\n";
  std::cout << "  sequential scan: " << seqNs << " ns  (rowId=" << foundSeq
            << ")\n";
  std::cout << "  AVL index     : " << idxNs << " ns  (rowId=" << foundIdx
            << ")\n";
  if (idxNs > 0)
    std::cout << "  speedup       : " << (double)seqNs / (double)idxNs << "x\n";
  if (g_logger) {
    g_logger->log("Sequential scan for c_custkey=" + std::to_string(targetKey) +
                  " took " + std::to_string(seqNs) + " ns");
    g_logger->log(
        "AVL indexed find for c_custkey=" + std::to_string(targetKey) +
        " took " + std::to_string(idxNs) + " ns");
  }
}

// ----------- Demo: memory stress / LRU eviction (Test Case D) -----------
static void demoLruStress() {
  std::cout << "\n=== Test Case D: LRU Memory Stress ===\n";
  // pool of 50 pages, each 128 bytes
  Pager pager(50, 128, "data/pages");
  // touch 5000 different "lineitem" pages - forces many evictions
  for (int i = 0; i < 5000; i++) {
    Page *p = pager.fetchPage(i);
    // simulate some write
    p->data[0] = (unsigned char)(i & 0xff);
    pager.markDirty(i);
  }
  std::cout << "  total LRU evictions: " << pager.evictions() << "\n";
  if (g_logger)
    g_logger->log("LRU stress: total evictions = " +
                  std::to_string(pager.evictions()));
}

// ----------- Demo: Priority Queue (Test Case E) -----------
static void demoPriorityQueue(Executor *ex) {
  std::cout << "\n=== Test Case E: Priority Queue Ordering ===\n";
  PriorityQueue<std::string> pq(128);
  // 50 normal queries
  for (int i = 0; i < 50; i++) {
    pq.push(1, "SELECT * FROM customer WHERE c_custkey == " +
                   std::to_string(i + 1));
  }
  // one admin query, submitted last but higher priority
  pq.push(10, "ADMIN INSERT INTO customer VALUES (99999, \"AdminPatch\", 1, "
              "9999.99, \"BUILDING\")");

  // execute in priority order
  int count = 0;
  while (!pq.empty()) {
    std::string q = pq.pop();
    count++;
    if (count <= 3 || count == 51) {
      std::cout << "  [" << count << "] executing: " << q.substr(0, 70) << "\n";
    }
    Statement *s = parseStatement(q);
    if (s->isAdmin && g_logger)
      g_logger->log("Admin query intercepted before background reads: " + q);
    ex->execute(s);
    delete s;
  }
  std::cout << "  (admin query ran first despite being enqueued last)\n";
}

// ----------- Run queries.txt -----------
static void runWorkload(Executor *ex, const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    std::cout << "-- no " << path << " found; skipping workload\n";
    return;
  }
  if (g_logger)
    g_logger->section("Workload: " + path);
  std::cout << "\n=== Running workload from " << path << " ===\n";
  std::string line;
  int idx = 0;
  while (std::getline(in, line)) {
    // trim
    while (!line.empty() &&
           (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    idx++;
    std::cout << "\n> Q" << idx << ": " << line << "\n";
    if (g_logger)
      g_logger->log("Parsing query #" + std::to_string(idx) + ": " + line);
    Statement *s = parseStatement(line);
    ex->execute(s);
    delete s;
  }
  in.close();
}

// ----------- main -----------
int main(int argc, char **argv) {
  bool reloadOnly = (argc > 1 && std::strcmp(argv[1], "--reload") == 0);

  initLogger();
  g_logger->section("NanoDB starting");

  Executor *ex = new Executor();

  // Build schemas (always) - then either generate fresh data or load from disk.
  // Defaults are set to 100,000 records total as per requirements
  int customerN = 20000, ordersN = 30000, lineitemN = 50000;
  const char *scaleEnv = std::getenv("NANODB_SCALE");
  if (scaleEnv) {
    int s = std::atoi(scaleEnv);
    if (s > 0) {
      customerN = s / 5;
      ordersN = s * 3 / 10;
      lineitemN = s / 2;
    }
  }

  if (reloadOnly) {
    std::cout << "-- reload mode: loading tables from disk\n";
    buildCustomer(ex, 0);
    buildOrders(ex, 0);
    buildLineitem(ex, 0);
    ex->loadAll();
  } else {
    // Attempt to load real TPC-H data from the expected directory
    bool loaded = loadTpchData(ex, "../Datset TPL-H", customerN, ordersN, lineitemN);
    if (!loaded) {
      std::cout << "-- [Warning] Could not load TPC-H dataset. Falling back to dummy data.\n";
      buildCustomer(ex, customerN);
      buildOrders(ex, ordersN);
      buildLineitem(ex, lineitemN);
    }
  }

  std::cout << "-- loaded customer(" << ex->getTable("customer")->numRows
            << "), orders(" << ex->getTable("orders")->numRows << "), lineitem("
            << ex->getTable("lineitem")->numRows << ")\n";

  // Run the workload
  runWorkload(ex, "queries.txt");

  // Explicit test cases B, D, E (A, C, F, G are covered in queries.txt +
  // persistence)
  demoIndexedVsScan(ex);
  demoLruStress();
  demoPriorityQueue(ex);

  // persistence test (Test Case G)
  std::cout << "\n=== Test Case G: Persistence ===\n";
  // Insert 5 new customers then save to disk
  for (int i = 0; i < 5; i++) {
    std::string q = "INSERT INTO customer VALUES (" +
                    std::to_string(50000 + i) + ", \"NewCust" +
                    std::to_string(i) + "\", 7, 8888.88, \"BUILDING\")";
    Statement *s = parseStatement(q);
    ex->execute(s);
    delete s;
  }
  ex->saveAll();
  std::cout << "  5 new customers inserted and all tables saved to disk.\n";
  std::cout << "  Re-run with: ./nanodb --reload  to verify they come back.\n";

  delete ex;
  g_logger->section("NanoDB shutdown");
  shutdownLogger();
  return 0;
}
