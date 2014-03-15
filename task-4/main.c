#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>

int main(int argc, char** argv)
{
   int lake[2];
   pid_t prog1_id, prog2_id, wc_id;
   int status1, status2, status3;
   if (argc != 10) 
   {
       fprintf(stderr, "Not enough arguments\n");
       return 1;
   }
   if (pipe(lake))
   {
       fprintf(stderr, "Unable to create pipe\n");
       return 2;
   }
   prog1_id = fork();
   if (prog1_id == 0)
   {
       close(lake[0]);
       dup2(lake[1], 1);
       close(lake[1]);
       execlp(argv[1], argv[1], argv[2], argv[3], NULL);
       return 3;
   }
   prog2_id = fork();
   if (prog2_id == 0)
   {
       dup2(lake[0], 0);
       dup2(lake[1], 1);
       close(lake[0]);
       close(lake[1]);
       execlp(argv[4], argv[4], argv[5], argv[6], NULL);
       return 4;
   }
   wc_id = fork();
   if (wc_id == 0)
   {
       close(lake[1]);
       dup2(lake[0], 0);
       close(lake[0]);
       execlp("wc", "wc", NULL);
       return 5;
   }
   close(lake[0]);
   close(lake[1]);
   waitpid(prog1_id, &status1, 0);
   waitpid(prog2_id, &status2, 0);
   waitpid(wc_id, &status3, 0);
   if (WIFEXITED(status1) && WIFEXITED(status2) && WIFEXITED(status3) 
        && WEXITSTATUS(status3) == 0)
   {
       int fd;
       fd = open(argv[9], O_CREAT | O_WRONLY | O_APPEND, 0666); 
       dup2(fd, 1);
       close(fd);
       execlp(argv[7], argv[7], argv[8], NULL);
       return 6;
   } else
   {
      execlp("echo","echo","error",NULL);
      return 7;
   }
   return 0;
}
