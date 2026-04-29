// executor.h - takes a Statement, runs it against the system catalog and tables.
// Evaluates WHERE postfix using our custom Stack<DataType*>.
// Performs multi-table joins using MST-ordered nested-loop.
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"
#include "schema.h"
#include "hash_map.h"
#include "avl_tree.h"

// Catalog entry per table
struct TableEntry {
    Table* table;
    std::string diskPath;
    AVLTree* primaryIndex;   // index on first column (assumed integer key)
    bool buildPrimaryIndex;
};

class Executor {
    HashMap* catalog;        // tableName -> TableEntry*
    // tiny inline array of owned entries we allocate - for cleanup
    TableEntry** entriesOwned;
    int entriesCount;
    int entriesCap;

public:
    Executor();
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    // register an existing table with the executor; executor takes ownership of
    // Table* and will delete it on destruction.
    void registerTable(Table* t, const std::string& diskPath, bool buildIndex);

    Table* getTable(const std::string& name) const;
    TableEntry* getEntry(const std::string& name) const;

    // run one statement. writes human-readable result to stdout.
    void execute(Statement* stmt);

    // persistence
    void saveAll();
    void loadAll();
};

// Evaluate a postfix WHERE expression against a row context where column name
// -> DataType* lookups come from a join-row mapping.
// joinRow: array of (Table*, Row*) pairs defining qualified-column scope.
class JoinContext {
public:
    Table** tables;          // array, not owned
    Row**   rows;            // array, not owned
    int     n;
    JoinContext() : tables(nullptr), rows(nullptr), n(0) {}
};

// returns a freshly-allocated DataType* (caller deletes) holding the value of
// the postfix expression. For boolean results, returns IntType(0/1).
DataType* evalPostfix(const TokenStream* post, const JoinContext& ctx);

#endif
