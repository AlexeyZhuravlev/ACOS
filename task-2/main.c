#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define REALLOC_PROBLEM 1;
#define READING_PROBLEM 2;

int delete_same_substrings(const char* source, char** result, int n)
{
    int l = strlen(source);
    int begin, part_length, flag, position, block, i;
    char *current_position, *new_position, *success;
    *result = (char*)malloc((l+1)*sizeof(char));
    if (*result == NULL)
        return REALLOC_PROBLEM;
    memcpy(*result, source, l+1);
    for (begin = 0; begin < l; begin++)
    {
        for (part_length = n; part_length >= 1; part_length--)
        {
            if (begin + part_length * 2 > l)
                continue;
            flag = 1;
            for (position = begin; position < begin + part_length; position++)
                if ((*result)[position] != (*result)[position + part_length])
                    flag = 0;
            if (flag)
            {
                current_position = (*result) + begin;
                new_position = current_position + part_length * 2;
                block = l - (begin + part_length * 2) + 1;
                for (i = 0; i < block; i++)
                    current_position[i] = new_position[i];
                l -= part_length * 2;
                success = (char*)realloc(*result, (l + 1) * sizeof(char));
                if (success == NULL)
                    return REALLOC_PROBLEM;
                *result = success;
                begin--;
                break;
            }
        }
    }
    return 0;
}

int safe_gets(FILE *f, char** string)
{
    int capacity = 0;
    int length = 0;
    char* result = NULL;
    char* success;
    char new_symbol;
    int pagesize = sysconf(_SC_PAGESIZE);
    if (f == NULL)
        return READING_PROBLEM;
    do
    {
        new_symbol = fgetc(f);
        if (new_symbol == EOF)
        {
            if (ferror(f))
            {
                free(result);
                return READING_PROBLEM;
            }
            else if (feof(f))
            {
                if (length == 0)
                    return EOF;
                else
                    new_symbol = '\0';
            }
        }
        if (new_symbol == '\n')
            new_symbol = '\0';

        length++;
        if (length > capacity)
        {
            capacity += pagesize;
            success = (char*)realloc(result, capacity * sizeof(char));
            if (success == NULL)
            {
                free(result);
                return REALLOC_PROBLEM;
            }
            result = success;
        }
        result[length - 1] = new_symbol;
    }
    while (new_symbol != '\0');
    success = (char*)realloc(result, length * sizeof(char));
    if (success == NULL)
    {
        free(result);
        return REALLOC_PROBLEM;
    }
    result = success;
    *string = result;
    return 0;
}

int main()
{
    int n;
    char* s;
    char* s1;
    FILE* input = fopen("input", "r");
    FILE* output = fopen("output", "w");
    if (input == NULL)
        fprintf(stderr, "Error opening file");
    else
    if (fscanf(input, "%d\n", &n) == EOF)
        fprintf(stderr, "Error");
    else
    {
        if (safe_gets(input, &s) == 0)
        {
            if (delete_same_substrings(s, &s1, n) == 0)
            {
                fprintf(output, "%s", s1);
                free(s1);
                free(s);
            }
            else
                fprintf(stderr, "Error");
        }
        else fprintf(stderr, "Error");
    }
    return 0;
}
