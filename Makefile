SRC=main.cpp \
	vulkfm.cpp \
	external/imgui/imgui.cpp \
	external/imgui/imgui_draw.cpp \
	external/imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.cpp

SRC_C=external/imgui/examples/libs/gl3w/GL/gl3w.c


OBJS=$(SRC:.cpp=.o)
OBJS_C:=$(SRC_C:.c=.o)
CFLAGS=-m64 -I external/imgui/examples/sdl_opengl_example \
		-I external/imgui/examples/libs/gl3w

CXXFLAGS=-std=c++14 -m64 $(shell sdl2-config --cflags) -O0 -g -I external/imgui \
								-I external/imgui/examples/sdl_opengl3_example \
								-I external/imgui/examples/libs/gl3w
LDFLAGS=$(shell sdl2-config --libs) -framework OpenGL -framework CoreFoundation
OUT=play

all: $(OBJS) $(OBJS_C)
	$(CXX) -o $(OUT) $(CXXFLAGS) $(LDFLAGS) $(OBJS) $(OBJS_C)


#%.o: %.cpp
#	$(CXX) -c -o $@ $(CXXFLAGS) $<

#%.o: %.c
#	$(CC) -c -o $@ $(CXXFLAGS) $<

clean:
	rm -f $(OBJS) $(OUT) $(OBJS_C)