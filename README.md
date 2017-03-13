# Radiance
**Radianace** is a custom-made game framework written in C++. Its goal is to share and experiment with different performant and cache-aware algorithms in a simple game library. It currently provides a low-level interface for creating objects and executing operations over them. Future iterations will include a full object typing system, syntactic sugar, physics, sound, user input, display, and all the other fun goodies game frameworks usually have!

## Main Features

### Installation
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++5

#### Dependencies

```
sudo apt-get install libglm-dev
sudo apt-get install libboost-dev
sudo apt-get install libsdl2-2.0
sudo apt-get install libsdl2-dev
sudo apt-get install libglew-dev
```

## Design

### Source
An object that exposes a Reader class that emits elements upon execution.

### Sink
An object that exposes a Writer class that takes in elements upon execution.

### Frame
A shared piece of memory that holds temporary data between function executions.

### System
A single unit of execution that reads/writes to/from a Stack Frame.

### Pipeline
A class that encapsulates a queue of Systems to execute reading from a Source to a Sink.


