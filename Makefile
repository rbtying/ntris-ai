EXE_NAME = ./ai-client/dropblox_ai

$(EXE_NAME): src/dropblox_ai.cpp
	g++ -o $@ $^

clean:
	rm $(EXE_NAME)
