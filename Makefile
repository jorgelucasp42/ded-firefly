CC=gcc
CFLAGS=-O3 -Wall -Wextra -std=c11
OMPFLAG=-fopenmp
INCLUDES=-Iinclude
LIBS=-lm

CORE=src/utils.c src/ded_data.c src/ded_model.c src/firefly.c

VALIDATE=src/main_validate.c
SEQ=src/main_seq.c
OMP=src/main_omp.c

VALIDATE30=src/main_validate30.c
SEQ30=src/main_seq30.c
OMP30=src/main_omp30.c

all: validate seq omp
all30: validate30 seq30 omp30

validate:
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(VALIDATE) $(LIBS) -o bin_validate

seq:
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ) $(LIBS) -o bin_seq

omp:
	$(CC) $(CFLAGS) $(OMPFLAG) $(INCLUDES) $(CORE) $(OMP) $(LIBS) -o bin_omp

validate30:
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(VALIDATE30) $(LIBS) -o bin_validate30

seq30:
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ30) $(LIBS) -o bin_seq30

omp30:
	$(CC) $(CFLAGS) $(OMPFLAG) $(INCLUDES) $(CORE) $(OMP30) $(LIBS) -o bin_omp30

clean:
	rm -f bin_validate bin_seq bin_omp bin_validate30 bin_seq30 bin_omp30
