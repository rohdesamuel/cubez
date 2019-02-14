# Directories
INC_DIR = include
SRC_DIR = src
LIB_DIR = lib
OBJ_DIR = obj

# Flags
CPP_STD = c++17

# Versioning
MAJOR_VERSION = 1
MINOR_VERSION = 1
BUILD_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION).$(BUILD_VERSION)

# Debug Vars
DEBUG_MODE = __ENGINE_DEBUG__

all:
	# Author: Samuel Rohde (rohde.samuel@gmail.com)
	#
	# Now building libcubez...
	# Current version is $(VERSION)
	g++ -v
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(LIB_DIR)
	g++ -c -fPIC -fno-exceptions -I$(INC_DIR) $(SRC_DIR)/*.cpp -Wall -Wextra -Werror -std=$(CPP_STD) -O3 -lSDL2 -lGLEW -lGL -lGLU -fopenmp
	g++ -shared -fPIC -fno-exceptions -Wl,-soname,libcubez.so.$(MAJOR_VERSION) -o $(LIB_DIR)/libcubez.so.$(VERSION) *.o -lc
	@ln -f -r -s $(LIB_DIR)/libcubez.so.$(VERSION) $(LIB_DIR)/libcubez.so.$(MAJOR_VERSION)
	@ln -f -r -s $(LIB_DIR)/libcubez.so.$(VERSION) $(LIB_DIR)/libcubez.so
	@mv *.o obj/

debug:
	g++ -c -fPIC -fno-exceptions -D$(DEBUG_MODE) -I$(INC_DIR) $(SRC_DIR)/*.cpp -Wall -Wextra -Werror -std=$(CPP_STD) -g -lSDL2 -lGLEW -lGL -lGLU -fopenmp
	g++ -shared -fPIC -fno-exceptions -Wl,-soname,libcubez.so.$(MAJOR_VERSION) -o $(LIB_DIR)/libcubez.so.$(VERSION) *.o -lc
	@ln -f -r -s $(LIB_DIR)/libcubez.so.$(VERSION) $(LIB_DIR)/libcubez.so.$(MAJOR_VERSION)
	@ln -f -r -s $(LIB_DIR)/libcubez.so.$(VERSION) $(LIB_DIR)/libcubez.so
	@mv *.o obj/

install:
	cp $(LIB_DIR)/libcubez.so.$(VERSION) /usr/lib/libcubez.so
	cp $(LIB_DIR)/libcubez.so.$(VERSION) /usr/lib/libcubez.so.$(MAJOR_VERSION)

clean:
	rm $(LIB_DIR)/*
	rm $(OBJ_DIR)/*
	rm /usr/lib/libcubez.so*
