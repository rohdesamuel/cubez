# Directories
INC_DIRS=../include	../deps/include
LIB_DIRS=../deps/linux ../lib
OBJ=obj

# Libraries
LIBS=X11
LIBS+=cubez
LIBS+=GLEW
LIBS+=GL
LIBS+=GLU
LIBS+=SDL2
LIBS+=Ultralight
LIBS+=UltralightCore
LIBS+=WebCore

# Generated Arguments
LIB_DIRS_ALL=$(foreach DIR, $(LIB_DIRS), -L$(DIR))
INC_DIRS_ALL=$(foreach DIR, $(INC_DIRS), -I$(DIR))
LIBS_ALL=$(foreach LIB, $(LIBS), -l$(LIB))

# Arguments
CC=g++
OPTIONS=-fno-exceptions -fno-rtti  -fopenmp -DGLM_ENABLE_EXPERIMENTAL -O3 -std=c++17 #-Wall -Wextra -Werror -ggdb
SOURCES=$(wildcard src/*.cpp)
OBJECTS=$(SOURCES:src/%.cpp=$(OBJ)/%.o)
TARGET=example

all: $(TARGET)
	
$(TARGET): $(OBJECTS)
	@echo "\nLinking..."
	@${CC} ${OPTIONS} ${INC_DIRS_ALL} ${LIB_DIRS_ALL} -Wl,-rpath,'$$ORIGIN/../deps/linux' -o $@ $^ ${LIBS_ALL}
	@echo "DONE!"
	@echo "Made "${TARGET}

${OBJ}/%.o: src/%.cpp src/%.h
	@echo "Making "$@
	@$(CC) ${OPTIONS} ${INC_DIRS_ALL} ${LIB_DIRS_ALL} -Wl,-rpath,'$$ORIGIN/../deps/linux' -c $< -o $@ ${LIBS_ALL}

${OBJ}/%.o: src/%.cpp
	@echo "Making "$@
	@$(CC) ${OPTIONS} ${INC_DIRS_ALL} ${LIB_DIRS_ALL} -Wl,-rpath,'$$ORIGIN/../deps/linux' -c $< -o $@ ${LIBS_ALL}

clean:
	rm -f $(OBJ)/*.o $(TARGET)

config:
	@echo "Includes:\n\t"${INC_DIRS}
	@echo "Library Location:\n\t"${LIB_DIRS}
	@echo "Libraries:\n\t"${LIBS}
	@echo "Compiler options:\n\t"${OPTIONS}

.PHONY: all clean config # Declare targets are not files
