# W72 plugin build. RACK_DIR points at the Rack SDK.
RACK_DIR ?= ../..

FLAGS +=
CFLAGS +=
CXXFLAGS +=
LDFLAGS +=

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk

# Brumista's server uses Winsock, and its screenshots read pixels back through OpenGL.
ifdef ARCH_WIN
	LDFLAGS += -lws2_32 -lopengl32
endif

# Panels: res/<Slug>.svg and src/<Slug>Panel.hpp from panels/<Slug>.json (needs Go).
.PHONY: panels
panels:
	cd tools/panelgen && go run . -root ../..
