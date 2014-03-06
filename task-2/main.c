#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REALLOC_PROBLEM 1;
#define READING_PROBLEM 2;

int delete_same_substrings(char** source, int n)
{
    int l = strlen(*source);
    int begin, part_length, flag, position, block, i;
    char *current_position, *new_position, *success;
    for (begin = 0; begin < l; begin++)
    {
        for (part_length = n; part_length >= 1; part_length--)
        {
            if (begin + part_length * 2 > l) 
                continue;
            flag = 1;
            for (position = begin; position < begin + part_length; position++)
                if ((*source)[position] != (*source)[position + part_length])
                    flag = 0;
            if (flag)
            {
                current_position = (*source) + begin;
                new_position = current_position + part_length * 2;
                block = l - (begin + part_length * 2) + 1;
                for (i = 0; i < block; i++)
                    current_position[i] = new_position[i];
				l -= part_length * 2;
                success = (char*)realloc(*source, (l + 1) * sizeof(char));
                if (success == NULL) 
                    return REALLOC_PROBLEM;
                *source = success;
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
    char* result;
    char* success;
    char new_symbol;
    result = NULL;
    do {
       length++;
       if (length > capacity)
       {
           if (capacity == 0) 
               capacity = 1;
           else capacity *= 2;
           success = (char*)realloc(result, capacity * sizeof(char));
           if (success == NULL)
           {
              free(result);
              return REALLOC_PROBLEM;
           }
           result = success;
       }
       new_symbol = fgetc(f);
       if (new_symbol == EOF)
           return READING_PROBLEM;
       if (new_symbol == '\n') 
           new_symbol = '\0';
       result[length - 1] = new_symbol;
    } while (new_symbol != '\0');
    success = (char*)realloc(result, length * sizeof(char));
    if (success == NULL)
    {
        free(result);
        return REALLOC_PROBLEM;
    }
    *string = result;
    return 0;
}


int main()
{
    int n;
    char* s;
    FILE* input = fopen("input", "r");
    FILE* output = fopen("output", "w");
    if (fscanf(input, "%d\n", &n) == EOF)
        printf("Error");
    else {
		 if (safe_gets(input, &s) == 0)
		 {
			 if (delete_same_substrings(&s, n) == 0)
			{
				fprintf(output, "%s", s);
			} else
                printf("Error");
		 } else printf("Error");
    }
    return 0;
}