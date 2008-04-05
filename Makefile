secho: secho.c cstring.h
	$(CC) -o secho secho.c

clean:
	rm -f secho secho.o
