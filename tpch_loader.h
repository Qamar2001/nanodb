#ifndef TPCH_LOADER_H
#define TPCH_LOADER_H

#include "executor.h"
#include <string>

// Loads TPC-H data from the given directory.
// Returns true if successful, false if files could not be opened.
bool loadTpchData(Executor* ex, const std::string& dirPath, int numCustomers, int numOrders, int numLineitems);

#endif
