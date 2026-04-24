// schema.cpp
#include "schema.h"
#include <cstdint>

// ------------- Row -------------
Row::Row(int n) : numCells(n), rowId(-1) {
    cells = new DataType*[n];
    for (int i = 0; i < n; i++) cells[i] = nullptr;
}

Row::~Row() {
    for (int i = 0; i < numCells; i++) {
        if (cells[i]) delete cells[i];
    }
    delete[] cells;
}

void Row::set(int i, DataType* v) {
    if (i < 0 || i >= numCells) { if (v) delete v; return; }
    if (cells[i]) delete cells[i];
    cells[i] = v;
}

void Row::serialize(std::ofstream& out) const {
    out.write((char*)&rowId, sizeof(int));
    out.write((char*)&numCells, sizeof(int));
    for (int i = 0; i < numCells; i++) {
        uint8_t present = cells[i] ? 1 : 0;
        out.write((char*)&present, 1);
        if (cells[i]) cells[i]->serialize(out);
    }
}

Row* Row::deserialize(std::ifstream& in) {
    int rid = -1, nc = 0;
    in.read((char*)&rid, sizeof(int));
    in.read((char*)&nc, sizeof(int));
    if (!in || nc < 0 || nc > 1024) return nullptr;
    Row* r = new Row(nc);
    r->rowId = rid;
    for (int i = 0; i < nc; i++) {
        uint8_t present = 0;
        in.read((char*)&present, 1);
        if (present) {
            DataType* d = DataType::deserialize(in);
            r->cells[i] = d;
        }
    }
    return r;
}

// ------------- Table -------------
Table::Table(const std::string& nm, Column* cols, int nCols)
    : name(nm), numColumns(nCols), numRows(0), rowsCapacity(64), nextRowId(0) {
    columns = new Column[nCols];
    for (int i = 0; i < nCols; i++) columns[i] = cols[i];
    rows = new Row*[rowsCapacity];
    for (int i = 0; i < rowsCapacity; i++) rows[i] = nullptr;
}

Table::~Table() {
    for (int i = 0; i < numRows; i++) {
        if (rows[i]) delete rows[i];
    }
    delete[] rows;
    delete[] columns;
}

int Table::colIndex(const std::string& colName) const {
    for (int i = 0; i < numColumns; i++) {
        if (columns[i].name == colName) return i;
    }
    return -1;
}

void Table::insert(Row* r) {
    if (numRows == rowsCapacity) {
        int nc = rowsCapacity * 2;
        Row** nr = new Row*[nc];
        for (int i = 0; i < numRows; i++) nr[i] = rows[i];
        for (int i = numRows; i < nc; i++) nr[i] = nullptr;
        delete[] rows;
        rows = nr;
        rowsCapacity = nc;
    }
    r->rowId = nextRowId++;
    rows[numRows++] = r;
}

// simple binary format:
// [name length u32][name bytes]
// [numColumns i32]
// foreach column: [name len u32][name bytes][type u8]
// [nextRowId i32][numRows i32]
// foreach row: serialize row
void Table::saveToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return;
    uint32_t nl = (uint32_t)name.size();
    out.write((char*)&nl, sizeof(uint32_t));
    out.write(name.data(), nl);
    out.write((char*)&numColumns, sizeof(int));
    for (int i = 0; i < numColumns; i++) {
        uint32_t cl = (uint32_t)columns[i].name.size();
        out.write((char*)&cl, sizeof(uint32_t));
        out.write(columns[i].name.data(), cl);
        uint8_t tt = (uint8_t)columns[i].type;
        out.write((char*)&tt, 1);
    }
    out.write((char*)&nextRowId, sizeof(int));
    out.write((char*)&numRows, sizeof(int));
    for (int i = 0; i < numRows; i++) rows[i]->serialize(out);
    out.close();
}

void Table::loadFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return;
    uint32_t nl = 0;
    in.read((char*)&nl, sizeof(uint32_t));
    if (!in || nl > 4096) return;
    std::string fileName; fileName.resize(nl);
    if (nl) in.read(&fileName[0], nl);
    int nCols = 0;
    in.read((char*)&nCols, sizeof(int));
    if (!in || nCols < 0 || nCols > 1024) return;
    // we don't override schema - just skip the column metadata to seek past it
    for (int i = 0; i < nCols; i++) {
        uint32_t cl = 0;
        in.read((char*)&cl, sizeof(uint32_t));
        std::string cname; if (cl) { cname.resize(cl); in.read(&cname[0], cl); }
        uint8_t tt = 0; in.read((char*)&tt, 1);
    }
    int nextId = 0, nRows = 0;
    in.read((char*)&nextId, sizeof(int));
    in.read((char*)&nRows, sizeof(int));
    // clear existing rows before appending
    for (int i = 0; i < numRows; i++) { delete rows[i]; rows[i] = nullptr; }
    numRows = 0;
    nextRowId = 0;
    for (int i = 0; i < nRows; i++) {
        Row* r = Row::deserialize(in);
        if (!r) break;
        if (numRows == rowsCapacity) {
            int nc2 = rowsCapacity * 2;
            Row** nr = new Row*[nc2];
            for (int j = 0; j < numRows; j++) nr[j] = rows[j];
            for (int j = numRows; j < nc2; j++) nr[j] = nullptr;
            delete[] rows; rows = nr; rowsCapacity = nc2;
        }
        rows[numRows++] = r;
    }
    nextRowId = nextId;
    in.close();
}
