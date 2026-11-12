# tekmato -- DOS/50 and TEKDOS disc image tool. Plain C++17, no dependencies.
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SOURCES   = tekmato.cpp command.cpp command_image.cpp command_list.cpp command_files.cpp command_tree.cpp \
            filesystem.cpp dos50_fs.cpp tekdos_fs.cpp dos50_volume.cpp tekdos_volume.cpp
OBJECTS   = $(SOURCES:.cpp=.o)

tekmato: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

%.o: %.cpp command.hpp filesystem.hpp dos50_volume.hpp tekdos_volume.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: tekmato
	scripts/regression.sh

clean:
	rm -f $(OBJECTS) tekmato

.PHONY: test clean
