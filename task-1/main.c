#include <stdio.h> 
#include <stdlib.h>

/* You should free memory before reading another string to the same char* variable 
   Example
   char* s = safe_gets(stdin);
   _someoperations_
   free(s);
   s = safe_gets(stdin);
   _new_operations_
   free(s);         */

char* safe_gets(FILE *f)
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
              return NULL;
           }
           result = success;
       }
       new_symbol = fgetc(f);
       if (new_symbol == '\n') 
           new_symbol = '\0';
       result[length - 1] = new_symbol;
    } while (new_symbol != '\0');
    return result;
}

int main()
{
   char* string;
   FILE* input = fopen("input", "r");;
   string = safe_gets(input);
   if (string != NULL)
       printf("%s", string);
   else printf("ERROR");
   free(string);
   return 0;
}
