# Directories
INC_DIRS=../include	../deps/include
LIB_DIRS=../deps/linux ../lib
OBJ_DIR=obj
SRC_DIR=src

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
SOURCES=$(wildcard ${SRC_DIR}/*.cpp)
OBJECTS=$(SOURCES:${SRC_DIR}/%.cpp=$(OBJ_DIR)/%.o)
TARGET=example

all: $(TARGET)
	
$(TARGET): $(OBJECTS)
	@printf "\nLinking..."
	@cp ../deps/linux/* lib/
	@${CC} ${OPTIONS} ${INC_DIRS_ALL} ${LIB_DIRS_ALL} -Wl,-rpath,'$$ORIGIN/lib' -o $@ $^ ${LIBS_ALL}
	@echo "Done"
	@echo "Made "${TARGET}

${OBJ_DIR}/%.o: ${SRC_DIR}/%.cpp ${SRC_DIR}/%.h
	@echo "Making "$@
	@$(CC) ${OPTIONS} ${INC_DIRS_ALL} -c $< -o $@

${OBJ_DIR}/%.o: ${SRC_DIR}/%.cpp
	@echo "Making "$@
	@$(CC) ${OPTIONS} ${INC_DIRS_ALL} -c $< -o $@

clean:
	rm -f ${OBJ_DIR}/*.o $(TARGET) lib/*

config:
	@echo "Includes:\n\t"${INC_DIRS}
	@echo "Library Location:\n\t"${LIB_DIRS}
	@echo "Libraries:\n\t"${LIBS}
	@echo "Compiler options:\n\t"${OPTIONS}

.PHONY: all clean config # Declare targets are not files
