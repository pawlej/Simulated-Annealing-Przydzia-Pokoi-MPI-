# ============================================================
#  Makefile – Monte Carlo SA, Przydial Pokoi (MPI)
#  AGH WFiIS
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

# ---- Przykladowy hostfile (16 wezlow po 1 slocie) ----
# Skopiuj do pliku 'hostfile' i dostosuj nazwy/adresy wezlow:
#
# node01 slots=1
# node02 slots=1
# node03 slots=1
# node04 slots=1
# node05 slots=1
# node06 slots=1
# node07 slots=1
# node08 slots=1
# node09 slots=1
# node10 slots=1
# node11 slots=1
# node12 slots=1
# node13 slots=1
# node14 slots=1
# node15 slots=1
# node16 slots=1
