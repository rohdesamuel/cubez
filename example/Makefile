all:
	make server
	make client

server:
	g++ -Isrc/shared -I.. -L. src/shared/*.cpp src/server/main.cpp -fno-exceptions -Wall -Wextra -Werror -std=c++14 -O3 -lX11 -lcubez -lGLEW -lGL -lGLU -lSDL2 -fopenmp -o server

client:
	g++ -Isrc/shared -I.. -L. src/shared/*.cpp src/client/main.cpp -fno-exceptions -Wall -Wextra -Werror -std=c++14 -O3 -lX11 -lcubez -lGLEW -lGL -lGLU -lSDL2 -fopenmp -o client

debug:
	g++ -I.. -L. *.cpp -fno-exceptions -Wextra -Werror -std=c++14 -ggdb -lX11 -lGLEW -lGL -lGLU -lcubez -lSDL2 -fopenmp
