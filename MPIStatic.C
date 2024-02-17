#include <stdio.h>
#include <mpi.h>
#include <time.h>

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
    FILE* pgmimg;
    int temp;
    pgmimg = fopen(filename, "wb");
    fprintf(pgmimg, "P2\n"); // Writing Magic Number to the File
    fprintf(pgmimg, "%d %d\n", WIDTH, HEIGHT);  // Writing Width and Height
    fprintf(pgmimg, "255\n");  // Writing the maximum gray value
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
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Calculate rows per process
    int rows_per_process = HEIGHT / (size - 1); // Adjusted to exclude master

    // Create a 2D array to store the image
    int image[HEIGHT][WIDTH];

    double start_time, end_time;

    start_time = MPI_Wtime(); // Start measuring time

    if (rank == 0) {
        // Master Process
        for (int i = 1; i < size; i++) {
            int row_start = (i - 1) * rows_per_process;
            int row_end = row_start + rows_per_process;
            MPI_Send(&row_start, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // Send start row number to slave process
            MPI_Send(&row_end, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // Send end row number to slave process
        }

        // Receive coordinates and colors from slave processes and update image
        for (int i = 1; i < size; i++) {
            int row_start = (i - 1) * rows_per_process;
            int row_end = row_start + rows_per_process;
            for (int row = row_start; row < row_end; row++) {
                for (int j = 0; j < WIDTH; j++) {
                    struct complex c;
                    MPI_Recv(&c, sizeof(struct complex), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    int color = cal_pixel(c);
                    image[row][j] = color;
                }
            }
        }

        // Save the image
        save_pgm("mandelbrot_mpi.pgm", image);

        end_time = MPI_Wtime(); // End measuring time
        printf("Total execution time: %f seconds\n", end_time - start_time);
    } else {
        // Slave Processes
        int row_start, row_end;
        MPI_Recv(&row_start, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // Receive start row number from master
        MPI_Recv(&row_end, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // Receive end row number from master

        // Compute rows assigned to this slave process and send coordinates and colors back
        for (int row = row_start; row < row_end; row++) {
            for (int j = 0; j < WIDTH; j++) {
                struct complex c;
                c.real = (j - WIDTH / 2.0) * 4.0 / WIDTH;
                c.imag = (row - HEIGHT / 2.0) * 4.0 / HEIGHT;
                MPI_Send(&c, sizeof(struct complex), MPI_BYTE, 0, 0, MPI_COMM_WORLD); // Send coordinates to master
            }
        }
    }

    MPI_Finalize();
    return 0;
}
