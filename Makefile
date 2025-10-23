CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

TARGET = myshell
OBJS = command.o tokenizer.o
.PHONY: all
all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)
command.o: command.cc command.h tokenizer.h
	@$(CXX) $(CXXFLAGS) -c command.cc -o command.o
tokenizer.o: tokenizer.cc tokenizer.h
	@$(CXX) $(CXXFLAGS) -c tokenizer.cc -o tokenizer.o
.PHONY: clean
clean:
	@rm -f $(OBJS) $(TARGET)

