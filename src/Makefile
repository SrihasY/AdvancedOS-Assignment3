main: main.o disk.o sfs.o directory.o
	gcc -o main main.o disk.o sfs.o directory.o -lm
main.o: main.c disk.h sfs.h
	gcc -c -g main.c
sfs.o: sfs.c sfs.h
	gcc -c -g sfs.c
disk.o: disk.c disk.h
	gcc -c -g disk.c
directory.o: directory.c
	gcc -c -g directory.c
clean:
	rm disk.o main.o sfs.o main directory.o