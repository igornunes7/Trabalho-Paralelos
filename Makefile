CC_SEQ = gcc
CC_PAR = mpicc

SEQ_TARGET = seq
PAR_TARGET = par

SEQ_SRC = seq.c
PAR_SRC = par.c

INPUT ?= /tmp/entrada.bin
MPIRUN ?= mpirun

HOSTFILE4 = hosts_mpi_4
HOSTFILE8 = hosts_mpi_8
HOSTFILE12 = hosts_mpi_12
HOSTFILE16 = hosts_mpi_16

CFLAGS = -O3 -flto -std=gnu11
LDFLAGS = -flto

PAR_PATH = /home/local/rgm47538/mpi/$(PAR_TARGET)

MPI_FLAGS = --mca btl_tcp_if_include eth0 \
            --mca oob_tcp_if_include eth0

.PHONY: all runseq run4 run8 run12 run16 clean

all: $(SEQ_TARGET) $(PAR_TARGET)

$(SEQ_TARGET): $(SEQ_SRC) Makefile
	$(CC_SEQ) $(CFLAGS) $(SEQ_SRC) -o $(SEQ_TARGET) $(LDFLAGS)

$(PAR_TARGET): $(PAR_SRC) Makefile
	$(CC_PAR) $(CFLAGS) $(PAR_SRC) -o $(PAR_TARGET) $(LDFLAGS)

runseq: $(SEQ_TARGET)
	./$(SEQ_TARGET) $(INPUT)

run4: $(PAR_TARGET)
	unset DISPLAY; $(MPIRUN) --hostfile $(HOSTFILE4) $(MPI_FLAGS) -np 4 $(PAR_PATH) $(INPUT)

run8: $(PAR_TARGET)
	unset DISPLAY; $(MPIRUN) --hostfile $(HOSTFILE8) $(MPI_FLAGS) -np 8 $(PAR_PATH) $(INPUT)

run12: $(PAR_TARGET)
	unset DISPLAY; $(MPIRUN) --hostfile $(HOSTFILE12) $(MPI_FLAGS) -np 12 $(PAR_PATH) $(INPUT)

run16: $(PAR_TARGET)
	unset DISPLAY; $(MPIRUN) --hostfile $(HOSTFILE16) $(MPI_FLAGS) -np 16 $(PAR_PATH) $(INPUT)

clean:
	rm -f $(SEQ_TARGET) $(PAR_TARGET) saida_seq.txt saida_par.txt
