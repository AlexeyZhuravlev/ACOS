#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#define WALL '#'
#define EMPTY '.'

#define MAX_TIMESPAN 50

#define WAITING 1001
#define RUNNING 1002
#define FINISHED 1003
#define CANCELED 1004


struct player_t
{
    char* name;
    int socket;
};

struct game_t
{
    char* game_name;
    struct player_t* players;
    int number_of_players;
    int max_number_of_players;
    int state; /* WAITING, RUNNING, FINISHED, CANCELED */
    pthread_t thread;
};

int server_active = 1;
char** game_map;
int game_map_n, game_map_m;
struct game_t* games = NULL;
int number_of_games = 0;
int* thread_game_numbers = NULL;
int n_thread_game_numbers = 0;
pthread_mutex_t game_mutex;
struct pollfd* players = NULL;
int n_players = 0;
struct pollfd* hosts = NULL;
int* host_to_game = NULL;
int n_hosts = 0;
pthread_mutex_t players_mutex;

char* strdup(const char *s);

void read_map(char* filename)
{
    int n, m, i, j;
    FILE* stream = fopen(filename, "r");
    if (stream == NULL)
    {
        printf("Invalid map filename\n");
        exit(1);
    }
    fscanf(stream, "%d%d", &n, &m);
    game_map = (char**)malloc(n * sizeof(char*));
    for (i = 0; i < n; i++)
    {
        game_map[i] = (char*)malloc(m * sizeof(char));
        for (j = 0; j < m; j++)
            game_map[i][j] = fgetc(stream);
        fgetc(stream);
    }
    game_map_n = n;
    game_map_m = m;
}

int open_connection(u_int16_t tmp_port)
{
    int server_socket;
    struct sockaddr_in addr;
    u_int16_t port = htons(tmp_port);
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr.s_addr = INADDR_ANY;
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        printf("Unable to open connection on port\n");
        exit(1);
    }
    listen(server_socket, 100);
    return server_socket;
}

void share_game_to_player(int socket, int n)
{
    int game_name_length = strlen(games[n].game_name) + 1;
    write(socket, "A", sizeof(char));
    write(socket, &n, sizeof(int));
    write(socket, &game_name_length, sizeof(int));
    write(socket, games[n].game_name, game_name_length * sizeof(char));
    write(socket, &games[n].max_number_of_players, sizeof(int));
}


void share_game_delete(int socket, int n)
{
    write(socket, "D", sizeof(char));
    write(socket, &n, sizeof(int));
}

void shake_names(int s1, char* n1, int s2, char* n2)
{
    int l1 = strlen(n1) + 1;
    int l2 = strlen(n2) + 1;
    write(s1, "+", sizeof(char));
    write(s1, &l2, sizeof(int));
    write(s1, n2, l2 * sizeof(char));
    write(s2, "+", sizeof(char));
    write(s2, &l1, sizeof(int));
    write(s2, n1, l1 * sizeof(char));
}

void share_all_active_games(int socket)
{
    int i;
    pthread_mutex_lock(&game_mutex);
    for (i = 0; i < number_of_games; i++)
        if (games[i].state == WAITING)
            share_game_to_player(socket, i);
    pthread_mutex_unlock(&game_mutex);
}

void accept_connections(struct pollfd* server)
{
   if (poll(server, 1, MAX_TIMESPAN))
   {
       struct sockaddr client_address;
       socklen_t addrlen;
       char* client_ip;
       int client_socket = accept(server[0].fd, &client_address, &addrlen);
       client_ip = inet_ntoa(((struct sockaddr_in*)&client_address)->sin_addr);
       printf("%s connected to server.\n", client_ip);
       share_all_active_games(client_socket);
       pthread_mutex_lock(&players_mutex);
       n_players++;
       players = realloc(players, n_players * sizeof(struct pollfd));
       players[n_players - 1].fd = client_socket;
       players[n_players - 1].events = POLLIN;
       pthread_mutex_unlock(&players_mutex);
   }
}

void parse_words(char* buf, char*** words, int* n)
{
    char* pointer;
    while ((pointer = strstr(buf, "~")) != NULL)
    {
        *pointer = '\0';
        (*n)++;
        *words = realloc(*words, (*n) * sizeof(char*));
        (*words)[(*n) - 1] = strdup(buf);
        buf = pointer + 1;
    }
}

void get_information()
{
    pthread_mutex_lock(&players_mutex);
    if (poll(players, n_players, MAX_TIMESPAN))
    {
        int i;
        for (i = 0; i < n_players; i++)
            if (players[i].revents == POLLIN)
            {
                char buf[150];
                int n_words = 0;
                char** words = NULL;
                int j;
                read(players[i].fd, buf, 150);
                parse_words(buf, &words, &n_words);
                if (words[1][0] == 'H')
                {
                    pthread_mutex_lock(&game_mutex);
                    number_of_games++;
                    games = realloc(games, number_of_games 
                                    * sizeof(struct game_t));
                    games[number_of_games - 1].game_name = words[2];
                    games[number_of_games - 1].number_of_players = 1;
                    games[number_of_games - 1].state = WAITING;
                    games[number_of_games - 1].players 
                        = malloc(sizeof(struct player_t));
                    games[number_of_games - 1].players[0].name = words[0];
                    games[number_of_games - 1].players[0].socket 
                        = players[i].fd;
                    games[number_of_games - 1].max_number_of_players 
                        = atoi(words[3]);
                    pthread_mutex_unlock(&game_mutex);
                    players[i].fd = -players[i].fd;
                    for (j = 0; j < n_players; j++)
                        if (players[j].fd > 0)
                            share_game_to_player(players[j].fd, 
                                                 number_of_games - 1);
                    n_hosts++;
                    hosts = realloc(hosts, (n_hosts) * sizeof(struct pollfd));
                    hosts[n_hosts - 1].fd = -players[i].fd;
                    hosts[n_hosts - 1].events = POLLIN;
                    host_to_game = realloc(host_to_game, n_hosts * sizeof(int));
                    host_to_game[n_hosts - 1] = number_of_games - 1;
                    printf("%s hosted game %s\n", words[0], words[2]);
                }
                else 
                if (words[1][0] == 'C')
                {
                    char* name = words[0];
                    int number_of_game = atoi(words[2]);
                    if (games[number_of_game].number_of_players 
                        == games[number_of_game].max_number_of_players ||
                        games[number_of_game].state != WAITING)
                        write(players[i].fd, "-", sizeof(char));
                    else
                    {
                        int n;
                        write(players[i].fd, "+", sizeof(char));
                        n_hosts++;
                        hosts = realloc(hosts, n_hosts * sizeof(struct pollfd));
                        hosts[n_hosts - 1].fd = players[i].fd;
                        hosts[n_hosts - 1].events = POLLIN;
                        host_to_game = realloc(host_to_game, n_hosts 
                                                            * sizeof(int));
                        host_to_game[n_hosts - 1] = number_of_game;
                        n = ++games[number_of_game].number_of_players;
                        games[number_of_game].players = 
                            realloc(games[number_of_game].players, 
                                        n * sizeof(struct player_t));
                        games[number_of_game].players[n - 1].name = name;
                        games[number_of_game].players[n - 1].socket 
                            = players[i].fd;
                        for (j = 0; 
                             j < games[number_of_game].number_of_players - 1; 
                             j++)
                            shake_names(players[i].fd, name, 
                                        games[number_of_game].players[j].socket,
                                        games[number_of_game].players[j].name);
                        printf("%s entered game %s\n", 
                                name, games[number_of_game].game_name);
                        players[i].fd = -players[i].fd;
                    }
                }
                else
                {
                    shutdown(players[i].fd, SHUT_RDWR);
                    printf("%s disconnected from server.\n", words[0]);
                    players[i].fd = -players[i].fd;
                }
            }
    }
    pthread_mutex_unlock(&players_mutex);
}

void return_to_menu(int socket)
{
    int i;
    pthread_mutex_lock(&players_mutex);
    for (i = 0; i < n_players; i++)
        if (players[i].fd == -socket)
            players[i].fd = -players[i].fd;
    pthread_mutex_unlock(&players_mutex);
    share_all_active_games(socket);
}

void* game_start(void* arg)
{
    struct pollfd* game_players;
    int i;
    int alive_players;
    struct game_t game;
    pthread_mutex_lock(&game_mutex);
    game = games[*((int*)arg)];
    pthread_mutex_unlock(&game_mutex);
    game_players = malloc(game.number_of_players * sizeof(struct pollfd));
    alive_players = game.number_of_players;
    for (i = 0; i < game.number_of_players; i++)
    {
        game_players[i].fd = game.players[i].socket;
        game_players[i].events = POLLIN;
    }
    while (alive_players)
    {
        if (poll(game_players, game.number_of_players, MAX_TIMESPAN))
        {
            for (i = 0; i < game.number_of_players; i++)
                if (game_players[i].revents == POLLIN)
                {
                    char decision;
                    read(game_players[i].fd, &decision, sizeof(char));
                    if (decision == 'D')
                    {
                        return_to_menu(game.players[i].socket);
                        game_players[i].fd = -game_players[i].fd;
                        printf("Player %s left the game %s.\n", 
                                game.players[i].name, game.game_name);
                        alive_players--;
                    }
                }
        }
    }
    free(game_players);
    pthread_mutex_lock(&game_mutex);
    games[*((int*)arg)].state = FINISHED;
    pthread_mutex_unlock(&game_mutex);
    printf("Game %s finished.\n", game.game_name);
    return NULL;
}

void get_inside_lobbie_information()
{
   if (poll(hosts, n_hosts, MAX_TIMESPAN))
   {
       int i;
       for (i = 0; i < n_hosts; i++)
           if (hosts[i].revents == POLLIN)
           {
               char decision;
               read(hosts[i].fd, &decision, sizeof(char));
               if (decision == 'S')
               {
                   int game_number = host_to_game[i];
                   if (games[game_number].number_of_players 
                           == games[game_number].max_number_of_players)
                   {
                       int j;
                       pthread_t game;
                       for (j = 0; 
                               j < games[game_number].number_of_players; 
                                 j++)
                           write(games[game_number].players[j].socket, 
                                   "S", sizeof(char));
                       for (j = 0; j < n_players; j++)
                           if (players[j].fd > 0)
                               share_game_delete(players[j].fd, game_number);
                       for (j = 0; j < n_hosts; j++)
                           if (host_to_game[j] == game_number)
                               hosts[j].fd = -hosts[j].fd;
                       games[game_number].state = RUNNING;
                       n_thread_game_numbers++;
                       thread_game_numbers = realloc(thread_game_numbers, 
                                    n_thread_game_numbers * sizeof(int));
                       thread_game_numbers[n_thread_game_numbers - 1] 
                           = game_number;
                       pthread_create(&game, NULL, game_start, 
                               &thread_game_numbers[n_thread_game_numbers - 1]);
                       games[game_number].thread = game;
                       printf("Game %s started.\n", 
                               games[game_number].game_name);
                   }
               }
               else
               {
                  int game_number;
                  int j;
                  game_number = host_to_game[i];
                  for (j = 0; j < games[game_number].number_of_players; j++)
                  {
                      write(games[game_number].players[j].socket, "C", 
                              sizeof(char));
                      return_to_menu(games[game_number].players[j].socket);
                  }
                  for (j = 0; j < n_hosts; j++)
                      if (host_to_game[j] == game_number)
                          hosts[j].fd = -hosts[j].fd;
                  pthread_mutex_lock(&players_mutex);
                  for (j = 0; j < n_players; j++)
                      if (players[j].fd > 0)
                          share_game_delete(players[j].fd, game_number);
                  pthread_mutex_unlock(&players_mutex);
                  games[game_number].state = CANCELED;
                  printf("Game %s was canceled. One player left.\n", 
                          games[game_number].game_name);
               }
           }
   }
}

void server_work(int server_socket)
{
   struct pollfd connection_waiting; 
   connection_waiting.fd = server_socket;
   connection_waiting.events = POLLIN;
   while (server_active)
   {
       accept_connections(&connection_waiting);
       get_information();
       get_inside_lobbie_information();
   }
}

void handler(int sig)
{
    server_active = 0;
}

void free_all_structures()
{
    int i;
    for (i = 0; i < game_map_n; i++)
        free(game_map[i]);
    free(game_map);
    for (i = 0; i < number_of_games; i++)
    {
        free(games[i].game_name);
        free(games[i].players);
        if (games[i].state == FINISHED)
            pthread_join(games[i].thread, NULL);
    }
    free(games);
    free(players);
    free(hosts);
    free(host_to_game);
}

int main(int argc, char** argv)
{
    u_int16_t port;
    int serv_socket_number;
    int journal;
    struct sigaction sigact;
    sigact.sa_handler = handler;
    sigaction(SIGINT, &sigact, NULL);
    journal = open("journal", O_CREAT | O_WRONLY, 0666);
    if (journal == -1)
    {
        printf("Unable to open/create journal file\n");
        return 1;
    }
 /*   dup2(journal, 1); */
    if (argc < 2)
    {
        printf("File with map expected\n");
        return 1;
    }
    read_map(argv[1]);
    if (argc < 3)
    {
        printf("Port number expected\n");
        return 1;
    }
    port = atoi(argv[2]);
    if (port < 0 || port >= 65536)
    {
        printf("Invalid port number\n");
        return 1;
    }
    pthread_mutex_init(&game_mutex, NULL);
    pthread_mutex_init(&players_mutex, NULL);
    serv_socket_number = open_connection(port);
    server_work(serv_socket_number);
    free_all_structures();
    return 0;
}
