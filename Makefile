CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude
DECISIONS ?= src
BUILD ?= build
COMMON = src/support.cpp src/engine.cpp src/controllers.cpp
DECISION_FILES = $(DECISIONS)/inorder_control.cpp $(DECISIONS)/scoreboard_control.cpp $(DECISIONS)/tomasulo_control.cpp
HEADERS = include/sim.hpp include/decisions.hpp
.PHONY: all clean demo check test evaluate experiments framework-test decisions-test
all: $(BUILD)/sim $(BUILD)/decision_probe $(BUILD)/framework_tests
$(BUILD)/sim: src/main.cpp $(COMMON) $(DECISION_FILES) $(HEADERS)
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp $(COMMON) $(DECISION_FILES) -o $@
$(BUILD)/decision_probe: tests/decision_probe.cpp $(DECISION_FILES) $(HEADERS)
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/decision_probe.cpp $(DECISION_FILES) -o $@
$(BUILD)/framework_tests: tests/framework_tests.cpp $(COMMON) $(DECISION_FILES) $(HEADERS)
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/framework_tests.cpp $(COMMON) $(DECISION_FILES) -o $@
framework-test: all
	./$(BUILD)/framework_tests
demo: all
	python3 tools/workflow.py demo --build $(BUILD) --decisions $(DECISIONS)
check test: all
	python3 tools/workflow.py check --build $(BUILD) --decisions $(DECISIONS) $(if $(MODEL),--model $(MODEL),)
decisions-test: all
	python3 tools/workflow.py check --build $(BUILD) --decisions $(DECISIONS) --decisions-only $(if $(MODEL),--model $(MODEL),)
evaluate experiments: all
	python3 tools/workflow.py evaluate --build $(BUILD) --decisions $(DECISIONS)
clean:
	rm -rf $(BUILD)
