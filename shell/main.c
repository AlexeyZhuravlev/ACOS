#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#define REALLOC_PROBLEM 1
#define READING_PROBLEM 2

#define JOB_END 100
#define JOB_END_BACK 101
#define PIPE 102
#define READ 103
#define WRITE 104
#define APPEND 105
#define COMMENT 106
#define WORD 107
#define ERROR 108

#define COMMAND_END 201
#define EMPTY_JOB 202

struct program
{
    char* name;
    int number_of_arguments;
    char** arguments;
    char *input_file, *output_file;
    int output_type; /* 1 - rewrite, 2 - apprend */
};

struct job
{
    int background;
    struct program* programs;
    int number_of_programs;
};

int divider(char a)
{
    return a == ' ' || a== ';' || a == '&' || a == '|'
           || a== '#' || a == '<' || a == '>' || a == '\0';
}

void decode_macros(char** macros, int* macros_len)
{
    (*macros)[0] = '@'; /* for testing */
    return;
}

void add_to_result(char** result, int* len, char** string, int* error)
{
    char* success;
    (*len)++;
    success = (char*)realloc(*result, (*len) * sizeof(char));
    if (success == NULL)
    {
        *error = 1;
        free(*result);
        *result = NULL;
    }
    else
    {
        success[*len - 1] = **string;
        *result = success;
    }
    (*string)++;
}

void get_macroname(char** result, int* len, char** string, int* error)
{
    char* macros = NULL;
    char* macros_cpy;
    int macros_len, i;
    macros_len = 0;
    if (**string == '?' || **string == '#')
        add_to_result(&macros, &macros_len, string, error);
    else
    {
        while (isalpha(**string) || isdigit(**string) || **string == '_')
            add_to_result(&macros, &macros_len, string, error);
    }
    if (macros == NULL)
        return;
    decode_macros(&macros, &macros_len);
    macros_cpy = macros;
    for (i = 0; i < macros_len; i++)
        add_to_result(result, len, &macros, error);
    free(macros_cpy);
}

int get_lexeme(char** string, char** result)
{
    int result_len = 0;
    int error = 0;
    char strend = '\0';
    char* strend_ptr = &strend;
    *result = NULL;
    while (**string == ' ')
        (*string)++;
    if (**string == '|')
    {
        (*string)++;
        return PIPE;
    }
    if (**string == ';')
    {
        (*string)++;
        return JOB_END;
    }
    if (**string == '&')
    {
        (*string)++;
        return JOB_END_BACK;
    }
    if (**string == '#' || **string == '\0')
    {
        return COMMENT;
    }
    if (**string == '<')
    {
        (*string)++;
        return READ;
    }
    if (**string == '>')
    {
        (*string)++;
        if (**string == '>')
        {
            (*string)++;
            return APPEND;
        }
        else
            return WRITE;
    }
    while (!divider(**string))
    {
        if (**string == '\\')
        {
            (*string)++;
            add_to_result(result, &result_len, string, &error);
        }
        else if (**string == '\'')
        {
            (*string)++;
            while (**string != '\0' && **string != '\'')
                add_to_result(result, &result_len, string, &error);
            if (**string == '\0')
            {
                fprintf(stderr, "Second ' expected\n");
                free(*result);
                return ERROR;
            }
            (*string)++;
        }
        else if (**string == '"')
        {
            (*string)++;
            while (**string != '\0' && **string != '"')
            {
                if (**string == '\\')
                {
                    (*string)++;
                    add_to_result(result, &result_len, string, &error);
                }
                else if (**string == '$')
                {
                    (*string)++;
                    get_macroname(result, &result_len, string, &error);
                }
                else
                    add_to_result(result, &result_len, string, &error);
            }
            if (**string == '\0')
            {
                fprintf(stderr, "second \" expected\n");
                free(*result);
                return ERROR;
            }
            (*string)++;
        }
        else if (**string == '$')
        {
            (*string)++;
            get_macroname(result, &result_len, string, &error);
        }
        else
            add_to_result(result, &result_len, string, &error);
    }
    add_to_result(result, &result_len, &strend_ptr, &error);
    if (error)
    {
        fprintf(stderr, "Error allocating memory\n");
        return ERROR;
    }
    return WORD;
}


int getprogram(char** string, struct program* new_program, int* res_job)
{
    char* lexeme;
    int res;
    char** success;
    while ((res = get_lexeme(string, &lexeme)) != JOB_END &&
            res != JOB_END_BACK && res != PIPE && res != COMMENT)
    {
        if (res == ERROR)
            return ERROR;
        if (res == WORD)
        {
            new_program->number_of_arguments++;
            success = (char**)realloc(new_program->arguments, new_program->number_of_arguments * sizeof(char*));
            if (success == NULL)
            {
                fprintf(stderr, "Error allocating memory\n");
                return ERROR;
            }
            success[new_program->number_of_arguments - 1] = lexeme;
            new_program->arguments = success;
        }
        else if (res == READ)
        {
            if (new_program->input_file != NULL)
            {
                free(lexeme);
                fprintf(stderr, "Error: too many inputs for program\n");
                return ERROR;
            }
            else
            {
                if (get_lexeme(string, &lexeme) != WORD)
                {
                    free(lexeme);
                    fprintf(stderr, "Error: name of input for program expected\n");
                    return ERROR;
                }
                else
                    new_program->input_file = lexeme;
            }
        }
        else if (res == WRITE || res == APPEND)
        {
            if (new_program->output_file != NULL)
            {
                free(lexeme);
                fprintf(stderr, "Error: too many outputs for program\n");
                return ERROR;
            }
            else
            {
                if (get_lexeme(string, &lexeme) != WORD)
                {
                    free(lexeme);
                    fprintf(stderr, "Error: name of output for program expected\n");
                    return ERROR;
                }
                else
                {
                    new_program->output_file = lexeme;
                    if (res == WRITE)
                        new_program->output_type = 1;
                    else
                        new_program->output_type = 2;
                }
            }
        }
    }
    *res_job = res;
    return 0;
}

void clear_program(struct program old_program)
{
    int i;
    free(old_program.name);
    free(old_program.input_file);
    free(old_program.output_file);
    for (i = 0; i < old_program.number_of_arguments; i++)
        free(old_program.arguments[i]);
    free(old_program.arguments);
}

void clear_job(struct job old_job)
{
    int i;
    for (i = 0; i < old_job.number_of_programs; i++)
        clear_program(old_job.programs[i]);
    free(old_job.programs);
}

int getjob(char** string, struct job* new_job)
{
    char* lexeme;
    int res = 0;
    struct program new_program;
    struct program* success;
    new_job->number_of_programs = 0;
    new_job->programs = NULL;

    do
    {
        res = get_lexeme(string, &lexeme);
        if (res == JOB_END || res == JOB_END_BACK || res == COMMENT)
        {
            if (new_job->number_of_programs > 0)
            {
                fprintf(stderr, "Program name after pipe expected\n");
                return ERROR;
            }
            break;
        }
        if (res == ERROR)
        {
            return ERROR;
        }
        if (res != WORD)
        {
            fprintf(stderr, "No Program Name\n");
            return ERROR;
        }
        new_program.name = lexeme;
        new_program.input_file = NULL;
        new_program.output_file = NULL;
        new_program.number_of_arguments = 0;
        new_program.arguments = NULL;
        new_job->number_of_programs++;
        if (getprogram(string, &new_program, &res) == ERROR)
        {
            clear_program(new_program);
            return ERROR;
        }
        success = (struct program*)realloc(new_job->programs, new_job->number_of_programs * sizeof(struct program));
        if (success == NULL)
        {
            fprintf(stderr, "Error allocation memory\n");
            return ERROR;
        }
        success[new_job->number_of_programs - 1] = new_program;
        new_job->programs = success;
    }
    while (res != JOB_END_BACK && res != JOB_END && res != COMMENT);
    if (res == JOB_END_BACK)
        new_job->background = 1;
    else
        new_job->background = 0;
    if ((*new_job).number_of_programs == 0)
    {
        if (res == COMMENT)
            return COMMAND_END;
        else
            return EMPTY_JOB;
    }
    return 0;
}

int command_parsing(char* command, struct job** jobs, int* number_of_jobs)
{
    int res;
    struct job new_job;
    struct job* success;
    *jobs = NULL;
    *number_of_jobs = 0;
    while ((res = getjob(&command, &new_job)) != COMMAND_END)
    {
        if (res == ERROR)
        {
            clear_job(new_job);
            return ERROR;
        }
        if (res != EMPTY_JOB)
        {
            (*number_of_jobs)++;
            success = (struct job*) realloc(*jobs, (*number_of_jobs) * sizeof(struct job));
            if (success == NULL)
            {
                fprintf(stderr,"Error allocating memory\n");
                return ERROR;
            }
            *jobs = success;
            (*jobs)[(*number_of_jobs) - 1] = new_job;
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

void print_program(struct program prog)
{
    int i;
    printf("Program: %s \n", prog.name);
    printf("	Arguments: ");
    for (i = 0; i < prog.number_of_arguments; i++)
        printf("%s ; ",prog.arguments[i]);
    printf("\n");
    printf("	Input redirection: ");
    if (prog.input_file == NULL)
        printf("none\n");
    else
        printf("%s\n", prog.input_file);
    printf("	Output redirection: ");
    if (prog.output_file == NULL)
        printf("none\n");
    else
    {
        printf("%s ", prog.output_file);
        if (prog.output_type == 1)
            printf("for rewrite");
        else
            printf("for append");
    }
}

void print_jobs(int n, struct job* jobs)
{
    int i, j;
    for (i = 0; i < n; i++)
    {
        printf("Job number %d:\n",i);
        printf("Background: %d \n",jobs[i].background);
        printf("Programs:\n");
        for (j = 0; j < jobs[i].number_of_programs; j++)
            print_program(jobs[i].programs[j]);
        printf("\n");
    }
}



void clear_information(struct job* jobs, int n)
{
    int i;
    for (i = 0; i < n; i++)
        clear_job(jobs[i]);
    free(jobs);
}

int main(int argc, char** argv)
{
    char *s;
    int n, res;
    struct job* jobs;
    safe_gets(stdin, &s);
    res = command_parsing(s,&jobs,&n);
    free(s);
    if (res == 0)
        print_jobs(n, jobs);
    clear_information(jobs, n);
    return 0;
}
