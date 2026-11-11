# tekmato -- DOS/50 disc image tool. Plain C++17, no dependencies.
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SOURCES   = tekmato.cpp command.cpp command_image.cpp command_list.cpp command_files.cpp command_tree.cpp dos50_volume.cpp
OBJECTS   = $(SOURCES:.cpp=.o)

tekmato: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

%.o: %.cpp command.hpp dos50_volume.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: tekmato
	scripts/regression.sh

clean:
	rm -f $(OBJECTS) tekmato

.PHONY: test clean
