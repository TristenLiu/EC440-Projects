CC=gcc -Werror -Wall -g 

tls: tls.c
	$(CC) -c -o tls.o tls.c

all: tls  test
	$(CC) -lpthread -o main test.o tls.o

test: test2.c
	$(CC) -c -o test.o test2.c

clean:
	rm tls.o main.o main
