CC      ?= cc
CFLAGS  ?= -Wall -Wextra -O2 -g -std=c11
SRCS    := main.c world.c entity.c physics.c vehicle.c ai.c wanted.c input.c renderer.c

OBJS_NC  := $(SRCS:.c=.nc.o)
OBJS_RAY := $(SRCS:.c=.ray.o)

BIN_NC  := gta_sim_ncurses
BIN_RAY := gta_sim

RAYLIB_INC := -Ideps/include
RAYLIB_LIB := deps/lib/libraylib.a
RAYLIB_SYS := -lGL -lm -lpthread -ldl -lrt -lX11

.PHONY: all raylib ncurses clean

all: raylib

raylib: $(BIN_RAY)

ncurses: $(BIN_NC)

$(BIN_RAY): $(OBJS_RAY)
	$(CC) $(CFLAGS) -o $@ $^ $(RAYLIB_LIB) $(RAYLIB_SYS)

$(BIN_NC): $(OBJS_NC)
	$(CC) $(CFLAGS) -o $@ $^ -lncurses -lm

%.ray.o: %.c
	$(CC) $(CFLAGS) -DUSE_RAYLIB $(RAYLIB_INC) -c -o $@ $<

%.nc.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS_NC) $(OBJS_RAY) $(BIN_NC) $(BIN_RAY)
