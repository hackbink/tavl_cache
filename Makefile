test : main.o tavl.o
		gcc -o test main.o tavl.o
main.o : main.c tavl.h
		gcc -c main.c
tavl.o : tavl.c tavl.h
		gcc -c tavl.c
clean:
	rm -f test main.o tavl.o
