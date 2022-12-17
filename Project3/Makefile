CC=gcc -Werror -Wall -g 

threadlib: threads.c
	$(CC) -c -o threads.o threads.c

all: threadlib  main
	$(CC) -o main main.o threads.o

stringtest: threadlib  test
	$(CC) -o test test.o threads.o

test: testreturn.c
	$(CC) -c -o test.o testreturn.c

main: main.c
	$(CC) -c -o main.o main.c

clean:
	rm threads.o main.o main
