# ====================================================================
# Makefile fuer simple3d
#
#   make lib         Statische Bibliothek lib/lib3d.a bauen
#   make             Bibliothek + alle Apps bauen (Alias: make apps)
#   make apps        Alle Apps bauen und linken
#   make lib/<name>  Einzelne App bauen/linken (z. B. make bin/ray)
#   make run APP=x   App x bauen und starten (x aus: $(APP_NAMES))
#   make run-x       Kurzform zum Starten (z. B. make run-ray)
#   make test        Unit-Test bauen und ausfuehren
#   make clean       Build-Artefakte loeschen
#
# Verzeichnisse:
#   lib/src   Quellen + Header der Bibliothek
#   lib       Ziel der statischen Bibliothek (lib3d.a)
#   apps      Quellen der Anwendungen (je App ein Unterordner mit main.cpp)
#   tests     Quelle des Unit-Tests
#   build     Objektdateien
#   bin       Fertige Programme
# ====================================================================

CXX      ?= g++
AR       ?= ar

CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CPPFLAGS += -Ilib/src

GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
GLFW_LIBS   := $(shell pkg-config --libs glfw3)
LDLIBS      := $(GLFW_LIBS) -lGL

BUILD := build
BIN   := bin
LIB   := lib/lib3d.a

# Objektdateien nicht als Zwischendateien loeschen (sonst Rebuild jede Runde).
.SECONDARY:

# --- Bibliothek -----------------------------------------------------
LIB_SRCS := $(wildcard lib/src/*.cpp)
LIB_OBJS := $(patsubst lib/src/%.cpp,$(BUILD)/lib/%.o,$(LIB_SRCS))

# --- Apps -----------------------------------------------------------
APP_NAMES := boids ray
APP_BINS  := $(addprefix $(BIN)/,$(APP_NAMES))

.PHONY: all apps lib test clean run $(addprefix run-,$(APP_NAMES))

all: apps

apps: $(APP_BINS)

lib: $(LIB)

# --- Objektdateien der Bibliothek -----------------------------------
$(BUILD)/lib/%.o: lib/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

# --- Statische Bibliothek -------------------------------------------
$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# --- Objektdateien der Apps/Tests -----------------------------------
$(BUILD)/apps/%/main.o: apps/%/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

$(BUILD)/tests/lib3d_test.o: tests/lib3d_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

# --- Linken ---------------------------------------------------------
$(BIN)/%: $(BUILD)/apps/%/main.o $(LIB)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BIN)/lib3d_test: $(BUILD)/tests/lib3d_test.o $(LIB)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDLIBS)

# --- Test -----------------------------------------------------------
test: $(BIN)/lib3d_test
	$(BIN)/lib3d_test

# --- Starten --------------------------------------------------------
# make run APP=<name>
run:
	@test -n "$(APP)" || { \
		echo "Fehler: APP fehlt."; \
		echo "Aufruf: make run APP=<name>   (verfuegbar: $(APP_NAMES))"; \
		exit 2; \
	}
	@$(MAKE) --no-print-directory $(BIN)/$(APP)
	@$(BIN)/$(APP)

# make run-<name>  (Kurzform)
$(addprefix run-,$(APP_NAMES)): run-%: $(BIN)/%
	$(BIN)/$*

# --- Aufraeumen -----------------------------------------------------
clean:
	rm -rf $(BUILD) $(BIN) $(LIB)
