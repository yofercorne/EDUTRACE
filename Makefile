CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -g
BUILD_DIR := build
SRC_DIR := src

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET := $(BUILD_DIR)/edutrace

.PHONY: all clean run test benchmark app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: all
	./$(TARGET) examples/variables.et --trace

test: all
	bash tests/run_tests.sh

benchmark: all
	python3 benchmarks/run_benchmarks.py

app: all
	python3 app/backend/server.py

clean:
	rm -rf $(BUILD_DIR) *.s *.out output app/tmp/*
