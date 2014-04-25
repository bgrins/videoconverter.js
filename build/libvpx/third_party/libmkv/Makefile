#Variables
CC=gcc
LINKER=gcc
FLAGS=-g -Wall

all: testlibmkv

#Build Targets
EbmlWriter.o: EbmlWriter.c EbmlWriter.h
	$(CC) $(FLAGS) -c EbmlWriter.c

EbmlBufferWriter.o: EbmlBufferWriter.c EbmlBufferWriter.h
	$(CC) $(FLAGS) -c EbmlBufferWriter.c

WebMElement.o: WebMElement.c WebMElement.h
	$(CC) $(FLAGS) -c WebMElement.c

testlibmkv.o: testlibmkv.c
	$(CC) $(FLAGS) -c testlibmkv.c

testlibmkv: testlibmkv.o WebMElement.o EbmlBufferWriter.o EbmlWriter.o
	$(LINKER) $(FLAGS) -o testlibmkv testlibmkv.o WebMElement.o EbmlBufferWriter.o EbmlWriter.o

clean:
	rm -rf *.o testlibmkv
