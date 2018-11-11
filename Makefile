CC = gcc
CXX = g++

CPPFLAGS = -Iinclude -Iinclude/imgui -Iinclude/SDL2 -Llib
CFLAGS = -O2 -Wall -Wextra -std=gnu99
CXXFLAGS = -O2 -Wall -Wextra -std=gnu++14

LDFLAGS = -lgcc -lSDL2 -lopengl32

BINARY = psx_emu

SOURCES = \
	src/dma.c \
	src/exp2.c \
	src/gui.cpp \
	src/main.c \
	src/psx.c \
	src/r3000.c \
	src/r3000_disassembler.c \
	src/r3000_interpreter.c \
	src/rb.c \
	src/spu.c \
	src/util.c \
	src/window.c

SOURCES += src/gl3w/gl3w.c

SOURCES += \
	src/imgui/imgui.cpp \
	src/imgui/imgui_draw.cpp \
	src/imgui/imgui_impl_opengl3.cpp \
	src/imgui/imgui_impl_sdl.cpp \
	src/imgui/imgui_widgets.cpp

OBJECTS = $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCES)))

$(BINARY): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(BINARY) $(OBJECTS)
