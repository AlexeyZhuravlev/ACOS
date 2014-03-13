#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>


void fill(int* array, int size, int* sum)
{
    int i;
    *sum = 0;
    for (i = 0; i < size; i++)
    {
        array[i] = rand() % 100;
        *sum += array[i];
    }
}

void swap(int* a,int *b)
{
    int c = *a;
    *a = *b;
    *b = c;
}

void generate_permutation_step(int* array, int size, int step)
{
    int i, position, j;
    position = 0;
    j=0;
    for (i=0; i < size; i++)
    {
        array[i] = position;
        position += step;
        if (position >= size)
            position = ++j;
    }
}

void generate_permutation(int* array, int size)
{
    int i;
    for (i = 0; i < size; i++)
        array[i] = i;
    for (i = 0; i< size * 10; i++)
    {
        int first, second;
        first = rand() % size;
        second = rand() % size;
        swap(&array[first], &array[second]);
    }
}

int getsum(int* array, int* permutation, int size)
{
    int i, sum;
    sum = 0;
    for (i = 0; i < size; i++)
        sum += array[permutation[i]];
    return sum;
}

int main(int argc, char** argv)
{
    int *array;
    clock_t t;
    int NumberOfElements, operation_type, sum, *permutation, step;
    if (argc < 3) 
        printf("Not enougth arguments");
    else 
    {
        NumberOfElements = atoi(argv[1]);
        operation_type = atoi(argv[2]);
        array = (int*) malloc(NumberOfElements * sizeof(int));
        fill(array, NumberOfElements, &sum);
        if (operation_type == 0) /* step */
        {
            if (argc != 4) 
                printf("No step found or too many arguments");
            else 
            {
                step = atoi(argv[3]);
                permutation = (int*) malloc(NumberOfElements * sizeof(int));
                generate_permutation_step(permutation, NumberOfElements, step);
                t = clock();
                if (getsum(array, permutation, NumberOfElements) == sum)
                {
                    printf("%f", difftime(clock(), t) / CLOCKS_PER_SEC);
                }
                else printf("Incorrect Sum");
            }
        } else if (operation_type == 1) /* random access */
        {
            if (argc!=3) 
                printf("Too many arguments");
            else 
            {
                permutation = (int*) malloc(NumberOfElements * sizeof(int));
                generate_permutation(permutation, NumberOfElements);
                t = clock();
                if (getsum(array, permutation, NumberOfElements) == sum)
                {
                    printf("%f", difftime(clock(),t) / CLOCKS_PER_SEC);   
                } 
                else printf("Incorrect Sum");
            }
        } else printf("Incorrect Operation Type");
        free(array);
    }
    return 0;
}
