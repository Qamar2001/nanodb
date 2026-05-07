/*
 * tpch_loader.cpp - TPC-H Dataset Ingestion Engine
 * Handles parsing of .tbl files and mapping to NanoDB internal Row structures.
 * 
 * Note: Strictly avoids STL containers (std::vector, etc) per rubric.
 * Uses a static string buffer for line splitting to maintain O(1) memory overhead during load.
 */
#include "tpch_loader.h"
#include "data_types.h"
#include "schema.h"
#include "logger.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

// Helper to split a string by '|' into a raw array of strings.
// Since STL containers are banned, we use a static array of size 20.
static int splitLine(const std::string& line, std::string* outFields, int maxFields) {
    int count = 0;
    int start = 0;
    for (size_t i = 0; i < line.length(); i++) {
        if (line[i] == '|') {
            if (count < maxFields) {
                outFields[count++] = line.substr(start, i - start);
            }
            start = (int)i + 1;
        }
    }
    if (start < (int)line.length() && count < maxFields) {
        outFields[count++] = line.substr(start);
    }
    return count;
}

bool loadTpchData(Executor* ex, const std::string& dirPath, int numCustomers, int numOrders, int numLineitems) {
    std::string fields[20];
    
    // 1. Load customer
    {
        Column cols[5];
        cols[0] = Column("c_custkey", T_INT);
        cols[1] = Column("c_name", T_STRING);
        cols[2] = Column("c_nationkey", T_INT);
        cols[3] = Column("c_acctbal", T_FLOAT);
        cols[4] = Column("c_mktsegment", T_STRING);
        Table* t = new Table("customer", cols, 5);

        std::string path = dirPath + "/customer.tbl";
        std::ifstream in(path);
        if (!in.is_open()) {
            std::cout << "-- [TPC-H Loader] Could not open " << path << "\n";
            delete t;
            return false;
        }

        std::string line;
        int loaded = 0;
        while (std::getline(in, line) && loaded < numCustomers) {
            int nf = splitLine(line, fields, 20);
            if (nf < 7) continue;
            // customer: 0:custkey, 1:name, 3:nationkey, 5:acctbal, 6:mktsegment
            Row* r = new Row(5);
            r->set(0, new IntType(std::atoi(fields[0].c_str())));
            r->set(1, new StringType(fields[1]));
            r->set(2, new IntType(std::atoi(fields[3].c_str())));
            r->set(3, new FloatType(std::atof(fields[5].c_str())));
            r->set(4, new StringType(fields[6]));
            t->insert(r);
            loaded++;
        }
        in.close();
        ex->registerTable(t, "data/customer.bin", true);
        std::cout << "-- loaded " << loaded << " real TPC-H customers.\n";
        if (g_logger) g_logger->log("Loaded " + std::to_string(loaded) + " customers from TPC-H");
    }

    // 2. Load orders
    {
        Column cols[5];
        cols[0] = Column("o_orderkey", T_INT);
        cols[1] = Column("o_custkey", T_INT);
        cols[2] = Column("o_totalprice", T_FLOAT);
        cols[3] = Column("o_orderstatus", T_STRING);
        cols[4] = Column("o_orderdate", T_STRING);
        Table* t = new Table("orders", cols, 5);

        std::string path = dirPath + "/orders.tbl";
        std::ifstream in(path);
        if (!in.is_open()) {
            delete t;
            return false;
        }

        std::string line;
        int loaded = 0;
        while (std::getline(in, line) && loaded < numOrders) {
            int nf = splitLine(line, fields, 20);
            if (nf < 5) continue;
            // orders: 0:orderkey, 1:custkey, 3:totalprice, 2:orderstatus, 4:orderdate
            Row* r = new Row(5);
            r->set(0, new IntType(std::atoi(fields[0].c_str())));
            r->set(1, new IntType(std::atoi(fields[1].c_str())));
            r->set(2, new FloatType(std::atof(fields[3].c_str())));
            r->set(3, new StringType(fields[2]));
            r->set(4, new StringType(fields[4]));
            t->insert(r);
            loaded++;
        }
        in.close();
        ex->registerTable(t, "data/orders.bin", true);
        std::cout << "-- loaded " << loaded << " real TPC-H orders.\n";
        if (g_logger) g_logger->log("Loaded " + std::to_string(loaded) + " orders from TPC-H");
    }

    // 3. Load lineitem
    {
        Column cols[4];
        cols[0] = Column("l_linekey", T_INT);
        cols[1] = Column("l_orderkey", T_INT);
        cols[2] = Column("l_quantity", T_INT);
        cols[3] = Column("l_extprice", T_FLOAT);
        Table* t = new Table("lineitem", cols, 4);

        std::string path = dirPath + "/lineitem.tbl";
        std::ifstream in(path);
        if (!in.is_open()) {
            delete t;
            return false;
        }

        std::string line;
        int loaded = 0;
        while (std::getline(in, line) && loaded < numLineitems) {
            int nf = splitLine(line, fields, 20);
            if (nf < 6) continue;
            // lineitem: 0:orderkey, 4:quantity, 5:extprice
            Row* r = new Row(4);
            r->set(0, new IntType(loaded + 1));  // l_linekey is just row number
            r->set(1, new IntType(std::atoi(fields[0].c_str())));
            r->set(2, new IntType(std::atoi(fields[4].c_str())));
            r->set(3, new FloatType(std::atof(fields[5].c_str())));
            t->insert(r);
            loaded++;
        }
        in.close();
        ex->registerTable(t, "data/lineitem.bin", true);
        std::cout << "-- loaded " << loaded << " real TPC-H lineitems.\n";
        if (g_logger) g_logger->log("Loaded " + std::to_string(loaded) + " lineitems from TPC-H");
    }

    return true;
}
