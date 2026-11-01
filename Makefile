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

# Panels: res/<Slug>.svg and src/<Slug>Panel.hpp from panels/<Slug>.json (needs Go).
.PHONY: panels
panels:
	cd tools/panelgen && go run . -root ../..
