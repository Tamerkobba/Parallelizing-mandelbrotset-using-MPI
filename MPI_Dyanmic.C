
#include <stdio.h>
#include <mpi.h>
#include <time.h>
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

void save_pgm(const char *filename, int *image, int width, int height) {
    FILE* pgmimg;
    int temp;
    pgmimg = fopen(filename, "wb");
    fprintf(pgmimg, "P2\n"); // Writing Magic Number to the File
    fprintf(pgmimg, "%d %d\n", width, height);  // Writing Width and Height
    fprintf(pgmimg, "255\n");  // Writing the maximum gray value
    int count = 0;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            temp = image[i * width + j];
            fprintf(pgmimg, "%d ", temp); 
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
    int rows_per_process = HEIGHT / size;

    // Calculate starting and ending rows for the current process
    int start_row = rank * rows_per_process;
    int end_row = start_row + rows_per_process;

    // Allocate memory for the local portion of the image
    int local_height = rows_per_process;
    int local_width = WIDTH;
    int *local_image = (int *)malloc(local_height * local_width * sizeof(int));
    if (local_image == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    // Start measuring time
    double start_time = MPI_Wtime();

    // Calculate Mandelbrot set for the local portion
    for (int i = 0; i < local_height; i++) {
        for (int j = 0; j < local_width; j++) {
            struct complex c;
            c.real = (j - WIDTH / 2.0) * 4.0 / WIDTH;
            c.imag = (i + start_row - HEIGHT / 2.0) * 4.0 / HEIGHT;
            local_image[i * local_width + j] = cal_pixel(c);
        }
    }

    // End measuring time
    double end_time = MPI_Wtime();

    // Gather results from all processes
    int *image = NULL;
    if (rank == 0) {
        image = (int *)malloc(HEIGHT * WIDTH * sizeof(int));
        if (image == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    MPI_Gather(local_image, local_height * local_width, MPI_INT, image, local_height * local_width, MPI_INT, 0, MPI_COMM_WORLD);

    // Save the image and print execution time
    if (rank == 0) {
        printf("Total execution time: %f seconds\n", end_time - start_time);
        save_pgm("mandelbrot_mpi.pgm", image, WIDTH, HEIGHT);
        free(image);
    }

    free(local_image);
    MPI_Finalize();
    return 0;
}

