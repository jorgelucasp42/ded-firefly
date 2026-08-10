CC=gcc
CFLAGS=-O3 -Wall -Wextra -std=c11
OPENMP_FLAGS=-fopenmp
ACC_CC ?= nvc
ACC_FLAGS ?= -O3 -acc -Minfo=accel
INCLUDES=-Iinclude
LIBS=-lm

CORE=src/core/utils.c src/core/ded_data.c src/core/ded_model.c src/core/ded_validation.c
SEQ_SRC=src/firefly/firefly_seq.c src/apps/main_seq.c
SEQ100_SRC=src/firefly/firefly_seq.c src/apps/main_seq_100.c
SEQ200_SRC=src/firefly/firefly_seq.c src/apps/main_seq_200.c
SEQ500_SRC=src/firefly/firefly_seq.c src/apps/main_seq_500.c
OMP_SRC=src/firefly/firefly_omp.c src/apps/main_omp.c
OMP100_SRC=src/firefly/firefly_omp.c src/apps/main_omp_100.c
OMP200_SRC=src/firefly/firefly_omp.c src/apps/main_omp_200.c
OMP500_SRC=src/firefly/firefly_omp.c src/apps/main_omp_500.c
ACC_SYNC_SRC=src/firefly/firefly_openacc_sync.c src/apps/main_acc_sync.c
VALIDATE_SRC=src/apps/main_validate.c

.PHONY: all validate seq omp acc_sync seq100 omp100 seq200 omp200 omp500 seq500 clean bench_seq bench_omp bench_acc bench_acc_sync

all: validate seq omp

validate:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(VALIDATE_SRC) $(LIBS) -o bin/bin_validate

seq:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ_SRC) $(LIBS) -o bin/bin_seq

omp:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP_SRC) $(LIBS) -o bin/bin_omp

acc_sync:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC_SYNC_SRC) $(LIBS) -o bin/bin_acc_sync

seq100:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ100_SRC) $(LIBS) -o bin/bin_seq100

seq200:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ200_SRC) $(LIBS) -o bin/bin_seq200

omp100:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP100_SRC) $(LIBS) -o bin/bin_omp100

omp200:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP200_SRC) $(LIBS) -o bin/bin_omp200

omp500:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP500_SRC) $(LIBS) -o bin/bin_omp500

seq500:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ500_SRC) $(LIBS) -o bin/bin_seq500

clean:
	rm -f bin/bin_validate bin/bin_seq bin/bin_omp bin/bin_acc_sync bin/bin_seq100 bin/bin_seq200 bin/bin_omp100 bin/bin_omp200 bin/bin_omp500 bin/bin_seq500 bin/bin_seq_bench bin/bin_omp_bench bin/bin_acc_bench bin/bin_acc_sync_bench

bench_seq:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_seq.c src/apps/main_seq_bench.c $(LIBS) -o bin/bin_seq_bench

bench_acc:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_openacc_sync.c src/apps/main_acc_sync_bench.c $(LIBS) -o bin/bin_acc_bench

bench_acc_sync:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_openacc_sync.c src/apps/main_acc_sync_bench.c $(LIBS) -o bin/bin_acc_sync_bench

bench_omp:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_omp.c src/apps/main_omp_bench.c $(LIBS) -o bin/bin_omp_bench
