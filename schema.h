// schema.h - Column/Row/Table definitions that sit on top of DataType.
#ifndef SCHEMA_H
#define SCHEMA_H

#include <string>
#include <fstream>
#include "data_types.h"

// a row is just a raw array of DataType*. we own them.
class Row {
public:
    DataType** cells;     // array of pointers
    int numCells;
    int rowId;            // unique per-table row id, set by Table::insert

    Row(int n);
    ~Row();
    Row(const Row&) = delete;
    Row& operator=(const Row&) = delete;

    // set cell - takes ownership of v
    void set(int i, DataType* v);
    DataType* get(int i) const { return cells[i]; }

    void serialize(std::ofstream& out) const;
    static Row* deserialize(std::ifstream& in);
};

class Column {
public:
    std::string name;
    TypeTag type;
    Column() : type(T_INT) {}
    Column(const std::string& n, TypeTag t) : name(n), type(t) {}
};

// Table - schema + raw growable array of Row*.
// No STL used; we manage a Row** array ourselves.
class Table {
public:
    std::string name;
    Column* columns;
    int numColumns;

    Row** rows;           // array of row pointers
    int numRows;
    int rowsCapacity;

    int nextRowId;        // auto-incrementing

    Table(const std::string& nm, Column* cols, int nCols);
    ~Table();

    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    int colIndex(const std::string& colName) const;  // -1 if not found
    void insert(Row* r);                              // takes ownership

    // binary persistence
    void saveToFile(const std::string& path) const;
    void loadFromFile(const std::string& path);
};

#endif
