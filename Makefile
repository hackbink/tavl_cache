
ifdef OS
	build = gcc -g 
	delete = del /Q
else
	ifeq ($(shell uname),Linux) 
		build = gcc -g -rdynamic -lSegFault
		delete = rm -f
	endif
endif

test : main.o tavl.o
		$(build) -o test main.o tavl.o
main.o : main.c tavl.h
		$(build) -O0 -c main.c
tavl.o : tavl.c tavl.h
		$(build) -O0 -c tavl.c

clean :
	$(delete) test test.exe main.o tavl.o

