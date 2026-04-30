// benchmark_runner.cpp - reproducible empirical measurements for the report.
#include <chrono>
#include <iostream>

#include "avl_tree.h"
#include "data_types.h"
#include "pager.h"
#include "schema.h"

static long long nsSince(std::chrono::high_resolution_clock::time_point start,
                         std::chrono::high_resolution_clock::time_point end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

static void runIndexBenchmark(int n) {
    Column cols[2];
    cols[0] = Column("id", T_INT);
    cols[1] = Column("balance", T_FLOAT);

    auto insertStart = std::chrono::high_resolution_clock::now();
    Table* table = new Table("bench", cols, 2);
    AVLTree* index = new AVLTree();
    for (int i = 1; i <= n; i++) {
        Row* row = new Row(2);
        row->set(0, new IntType(i));
        row->set(1, new FloatType((double)(i % 10000)));
        table->insert(row);
        index->insert(i, row->rowId);
    }
    auto insertEnd = std::chrono::high_resolution_clock::now();

    int target = n;
    int seqRow = -1;
    auto seqStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < table->numRows; i++) {
        DataType* v = table->rows[i]->cells[0];
        if (v && v->getType() == T_INT && ((IntType*)v)->value == target) {
            seqRow = table->rows[i]->rowId;
            break;
        }
    }
    auto seqEnd = std::chrono::high_resolution_clock::now();

    auto avlStart = std::chrono::high_resolution_clock::now();
    int avlRow = index->find(target);
    auto avlEnd = std::chrono::high_resolution_clock::now();

    std::cout << n << ","
              << nsSince(insertStart, insertEnd) << ","
              << nsSince(seqStart, seqEnd) << ","
              << nsSince(avlStart, avlEnd) << ","
              << seqRow << ","
              << avlRow << "\n";

    delete index;
    delete table;
}

static void runLruBenchmark(int touches) {
    Pager pager(50, 128, "data/benchmark_pages");
    for (int i = 0; i < touches; i++) {
        Page* page = pager.fetchPage(i);
        page->data[0] = (unsigned char)(i & 0xff);
        pager.markDirty(i);
    }
    std::cout << touches << "," << pager.evictions() << "\n";
}

int main() {
    std::cout << "records,insert_ns,sequential_scan_ns,avl_find_ns,seq_row,avl_row\n";
    runIndexBenchmark(1000);
    runIndexBenchmark(10000);
    runIndexBenchmark(100000);

    std::cout << "\npage_touches,lru_evictions\n";
    runLruBenchmark(1000);
    runLruBenchmark(5000);
    runLruBenchmark(10000);
    return 0;
}
