#include <stdio.h>
#include <time.h>
#include <mpi.h>
#include <stdlib.h>
#define WIDTH  640
#define HEIGHT  480
#define MAX_ITER  255

struct complex {
    double real;
    double imag;
};

int cal_pixel(struct complex c) {
    double z_real = 0;
    double z_imag = 0;
    double z_real2, z_imag2, lengthsq;
    int iter = 0;
    do {
        z_real2 = z_real * z_real;
        z_imag2 = z_imag * z_imag;
        z_imag = 2 * z_real * z_imag + c.imag;
        z_real = z_real2 - z_imag2 + c.real;
        lengthsq = z_real2 + z_imag2;
        iter++;
    } while ((iter < MAX_ITER) && (lengthsq < 4.0));
    return iter;
}

void save_pgm(const char *filename, int image[HEIGHT][WIDTH]) {
    FILE *pgmimg;
    int temp;
    pgmimg = fopen(filename, "wb");
    fprintf(pgmimg, "P2\n"); // Writing Magic Number to the File
    fprintf(pgmimg, "%d %d\n", WIDTH, HEIGHT); // Writing Width and Height
    fprintf(pgmimg, "255\n");                   // Writing the maximum gray value
    int count = 0;

    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            temp = image[i][j];
            fprintf(pgmimg, "%d ", temp); // Writing the gray values in the 2D array to the file
        }
        fprintf(pgmimg, "\n");
    }
    fclose(pgmimg);
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        fprintf(stderr, "This program requires at least 2 MPI tasks.\n");
        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {
        // Master process
        int *image = malloc(WIDTH * HEIGHT * sizeof(int));
        int current_row = 0;
        MPI_Status status;
        int worker;

        for (worker = 1; worker < size; worker++) {
            if (current_row < HEIGHT) {
                MPI_Send(&current_row, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);
                current_row++;
            } else {
                break; // No more rows to distribute
            }
        }

        while (current_row < HEIGHT) {
            // Receive completed row from any worker
            MPI_Recv(image + (current_row * WIDTH), WIDTH, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            // Send a new row to the worker that just finished
            MPI_Send(&current_row, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
            current_row++;
        }

        // Collect the remaining rows from workers
        for (worker = 1; worker < size; worker++) {
            MPI_Recv(image + (current_row * WIDTH), WIDTH, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        }

        // Send termination signal to workers
        current_row = -1; // Use -1 as termination signal
        for (worker = 1; worker < size; worker++) {
            MPI_Send(&current_row, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);
        }

        save_pgm("mandelbrot_mpi_dynamic.pgm", image, WIDTH, HEIGHT);
        free(image);
    } else {
        // Worker process
        int row;
        while (1) {
            MPI_Recv(&row, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (row == -1) break; // Termination signal received

            int *row_data = malloc(WIDTH * sizeof(int));
            // Compute the Mandelbrot set for the assigned row here
            MPI_Send(row_data, WIDTH, MPI_INT, 0, 0, MPI_COMM_WORLD);
            free(row_data);
        }
    }

    MPI_Finalize();
    return 0;
}

