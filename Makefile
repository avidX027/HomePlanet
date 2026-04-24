# ============================================================
#  HOME PLANET: VOID RUNNER — Windows MinGW Makefile
#  Expects:  src/main.c  src/world.h  src/player.h  src/ui.h
#  Outputs:  bin/HomePlanet.exe
#  Build:    mingw32-make   (triggered by VS Code tasks.json)
#
#  Raylib setup (do this once):
#    1. Download raylib Windows MinGW build from:
#       https://github.com/raysan5/raylib/releases
#    2. Copy raylib.h + libraylib.a into your MinGW:
#       C:/mingw64/include/   and   C:/mingw64/lib/
# ============================================================

CC      = gcc
CFLAGS  = -std=c99 -O2 -Wall -Wextra -I src
LIBS    = -lraylib -lopengl32 -lgdi32 -lwinmm -lm
SRC     = src/main.c
OUT     = bin/HomePlanet.exe

all: bin $(OUT)

bin:
	mkdir -p bin

$(OUT): $(SRC) src/world.h src/player.h src/ui.h
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

clean:
	del /Q bin\HomePlanet.exe 2>nul || true

.PHONY: all clean
