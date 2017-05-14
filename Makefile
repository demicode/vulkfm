SRC=main.cpp
OBJS=$(SRC:.cpp=.o)
CXXFLAGS=-std=c++14 -m64 $(shell sdl2-config --cflags)
LDFLAGS=$(shell sdl2-config --libs)
OUT=play

all: $(OBJS) 
	$(CXX) -o $(OUT) $(CXXFLAGS) $(LDFLAGS) $(OBJS)


%.o: %.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $<

clean:
	rm -f $(OBJS) $(OUT)