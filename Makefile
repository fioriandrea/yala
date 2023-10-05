TARGET=yala
SOURCES=$(wildcard *.c) $(wildcard */*.c)
OBJS=$(patsubst %.c, %.o, $(SOURCES))
FLAGS=-g -std=c99 -pedantic -Wall

.PHONY = all purge clean cleanbuild
.DEFAULT = all

$(TARGET): $(OBJS)
	cc $^ -o $@

%.o: %.c
	cc -c $< $(FLAGS) -o $@

all: $(TARGET)

purge: clean
	rm -f $(TARGET)

clean:
	rm -f $(OBJS)

cleanbuild: purge all