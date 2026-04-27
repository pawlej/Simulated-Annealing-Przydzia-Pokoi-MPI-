/**
 * ============================================================
 *  Równoległy Simulated Annealing – Przydział Pokoi (MPI)
 *  Akademia Górniczo-Hutnicza, WFiIS
 * ============================================================
 *
 * Problem:
 *   Mamy N gości i N pokoi. Każdy gość ma preferencje wzgledem
 *   kazdego pokoju (wartosci 0..MAX_PREF). Szukamy bijekcji
 *   (permutacji) gosc->pokoj minimalizujacej sumaryczny koszt
 *   "niezadowolenia": sum(MAX_PREF - pref[i][assign[i]]).
 *
 * Strategia rownoleglá (model wyspowyy):
 *   - Kazdy z P procesow MPI prowadzi niezalezny lancuch SA
 *     z losowym stanem startowym (Monte Carlo).
 *   - Co SYNC_INTERVAL iteracji procesy wymieniaja najlepsze
 *     rozwiazania (Allreduce min + Bcast stanu globalnego opt.)
 *   - Na koncu rank 0 zbiera wyniki, drukuje i zapisuje do pliku.
 * ============================================================
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <limits.h>

/* ---- Domyslne parametry problemu (nadpisywane z pliku) ---- */
#define MAX_N         200     /* maks. rozmiar macierzy preferencji */
#define MAX_PREF      10      /* maks. wartosc preferencji          */

/* ---- Parametry SA ---- */
#define T_START       150.0
#define T_END         0.0001
#define ALPHA         0.99980
#define MAX_ITER      1000000
#define SYNC_INTERVAL 10000
#define INJECT_PROB   0.25    /* prawdopodobienstwo iniekcji glob.  */

/* ================================================================ */

static int N = 0;  /* rzeczywisty rozmiar (z pliku danych) */
static int pref[MAX_N][MAX_N];

/* ---------- pomocnicze RNG (thread-safe rand_r) ---------- */
static inline int rng_int(unsigned int *s, int mod) {
    return (int)(rand_r(s) % (unsigned int)mod);
}
static inline double rng_double(unsigned int *s) {
    return rand_r(s) / (double)RAND_MAX;
}

/* ---------- funkcja kosztu (pelna) ---------- */
static int full_cost(const int *assign) {
    int c = 0;
    for (int i = 0; i < N; i++)
        c += (MAX_PREF - pref[i][assign[i]]);
    return c;
}

/* ---------- losowa permutacja (Fisher-Yates) ---------- */
static void random_perm(int *a, unsigned int *s) {
    for (int i = 0; i < N; i++) a[i] = i;
    for (int i = N - 1; i > 0; i--) {
        int j = rng_int(s, i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

/* ---------- wczytanie danych z pliku ---------- */
static int load_data(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) { perror(fname); return -1; }
    if (fscanf(f, "%d", &N) != 1 || N < 1 || N > MAX_N) {
        fprintf(stderr, "Blad: nieprawidlowy rozmiar N.\n");
        fclose(f); return -1;
    }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (fscanf(f, "%d", &pref[i][j]) != 1) {
                fprintf(stderr, "Blad: za malo danych w pliku.\n");
                fclose(f); return -1;
            }
    fclose(f);
    return 0;
}

/* ================================================================
 *  GLOWNA FUNKCJA
 * ================================================================ */
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* ---- wczytaj dane na rank 0, rozsil do pozostalych ---- */
    if (rank == 0) {
        const char *fname = (argc > 1) ? argv[1] : "dane.txt";
        if (load_data(fname) != 0) { MPI_Abort(MPI_COMM_WORLD, 1); }
        if (rank == 0)
            printf("Wczytano dane: N=%d gosci/pokoi, %d procesow MPI\n",
                   N, size);
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(pref, MAX_N * MAX_N, MPI_INT, 0, MPI_COMM_WORLD);

    /* ---- seed Monte Carlo unikalny dla kazdego procesu ---- */
    unsigned int seed = (unsigned int)(time(NULL) * 1009UL)
                        ^ ((unsigned int)rank * 2654435761U);

    /* ---- stan lokalny ---- */
    int *current    = malloc(N * sizeof(int));
    int *neighbour  = malloc(N * sizeof(int));
    int *best_local = malloc(N * sizeof(int));
    int *gbest      = malloc(N * sizeof(int));  /* globalny najlepszy */

    random_perm(current, &seed);
    memcpy(best_local, current, N * sizeof(int));

    int cost_cur  = full_cost(current);
    int cost_best = cost_cur;

    double T = T_START;
    double t_wall_start = MPI_Wtime();

    /* ================================================================
     *  Petla SA
     * ================================================================ */
    for (long iter = 1; iter <= MAX_ITER; iter++) {

        /* -- ruch: zamiana pokoi dwoch losowych gosci (swap) -- */
        int g1 = rng_int(&seed, N);
        int g2;
        do { g2 = rng_int(&seed, N); } while (g2 == g1);

        /* -- delta kosztu (O(1) – tylko zmienione pozycje) -- */
        int d = - (MAX_PREF - pref[g1][current[g1]])
                - (MAX_PREF - pref[g2][current[g2]]);
        int tmp_room = current[g1];
        current[g1] = current[g2];
        current[g2] = tmp_room;
        d += (MAX_PREF - pref[g1][current[g1]])
           + (MAX_PREF - pref[g2][current[g2]]);

        /* -- kryterium Metropolisa -- */
        if (d < 0 || rng_double(&seed) < exp(-(double)d / T)) {
            cost_cur += d;
            if (cost_cur < cost_best) {
                cost_best = cost_cur;
                memcpy(best_local, current, N * sizeof(int));
            }
        } else {
            /* cofnij ruch */
            tmp_room = current[g1];
            current[g1] = current[g2];
            current[g2] = tmp_room;
        }

        /* -- chlodzenie -- */
        T *= ALPHA;
        if (T < T_END) T = T_END;

        /* ============================================================
         *  Synchronizacja co SYNC_INTERVAL
         * ============================================================ */
        if (iter % SYNC_INTERVAL == 0) {

            /* globalny min koszt + rank wlasciciela */
            struct { int cost; int rank; } local_v = { cost_best, rank },
                                           global_v;
            MPI_Allreduce(&local_v, &global_v, 1,
                          MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);

            /* broadcast najlepszego przydzialu */
            int *bcast_buf = (rank == global_v.rank) ? best_local : gbest;
            MPI_Bcast(bcast_buf, N, MPI_INT, global_v.rank, MPI_COMM_WORLD);
            if (rank != global_v.rank)
                memcpy(gbest, bcast_buf, N * sizeof(int));
            else
                memcpy(gbest, best_local, N * sizeof(int));

            /* iniekcja globalnego optimum do lancucha lokalnego */
            if (global_v.cost < cost_best ||
                rng_double(&seed) < INJECT_PROB) {
                memcpy(current, gbest, N * sizeof(int));
                cost_cur = global_v.cost;
                if (global_v.cost < cost_best) {
                    cost_best = global_v.cost;
                    memcpy(best_local, gbest, N * sizeof(int));
                }
            }

            /* raport posteppu (co 100k) */
            if (rank == 0 && iter % 100000 == 0) {
                printf("  iter %7ld | T=%8.4f | koszt_glob=%d\n",
                       iter, T, global_v.cost);
                fflush(stdout);
            }
        }
    }

    /* ================================================================
     *  Zbieranie wynikow koncowych
     * ================================================================ */
    /* Gather: kazdy wysyla [koszt(1) + przydzia(N)] do rank 0 */
    int *send_buf = malloc((1 + N) * sizeof(int));
    send_buf[0] = cost_best;
    memcpy(send_buf + 1, best_local, N * sizeof(int));

    int *all_buf = NULL;
    if (rank == 0)
        all_buf = malloc(size * (1 + N) * sizeof(int));

    MPI_Gather(send_buf, 1 + N, MPI_INT,
               all_buf,  1 + N, MPI_INT,
               0, MPI_COMM_WORLD);

    double t_wall_end = MPI_Wtime();

    if (rank == 0) {
        /* szukaj globalnego optimum */
        int gmin_cost = all_buf[0];
        int gmin_rank = 0;
        for (int r = 1; r < size; r++) {
            int c = all_buf[r * (1 + N)];
            if (c < gmin_cost) { gmin_cost = c; gmin_rank = r; }
        }
        memcpy(gbest, all_buf + gmin_rank * (1 + N) + 1, N * sizeof(int));
        free(all_buf);

        /* ---- wydruk na stdout ---- */
        printf("\n");
        printf("============================================================\n");
        printf("  Monte Carlo Simulated Annealing -- Przydial Pokoi (MPI)  \n");
        printf("============================================================\n");
        printf("  Goscie/Pokoje : %d\n", N);
        printf("  Procesy MPI   : %d\n", size);
        printf("  Iteracje/proc : %d\n", MAX_ITER);
        printf("  T_start=%.1f  T_end=%.4f  alpha=%.5f\n",
               T_START, T_END, ALPHA);
        printf("------------------------------------------------------------\n");
        printf("  Koszt globalny   : %d  (maks. mozliwy: %d)\n",
               gmin_cost, N * MAX_PREF);
        printf("  Srednia preferencja na goscia: %.2f / %d\n",
               (double)(N * MAX_PREF - gmin_cost) / N + 0.0, MAX_PREF);
        printf("  Czas obliczen    : %.2f s\n", t_wall_end - t_wall_start);
        printf("  Najlepszy rank   : %d\n", gmin_rank);
        printf("------------------------------------------------------------\n");
        printf("  Przydial (gosc -> pokoj) [pierwsze 20]:\n");
        int show = (N < 20) ? N : 20;
        for (int i = 0; i < show; i++)
            printf("    Gosc %3d -> Pokoj %3d  (pref=%2d)\n",
                   i, gbest[i], pref[i][gbest[i]]);
        if (N > show) printf("    ... (%d dalszych wierszy)\n", N - show);

        /* weryfikacja permutacji */
        int *used = calloc(N, sizeof(int));
        int ok = 1;
        for (int i = 0; i < N; i++) {
            if (gbest[i] < 0 || gbest[i] >= N || used[gbest[i]]++) {
                ok = 0; break;
            }
        }
        free(used);
        printf("  Weryfikacja permutacji: %s\n",
               ok ? "OK - poprawna bijekacja" : "BLAD!");
        printf("============================================================\n");

        /* ---- zapis wynikow do pliku ---- */
        FILE *fout = fopen("wyniki.txt", "w");
        if (fout) {
            fprintf(fout, "# Wyniki: Monte Carlo SA - Przydial Pokoi\n");
            fprintf(fout, "# N=%d  procesy=%d  koszt=%d\n",
                    N, size, gmin_cost);
            fprintf(fout, "# czas=%.2fs\n", t_wall_end - t_wall_start);
            fprintf(fout, "KOSZT %d\n", gmin_cost);
            fprintf(fout, "PERMUTACJA\n");
            for (int i = 0; i < N; i++)
                fprintf(fout, "%d %d %d\n", i, gbest[i], pref[i][gbest[i]]);
            fclose(fout);
            printf("  Wyniki zapisane do pliku: wyniki.txt\n");
        }

        free(gbest);
    }

    free(current); free(neighbour); free(best_local);
    free(send_buf);

    MPI_Finalize();
    return 0;
}
