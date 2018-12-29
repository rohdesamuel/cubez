test:
	g++ -I../include -I../deps/include -L../deps/linux -L../lib -Wl,-rpath,'$$ORIGIN/../deps/linux'  *.cpp -fno-exceptions -fno-rtti -DGLM_ENABLE_EXPERIMENTAL -std=c++17 -O3 -lX11 -lcubez -lGLEW -lGL -lGLU -lSDL2 -lUltralight -lUltralightCore -lWebCore -fopenmp

all:
	g++ -I../include -I../deps/include -L. *.cpp -fno-exceptions -DGLM_ENABLE_EXPERIMENTAL -Wall -Wextra -Werror -std=c++17 -O3 -lX11 -lcubez -lGLEW -lGL -lGLU -lSDL2 -fopenmp

debug:
	g++ -I../include -I../deps/include -I../deps/include -L. *.cpp -fno-exceptions -DGLM_ENABLE_EXPERIMENTAL -Wextra -Werror -std=c++17 -ggdb -lX11 -lGLEW -lGL -lGLU -lcubez -lSDL2 -fopenmp
