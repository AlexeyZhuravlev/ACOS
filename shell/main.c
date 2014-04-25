#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define REALLOC_PROBLEM 1
#define READING_PROBLEM 2

#define JOB_END 100
#define JOB_END_BACK 101
#define PIPE 102
#define READ 103
#define WRITE 104
#define APPEND 105
#define WORD 107
#define ERROR 108

#define COMMAND_END 201
#define EMPTY_JOB 202

#define RUN_ERROR 301
#define BUILT_IN 302

#define RUNNING 1001
#define SUSPENDED 1002
#define FINISHED 1003

#define EMPTY_SAMPLE 501
#define MEMORY_PROBLEM 502

struct variable
{
    char* name;
    char* value;
};

struct variable* variables = NULL;
char** exported_variables = NULL;
int number_of_exported_variables = 0;
int number_of_variables = 0;
int last_pid = 0;
int exitflag = 0;

struct program
{
    char* name;
    int number_of_arguments;
    char** arguments;
    char *input_file, *output_file;
    int output_type; /* 1 - rewrite, 2 - append */
};

struct job
{
    int background;
    struct program* programs;
    int number_of_programs;
};

struct process
{
    int pid;
    int status; /* 1001 - running, 1002 - suspended, 1003 - finished */
};

struct active_job
{
    struct process* processes;
    int number_of_processes;
    int status; /* 1001 - running, 1002 - suspended, 1003 - finished */
    char* job_name;
    int group;
};

struct active_job* current_jobs = NULL;
int number_of_jobs = 0;

char *strdup(const char *s);
pid_t getpgid(pid_t pid);
int safe_gets(FILE*, char**);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int gethostname(char *name, size_t len);
char *get_current_dir_name(void);
int setenv(const char *name, const char *value, int overwrite);
int putenv(char *string);
int kill(pid_t pid, int sig);

int divider(char a)
{
    return a == ' ' || a== ';' || a == '&' || a == '|'
           || a== '#' || a == '<' || a == '>' || a == '\0';
}

char* decode_macros(char** macros, int* macros_len)
{
    int i;
    char* result;
    if (strcmp(*macros, "?") == 0)
    {
        free(*macros);
        result = (char*)malloc(9 * sizeof(char));
        if (result == NULL)
        {
            *macros_len = 0;
            return strdup("");
        }
        sprintf(result, "%d", last_pid);
        *macros_len = strlen(result);
        return result;
    }
    for (i = 0; i < number_of_variables; i++)
        if (strcmp(variables[i].name, *macros) == 0)
        {
            free(*macros);
            *macros_len = strlen(variables[i].value);
            return strdup(variables[i].value);
        }
    result = getenv(*macros);
    free(*macros);
    if (result == NULL)
    {
        *macros_len = 0;
        return NULL;
    }
    *macros_len = strlen(result);
    return strdup(result);
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
    char* back_macros;
    char* macros = NULL;
    int macros_len, i;
    char strend = '\0';
    char* strend_ptr = &strend;
    macros_len = 0;
    if (**string == '?' || **string == '#')
        add_to_result(&macros, &macros_len, string, error);
    else if (**string == '{')
    {
        (*string)++;
        while (**string != '\0' && **string != '}')
            add_to_result(&macros, &macros_len, string, error);
        if (**string == '}')
            (*string)++;
    }
    else
    {
        while (isalpha(**string) || isdigit(**string) || **string == '_')
            add_to_result(&macros, &macros_len, string, error);
    }
    if (macros == NULL)
        return;
    add_to_result(&macros, &macros_len, &strend_ptr, error);
    back_macros = macros = decode_macros(&macros, &macros_len);
    if (macros == NULL)
        return;
    for (i = 0; i < macros_len; i++)
        add_to_result(result, len, &macros, error);
    free(back_macros);
}

int get_lexeme(char** begin, char** string, char** result)
{
    int result_len = 0;
    int error = 0;
    char strend = '\0';
    char enter = '\n';
    char* strend_ptr = &strend;
    char* enter_ptr = &enter;
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
    if (**string == '\0' || **string == '#')
        return COMMAND_END;
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
            if (**string == '\0')
            {
                free(*begin);
                printf("> ");
                safe_gets(stdin, begin);
                (*string) = (*begin);
            }
            else
                add_to_result(result, &result_len, string, &error);
        }
        else if (**string == '\'')
        {
            (*string)++;
            while (**string != '\'')
            {
                if (**string == '\0')
                {
                    add_to_result(result, &result_len, &enter_ptr, &error);
                    enter_ptr = &enter;
                    free(*begin);
                    printf("> ");
                    if (safe_gets(stdin, begin) == EOF)
                    {
                        free(*result);
                        fprintf(stderr, "Unexpected end of file");
                        return ERROR;
                    }
                    (*string) = *begin;
                }
                else
                    add_to_result(result, &result_len, string, &error);
            }
            (*string)++;
        }
        else if (**string == '"')
        {
            (*string)++;
            while (**string != '"')
            {
                if (**string == '\0')
                {
                    add_to_result(result, &result_len, &enter_ptr, &error);
                    enter_ptr = &enter;
                    free(*begin);
                    printf("> ");
                    if (safe_gets(stdin, begin) == EOF)
                    {
                        free(*result);
                        fprintf(stderr, "Unexpected end of file");
                        return ERROR;
                    }
                    (*string) = *begin;
                }
                else if (**string == '\\')
                {
                    (*string)++;
                    if (**string == '\0')
                    {
                        free(*begin);
                        printf("> ");
                        if (safe_gets(stdin, begin) == EOF)
                        {
                            free(*result);
                            fprintf(stderr, "Unexpected end of file");
                            return ERROR;
                        }
                        (*string) = *begin;
                    }
                    else
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


int getprogram(char** begin, char** string, struct program* new_program, int* res_job)
{
    char* lexeme;
    int res;
    char** success;
    while ((res = get_lexeme(begin, string, &lexeme)) != JOB_END &&
            res != JOB_END_BACK && res != PIPE && res != COMMAND_END)
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
                if (get_lexeme(begin, string, &lexeme) != WORD)
                {
                    free(lexeme);
                    fprintf(stderr, "Error: name of input for program expected\n");
                    return ERROR;
                }
                else if (strcmp(lexeme, "") == 0)
                {
                    free(lexeme);
                    fprintf(stderr, "Invalid input redirection\n");
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
                if (get_lexeme(begin, string, &lexeme) != WORD)
                {
                    free(lexeme);
                    fprintf(stderr, "Error: name of output for program expected\n");
                    return ERROR;
                }
                else if (strcmp(lexeme, "") ==0)
                {
                    free(lexeme);
                    fprintf(stderr, "Invalid output redirection\n");
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

int getjob(char** begin, char** string, struct job* new_job)
{
    char* lexeme;
    int res = 0;
    struct program new_program;
    struct program* success;
    char** succ;
    new_job->number_of_programs = 0;
    new_job->programs = NULL;

    do
    {
        res = get_lexeme(begin, string, &lexeme);
        if (res == JOB_END || res == JOB_END_BACK || res == COMMAND_END)
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
        new_program.number_of_arguments = 1;
        new_program.arguments = (char**)malloc(sizeof(char*));
        if (new_program.arguments != NULL)
            new_program.arguments[0] = strdup(lexeme);
        if (getprogram(begin, string, &new_program, &res) == ERROR)
        {
            clear_program(new_program);
            return ERROR;
        }
        succ = (char**)realloc(new_program.arguments, (new_program.number_of_arguments + 1) * sizeof(char*));
        if (succ == NULL)
        {
            clear_program(new_program);
            fprintf(stderr, "Error allocation memory\n");
            return ERROR;
        }
        succ[new_program.number_of_arguments] = NULL;
        new_program.arguments = succ;
        new_job->number_of_programs++;
        success = (struct program*)realloc(new_job->programs, new_job->number_of_programs * sizeof(struct program));
        if (success == NULL || new_program.arguments == NULL)
        {
            fprintf(stderr, "Error allocation memory\n");
            return ERROR;
        }
        success[new_job->number_of_programs - 1] = new_program;
        new_job->programs = success;
    }
    while (res != JOB_END_BACK && res != JOB_END && res != COMMAND_END);
    if (res == JOB_END_BACK)
        new_job->background = 1;
    else
        new_job->background = 0;
    if ((*new_job).number_of_programs == 0)
    {
        if (res == COMMAND_END)
            return COMMAND_END;
        else
            return EMPTY_JOB;
    }
    return 0;
}

int command_parsing(char** begin, char* command, struct job** jobs, int* number_of_jobs)
{
    int res;
    struct job new_job;
    struct job* success;
    while ((res = getjob(begin, &command, &new_job)) != COMMAND_END)
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
            printf("for rewrite\n");
        else
            printf("for append\n");
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

void mcd(int argc, char** argv)
{
    int success;
    if (argc == 1)
        success = chdir(getenv("HOME"));
    else
        success = chdir(argv[1]);
    if (success == -1)
        fprintf(stderr, "Unable to change directory\n");
    else
    {
        char* new_path = get_current_dir_name();
        setenv("PWD", new_path, 1);
        free(new_path);
    }
}

void mexit()
{
    exitflag = 1;
}

void free_exported_variables()
{
    int i;
    if (number_of_exported_variables == 0)
        return;
    for (i = 0; i < number_of_exported_variables; i++)
        free(exported_variables[i]);
    free(exported_variables);
}

char* add_exp_variable(char* word)
{
    char** success = (char**)realloc(exported_variables, (++number_of_exported_variables)*sizeof(char*));
    if (success == NULL)
    {
        fprintf(stderr, "Not enought memory\n");
        number_of_exported_variables--;
        free_exported_variables();
        exit(1);
    }
    success[number_of_exported_variables - 1] = strdup(word);
    exported_variables = success;
    return success[number_of_exported_variables - 1];
}

void export(int argc, char** argv)
{
    int i;
    for (i = 0; i < argc; i++)
        putenv(add_exp_variable(argv[i]));
}

void mpwd()
{
    printf("%s\n", getenv("PWD"));
    exit(0);
}

void jobs()
{
    int i;
    for (i = 0; i < number_of_jobs; i++)
        if (current_jobs[i].status != FINISHED)
        {
            printf("[%d] %s ", i, current_jobs[i].job_name);
            if (current_jobs[i].status == SUSPENDED)
                printf("suspended\n");
            else
                printf("running\n");
        }
    exit(0);
}

int get_first_not_finished()
{
    int i;
    int n = -1;
    for (i = 0; i < number_of_jobs; i++)
        if (current_jobs[i].status != FINISHED)
        {
            n = i;
            break;
        }
    return n;
}

void wake_job(int n)
{
    int i;
    for (i = 0; i < current_jobs[n].number_of_processes; i++)
        if (current_jobs[n].processes[i].status == SUSPENDED)
        {
            kill(current_jobs[n].processes[i].pid, SIGCONT);
            current_jobs[n].processes[i].status = RUNNING;
        }
    current_jobs[n].status = RUNNING;
    fprintf(stderr, " Job %s has continued\n", current_jobs[n].job_name);
}

void fg(int argc, char** argv)
{
    int n;
    if (argc == 1)
        n = get_first_not_finished();
    else
        n = atoi(argv[1]);
    if (n == -1)
        return;
    if (n >= number_of_jobs || current_jobs[n].status == FINISHED)
        fprintf(stderr, "No active job with number %d\n", n);
    else
    {
        int i, status, has_stop = 0;
        tcsetpgrp(0, current_jobs[n].group);
        if (current_jobs[n].status == SUSPENDED)
            wake_job(n);
        for (i = 0; i < current_jobs[n].number_of_processes; i++)
        {
            waitpid(current_jobs[n].processes[i].pid, &status, WUNTRACED);
            if (WIFSTOPPED(status))
            {
                current_jobs[n].processes[i].status = SUSPENDED;
                has_stop = 1;
            }
            else
                current_jobs[n].processes[i].status = FINISHED;
        }
        if (has_stop)
        {
            printf(" Job %s has been suspended\n", current_jobs[n].job_name);
            current_jobs[n].status = SUSPENDED;
        }
        else
            current_jobs[n].status = FINISHED;
        tcsetpgrp(0, getpgid(getpid()));
    }
}

void bg(int argc, char** argv)
{
    int n;
    if (argc == 1)
        n = get_first_not_finished();
    else
        n = atoi(argv[1]);
    if (n == -1)
        return;
    if (n >= number_of_jobs || current_jobs[n].status != SUSPENDED)
    {
        if (n >= number_of_jobs || current_jobs[n].status == FINISHED)
            fprintf(stderr, "No active job with number %d\n", n);
        else if (current_jobs[n].status == RUNNING)
            fprintf(stderr, "Job number %d is already running", n);
    }
    else
        wake_job(n);
}

int try_built_in(struct job* new_job)
{
    if (new_job->number_of_programs != 1)
        return 0;
    if (strcmp(new_job->programs[0].name, "cd") == 0)
    {
        mcd(new_job->programs[0].number_of_arguments, new_job->programs[0].arguments);
        return 1;
    }
    if (strcmp(new_job->programs[0].name, "exit") == 0)
    {
        mexit();
        return 1;
    }
    if (strcmp(new_job->programs[0].name, "export") == 0)
    {
        export(new_job->programs[0].number_of_arguments, new_job->programs[0].arguments);
        return 1;
    }
    if (strcmp(new_job->programs[0].name, "fg") == 0)
    {
        fg(new_job->programs[0].number_of_arguments, new_job->programs[0].arguments);
        return 1;
    }
    if (strcmp(new_job->programs[0].name, "bg") == 0)
    {
        bg(new_job->programs[0].number_of_arguments, new_job->programs[0].arguments);
        return 1;
    }
    return 0;
}

void mcat(int argc, char** argv)
{
    FILE* input;
    char* string;
    int code;
    if (argc == 1)
        input = stdin;
    else
        input = fopen(argv[1], "r");
    if (input == NULL)
    {
        fprintf(stderr, "Unable to open %s\n", argv[1]);
        exit(1);
    }
    while ((code = safe_gets(input, &string)) != EOF)
    {
        if (code == 0)
        {
            printf("%s\n", string);
            free(string);
        }
        else
        {
            fprintf(stderr, "Reading problems\n");
            exit(code);
        }
    }
    fclose(input);
    exit(0);
}

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
    positions = (int*) calloc(len, sizeof(int));
    if (positions == NULL)
        return MEMORY_PROBLEM;
    k = 0;
    previous_prefix = 0;
    for (i = 0; i < len; i++)
    {
        positions[i] = 0;
        j = previous_prefix;
        while (j>0 && old_sample[j] != source[i])
            j = prefix_sample[j - 1];
        if (old_sample[j] == source[i])
            j++;
        if (j == sample_len)
        {
            positions[i - sample_len + 1] = 1;
            k++;
            previous_prefix = 0;
        }
        else
            previous_prefix = j;
    }
    free(prefix_sample);
    result_size = len + k * (new_sample_len - sample_len);
    *result = (char*)malloc(result_size + 1);
    if (*result == NULL)
        return MEMORY_PROBLEM;
    j = 0;
    for (i = 0; i < len; i++)
    {
        if (positions[i] == 0)
        {
            (*result)[j] = source[i];
            j++;
        }
        else
        {
            for (z = 0; z < new_sample_len; z++)
                (*result)[j+z] = new_sample[z];
            j += new_sample_len;
            i += sample_len - 1;
        }
    }
    free(positions);
    (*result)[result_size] = '\0';
    return 0;
}

void msed(int argc, char** argv)
{
    int code, replace_code;
    char *string, *old_sample, *new_sample, *new_string;
    if (argc < 3)
    {
        fprintf(stderr, "Not enough arguments for msed\n");
        exit(1);
    }
    old_sample = argv[1];
    new_sample = argv[2];
    while ((code = safe_gets(stdin, &string)) != EOF)
    {
        if (code == 0)
        {
            replace_code = replace(string, old_sample, new_sample, &new_string);
            if (replace_code == 0)
            {
                printf("%s\n", new_string);
                free(string);
                free(new_string);
            }
            else
            {
                fprintf(stderr, "Memory allocation problem\n");
                exit(replace_code);
            }
        }
        else
        {
            fprintf(stderr, "Reading problems\n");
            exit(code);
        }
    }
    exit(0);
}

void free_strings(char** strings, int number_of_strings)
{
    int i;
    for (i = 0; i < number_of_strings; i++)
        free(strings[i]);
    free(strings);
}

int strcmp_d(const void* a, const void* b)
{
    return strcmp(*(char**)a, *(char**)b);
}

int strcmp_u(const void* a, const void* b)
{
    return strcmp(*(char**)b, *(char**)a);
}

void msort(int argc, char** argv)
{
    FILE *f1, *f2;
    int code, number_of_strings = 0, i;
    char* string;
    char** strings = NULL;
    char** success;
    int (*cmp)(const void*, const void*);
    if (argc < 2)
    {
        fprintf(stderr, "Name of file expected\n");
        exit(1);
    }
    f1 = fopen(argv[1], "r");
    if (f1 == NULL)
    {
        fprintf(stderr, "Unable to open %s for reading\n", argv[1]);
        exit(1);
    }
    if (argc < 3)
    {
        fprintf(stderr, "Sorting option expected: -u or -d\n");
        exit(1);
    }
    if (strcmp(argv[2], "-d") == 0)
    {
        cmp = strcmp_d;
    }
    else if (strcmp(argv[2], "-u") == 0)
    {
        cmp = strcmp_u;
    }
    else
    {
        fprintf(stderr, "Invalid sorting option: -u or -d expected\n");
        exit(1);
    }
    while ((code = safe_gets(f1, &string)) != EOF)
    {
        if (code == 0)
        {
            number_of_strings++;
            success = (char**)realloc(strings, number_of_strings * sizeof(char*));
            if (success == NULL)
            {
                free(strings);
                free_strings(strings, number_of_strings);
                fprintf(stderr, "Memory allocation problem\n");
                exit(1);
            }
            else
            {
                strings = success;
                strings[number_of_strings - 1] = string;
            }
        }
        else
        {
            fprintf(stderr, "Reading problems\n");
            free_strings(strings, number_of_strings);
            exit(code);
        }
    }
    qsort(strings, number_of_strings, sizeof(char*), cmp);
    fclose(f1);
    f2 = fopen(argv[1], "w");
    if (f2 == NULL)
    {
        fprintf(stderr, "Unable to open %s for rewriting\n", argv[1]);
        exit(1);
    }
    for (i = 0; i < number_of_strings; i++)
        fprintf(f2, "%s\n", strings[i]);
    fclose(f2);
    free_strings(strings, number_of_strings);
    exit(0);
}

void try_built_in_conv(struct program* new_program)
{
    if (strcmp(new_program->name, "pwd") == 0)
        mpwd();
    if (strcmp(new_program->name, "jobs") == 0)
        jobs();
    if (strcmp(new_program->name, "mcat") == 0)
        mcat(new_program->number_of_arguments, new_program->arguments);
    if (strcmp(new_program->name, "msed") == 0)
        msed(new_program->number_of_arguments, new_program->arguments);
    if (strcmp(new_program->name, "msort") == 0)
        msort(new_program->number_of_arguments, new_program->arguments);
}

void free_lakes(int** lakes, int n)
{
    int i;
    for (i = 0; i < n; i++)
        free(lakes[i]);
    free(lakes);
}

int run_job(struct job* new_job, struct active_job* possible_job)
{
    int i, pid, j, group_number;
    int** lakes;
    if (try_built_in(new_job) != 0)
        return BUILT_IN;
    possible_job->number_of_processes = new_job->number_of_programs;
    possible_job->processes = (struct process*)malloc((new_job->number_of_programs)*sizeof(struct process));
    lakes = (int**)malloc((new_job->number_of_programs - 1) * sizeof(int*));
    for (i = 0; i < new_job->number_of_programs - 1; i++)
    {
        lakes[i] = (int*)malloc(2 * sizeof(int));
        pipe(lakes[i]);
    }
    for (i = 0; i < new_job->number_of_programs; i++)
    {
        pid = fork();
        if (pid !=0)
        {
            possible_job->processes[i].pid = pid;
            possible_job->processes[i].status = RUNNING;
        }
        if (pid == -1)
        {
            free_lakes(lakes, new_job-> number_of_programs - 1);
            fprintf(stderr, "Execution problem\n");
            return RUN_ERROR;
        }
        if (i == 0 && pid != 0)
            group_number = pid;
        if (pid != 0)
            setpgid(pid, group_number);
        if (pid == 0)
        {
            signal(SIGTTOU, SIG_DFL);
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            for (j = 0; j < new_job->number_of_programs - 1; j++)
            {
                if (j != i - 1)
                    close(lakes[j][0]);
                if (j != i)
                    close(lakes[j][1]);
            }
            if (new_job->programs[i].input_file != NULL)
            {
                int input = open(new_job->programs[i].input_file, O_RDONLY);
                if (input != -1)
                {
                    dup2(input, 0);
                    close(input);
                }
                else
                {
                    fprintf(stderr, "unable to open input file\n");
                    exit(1);
                }
                if (i > 0)
                    close(lakes[i - 1][0]);
            }
            else if (i > 0)
            {
                if (new_job->programs[i - 1].output_file == NULL)
                    dup2(lakes[i - 1][0], 0);
                else
                    close(0);
                close(lakes[i - 1][0]);
            }
            if (new_job->programs[i].output_file != NULL)
            {
                int output;
                int flags = O_WRONLY | O_CREAT;
                if (new_job->programs[i].output_type == 2)
                    flags = flags | O_APPEND;
                output = open(new_job->programs[i].output_file, flags, 0666);
                dup2(output, 1);
                close(output);
                if (i < new_job->number_of_programs - 1)
                    close(lakes[i][1]);
            }
            else if (i < new_job->number_of_programs - 1)
            {
                if (new_job->programs[i + 1].input_file == NULL)
                    dup2(lakes[i][1], 1);
                else
                    close(1);
                close(lakes[i][1]);
            }
            free_lakes(lakes, new_job->number_of_programs - 1);
            try_built_in_conv(&new_job->programs[i]);
            execvp(new_job->programs[i].name, new_job->programs[i].arguments);
            fprintf(stderr, "Problem with execution %s\n", new_job->programs[i].name);
            exit(1);
        }
    }
    for (i = 0; i < new_job->number_of_programs - 1; i++)
    {
        close(lakes[i][0]);
        close(lakes[i][1]);
    }
    free_lakes(lakes, new_job->number_of_programs - 1);
    possible_job->group = group_number;
    return 0;
}

char* form_job_name(struct job* new_job)
{
    int i, j, len = 1;
    char* result = NULL;
    for (i = 0; i < new_job->number_of_programs; i++)
    {
        for (j = 0; j < new_job->programs[i].number_of_arguments; j++)
            len += strlen(new_job->programs[i].arguments[j]) + 1;
        if (new_job->programs[i].input_file != NULL)
            len += strlen(new_job->programs[i].input_file) + 3;
        if (new_job->programs[i].output_file != NULL)
            len += strlen(new_job->programs[i].output_file) + 4;
        len += 2;
    }
    result = (char*)malloc(len * sizeof(char));
    if (result == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }
    result[0] = '\0';
    for (i = 0; i < new_job->number_of_programs; i++)
    {
        for (j = 0; j < new_job->programs[i].number_of_arguments; j++)
        {
            strcat(result, new_job->programs[i].arguments[j]);
            strcat(result, " ");
        }
        if (new_job->programs[i].input_file != NULL)
        {
            strcat(result, "< ");
            strcat(result, new_job->programs[i].input_file);
            strcat(result, " ");
        }
        if (new_job->programs[i].output_file != NULL)
        {
            if (new_job->programs[i].output_type == 1)
                strcat(result, "> ");
            else
                strcat(result, ">> ");
            strcat(result, new_job->programs[i].output_file);
            strcat(result, " ");
        }
        if (i < new_job->number_of_programs - 1)
            strcat(result, "| ");
    }
    return result;
}

int add_active_job(struct active_job new_job)
{
    struct active_job* success;
    number_of_jobs++;
    success = (struct active_job*)realloc(current_jobs, number_of_jobs * sizeof(struct active_job));
    if (success == NULL)
        return ERROR;
    current_jobs = success;
    current_jobs[number_of_jobs - 1] = new_job;
    return 0;
}

void run_job_foreground(struct job* new_job)
{
    struct active_job possible_job;
    if (run_job(new_job, &possible_job) == 0)
    {
        int i, status, flag = 0;
        tcsetpgrp(0, possible_job.group);
        possible_job.job_name = form_job_name(new_job);
        for (i = 0; i < possible_job.number_of_processes; i++)
        {
            waitpid(possible_job.processes[i].pid, &status, WUNTRACED);
            if (WIFSTOPPED(status))
            {
                possible_job.processes[i].status = SUSPENDED;
                flag = 1;
            }
            last_pid = WEXITSTATUS(status);
        }
        if (flag)
        {
            possible_job.status = SUSPENDED;
            if (add_active_job(possible_job) == ERROR)
            {
                fprintf(stderr, "Allocation memory error\n");
                free(possible_job.processes);
                free(possible_job.job_name);
                return;
            }
            fprintf(stderr, " Job %s has been suspended\n", possible_job.job_name);
        }
        else
        {
            free(possible_job.processes);
            free(possible_job.job_name);
        }
        tcsetpgrp(0, getpgid(getpid()));
    }
}

void run_job_background(struct job* new_job)
{
    struct active_job possible_job;
    if (run_job(new_job, &possible_job) == 0)
    {
        possible_job.job_name = form_job_name(new_job);
        possible_job.status = RUNNING;
        if (add_active_job(possible_job) == ERROR)
        {
            fprintf(stderr, "Allocation memory error\n");
            free(possible_job.processes);
            free(possible_job.job_name);
        }
        printf(" Job %s is running\n", possible_job.job_name);
    }
}

void run_jobs(int n, struct job* jobs)
{
    int i;
    for (i = 0; i < n; i++)
    {
        if (jobs[i].background == 0)
            run_job_foreground(&jobs[i]);
        else
            run_job_background(&jobs[i]);
    }
}

void free_variables();

void add_variable(char* name, char* value)
{
    struct variable* success;
    success = (struct variable*)realloc(variables, (++number_of_variables)*sizeof(struct variable));
    if (success == NULL)
    {
        fprintf(stderr, "Not enough memory\n");
        number_of_variables--;
        free_variables();
        exit(1);
    }
    success[number_of_variables - 1].name = name;
    success[number_of_variables - 1].value = value;
    variables = success;
}

void checknull(char* buf)
{
    if (buf == NULL)
    {
        fprintf(stderr, "Not enough memory\n");
        exit(1);
    }
}

void init_variables(int argc, char** argv)
{
    int i, k;
    char* buf = (char*)malloc(9 * sizeof(char));
    checknull(buf);
    sprintf(buf, "%d", argc);
    add_variable(strdup("#"), buf);

    for (i = 0; i < argc; i++)
    {
        buf = (char*)malloc(9 * sizeof(char));
        checknull(buf);
        sprintf(buf, "%d", i);
        add_variable(buf, strdup(argv[i]));
    }

    buf = (char*)malloc(100 * sizeof(char));
    checknull(buf);
    if ((k = readlink("/proc/self/exe", buf, 100 * sizeof(char))) == -1)
    {
        fprintf(stderr, "Too long path to execution file\n");
        exit(1);
    }
    buf[k] = '\0';
    add_variable(strdup("SHELL"), buf);

    buf = (char*)malloc(9 * sizeof(char));
    checknull(buf);
    sprintf(buf, "%d", getpid());
    add_variable(strdup("PID"), buf);

    buf = (char*)malloc(9 * sizeof(char));
    checknull(buf);
    sprintf(buf, "%d", getuid());
    add_variable(strdup("UID"), buf);

    buf = (char*)malloc(50 * sizeof(char));
    checknull(buf);
    gethostname(buf, 50 * sizeof(char));
    add_variable(strdup("HOSTNAME"), buf);
}

void free_variables()
{
    int i;
    if (number_of_variables == 0)
        return;
    for (i = 0; i < number_of_variables; i++)
    {
        free(variables[i].name);
        free(variables[i].value);
    }
    free(variables);
}

void free_active_jobs()
{
    int i;
    for (i = 0; i < number_of_jobs; i++)
    {
        free(current_jobs[i].job_name);
        free(current_jobs[i].processes);
    }
    free(current_jobs);
}

void check_finished_processes()
{
    int i, j, status, has_run = 0, has_susp = 0;
    for (i = 0; i < number_of_jobs; i++)
        if (current_jobs[i].status == RUNNING)
        {
            has_run = 0;
            has_susp = 0;
            for (j = 0; j < current_jobs[i].number_of_processes; j++)
                if (current_jobs[i].processes[j].status == RUNNING)
                {
                    if (waitpid(current_jobs[i].processes[j].pid, &status, WUNTRACED | WNOHANG)
                            == current_jobs[i].processes[j].pid)
                    {
                        last_pid = WEXITSTATUS(status);
                        if (WIFSTOPPED(status))
                            current_jobs[i].processes[j].status = SUSPENDED;
                        else
                            current_jobs[i].processes[j].status = FINISHED;
                    }
                }
            for (j = 0; j < current_jobs[i].number_of_processes; j++)
            {
                if (current_jobs[i].processes[j].status == RUNNING)
                    has_run = 1;
                if (current_jobs[i].processes[j].status == SUSPENDED)
                    has_susp = 1;
            }
            if (has_run == 0)
            {
                if (has_susp == 1)
                {
                    fprintf(stderr, " Job %s has been suspended\n", current_jobs[i].job_name);
                    current_jobs[i].status = SUSPENDED;
                }
                else
                {
                    fprintf(stderr, " Job %s has finished\n", current_jobs[i].job_name);
                    current_jobs[i].status = FINISHED;
                }
            }
        }
}

void kill_active_ps()
{
    int i;
    for (i = 0; i < number_of_jobs; i++)
        if (current_jobs[i].status != FINISHED)
        {
            kill(-current_jobs[i].group, SIGCONT);
            kill(-current_jobs[i].group, SIGTERM);
        }
}

int main(int argc, char** argv)
{
    char *s;
    signal(SIGTTOU, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    init_variables(argc, argv);
    printf("msh$ ");
    while (safe_gets(stdin, &s) != EOF)
    {
        char **back_ptr = &s;
        int n = 0, res;
        struct job* jobs = NULL;
        res = command_parsing(back_ptr, s, &jobs, &n);
        free(*back_ptr);
        if (res == 0)
            run_jobs(n, jobs);
        clear_information(jobs, n);
        if (exitflag == 0)
        {
            check_finished_processes();
            printf("msh$ ");
        }
        else
            break;
    }
    free_variables();
    free_exported_variables();
    kill_active_ps();
    free_active_jobs();
    return 0;
}
