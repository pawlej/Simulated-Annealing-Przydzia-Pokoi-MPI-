# Instrukcja uruchomienia projektu MPI na Taurusie

Potrzebne są 3 pliki z SimAnnMPI:

``` txt
room_sa.c
Makefile
dane.txt
```

Plik `dane.txt` powinien zaczynać się od liczby `N`, np.:

``` txt
20
```

Potem macierz preferencji:

``` txt
10 2 ... P_N-1
5 4 ... P_N-1
: :
P_N-1 P_N-1
```

------------------------------------------------------------------------

## 1. Na turusie:

``` bash
mkdir ~/projekt
```

``` bash
cd ~/projekt
```

Następnie kopiujemy tam `room_sa.c`, `Makefile` i `dane.txt`.

Sprawdź zawartość katalogu:

``` bash
ls
```

Powinno być widać między innymi:

``` txt
room_sa.c  Makefile  dane.txt
```

------------------------------------------------------------------------

## 2. Załadowanie MPICH

Na Taurusie trzeba załadować środowisko MPICH:

``` bash
source /opt/nfs/config/source_mpich32.sh
```

Sprawdzić czy narzędzia MPI są dostępne:

``` bash
which mpicc
which mpiexec
```

Poprawny wynik powinien wyglądać mniej więcej tak:

``` txt
/opt/nfs/mpich-3.2/bin/mpicc
/opt/nfs/mpich-3.2/bin/mpiexec
```

------------------------------------------------------------------------

## 3. Kompilacja projektu

W katalogu projektu:

``` bash
make clean
make
```

Po kompilacji powinien powstać plik wykonywalny:

``` txt
room_sa
```

Można sprawdzić:

``` bash
ls -l room_sa
```

------------------------------------------------------------------------

## 4. Przygotowanie listy aktywnych komputerów

Przed każdym uruchomieniem programu MPI warto wygenerować aktualną listę dostępnych stacji:

``` bash
/opt/nfs/config/station204_name_list.sh 1 16 > nodes
```

które stacje są aktywne:

``` bash
cat nodes
```

Przykładowy wynik:

``` txt
stud204-06
stud204-15
stud204-13
stud204-10
stud204-01
stud204-05
```

Liczbę aktywnych stacji można sprawdzić tak:

``` bash
cat nodes | wc -l
```

Uruchomienie:

``` bash
mpiexec -f nodes -n $(cat nodes | wc -l) ./room_sa dane.txt
```

------------------------------------------------------------------------

## 5. Przygotowanie biblioteki `libgfortran`(jeśli nie działa)

Na tym klastrze może pojawić się błąd:

``` txt
./room_sa: error while loading shared libraries: libgfortran.so.3: cannot open shared object file: No such file or directory
```

Dlatego przed uruchomieniem programu wykonaj:

``` bash
cp /usr/lib/x86_64-linux-gnu/libgfortran.so.3* .
export LD_LIBRARY_PATH=$PWD:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
```

Sprawdź, czy zmienna środowiskowa się ustawiła:

``` bash
echo $LD_LIBRARY_PATH
```

`LD_LIBRARY_PATH` powinien wskazywać katalog, a nie konkretny plik biblioteki.

Poprawnie:

``` bash
export LD_LIBRARY_PATH=$PWD:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
```

Niepoprawnie:

``` bash
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/libgfortran.so.3
```

------------------------------------------------------------------------

## 6. Uruchomienie programu na klastrze, jeśli był błąd z fortranem

### Wariant podstawowy: jeden proces na każdy aktywny komputer

``` bash
mpiexec -f nodes \
  -genv LD_LIBRARY_PATH "$LD_LIBRARY_PATH" \
  -n $(cat nodes | wc -l) \
  ./room_sa dane.txt
```

------------------------------------------------------------------------

### Wariant mocniejszy: dwa procesy na każdy aktywny komputer

``` bash
mpiexec -f nodes \
  -genv LD_LIBRARY_PATH "$LD_LIBRARY_PATH" \
  -n $(( 2 * $(cat nodes | wc -l) )) \
  ./room_sa dane.txt
```

------------------------------------------------------------------------

## 7. Sprawdzenie wyniku

Program powinien wypisać wynik w terminalu i utworzyć plik:

``` txt
wyniki.txt
```

------------------------------------------------------------------------

# Całość do skopiowania naraz(wraz z niedziałającą libgfortran)

Najpierw można odpalić ten zestaw komend:

``` bash
cd ~/projekt

source /opt/nfs/config/source_mpich32.sh

make clean
make

/opt/nfs/config/station204_name_list.sh 1 16 > nodes

cat nodes

cp /usr/lib/x86_64-linux-gnu/libgfortran.so.3* .

export LD_LIBRARY_PATH=$PWD:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

mpiexec -f nodes \
  -genv LD_LIBRARY_PATH "$LD_LIBRARY_PATH" \
  -n $(cat nodes | wc -l) \
  ./room_sa dane.txt
```

------------------------------------------------------------------------
