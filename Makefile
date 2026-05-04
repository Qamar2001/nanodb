# Makefile for NanoDB
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

# shared object files - everything except the two main()s
OBJS := logger.o data_types.o schema.o hash_map.o lru_cache.o pager.o \
        avl_tree.o graph.o parser.o executor.o stack.o queue.o tpch_loader.o

.PHONY: all clean run reload test benchmark

all: nanodb test_runner benchmark_runner

nanodb: $(OBJS) main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test_runner: $(OBJS) test_runner.o
	$(CXX) $(CXXFLAGS) -o $@ $^

benchmark_runner: logger.o data_types.o schema.o hash_map.o lru_cache.o pager.o avl_tree.o benchmark_runner.o
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: nanodb
	./nanodb

reload: nanodb
	./nanodb --reload

test: test_runner
	./test_runner

benchmark: benchmark_runner
	./benchmark_runner

clean:
	rm -f *.o nanodb test_runner benchmark_runner nanodb_execution.log
	rm -rf data

# header deps (basic)
logger.o: logger.cpp logger.h
data_types.o: data_types.cpp data_types.h
schema.o: schema.cpp schema.h data_types.h
hash_map.o: hash_map.cpp hash_map.h
lru_cache.o: lru_cache.cpp lru_cache.h hash_map.h
pager.o: pager.cpp pager.h lru_cache.h logger.h
avl_tree.o: avl_tree.cpp avl_tree.h
graph.o: graph.cpp graph.h
parser.o: parser.cpp parser.h stack.h logger.h data_types.h
executor.o: executor.cpp executor.h parser.h schema.h hash_map.h avl_tree.h stack.h graph.h logger.h
stack.o: stack.cpp stack.h
queue.o: queue.cpp queue.h
main.o: main.cpp
test_runner.o: test_runner.cpp
benchmark_runner.o: benchmark_runner.cpp
tpch_loader.o: tpch_loader.cpp tpch_loader.h data_types.h schema.h logger.h
