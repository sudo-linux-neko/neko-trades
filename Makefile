CXX      := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pthread -march=native -MMD
SRC      := src/main.cpp
BIN      := neko_trades
DEPS     := $(SRC:.cpp=.d)

.PHONY: all clean run

all: $(BIN)

-include $(DEPS)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) trades_log.csv $(DEPS)
