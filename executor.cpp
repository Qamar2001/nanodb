// executor.cpp
#include "executor.h"
#include "stack.h"
#include "graph.h"
#include "logger.h"
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

static void ensureDir(const std::string& d) {
#ifdef _WIN32
    _mkdir(d.c_str());
#else
    mkdir(d.c_str(), 0755);
#endif
}

// ---------------- helpers ----------------
// look up a column value in the join context. supports "colname" and
// "tblname.colname".
static DataType* lookupColumn(const std::string& name, const JoinContext& ctx) {
    // split on '.'
    std::string tbl, col;
    int dot = -1;
    for (size_t i = 0; i < name.size(); i++) if (name[i] == '.') { dot = (int)i; break; }
    if (dot >= 0) {
        tbl = name.substr(0, dot);
        col = name.substr(dot + 1);
    } else {
        col = name;
    }
    for (int i = 0; i < ctx.n; i++) {
        if (!tbl.empty() && ctx.tables[i]->name != tbl) continue;
        int ci = ctx.tables[i]->colIndex(col);
        if (ci >= 0) return ctx.rows[i]->cells[ci];
    }
    return nullptr;
}

// For postfix operand tokens: if it's a number/string literal, build a fresh
// DataType*. If it's an identifier, resolve against ctx (returns a CLONE so the
// stack always owns its values and we can safely delete).
static DataType* materialize(const Token& t, const JoinContext& ctx) {
    if (t.type == TK_NUMBER) {
        if (t.isFloat) return new FloatType(t.numVal);
        return new IntType((int)t.numVal);
    }
    if (t.type == TK_STRING) return new StringType(t.text);
    if (t.type == TK_IDENT) {
        DataType* v = lookupColumn(t.text, ctx);
        if (v) return v->clone();
        // unknown identifier - default to 0
        return new IntType(0);
    }
    return new IntType(0);
}

static bool toBool(DataType* v) {
    if (!v) return false;
    if (v->getType() == T_INT) return ((IntType*)v)->value != 0;
    if (v->getType() == T_FLOAT) return ((FloatType*)v)->value != 0.0;
    if (v->getType() == T_STRING) return !((StringType*)v)->value.empty();
    return false;
}

static void rebuildPrimaryIndex(TableEntry* e) {
    if (!e || !e->table) return;
    if (e->primaryIndex) {
        delete e->primaryIndex;
        e->primaryIndex = nullptr;
    }
    Table* t = e->table;
    if (!e->buildPrimaryIndex || t->numColumns == 0 || t->columns[0].type != T_INT)
        return;
    e->primaryIndex = new AVLTree();
    for (int i = 0; i < t->numRows; i++) {
        DataType* v = t->rows[i]->cells[0];
        if (v && v->getType() == T_INT)
            e->primaryIndex->insert(((IntType*)v)->value, t->rows[i]->rowId);
    }
    if (g_logger) {
        g_logger->log("AVL primary index rebuilt for " + t->name +
                      " with " + std::to_string(t->numRows) + " keys");
    }
}

DataType* evalPostfix(const TokenStream* post, const JoinContext& ctx) {
    Stack<DataType*> st(32);
    for (int i = 0; i < post->count; i++) {
        const Token& t = post->tokens[i];
        if (t.type == TK_NUMBER || t.type == TK_STRING || t.type == TK_IDENT) {
            st.push(materialize(t, ctx));
        } else if (t.type == TK_OP) {
            if (st.size() < 2) {
                // malformed - push default and continue
                st.push(new IntType(0));
                continue;
            }
            DataType* b = st.pop();
            DataType* a = st.pop();
            DataType* r = nullptr;

            const std::string& op = t.text;
            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
                double av = a->asNumber(), bv = b->asNumber();
                double rv = 0;
                if (op == "+") rv = av + bv;
                else if (op == "-") rv = av - bv;
                else if (op == "*") rv = av * bv;
                else if (op == "/") rv = (bv != 0.0) ? av / bv : 0.0;
                else if (op == "%") rv = (bv != 0.0) ? (double)((long long)av % (long long)bv) : 0.0;
                // keep int if both were int-like
                if (!a->isNumeric() || !b->isNumeric()) r = new IntType(0);
                else if (a->getType() == T_INT && b->getType() == T_INT && op != "/")
                    r = new IntType((int)rv);
                else r = new FloatType(rv);
            } else if (op == "==") {
                r = new IntType((*a == *b) ? 1 : 0);
            } else if (op == "!=") {
                r = new IntType((*a != *b) ? 1 : 0);
            } else if (op == "<")  { r = new IntType((*a < *b) ? 1 : 0); }
            else if (op == ">")  { r = new IntType((*a > *b) ? 1 : 0); }
            else if (op == "<=") { r = new IntType((*a <= *b) ? 1 : 0); }
            else if (op == ">=") { r = new IntType((*a >= *b) ? 1 : 0); }
            else if (op == "AND") { r = new IntType((toBool(a) && toBool(b)) ? 1 : 0); }
            else if (op == "OR")  { r = new IntType((toBool(a) || toBool(b)) ? 1 : 0); }
            else r = new IntType(0);

            delete a; delete b;
            st.push(r);
        }
    }
    if (st.empty()) return new IntType(1);    // empty where => always true
    DataType* top = st.pop();
    // cleanup any leftovers (shouldn't happen in well-formed expressions)
    while (!st.empty()) { DataType* x = st.pop(); delete x; }
    return top;
}

// ---------------- Executor ----------------
Executor::Executor() {
    catalog = new HashMap(32);
    entriesCap = 8; entriesCount = 0;
    entriesOwned = new TableEntry*[entriesCap];
    for (int i = 0; i < entriesCap; i++) entriesOwned[i] = nullptr;
}

Executor::~Executor() {
    for (int i = 0; i < entriesCount; i++) {
        if (entriesOwned[i]) {
            if (entriesOwned[i]->table) delete entriesOwned[i]->table;
            if (entriesOwned[i]->primaryIndex) delete entriesOwned[i]->primaryIndex;
            delete entriesOwned[i];
        }
    }
    delete[] entriesOwned;
    delete catalog;
}

void Executor::registerTable(Table* t, const std::string& diskPath, bool buildIndex) {
    TableEntry* e = new TableEntry();
    e->table = t;
    e->diskPath = diskPath;
    e->primaryIndex = nullptr;
    e->buildPrimaryIndex = buildIndex;

    rebuildPrimaryIndex(e);

    catalog->put(t->name, (void*)e);

    if (entriesCount == entriesCap) {
        int nc = entriesCap * 2;
        TableEntry** ne = new TableEntry*[nc];
        for (int i = 0; i < entriesCount; i++) ne[i] = entriesOwned[i];
        for (int i = entriesCount; i < nc; i++) ne[i] = nullptr;
        delete[] entriesOwned; entriesOwned = ne; entriesCap = nc;
    }
    entriesOwned[entriesCount++] = e;
}

Table* Executor::getTable(const std::string& name) const {
    TableEntry* e = (TableEntry*)catalog->get(name);
    return e ? e->table : nullptr;
}
TableEntry* Executor::getEntry(const std::string& name) const {
    return (TableEntry*)catalog->get(name);
}

void Executor::saveAll() {
    ensureDir("data");
    HashMap::Iter it = catalog->begin();
    while (catalog->valid(it)) {
        TableEntry* e = (TableEntry*)catalog->valueAt(it);
        if (e && e->table) e->table->saveToFile(e->diskPath);
        catalog->advance(it);
    }
}

void Executor::loadAll() {
    HashMap::Iter it = catalog->begin();
    while (catalog->valid(it)) {
        TableEntry* e = (TableEntry*)catalog->valueAt(it);
        if (e && e->table) {
            e->table->loadFromFile(e->diskPath);
            rebuildPrimaryIndex(e);
        }
        catalog->advance(it);
    }
}

// --- SELECT path ---
static bool isComparisonOp(const std::string& op) {
    return op == "==" || op == "!=" || op == "<" || op == ">" ||
           op == "<=" || op == ">=";
}

static bool isLiteralToken(const Token& t) {
    return t.type == TK_NUMBER || t.type == TK_STRING;
}

static DataType* literalToValue(const Token& t) {
    if (t.type == TK_STRING) return new StringType(t.text);
    if (t.type == TK_NUMBER) {
        if (t.isFloat) return new FloatType(t.numVal);
        return new IntType((int)t.numVal);
    }
    return new IntType(0);
}

static bool compareValues(DataType* left, const std::string& op, DataType* right) {
    if (!left || !right) return false;
    if (op == "==") return *left == *right;
    if (op == "!=") return *left != *right;
    if (op == "<")  return *left < *right;
    if (op == ">")  return *left > *right;
    if (op == "<=") return *left <= *right;
    if (op == ">=") return *left >= *right;
    return false;
}

static bool resolveColumnOnTable(Table* table, const std::string& name, int& colOut) {
    std::string tbl, col;
    int dot = -1;
    for (size_t i = 0; i < name.size(); i++) {
        if (name[i] == '.') { dot = (int)i; break; }
    }
    if (dot >= 0) {
        tbl = name.substr(0, dot);
        col = name.substr(dot + 1);
        if (tbl != table->name) return false;
    } else {
        col = name;
    }
    colOut = table->colIndex(col);
    return colOut >= 0;
}

struct SimpleConstraint {
    std::string column;
    std::string op;
    Token literal;
    bool literalOnLeft;
};

static bool postfixHasOr(const TokenStream* post) {
    if (!post) return false;
    for (int i = 0; i < post->count; i++) {
        if (post->tokens[i].type == TK_OP && post->tokens[i].text == "OR")
            return true;
    }
    return false;
}

static void pushConstraint(SimpleConstraint*& out, int& count, int& cap,
                           const std::string& column, const std::string& op,
                           const Token& literal, bool literalOnLeft) {
    if (count == cap) {
        int nc = cap ? cap * 2 : 8;
        SimpleConstraint* tmp = new SimpleConstraint[nc];
        for (int i = 0; i < count; i++) tmp[i] = out[i];
        delete[] out;
        out = tmp;
        cap = nc;
    }
    out[count].column = column;
    out[count].op = op;
    out[count].literal = literal;
    out[count].literalOnLeft = literalOnLeft;
    count++;
}

static SimpleConstraint* extractSimpleConstraints(const TokenStream* post, int& count) {
    count = 0;
    int cap = 0;
    SimpleConstraint* out = nullptr;
    if (!post || postfixHasOr(post)) return out;

    for (int i = 2; i < post->count; i++) {
        const Token& op = post->tokens[i];
        if (op.type != TK_OP || !isComparisonOp(op.text)) continue;

        const Token& a = post->tokens[i - 2];
        const Token& b = post->tokens[i - 1];
        if (a.type == TK_IDENT && isLiteralToken(b)) {
            pushConstraint(out, count, cap, a.text, op.text, b, false);
        } else if (isLiteralToken(a) && b.type == TK_IDENT) {
            pushConstraint(out, count, cap, b.text, op.text, a, true);
        }
    }
    return out;
}

static bool rowMatchesConstraints(Table* table, Row* row,
                                  SimpleConstraint* constraints, int constraintCount) {
    for (int i = 0; i < constraintCount; i++) {
        int ci = -1;
        if (!resolveColumnOnTable(table, constraints[i].column, ci)) continue;

        DataType* lit = literalToValue(constraints[i].literal);
        DataType* cell = row->cells[ci];
        bool ok = constraints[i].literalOnLeft
            ? compareValues(lit, constraints[i].op, cell)
            : compareValues(cell, constraints[i].op, lit);
        delete lit;
        if (!ok) return false;
    }
    return true;
}

struct CandidateList {
    Row** rows;
    int count;
    int capacity;
    bool owns;
};

static CandidateList buildCandidates(Table* table,
                                     SimpleConstraint* constraints,
                                     int constraintCount) {
    CandidateList list;
    list.rows = nullptr;
    list.count = 0;
    list.capacity = 0;
    list.owns = false;

    if (constraintCount == 0) {
        list.rows = table->rows;
        list.count = table->numRows;
        return list;
    }

    list.capacity = 64;
    list.rows = new Row*[list.capacity];
    list.owns = true;
    for (int i = 0; i < table->numRows; i++) {
        if (!rowMatchesConstraints(table, table->rows[i], constraints, constraintCount))
            continue;
        if (list.count == list.capacity) {
            int nc = list.capacity * 2;
            Row** tmp = new Row*[nc];
            for (int j = 0; j < list.count; j++) tmp[j] = list.rows[j];
            delete[] list.rows;
            list.rows = tmp;
            list.capacity = nc;
        }
        list.rows[list.count++] = table->rows[i];
    }
    return list;
}

static void freeCandidates(CandidateList& list) {
    if (list.owns) delete[] list.rows;
    list.rows = nullptr;
    list.count = 0;
    list.capacity = 0;
    list.owns = false;
}

static bool intColumnValue(Table* table, Row* row, const std::string& colName,
                           int& valueOut) {
    int ci = table->colIndex(colName);
    if (ci < 0) return false;
    DataType* v = row->cells[ci];
    if (!v || v->getType() != T_INT) return false;
    valueOut = ((IntType*)v)->value;
    return true;
}

static bool knownRelationshipOk(Table* a, Row* ar, Table* b, Row* br, bool& known) {
    known = true;
    int left = 0, right = 0;
    if (a->name == "customer" && b->name == "orders") {
        return intColumnValue(a, ar, "c_custkey", left) &&
               intColumnValue(b, br, "o_custkey", right) &&
               left == right;
    }
    if (a->name == "orders" && b->name == "customer") {
        return intColumnValue(a, ar, "o_custkey", left) &&
               intColumnValue(b, br, "c_custkey", right) &&
               left == right;
    }
    if (a->name == "orders" && b->name == "lineitem") {
        return intColumnValue(a, ar, "o_orderkey", left) &&
               intColumnValue(b, br, "l_orderkey", right) &&
               left == right;
    }
    if (a->name == "lineitem" && b->name == "orders") {
        return intColumnValue(a, ar, "l_orderkey", left) &&
               intColumnValue(b, br, "o_orderkey", right) &&
               left == right;
    }
    known = false;
    return true;
}

static bool relationshipsOk(Table** tbls, Row** rows, int nTbls) {
    for (int i = 0; i < nTbls; i++) {
        for (int j = i + 1; j < nTbls; j++) {
            bool known = false;
            bool ok = knownRelationshipOk(tbls[i], rows[i], tbls[j], rows[j], known);
            if (known && !ok) return false;
        }
    }
    return true;
}

static void printRowHeader(Table** tbls, int nTbls,
                           const std::string* projCols, int projCount) {
    bool starProj = (projCount == 1 && projCols[0] == "*");
    bool first = true;
    if (starProj) {
        for (int t = 0; t < nTbls; t++) {
            for (int c = 0; c < tbls[t]->numColumns; c++) {
                if (!first) std::cout << " | ";
                std::cout << tbls[t]->name << "." << tbls[t]->columns[c].name;
                first = false;
            }
        }
    } else {
        for (int p = 0; p < projCount; p++) {
            if (!first) std::cout << " | ";
            std::string colName = projCols[p];
            std::string tbl, col;
            int dot = -1;
            for (size_t k = 0; k < colName.size(); k++) {
                if (colName[k] == '.') { dot = (int)k; break; }
            }
            if (dot >= 0) {
                tbl = colName.substr(0, dot);
                col = colName.substr(dot + 1);
            } else {
                col = colName;
            }
            bool found = false;
            for (int t = 0; t < nTbls; t++) {
                if (!tbl.empty() && tbls[t]->name != tbl) continue;
                if (tbls[t]->colIndex(col) >= 0) {
                    std::cout << tbls[t]->name << "." << col;
                    found = true;
                    break;
                }
            }
            if (!found) std::cout << colName;
            first = false;
        }
    }
    std::cout << "\n";
}

static void printJoinedRow(Table** tbls, Row** rows, int n,
                           const std::string* projCols, int projCount) {
    // if "*" => print all columns
    bool starProj = (projCount == 1 && projCols[0] == "*");
    bool first = true;
    if (starProj) {
        for (int t = 0; t < n; t++) {
            for (int c = 0; c < tbls[t]->numColumns; c++) {
                if (!first) std::cout << " | ";
                std::cout << rows[t]->cells[c]->toString();
                first = false;
            }
        }
    } else {
        for (int p = 0; p < projCount; p++) {
            // resolve col across tables
            std::string colName = projCols[p];
            // allow "tbl.col"
            std::string tbl, col;
            int dot = -1;
            for (size_t k = 0; k < colName.size(); k++) if (colName[k] == '.') { dot = (int)k; break; }
            if (dot >= 0) { tbl = colName.substr(0, dot); col = colName.substr(dot + 1); }
            else          { col = colName; }
            bool found = false;
            for (int t = 0; t < n; t++) {
                if (!tbl.empty() && tbls[t]->name != tbl) continue;
                int ci = tbls[t]->colIndex(col);
                if (ci >= 0) {
                    if (!first) std::cout << " | ";
                    std::cout << rows[t]->cells[ci]->toString();
                    first = false; found = true; break;
                }
            }
            if (!found) {
                if (!first) std::cout << " | ";
                std::cout << "NULL";
                first = false;
            }
        }
    }
    std::cout << "\n";
}

// Filtered nested-loop join driver.
// Simple per-table predicates are pushed down before the cross product, and
// known TPC-H relationships are checked before evaluating the full WHERE.
static void runJoin(Executor* ex, Statement* stmt,
                    Table** tbls, int nTbls) {
    Row** currentRows = new Row*[nTbls];
    for (int i = 0; i < nTbls; i++) currentRows[i] = nullptr;

    int constraintCount = 0;
    SimpleConstraint* constraints =
        extractSimpleConstraints(stmt->wherePostfix, constraintCount);

    CandidateList* candidates = new CandidateList[nTbls];
    for (int i = 0; i < nTbls; i++)
        candidates[i] = buildCandidates(tbls[i], constraints, constraintCount);

    long long matched = 0, examined = 0;
    int* idx = new int[nTbls];
    for (int i = 0; i < nTbls; i++) idx[i] = 0;

    bool anyEmpty = false;
    for (int i = 0; i < nTbls; i++) {
        if (candidates[i].count == 0) { anyEmpty = true; break; }
    }

    if (g_logger) {
        std::string msg = "Join candidate pushdown:";
        for (int i = 0; i < nTbls; i++) {
            msg += " " + tbls[i]->name + "=" + std::to_string(candidates[i].count);
        }
        g_logger->log(msg);
    }

    if (!anyEmpty) {
        while (true) {
            for (int i = 0; i < nTbls; i++)
                currentRows[i] = candidates[i].rows[idx[i]];

            bool keep = relationshipsOk(tbls, currentRows, nTbls);
            JoinContext ctx; ctx.tables = tbls; ctx.rows = currentRows; ctx.n = nTbls;
            if (keep && stmt->wherePostfix) {
                DataType* r = evalPostfix(stmt->wherePostfix, ctx);
                keep = toBool(r);
                delete r;
            }
            examined++;
            if (keep) {
                matched++;
                printJoinedRow(tbls, currentRows, nTbls, stmt->selectCols, stmt->selectColCount);
            }
            // increment idx like an odometer
            int k = nTbls - 1;
            while (k >= 0) {
                idx[k]++;
                if (idx[k] < candidates[k].count) break;
                idx[k] = 0; k--;
            }
            if (k < 0) break;
        }
    }

    std::cout << "-- " << matched << " rows matched (" << examined << " examined)\n";
    delete[] idx;
    for (int i = 0; i < nTbls; i++) freeCandidates(candidates[i]);
    delete[] candidates;
    delete[] constraints;
    delete[] currentRows;
    (void)ex;
}

// --- INSERT path ---
static void runInsert(Executor* ex, Statement* stmt) {
    Table* t = ex->getTable(stmt->insertTable);
    if (!t) { std::cout << "ERROR: no such table '" << stmt->insertTable << "'\n"; return; }
    if (stmt->insertValCount != t->numColumns) {
        std::cout << "ERROR: column count mismatch for " << t->name
                  << " (expected " << t->numColumns << ", got " << stmt->insertValCount << ")\n";
        return;
    }
    Row* r = new Row(t->numColumns);
    for (int i = 0; i < t->numColumns; i++) {
        const std::string& s = stmt->insertVals[i];
        if (t->columns[i].type == T_INT) {
            r->set(i, new IntType(atoi(s.c_str())));
        } else if (t->columns[i].type == T_FLOAT) {
            r->set(i, new FloatType(atof(s.c_str())));
        } else {
            r->set(i, new StringType(s));
        }
    }
    t->insert(r);

    // update index if present
    TableEntry* e = ex->getEntry(stmt->insertTable);
    if (e && e->primaryIndex && t->numColumns > 0 && t->columns[0].type == T_INT) {
        DataType* v = r->cells[0];
        if (v && v->getType() == T_INT) e->primaryIndex->insert(((IntType*)v)->value, r->rowId);
    }
    std::cout << "-- inserted 1 row into " << t->name << "\n";
}

// --- top-level execute ---
void Executor::execute(Statement* stmt) {
    if (!stmt) return;
    if (stmt->kind == STMT_INSERT) { runInsert(this, stmt); return; }
    if (stmt->kind == STMT_HELP) {
        std::cout << "\n=== NanoDB Help & Statistics ===\n";
        std::cout << "Supported Syntax:\n";
        std::cout << "  - SELECT <cols|*> FROM <t1> [JOIN <t2>] [WHERE <expr>]\n";
        std::cout << "  - INSERT INTO <t1> VALUES (v1, v2, ...)\n";
        std::cout << "  - ADMIN INSERT ... (gives priority in queue)\n";
        std::cout << "  - HELP (this screen)\n\n";
        std::cout << "Database Catalog:\n";
        if (entriesCount == 0) {
            std::cout << "  (no tables registered)\n";
        } else {
            for (int i = 0; i < entriesCount; i++) {
                TableEntry* e = entriesOwned[i];
                std::cout << "  - " << e->table->name << ": " << e->table->numRows << " rows, "
                          << e->table->numColumns << " columns";
                if (e->primaryIndex) std::cout << " [AVL Index Active]";
                std::cout << "\n";
            }
        }
        std::cout << "=================================\n\n";
        return;
    }
    if (stmt->kind != STMT_SELECT) {
        std::cout << "-- unsupported statement kind\n";
        return;
    }

    // resolve tables
    int nT = stmt->fromTableCount;
    if (nT == 0) { std::cout << "-- SELECT without FROM is unsupported\n"; return; }

    Table** tbls = new Table*[nT];
    bool missing = false;
    for (int i = 0; i < nT; i++) {
        tbls[i] = getTable(stmt->fromTables[i]);
        if (!tbls[i]) {
            std::cout << "ERROR: no such table '" << stmt->fromTables[i] << "'\n";
            missing = true; break;
        }
    }
    if (missing) { delete[] tbls; return; }

    // for multi-table => build MST over tables to determine a cheap join path.
    // Edge weight = product of row counts (a toy cost proxy).
    if (nT > 1) {
        Graph g;
        for (int i = 0; i < nT; i++) g.addNode(tbls[i]->name);
        for (int i = 0; i < nT; i++) {
            for (int j = i + 1; j < nT; j++) {
                double w = (double)tbls[i]->numRows * (double)tbls[j]->numRows + 1.0;
                g.addEdge(tbls[i]->name, tbls[j]->name, w);
            }
        }
        Edge* mst = nullptr;
        int k = g.mstKruskal(mst);
        std::string path;
        for (int i = 0; i < k; i++) {
            if (!path.empty()) path += " -> ";
            path += g.nodeName(mst[i].u) + "-" + g.nodeName(mst[i].v);
        }
        if (g_logger) g_logger->log("Multi-table join routed via MST: " + path);
        std::cout << "-- join MST path: " << path << "\n";
        delete[] mst;
    }

    printRowHeader(tbls, nT, stmt->selectCols, stmt->selectColCount);
    runJoin(this, stmt, tbls, nT);
    delete[] tbls;
}
