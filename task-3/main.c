#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MEMORY_PROBLEM 11;
#define EMPTY_SAMPLE 12;
#define REALLOC_PROBLEM 1;
#define READING_PROBLEM 2;


int replace(const char* source, const char* old_sample, const char* new_sample, char** result)
{
    int len = strlen(source);
    int sample_len = strlen(old_sample);
    int new_sample_len = strlen(new_sample);
    int i, j, z, k, previous_prefix, result_size;
    int* prefix_sample;
    int* positions;
    if (sample_len == 0)
        return EMPTY_SAMPLE;
    /* Заполняем массив со значениями префикс-функции образца */
    prefix_sample = (int*) malloc(sample_len * sizeof(int));
    if (prefix_sample == NULL)
        return MEMORY_PROBLEM;
    prefix_sample[0] = 0;
    for (i = 1; i < sample_len; i++)
    {
        j = prefix_sample[i - 1];
        while (j > 0 && old_sample[i] != old_sample[j])
            j = prefix_sample[j - 1];
        if (old_sample[j] == old_sample[i])
            j++;
        prefix_sample[i] = j;
    }
    /* Заполняем массив позиций, в которых должна производиться замена, считаем количество замен */
    positions = (int*) calloc(len, sizeof(int));
    if (positions == NULL)
        return MEMORY_PROBLEM;
    k = 0; /* Количество замен */
    previous_prefix = 0; /* Значение префикс-функции на предыдущем символе */
    for (i = 0; i < len; i++)
    {
        positions[i] = 0;
        j = previous_prefix;
        while (j>0 && old_sample[j] != source[i])
            j = prefix_sample[j - 1];
        if (old_sample[j] == source[i])
            j++;
        if (j == sample_len)  /* Подстрока найдена */
        {
            positions[i - sample_len + 1] = 1; /* Запоминаем место, в которое нужно будет вставить новый образец */
            k++;
            previous_prefix = 0; /* Избегаем наложения, следующая удалённая строка не должна пересечься с текущей */
        }
        else
            previous_prefix = j;
    }
    free(prefix_sample);
    /* Выделяем память под результирующую строку */
    result_size = len + k * (new_sample_len - sample_len);
    *result = (char*)malloc(result_size + 1);
    if (*result == NULL)
        return MEMORY_PROBLEM;
    /* Заполняем результат */
    j = 0;
    for (i = 0; i < len; i++)
    {
        if (positions[i] == 0)
        {
            (*result)[j] = source[i];
            j++;
        }
        else     /* Вставляем новый образец */
        {
            for (z = 0; z < new_sample_len; z++)
                (*result)[j+z] = new_sample[z];
            j += new_sample_len;
            i += sample_len - 1; /* Перепрыгиваем старый образец в исходной строке, учитывая, что for увеличит параметр ещё на 1 */
        }
    }
    free(positions);
    /* Завершаем строку */
    (*result)[result_size] = '\0';
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
            else
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
    FILE* input = fopen("input","r");
    FILE* output = fopen("output","w");
    char *string, *old_sample, *new_sample, *result;
    int code;

    code = safe_gets(input, &string);
    if (code)
        return code;
    code = safe_gets(input, &old_sample);
    if (code)
        return code;
    code = safe_gets(input, &new_sample);
    if (code)
        return code;
    code = replace(string, old_sample, new_sample, &result);
    if (code)
        return code;
    fprintf(output, "%s", result);

    free(string);
    free(old_sample);
    free(new_sample);
    free(result);
    return 0;
}
