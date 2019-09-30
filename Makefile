
CC = cc
CFLAGS = -O -m64
PROG = sets

$(PROG): $(PROG).c
	$(CC) $(CFLAGS) -o $(PROG) $(PROG).c
	strip $(PROG)

clean:
	rm -f *.o sets
