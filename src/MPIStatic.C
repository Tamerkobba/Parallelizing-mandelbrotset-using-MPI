#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 640
#define HEIGHT 480
#define MAX_ITER 255
#define NUM_TRIALS 10 // Define the number of trials

struct complex {
    double real;
    double imag;
};

int cal_pixel(struct complex c) {
    double z_real = 0, z_imag = 0;
    int iter = 0;
    while (z_real * z_real + z_imag * z_imag < 4 && iter < MAX_ITER) {
        double temp = z_real * z_real - z_imag * z_imag + c.real;
        z_imag = 2 * z_real * z_imag + c.imag;
        z_real = temp;
        iter++;
    }
    return iter;
}

void save_pgm(const char *filename, int *image) {
    FILE *pgmimg = fopen(filename, "wb");
    fprintf(pgmimg, "P2\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            fprintf(pgmimg, "%d ", image[i * WIDTH + j]);
        }
        fprintf(pgmimg, "\n");
    }
    fclose(pgmimg);
}
int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double total_cpu_time_used = 0; // Accumulate CPU time used for all trials
    double total_comp_time = 0; // Accumulate computation time for all trials

    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        int rows_per_process = HEIGHT / size;
        int remainder_rows = HEIGHT % size;
        if (rank < remainder_rows) {
            rows_per_process++;
        }
        int *local_image = (int *)malloc(rows_per_process * WIDTH * sizeof(int));

        clock_t start_time = clock(), comp_start, comp_end;
        double comp_time_used = 0; // Computation time for this trial

        comp_start = clock(); // Start timing computation
        for (int i = 0; i < rows_per_process; i++) {
            for (int j = 0; j < WIDTH; j++) {
                struct complex c;
                c.real = (j - WIDTH / 2.0) * 4.0 / WIDTH;
                c.imag = ((rank * rows_per_process + i) - HEIGHT / 2.0) * 4.0 / HEIGHT;
                local_image[i * WIDTH + j] = cal_pixel(c);
            }
        }
        comp_end = clock(); // End timing computation
        comp_time_used = ((double)(comp_end - comp_start)) / CLOCKS_PER_SEC;
        total_comp_time += comp_time_used; // Accumulate computation time

        if (rank == 0) {
            int *image = (int *)malloc(HEIGHT * WIDTH * sizeof(int));
            int current_row = 0;
            for (int i = 0; i < rows_per_process; i++, current_row++) {
                for (int j = 0; j < WIDTH; j++) {
                    image[current_row * WIDTH + j] = local_image[i * WIDTH + j];
                }
            }

            MPI_Status status;
            for (int proc = 1; proc < size; proc++) {
                int rows_from_proc = HEIGHT / size;
                if (proc < remainder_rows) {
                    rows_from_proc++;
                }
                int *temp_image = (int *)malloc(rows_from_proc * WIDTH * sizeof(int));
                MPI_Recv(temp_image, rows_from_proc * WIDTH, MPI_INT, proc, 0, MPI_COMM_WORLD, &status);
                for (int i = 0; i < rows_from_proc; i++, current_row++) {
                    for (int j = 0; j < WIDTH; j++) {
                        image[current_row * WIDTH + j] = temp_image[i * WIDTH + j];
                    }
                }
                free(temp_image);
            }
            clock_t end_time = clock();
            double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
            total_cpu_time_used += cpu_time_used; // Accumulate total CPU time

            if (trial == NUM_TRIALS - 1) { // Print average times on last trial
                printf("Average CPU time used (including communication): %f seconds\n", total_cpu_time_used / NUM_TRIALS);
                printf("Average computation time only: %f seconds\n", total_comp_time / NUM_TRIALS);
                save_pgm("mandelbrot_mpi.pgm", image);
            }
            free(image);
        } else {
            MPI_Send(local_image, rows_per_process * WIDTH, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
        free(local_image);
    }

    MPI_Finalize();
    return 0;
}
