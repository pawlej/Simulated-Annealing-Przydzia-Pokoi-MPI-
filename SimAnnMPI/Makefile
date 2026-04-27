# ============================================================
#  Makefile – Monte Carlo SA, Przydzial Pokoi (MPI)
# ============================================================

CC       = mpicc
CFLAGS   = -O2 -Wall -Wextra -std=c11
LDFLAGS  = -lm

TARGET   = room_sa
SRC      = room_sa.c
DATA     = dane.txt
NP       = 16

# ============================================================
.PHONY: all run clean

## all: kompilacja programu
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

## run: uruchomienie z danymi przykladowymi (lokalnie, NP procesow)
run: $(TARGET)
	mpirun -np $(NP) --oversubscribe ./$(TARGET) $(DATA)

## run_cluster: uruchomienie na klastrze (wymaga pliku hostfile)
run_cluster: $(TARGET)
	mpirun -np $(NP) --hostfile hostfile ./$(TARGET) $(DATA)

## clean: przywrocenie stanu wyjsciowego
clean:
	rm -f $(TARGET) wyniki.txt
