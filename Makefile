CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CPPFLAGS += -I.

GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
GLFW_LIBS   := $(shell pkg-config --libs glfw3)
LDLIBS      := $(GLFW_LIBS) -lGL

BUILD := build
BIN   := bin

# Objektdateien nicht als Zwischendateien loeschen (sonst Rebuild jede Runde).
.SECONDARY:

LIB_SRCS := $(wildcard lib3d/*.cpp)
LIB_OBJS := $(patsubst lib3d/%.cpp,$(BUILD)/lib/%.o,$(LIB_SRCS))

BODIES_SRCS := $(wildcard bodies/*.cpp)
BODIES_OBJS := $(patsubst bodies/%.cpp,$(BUILD)/bodies/%.o,$(BODIES_SRCS))

APP_NAMES := vorlage beispiel instanzen ray
APP_BINS  := $(addprefix $(BIN)/,$(APP_NAMES))

.PHONY: all build test clean run-vorlage run-beispiel run-instanzen run-ray

all: $(APP_BINS) $(BIN)/lib3d_test

build: $(APP_BINS)

$(BUILD)/lib/%.o: lib3d/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

$(BUILD)/bodies/%.o: bodies/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

$(BUILD)/apps/%/main.o: apps/%/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

$(BUILD)/tests/lib3d_test.o: tests/lib3d_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

$(BIN)/%: $(BUILD)/apps/%/main.o $(LIB_OBJS) $(BODIES_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BIN)/lib3d_test: $(BUILD)/tests/lib3d_test.o $(LIB_OBJS) $(BODIES_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

test: $(BIN)/lib3d_test
	$(BIN)/lib3d_test

run-vorlage: $(BIN)/vorlage
	$(BIN)/vorlage

run-beispiel: $(BIN)/beispiel
	$(BIN)/beispiel

run-instanzen: $(BIN)/instanzen
	$(BIN)/instanzen

run-ray: $(BIN)/ray
	$(BIN)/ray

clean:
	rm -rf $(BUILD) $(BIN)
