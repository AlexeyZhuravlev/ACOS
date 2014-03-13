#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>

#define REALLOC_PROBLEM 1;
#define READING_PROBLEM 2;

/* You should free memory before reading another string to the same char* variable 
   Example
   char* s = safe_gets(stdin);
   _someoperations_
   free(s);
   s = safe_gets(stdin);
   _new_operations_
   free(s);         */

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
    do {
       new_symbol = fgetc(f);
       if (new_symbol == EOF)
       {
           if (ferror(f)) {
               free(result);
               return READING_PROBLEM;
           } else {
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
   int code;
   char* string;
   int k =0;
   FILE* input = fopen("input", "r");
   if (input == NULL) {
       printf("Cannot open file");
       return -2;
   }
   while ((code = safe_gets(input, &string)) != EOF)
   {
       k++;
       if (code == 0)
       {
           printf("%s\n", string);
           free(string);
       } else {
           printf("ERROR");
           return code;
       }
   }
   return 0;
}
