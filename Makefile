TARGET=yala
SOURCES=$(wildcard *.c)
OBJS=$(patsubst %.c, %.o, $(SOURCES))

.PHONY = all purge clean cleanbuild
.DEFAULT = all

$(TARGET): $(OBJS)
	cc $^ -o $@

%.o: %.c
	cc -c $<

all: $(TARGET)

purge: clean
	rm -f $(TARGET)

clean:
	rm -f $(OBJS)

cleanbuild: purge all