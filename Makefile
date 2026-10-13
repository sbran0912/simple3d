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

# -MMD -MP erzeugt je Objektdatei eine .d-Datei mit den Header-Abhaengigkeiten
# (siehe DEPS unten). So werden Aenderungen an Headern wie vehicle.hpp erkannt.
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -MMD -MP
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
# Jede App ist ein Unterordner apps/<name>/ und darf mehrere .cpp enthalten
# (z. B. main.cpp + vehicle.cpp); alle werden gebaut und zusammen gelinkt.
APP_NAMES := boids ray
APP_BINS  := $(addprefix $(BIN)/,$(APP_NAMES))
APP_SRCS  := $(wildcard apps/*/*.cpp)
APP_OBJS  := $(patsubst apps/%.cpp,$(BUILD)/apps/%.o,$(APP_SRCS))

# --- Header-Abhaengigkeiten -----------------------------------------
# Zu jedem .o gehoert eine vom Compiler (-MMD -MP) erzeugte .d-Datei. Sie wird
# am Ende eingebunden, damit Header-Aenderungen ein Neuuebersetzen ausloesen.
DEPS := $(LIB_OBJS:.o=.d) $(APP_OBJS:.o=.d) $(BUILD)/tests/lib3d_test.d

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
$(BUILD)/apps/%.o: apps/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

$(BUILD)/tests/lib3d_test.o: tests/lib3d_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) -c $< -o $@

# --- Linken ---------------------------------------------------------
# Pro App eine explizite Regel: Objektdateien aller Quellen der App zusammen
# mit der Bibliothek linken. Ueber foreach/eval erzeugt, damit die Wildcard je
# App korrekt ausgewertet wird.
define APP_LINK_RULE
$(BIN)/$(1): $$(patsubst apps/%.cpp,$(BUILD)/apps/%.o,$$(wildcard apps/$(1)/*.cpp)) $(LIB)
	@mkdir -p $$(dir $$@)
	$$(CXX) $$^ -o $$@ $$(LDLIBS)
endef

$(foreach app,$(APP_NAMES),$(eval $(call APP_LINK_RULE,$(app))))

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

# Eingebundene Header-Abhaengigkeiten (fehlende .d-Dateien werden ignoriert).
-include $(DEPS)
