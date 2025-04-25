PROGS=client server
all: mylib.so $(PROGS)

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -L../lib -I../include -c mylib.c

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

server: server.c mylib.so
	gcc -Wall -fPIC -DPIC -L../lib -I../include -o server server.c ../lib/libdirtree.so

clean:
	rm -f *.o *.so $(PROGS)