

COMMON_INC = -I ../common

SOURCE := $(wildcard *.cpp) $(wildcard ../common/*.cpp)
OBJS := $(patsubst %.cpp, %.o, $(SOURCE))

CXXFLAGS := -g -Wall -DDEBUG $(COMMON_INC)

main.exe: $(OBJS)
	g++ -o ace.exe $(OBJS) 
