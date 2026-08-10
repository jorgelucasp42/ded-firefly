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
ACC_SRC=src/firefly/firefly_openacc.c src/apps/main_acc.c
ACC100_SRC=src/firefly/firefly_openacc.c src/apps/main_acc_100.c
ACC200_SRC=src/firefly/firefly_openacc.c src/apps/main_acc_200.c
ACC500_SRC=src/firefly/firefly_openacc.c src/apps/main_acc_500.c
ACC_FULL_SRC=src/firefly/firefly_openacc_full.c src/apps/main_acc.c
ACC_POP_SRC=src/firefly/firefly_openacc_pop.c src/apps/main_acc_pop.c
ACC_SYNC_SRC=src/firefly/firefly_openacc_sync.c src/apps/main_acc_sync.c
ACC_SYNC100_SRC=src/firefly/firefly_openacc_sync.c src/apps/main_acc_sync_100.c
VALIDATE_SRC=src/apps/main_validate.c

.PHONY: all validate seq omp acc acc_full acc_pop acc_sync acc_sync100 seq100 omp100 acc100 seq200 omp200 acc200 omp500 acc500 seq500 clean

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

acc:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC_SRC) $(LIBS) -o bin/bin_acc

acc_full:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC_FULL_SRC) $(LIBS) -o bin/bin_acc_full

acc_pop:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC_POP_SRC) $(LIBS) -o bin/bin_acc_pop

acc_sync:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC_SYNC_SRC) $(LIBS) -o bin/bin_acc_sync

acc_sync100:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC_SYNC100_SRC) $(LIBS) -o bin/bin_acc_sync100

seq100:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ100_SRC) $(LIBS) -o bin/bin_seq100

seq200:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ200_SRC) $(LIBS) -o bin/bin_seq200

omp100:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP100_SRC) $(LIBS) -o bin/bin_omp100

acc100:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC100_SRC) $(LIBS) -o bin/bin_acc100

omp200:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP200_SRC) $(LIBS) -o bin/bin_omp200

acc200:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC200_SRC) $(LIBS) -o bin/bin_acc200

omp500:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) $(OMP500_SRC) $(LIBS) -o bin/bin_omp500

acc500:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) $(ACC500_SRC) $(LIBS) -o bin/bin_acc500

seq500:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(SEQ500_SRC) $(LIBS) -o bin/bin_seq500

clean:
	rm -f bin/bin_validate bin/bin_seq bin/bin_omp bin/bin_acc bin/bin_acc_full bin/bin_acc_pop bin/bin_acc_sync bin/bin_acc_sync100 bin/bin_seq100 bin/bin_seq200 bin/bin_omp100 bin/bin_acc100 bin/bin_omp200 bin/bin_acc200 bin/bin_omp500 bin/bin_acc500 bin/bin_seq500

bench_seq:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_seq.c src/apps/main_seq_bench.c $(LIBS) -o bin/bin_seq_bench

bench_acc:
	mkdir -p bin
	$(ACC_CC) $(ACC_FLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_openacc_sync.c src/apps/main_acc_sync_bench.c $(LIBS) -o bin/bin_acc_bench

bench_omp:
	mkdir -p bin
	$(CC) $(OPENMP_FLAGS) $(CFLAGS) $(INCLUDES) $(CORE) src/firefly/firefly_omp.c src/apps/main_omp_bench.c $(LIBS) -o bin/bin_omp_bench
