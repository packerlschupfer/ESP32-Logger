# Host tests for DeferredLog - no ESP32 needed
CXX ?= g++
CXXFLAGS = -std=gnu++17 -O1 -g -fsanitize=address,undefined -Wall -Wextra -Wformat=2 -pthread
INCLUDES = -Istub -I../../src

test_deferred_log: test_deferred_log.cpp ../../src/DeferredLog.cpp ../../src/DeferredLog.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

run: test_deferred_log
	./test_deferred_log

.DEFAULT_GOAL := run
.PHONY: run
