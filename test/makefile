
INDEX_SERVER_TARGET = index_server
PEER_CLIENT_TARGET = client
COMMON_INC = -I ../common

INDEX_SERVER_SOURCE := $(wildcard ../server/*.cpp) $(wildcard ../common/*.cpp)
INDEX_SERVER_OBJS := $(patsubst %.cpp, %.o, $(INDEX_SERVER_SOURCE))

PEER_CLIENT_SOURCE := $(wildcard ../peer-client/*.cpp) $(wildcard ../common/*.cpp)
PEER_CLIENT_OBJS := $(patsubst %.cpp, %.o, $(PEER_CLIENT_SOURCE))

CXXFLAGS := -g -Wall -DDEBUG $(COMMON_INC)

all: $(INDEX_SERVER_TARGET) $(PEER_CLIENT_TARGET)

$(INDEX_SERVER_TARGET): $(INDEX_SERVER_OBJS)
	g++ -o $(INDEX_SERVER_TARGET) $(INDEX_SERVER_OBJS) -l pthread

$(PEER_CLIENT_TARGET): $(PEER_CLIENT_OBJS)
	g++ -o $(PEER_CLIENT_TARGET) $(PEER_CLIENT_OBJS) -l pthread

clean:
	rm -rf $(INDEX_SERVER_OBJS) $(INDEX_SERVER_TARGET) $(PEER_CLIENT_OBJS) $(PEER_CLIENT_TARGET)
