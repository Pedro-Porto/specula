CXX      := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pthread
INCLUDE  := -Iinclude
BIN      := controller

all: $(BIN)

$(BIN):
	$(CXX) $(CXXFLAGS) $(INCLUDE) ./src/*.cpp -o $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
