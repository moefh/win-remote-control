
CC = gcc
WINDRES = windres
CFLAGS = -O2 -Wall
LDFLAGS = -s -mwindows
#LDFLAGS = -s

OBJS = main.o parse.o network.o systray.o about.o resources.o
LIBS = -lcomctl32 -lWs2_32 -lgdi32

.PHONY: all clean

all: remote-control

clean:
	rm -f *.o *~

remote-control: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.rc
	$(WINDRES) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
