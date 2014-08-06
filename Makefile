CFLAGS = -W -Wall -pthread

all: myshell

myshell: myshell.c
	gcc $(CFLAGS) -o myshell myshell.c

clean:
	rm -rf *.o myshell
