CXXFLAGS ?= -O3 -Wall -Wextra -pedantic
PKG_CONFIG ?= pkg-config
PYTHON ?= python3

.PHONY: proof film

proof: build/stained-glass
	cd build && ./stained-glass proof
	cd build && ./stained-glass panes

film: build/stained-glass
	$(PYTHON) tools/render-film.py $< build/stained-glass-geometry.mp4

build/stained-glass: src/layered.cpp src/painted_glass.hpp src/scene-typeset.inc
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++20 $(shell $(PKG_CONFIG) --cflags pangocairo) $< $(LDFLAGS) $(shell $(PKG_CONFIG) --libs pangocairo) -o $@
