#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define COUNT_PTH 4

int** matrixa;
int** matrixb;
int** matrixc;
int arg[COUNT_PTH];

int n, k, m, k1;

void read_matrix(FILE* f, int* n, int* m, int*** a)
{
    int i, j;
    fscanf(f, "%d%d", n, m);
    *a = (int**)malloc((*n) * sizeof(int*));
    for (i = 0; i < *n; i++)
    {
        (*a)[i] = (int*)malloc((*m) * sizeof(int));
        for (j = 0; j < *m; j++)
            fscanf(f, "%d", &(*a)[i][j]);
    }
}

void free_matrix(int** a, int n)
{
    int i;
    for (i = 0; i < n; i++)
        free(a[i]);
    free(a);
}

void output_matrix(int** a, int n, int m)
{
    int i, j;
    for (i = 0; i < n; i++)
    {
        for (j = 0; j < m; j++)
        {
            printf("%d ", a[i][j]);
        }
        printf("\n");
    }
}

void* calc(void* arg)
{
    int i, j, z;
    int number = *(int*)arg;
    for (i = number; i < n; i += COUNT_PTH)
        for (j = 0; j < m; j++)
        {
            matrixc[i][j] = 0;
            for (z = 0; z < k; z++)
                matrixc[i][j] += matrixa[i][z] * matrixb[z][j];
        }
    return NULL;
}

int main(int argc, char** argv)
{
    FILE *f1, *f2;
    int i;
    pthread_t pth[COUNT_PTH];
    if (argc < 2)
    {
       fprintf(stderr, "Name of input files expected\n");
       return 1;
    }
    f1 = fopen(argv[1], "r");
    f2 = fopen(argv[2], "r");
    if (f1 == NULL || f2 == NULL)
    {
        fprintf(stderr, "Unable to open input files\n");
        return 1;
    }
    read_matrix(f1, &n, &k, &matrixa);
    read_matrix(f2, &k1, &m, &matrixb);
    if (k != k1)
    {
        fprintf(stderr, "Unable to multiply matrices: invalid sizes");
        free_matrix(matrixa, n);
        free_matrix(matrixb, k1);
        return 1;
    }
    matrixc = (int**)malloc(n * sizeof(int*));
    for (i = 0; i < n; i++)
        matrixc[i] = (int*)malloc(m * sizeof(int));
    for (i = 0; i < COUNT_PTH; i++)
    {
        arg[i] = i;
        pthread_create(&pth[i], NULL, calc, &arg[i]);
    }
    for (i = 0; i < COUNT_PTH; i++)
        pthread_join(pth[i], NULL);
    output_matrix(matrixc, n, m);
    return 0;
}
