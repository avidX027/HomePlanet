# ============================================================
#  HOME PLANET: VOID RUNNER — Cross-platform Makefile
#  Expects:  src/main.c  src/world.h  src/player.h  src/ui.h
#
#  Windows (MSYS2 UCRT64):
#    C:/msys64/ucrt64/bin/mingw32-make.exe   ->  bin/HomePlanet.exe
#    raylib expected at C:/raylib/include and C:/raylib/lib
#  macOS:
#    make                                    ->  bin/HomePlanet
#    raylib expected at /usr/local (dylib install)
# ============================================================

CFLAGS  = -std=c99 -O2 -g -Wall -Wextra -I src
SRC     = src/main.c
# wildcard = "every .h in src/" — add or delete headers freely,
# the Makefile notices them automatically
HDRS    = $(wildcard src/*.h)

ifeq ($(OS),Windows_NT)
    CC      = C:/msys64/ucrt64/bin/gcc.exe
    OUT     = bin/HomePlanet.exe
    CFLAGS += -I C:/raylib/include
    LIBS    = -L C:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lm
    MKDIR   = if not exist bin mkdir bin
    RM      = del /Q bin\HomePlanet.exe 2>nul || exit 0
    RUN     = bin\HomePlanet.exe
else
    CC      = clang
    OUT     = bin/HomePlanet
    CFLAGS += -I /usr/local/include
    LIBS    = /usr/local/lib/libraylib.dylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm
    MKDIR   = mkdir -p bin
    RM      = rm -f bin/HomePlanet
    RUN     = ./bin/HomePlanet
endif

all: $(OUT)

$(OUT): $(SRC) $(HDRS)
	$(MKDIR)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: $(OUT)
	$(RUN)

clean:
	$(RM)

.PHONY: all run clean
