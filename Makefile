MakefileCC=gcc
CFLAGS=-O3 -Wall -Wextra -std=c11
OMPFLAG=-fopenmp
INCLUDES=-Iinclude

SRC=src/utils.c src/ded_data.c src/ded_model.c
VAL=src/main_validate.c

all: validate

validate:
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(VAL) -lm -o bin_validate

clean:
	rm -f bin_validate
