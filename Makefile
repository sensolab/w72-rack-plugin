# W72 plugin build. RACK_DIR points at the Rack SDK.
RACK_DIR ?= ../..

FLAGS +=
CFLAGS +=
CXXFLAGS +=
LDFLAGS +=

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

# The engine's host test needs no SDK, so it runs without one.
ifneq ($(MAKECMDGOALS),test-engine)
include $(RACK_DIR)/plugin.mk
endif

# Brumista's server uses Winsock, and its screenshots read pixels back through OpenGL.
ifdef ARCH_WIN
	LDFLAGS += -lws2_32 -lopengl32
endif

# Panels: res/<Slug>.svg and src/<Slug>Panel.hpp from panels/<Slug>.json (needs Go).
.PHONY: panels
panels:
	cd tools/panelgen && go run . -root ../..

# Host tests of Moleta's and Gibigiana's engines and Brumista's engine watch
# (src/MoletaEngine.hpp, src/GibigianaEngine.hpp and src/BrumistaEngineWatch.hpp have
# no Rack includes), with the plain host compiler.
# Builds into build/, outside the plugin package.
HOST_CXX ?= g++
.PHONY: test-engine
test-engine:
	mkdir -p build/tests
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -o build/tests/moleta_engine_test tests/moleta_engine_test.cpp
	build/tests/moleta_engine_test
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -o build/tests/gibigiana_engine_test tests/gibigiana_engine_test.cpp
	build/tests/gibigiana_engine_test
	$(HOST_CXX) -std=c++11 -O2 -Wall -Wextra -o build/tests/engine_watch_test tests/engine_watch_test.cpp
	build/tests/engine_watch_test
