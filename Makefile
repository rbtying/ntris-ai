EXE_NAME = ./ai-client/dropblox_ai

CXXFLAGS += -std=c++0x -O3 -g -Wall -fopenmp

$(EXE_NAME): src/dropblox_ai.cpp src/choose_move.cpp
	g++ $(CXXFLAGS) -o $@ $^

clean:
	rm $(EXE_NAME)
