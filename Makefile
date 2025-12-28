CC=gcc
CFLAGS=-O3 -Wall -Wextra -std=c11
OMPFLAG=-fopenmp
INCLUDES=-Iinclude
LIBS=-lm

CORE=src/utils.c src/ded_data.c src/ded_model.c src/firefly.c

VALIDATE=src/main_validate.c
SEQ=src/main_seq.c
OMP=src/main_omp.c

all: validate seq omp

validate:
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(VALIDATE) $(LIBS) -o bin_validate

seq:
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ) $(LIBS) -o bin_seq

omp:
	$(CC) $(CFLAGS) $(OMPFLAG) $(INCLUDES) $(CORE) $(OMP) $(LIBS) -o bin_omp

clean:
	rm -f bin_validate bin_seq bin_omp
