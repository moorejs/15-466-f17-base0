# Hammer

In this game, the player controls a hammer. The hammer may only strike the balls that are flying through the air with its sides. If the balls hit the top or bottom of the hammer, the balls are slowed and the hammer shrinks in size. If the hammer gets too small, you lose. You must destroy all balls to proceed to the next level (as indicated by the levels counter at the top of the game window). If you clear all windows, you unlock a special hammer color.

See `screenshots/` for more samples of the gameplay. The game begins in a menu, as indicated by the red and green bars at the top.

This game was designed by bpx (see http://graphics.cs.cmu.edu/courses/15-466-f17/game0-designs/bpx/).

I implemented the game with 5 levels specifically. More can easily be added to the source.

# Controls

Click the right mouse button to start the game. If you reach a game over, you must click to restart.

The mouse movement controls the hammer.

## Requirements for building

 - glm
 - libSDL2
 - modern C++ compiler

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

### Linux
```
  g++ -g -Wall -Werror -o main main.cpp Draw.cpp `sdl2-config --cflags --libs` -lGL
```
or:
```
  make
```

### OSX
```
  clang++ -g -Wall -Werror -o main main.cpp Draw.cpp `sdl2-config --cflags --libs`
```
or:
```
  make
```