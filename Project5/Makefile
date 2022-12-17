CC=gcc -g 

fs: fs.c
	$(CC) -c fs.c -o fs.o

disk: disk.c
	$(CC) -c disk.c -o disk.o

test: mount_test.c
	$(CC) -c -o test.o mount_test.c

all: fs  test disk
	$(CC) fs.o disk.o test.o -o main

clean:
	rm fs.o main.o main