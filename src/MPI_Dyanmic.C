#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 640
#define HEIGHT 480
#define MAX_ITER 255
#define NUM_TRIALS 10 // Number of times to run the computation

#define MPI_PERFORM_TAG 1
#define MPI_DONE_TAG 2
#define MPI_TERMINATE_TAG 3

struct complex {
    double real;
    double imag;
};

int cal_pixel(struct complex c) {
    double z_real = c.real, z_imag = c.imag;
    int n = 0;
    for (n = 0; n < MAX_ITER; n++) {
        double z_real2 = z_real * z_real, z_imag2 = z_imag * z_imag;
        if (z_real2 + z_imag2 > 4.0) break;
        z_imag = 2 * z_real * z_imag + c.imag;
        z_real = z_real2 - z_imag2 + c.real;
    }
    return n;
}

void save_pgm(const char *filename, int image[HEIGHT][WIDTH]) {
    FILE *file = fopen(filename, "w");
    fprintf(file, "P2\n%d %d\n%d\n", WIDTH, HEIGHT, MAX_ITER);
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            fprintf(file, "%d ", image[i][j]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double total_time = 0.0; // Accumulate total execution time

    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        int image[HEIGHT][WIDTH] = {0};
        clock_t start_time, end_time;

        if (rank == 0) {
            start_time = clock();
            int master_row = 0;
            int worker_row[WIDTH + 1];

            for (int i = 1; i < size; i++) {
                MPI_Send(&master_row, 1, MPI_INT, i, MPI_PERFORM_TAG, MPI_COMM_WORLD);
                master_row++;
            }

            MPI_Status status;
            while (master_row < HEIGHT) {
                MPI_Recv(worker_row, WIDTH + 1, MPI_INT, MPI_ANY_SOURCE, MPI_DONE_TAG, MPI_COMM_WORLD, &status);
                int row = worker_row[WIDTH]; 
                for (int j = 0; j < WIDTH; j++) {
                    image[row][j] = worker_row[j];
                }
                if (master_row < HEIGHT) {
                    MPI_Send(&master_row, 1, MPI_INT, status.MPI_SOURCE, MPI_PERFORM_TAG, MPI_COMM_WORLD);
                    master_row++;
                }
            }

            for (int i = 1; i < size; i++) {
                MPI_Send(NULL, 0, MPI_INT, i, MPI_TERMINATE_TAG, MPI_COMM_WORLD);
            }

            end_time = clock();
            total_time += ((double) (end_time - start_time)) / CLOCKS_PER_SEC;

            if (trial == NUM_TRIALS - 1) { // Save image only at the last trial
                save_pgm("mandelbrot_mpi_dynamic.pgm", image);
            }
        } else {
            int my_row[WIDTH + 1];
            MPI_Status status;
            while (1) {
                int row_number;
                MPI_Recv(&row_number, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                if (status.MPI_TAG == MPI_TERMINATE_TAG) break;
                for (int j = 0; j < WIDTH; j++) {
                    struct complex c = {.real = (j - WIDTH / 2.0) * 4.0 / WIDTH, .imag = (row_number - HEIGHT / 2.0) * 4.0 / HEIGHT};
                    my_row[j] = cal_pixel(c);
                }
                my_row[WIDTH] = row_number;
                MPI_Send(my_row, WIDTH + 1, MPI_INT, 0, MPI_DONE_TAG, MPI_COMM_WORLD);
            }
        }

        MPI_Barrier(MPI_COMM_WORLD); // Ensure all processes are synchronized before next trial
    }

    if (rank == 0) {
        printf("The average execution time over %d trials is: %f seconds\n", NUM_TRIALS, total_time / NUM_TRIALS);
    }

    MPI_Finalize();
    return 0;
}
