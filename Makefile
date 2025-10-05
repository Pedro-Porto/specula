CXX       := g++
CXXFLAGS  := -O2 -std=c++17 -Wall -Wextra -pthread

CORE_INC      := -Icore/include
AGENT_INC     := $(CORE_INC) -Iagent/include
CONTROLLER_INC:= $(CORE_INC) -Icontroller/include

BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
LIB_DIR   := $(BUILD_DIR)/lib

CORE_SRCS := $(wildcard core/src/*.cpp)
CORE_OBJS := $(patsubst core/src/%.cpp,$(BUILD_DIR)/core/%.o,$(CORE_SRCS))
CORE_LIB  := $(LIB_DIR)/libvigilor_core.a

AGENT_MAIN := agent/main.cpp
AGENT_SRCS := $(wildcard agent/src/*.cpp)
AGENT_OBJS := $(patsubst agent/src/%.cpp,$(BUILD_DIR)/agent/%.o,$(AGENT_SRCS))
AGENT_BIN  := $(BIN_DIR)/agent

CTRL_MAIN := controller/main.cpp
CTRL_SRCS := $(wildcard controller/src/*.cpp)
CTRL_OBJS := $(patsubst controller/src/%.cpp,$(BUILD_DIR)/controller/%.o,$(CTRL_SRCS))
CTRL_BIN  := $(BIN_DIR)/controller

.PHONY: all clean run-agent run-controller dirs
all: dirs $(CORE_LIB) $(AGENT_BIN) $(CTRL_BIN)

$(CORE_LIB): $(CORE_OBJS)
	@mkdir -p $(LIB_DIR)
	ar rcs $@ $^

$(BUILD_DIR)/core/%.o: core/src/%.cpp
	@mkdir -p $(BUILD_DIR)/core
	$(CXX) $(CXXFLAGS) $(CORE_INC) -c $< -o $@

$(AGENT_BIN): $(AGENT_OBJS) $(AGENT_MAIN) $(CORE_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(AGENT_INC) $(AGENT_OBJS) $(AGENT_MAIN) $(CORE_LIB) -o $@

$(BUILD_DIR)/agent/%.o: agent/src/%.cpp
	@mkdir -p $(BUILD_DIR)/agent
	$(CXX) $(CXXFLAGS) $(AGENT_INC) -c $< -o $@

$(CTRL_BIN): $(CTRL_OBJS) $(CTRL_MAIN) $(CORE_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(CONTROLLER_INC) $(CTRL_OBJS) $(CTRL_MAIN) $(CORE_LIB) -o $@

$(BUILD_DIR)/controller/%.o: controller/src/%.cpp
	@mkdir -p $(BUILD_DIR)/controller
	$(CXX) $(CXXFLAGS) $(CONTROLLER_INC) -c $< -o $@

dirs: $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

run-agent: $(AGENT_BIN)
	$(AGENT_BIN)

run-controller: $(CTRL_BIN)
	$(CTRL_BIN)

clean:
	rm -rf $(BUILD_DIR)
