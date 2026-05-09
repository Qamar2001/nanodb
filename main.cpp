/*
 * main.cpp - NanoDB Primary Entry Point
 * Implements the Interactive CLI Shell and delegates to the Executor.
 *
 * Features:
 * - Data generation fallbacks
 * - Real TPC-H loading
 * - Automated benchmark integration
 * - Priority-based query scheduling
 */
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
#include "logger.h"
#include "pager.h"
#include "parser.h"
#include "queue.h"
#include "schema.h"
#include "stack.h"
#include "tpch_loader.h"

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

  int minKey = 1;
  int maxKey = c->numRows;

  // Ask user for a valid target key at runtime
  int targetKey = -1;
  while (true) {
    std::cout << "  Enter target c_custkey to search [" << minKey << " - "
              << maxKey << "]: ";
    std::string input;
    std::getline(std::cin, input);
    try {
      int val = std::stoi(input);
      if (val >= minKey && val <= maxKey) {
        targetKey = val;
        break;
      } else {
        std::cout << "  [Error] Value out of range. Valid range is " << minKey
                  << " to " << maxKey << ".\n";
        std::cout << "  Examples: " << minKey << ", " << maxKey / 2 << ", "
                  << maxKey << "\n";
      }
    } catch (...) {
      std::cout << "  [Error] Not a valid integer. Please enter a number.\n";
      std::cout << "  Examples: " << minKey << ", " << maxKey / 2 << ", "
                << maxKey << "\n";
    }
  }

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
  std::cout << "  AVL index     : "
            << (idxNs > 0 ? std::to_string(idxNs) + " ns" : "< 1 ns")
            << "  (rowId=" << foundIdx << ")\n";
  if (idxNs > 0)
    std::cout << "  speedup       : " << (double)seqNs / (double)idxNs << "x\n";
  else if (seqNs > 0)
    std::cout << "  speedup       : >" << seqNs
              << "x  (AVL lookup sub-nanosecond)\n";
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
  initLogger();
  g_logger->section("NanoDB starting");

  Executor *ex = new Executor();
  int customerN = 100000, ordersN = 150000, lineitemN = 300000;

  bool running = true;
  bool dataLoaded = false;

  std::cout << "===========================================\n";
  std::cout << "        NanoDB Interactive Shell\n";
  std::cout << "===========================================\n";

  while (running) {
    std::cout << "\n--- Main Menu ---\n";
    std::cout << "1. Load TPC-H Dataset (100k records)\n";
    std::cout << "2. Load Dummy Data (Fallback)\n";
    std::cout << "3. Execute Test Suite (queries.txt)\n";
    std::cout << "4. Run Benchmarks (Sequential vs AVL)\n";
    std::cout << "5. Run LRU Cache Stress Test\n";
    std::cout << "6. Run Priority Queue Test\n";
    std::cout << "7. Interactive SQL Shell\n";
    std::cout << "8. Exit\n";
    std::cout << "Select an option: ";

    std::string choice;
    if (!std::getline(std::cin, choice))
      break;

    if (choice == "1") {
      if (!dataLoaded) {
        bool loaded =
            loadTpchData(ex, "../Datset TPL-H", customerN, ordersN, lineitemN);
        if (loaded) {
          std::cout << "-- Successfully loaded real TPC-H dataset.\n";
          dataLoaded = true;
        } else {
          std::cout << "-- [Error] Could not load TPC-H dataset from "
                       "'../Datset TPL-H'.\n";
        }
      } else {
        std::cout << "-- Data already loaded.\n";
      }
    } else if (choice == "2") {
      if (!dataLoaded) {
        buildCustomer(ex, customerN);
        buildOrders(ex, ordersN);
        buildLineitem(ex, lineitemN);
        std::cout << "-- Loaded dummy data.\n";
        dataLoaded = true;
      } else {
        std::cout << "-- Data already loaded.\n";
      }
    } else if (choice == "3") {
      if (!dataLoaded)
        std::cout << "-- Please load data first (Option 1 or 2).\n";
      else
        runWorkload(ex, "queries.txt");
    } else if (choice == "4") {
      if (!dataLoaded)
        std::cout << "-- Please load data first (Option 1 or 2).\n";
      else
        demoIndexedVsScan(ex);
    } else if (choice == "5") {
      demoLruStress();
    } else if (choice == "6") {
      if (!dataLoaded)
        std::cout << "-- Please load data first (Option 1 or 2).\n";
      else
        demoPriorityQueue(ex);
    } else if (choice == "7") {
      if (!dataLoaded) {
        std::cout << "-- Warning: No data loaded. Some queries may fail.\n";
      }
      std::cout << "\n--- Interactive SQL Shell ---\n";
      std::cout << "Type 'EXIT' or 'QUIT' to return to menu.\n";
      while (true) {
        std::cout << "SQL> ";
        std::string query;
        if (!std::getline(std::cin, query))
          break;
        if (query.empty())
          continue;

        std::string upperQ = query;
        for (char &c : upperQ)
          c = toupper((unsigned char)c);
        if (upperQ == "EXIT" || upperQ == "QUIT")
          break;

        Statement *s = parseStatement(query);
        ex->execute(s);
        delete s;
      }
    } else if (choice == "8") {
      running = false;
    } else {
      std::cout << "-- Invalid option. Try again.\n";
    }
  }

  std::cout << "\nSaving all tables to disk before exit...\n";
  ex->saveAll();

  delete ex;
  g_logger->section("NanoDB shutdown");
  shutdownLogger();
  return 0;
}
