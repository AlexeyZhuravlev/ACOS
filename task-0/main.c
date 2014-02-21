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

int getsum_step(int* array, int size, int step)
{
    int i, sum, position, j;
    position = 0;
    sum = 0;
    j=0;
    for (i=0; i < size; i++)
    {
        sum += array[position];
        position += step;
        if (position >= size)
            position = ++j;
    }
    return sum;
}

void swap(int* a,int *b)
{
    int c = *a;
    *a = *b;
    *b = c;
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

int getsum_random(int* array, int* permutation, int size)
{
    int i, sum;
    sum = 0;
    for (i = 0; i < size; i++)
        sum += array[permutation[i]];
    return sum;
}


void test_mode(int size)
{
    clock_t t;
    int *array, sum,i;
    freopen("result","w",stdout);
    array = (int*) malloc(size * sizeof(int));
    fill(array, size, &sum);
    for (i = 0; i < 100; i++)
    {
        int step = 10 * i + 1;
        t = clock();
        if (getsum_step(array, size, step) == sum)
            printf("%d %f\n", step, difftime(clock(), t) / CLOCKS_PER_SEC);
        else printf("Error!");
    }
    free(array);
}

int main(int argc, char** argv)
{
    int *array;
    clock_t t;
    if (argc == 2) 
        test_mode(atoi(argv[1]));
    else if (argc < 3) 
        printf("Not enougth arguments");
    else 
    {
        int NumberOfElements = atoi(argv[1]);
        int operation_type = atoi(argv[2]);
        int sum;
        array = (int*) malloc(NumberOfElements * sizeof(int));
        fill(array, NumberOfElements, &sum);
        if (operation_type == 0) /* step */
        {
            if (argc != 4) 
                printf("No step found or too many arguments");
            else 
            {
                int step = atoi(argv[3]);
                t = clock();
                if (getsum_step(array, NumberOfElements, step) == sum)
                {
                    printf("%f", difftime(clock(), t) / CLOCKS_PER_SEC);
                }
                else printf("Incorrect Sum");
            }
        } else if (operation_type == 1) /* random access */
        {
            if (argc!=3) 
                printf("Too many arguments");
            else {
                int* permutation = (int*) malloc(NumberOfElements * sizeof(int));
                generate_permutation(permutation, NumberOfElements);
                t = clock();
                if (getsum_random(array, permutation, NumberOfElements) == sum)
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
