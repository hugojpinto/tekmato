# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# tekmato -- DOS/50 and TEKDOS disc image tool. Plain C++17, no dependencies.
#   make                                  ./tekmato (objects in build/)
#   make CROSS=x86_64-w64-mingw32-        build-win/tekmato.exe (Windows x64, statically linked)
#   make test                             scripts/regression.sh
CROSS    ?=
CXX       = $(CROSS)c++
ifneq ($(CROSS),)
CXX       = $(CROSS)g++
endif
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDFLAGS  ?=
OBJDIR    = build
PROGRAM   = tekmato
ifneq ($(findstring mingw,$(CROSS)),)
OBJDIR    = build-win
PROGRAM   = build-win/tekmato.exe
LDFLAGS  += -static -s
endif
# the version (VERSION), compiled in (-version)
VERSION  := $(shell cat VERSION 2>/dev/null || echo unknown)
SOURCES   = tekmato.cpp command.cpp command_image.cpp command_list.cpp command_files.cpp command_tree.cpp \
            filesystem.cpp dos50_fs.cpp tekdos_fs.cpp dos50_volume.cpp tekdos_volume.cpp
OBJECTS   = $(SOURCES:%.cpp=$(OBJDIR)/%.o)

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(OBJDIR)/%.o: %.cpp command.hpp filesystem.hpp dos50_volume.hpp tekdos_volume.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/command.o: VERSION
$(OBJDIR)/command.o: CXXFLAGS += -DTEKMATO_VERSION_STRING='"$(VERSION)"'

test: tekmato
	scripts/regression.sh

clean:
	rm -rf build build-win tekmato *.o

.PHONY: test clean
