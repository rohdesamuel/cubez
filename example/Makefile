all:
	g++ -I../include -L. *.cpp -fno-exceptions -Wall -Wextra -Werror -std=c++14 -O3 -lX11 -lcubez -lGLEW -lGL -lGLU -lSDL2 -fopenmp

debug:
	g++ -I../include -L. *.cpp -fno-exceptions -Wextra -Werror -std=c++14 -ggdb -lX11 -lGLEW -lGL -lGLU -lcubez -lSDL2 -fopenmp
