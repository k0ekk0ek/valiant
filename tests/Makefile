CC=gcc
CFLAGS=-Wall -Wunreachable-code -Wno-pointer-sign -I../include -g
LDFLAGS=-lpthread -lunistring -lcunit

all:
	$(CC) $(CFLAGS) ../src/value.c value.c $(LDFLAGS) -o value
	$(CC) $(CFLAGS) ../src/string.c string.c $(LDFLAGS) -o string
	$(CC) $(CFLAGS) ../src/value.c ../src/string.c ../src/lexer.c lexer.c $(LDFLAGS) -o lexer