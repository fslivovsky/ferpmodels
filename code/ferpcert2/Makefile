
CC  = gcc
CFLAGS = -Wall -g
DEP = formula.h admin.h proof.h
OBJ = formula.o admin.o ferpcert.o proof.o extract.o simpleaig.o
LIBS = 

ferpcert: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(DEP)
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS)

clean: 
	rm *.o ferpcert
