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

#define MAX_TIMESPAN 50

#define WAITING 1001
#define RUNNING 1002
#define FINISHED 1003
#define CANCELED 1004

#define EMPTY '.'
#define WALL '#'
#define MEDKIT '+'
#define POISON '-'
#define SURPRISE '?'

struct player_t
{
    char* name;
    int socket;
    int health;
    int y, x;
};

struct game_t
{
    char* game_name;
    struct player_t* players;
    int number_of_players;
    int max_number_of_players;
    int state; /* WAITING, RUNNING, FINISHED, CANCELED */
    int n_winner;
    pthread_t thread;
};

struct koordinate_t
{
    int y;
    int x;
};

int server_active = 1;
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

char** game_map;
int game_map_n, game_map_m;
int max_health, max_plus, max_minus;
int damage_base, damage_delta, damage_radius;
int n_medkits, n_poisons, n_surprise;
struct koordinate_t* empties;
int n_empties;
int moratory;

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
    fscanf(stream, "%d%d%d\n", &max_health, &max_plus, &max_minus);
    fscanf(stream, "%d%d%d\n", &n_medkits, &n_poisons, &n_surprise);
    fscanf(stream, "%d%d%d\n", &damage_base, &damage_delta, &damage_radius);
    fscanf(stream, "%d\n", &moratory);
    fscanf(stream, "%d%d\n", &n, &m);
    empties = malloc(n * m * sizeof(struct koordinate_t));
    n_empties = 0;
    game_map = (char**)malloc(n * sizeof(char*)); 
    for (i = 0; i < n; i++)
    {
        game_map[i] = (char*)malloc(m * sizeof(char));
        for (j = 0; j < m; j++)
        {
            game_map[i][j] = fgetc(stream);
            if (game_map[i][j] == EMPTY)
            {
                n_empties++;
                empties[n_empties - 1].y = i;
                empties[n_empties - 1].x = j;
            }
        }
        fgetc(stream);
    }
    empties = realloc(empties, n_empties * sizeof(struct koordinate_t));
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

struct koordinate_t put_in_random_place(char** map, int symbol)
{
    char flag = 1;
    int y, x, ind;
    struct koordinate_t result;
    do
    {
        ind = rand() % n_empties;
        y = empties[ind].y;
        x = empties[ind].x;
        if (map[y][x] == EMPTY)
        {
           map[y][x] = symbol;
           flag = 0;
        }
    } while (flag);
    result.x = x;
    result.y = y;
    return result;
}

void init_map(char** map, struct player_t* players, int number_of_players)
{
    int i, j;
    struct koordinate_t player_place;
    for (i = 0; i < game_map_n; i++)
        for (j = 0; j < game_map_m; j++)
            map[i][j] = game_map[i][j];
    for (i = 0; i < n_medkits; i++)
        put_in_random_place(map, MEDKIT);
    for (i = 0; i < n_poisons; i++)
        put_in_random_place(map, POISON);
    for (i = 0; i < n_surprise; i++)
        put_in_random_place(map, SURPRISE);
    for (i = 0; i < number_of_players; i++)
    {
        player_place = put_in_random_place(map, '1' + i);
        players[i].x = player_place.x;
        players[i].y = player_place.y;
    }
}

void send_map(char** map, int socket)
{
    int i;
    write(socket, &max_health, sizeof(int));
    write(socket, &moratory, sizeof(int));
    write(socket, &game_map_n, sizeof(int));
    write(socket, &game_map_m, sizeof(int));
    for (i = 0; i < game_map_n; i++)
        write(socket, map[i], game_map_m * sizeof(char));
}

void send_players(int socket, struct player_t* players, int n_players)
{
    int i, len;
    write(socket, &n_players, sizeof(int));
    for (i = 0; i < n_players; i++)
    {
        len = strlen(players[i].name) + 1;
        write(socket, &len, sizeof(int));
        write(socket, players[i].name, len * sizeof(char));
        write(socket, &players[i].x, sizeof(int));
        write(socket, &players[i].y, sizeof(int));
    }
}

void share_player_information(int number, struct player_t* players, 
                              int n_players, char** map)
{
    int i, socket;
    struct player_t player = players[number];
    char letter = '1' + number;
    for (i = 0; i < n_players; i++)
    {
        socket = players[i].socket;
        if (socket > 0)
        {
            write(socket, &letter, sizeof(char));
            write(socket, &player.x, sizeof(int));
            write(socket, &player.y, sizeof(int));
            write(socket, &player.health, sizeof(int));
        }
    }
    if (players[number].health == 0)
        map[players[number].y][players[number].x] = EMPTY;
}

int check_game_finish(struct player_t* players, int n_players)
{
    int i, socket;
    int k = 0;
    int q = -1;
    for (i = 0; i < n_players; i++)
        if (players[i].health > 0)
        {
            k++;
            q = i;
        }
    if (k == 1)
    {
        for (i = 0; i < n_players; i++)
            if (players[i].socket > 0)
            {
                socket = players[i].socket;
                write(socket, "W", sizeof(char));
                write(socket, &q, sizeof(int));
            }
        return q;
    }
    else
        return -1;
}

int valid_point(char** map, int x, int y)
{
    if (y >= game_map_n || x >= game_map_m || y < 0 || x < 0)
        return 0;
    return (map[y][x] == EMPTY || map[y][x] == MEDKIT
            || map[y][x] == POISON || map[y][x] == SURPRISE);
}

void eat_medkit(struct player_t* player)
{
    int heal = rand() % max_plus + 1;
    player->health += heal;
}

void drink_poison(struct player_t* player)
{
    int damage = rand() % max_minus + 1;
    player->health -= damage;
    if (player->health < 0)
        player->health = 0;
}

int move(int player_number, struct player_t* players, int n_players, 
             char** local_map, int dx, int dy)
{
    int old_x = players[player_number].x;
    int old_y = players[player_number].y;
    int new_x = old_x + dx;
    int new_y = old_y + dy;
    if (players[player_number].health == 0 || 
        !valid_point(local_map, new_x, new_y))
    return -1;
    if (local_map[new_y][new_x] == MEDKIT)
        eat_medkit(&players[player_number]);
    if (local_map[new_y][new_x] == POISON)
        drink_poison(&players[player_number]);
    if (local_map[new_y][new_x] == SURPRISE)
    {
        if (rand() % 2 == 1)
            eat_medkit(&players[player_number]);
        else
            drink_poison(&players[player_number]);
    }
    local_map[new_y][new_x] = local_map[old_y][old_x];
    local_map[old_y][old_x] = EMPTY;
    players[player_number].x = new_x;
    players[player_number].y = new_y;
    share_player_information(player_number, players, n_players, local_map);
    if (players[player_number].health == 0)
        return check_game_finish(players, n_players);
    else
        return -1;
}

void damage_from_blow(int player_number, int distanse, struct player_t* players,
                        int n_players, char** local_map)
{
    int damage = damage_base - (distanse - 1) * damage_delta;
    players[player_number].health -= damage;
    if (players[player_number].health < 0)
        players[player_number].health = 0;
    share_player_information(player_number, players, n_players, local_map);
}

void try_add_to_queue(struct koordinate_t* queue, char** used, 
                      char** local_map, int* r, int x, int y, int oldval)
{
    if (y >= game_map_n || x >= game_map_m || y < 0 || x < 0)
        return;
    if (used[y][x] == 0 && local_map[y][x] != WALL && oldval <= damage_radius)
    {
        used[y][x] = oldval + 1;
        (*r)++;
        queue[*r].x = x;
        queue[*r].y = y;
    }
}

int blow(int player_number, struct player_t* players, int n_players,
           char** local_map)
{
    int i, max_queue_size, j, l, r, x, y;
    char** used;
    struct koordinate_t* queue;
    if (players[player_number].health == 0)
        return -1;
    used = (char**)malloc(game_map_n * sizeof(char*));
    for (i = 0; i < game_map_n; i++)
        used[i] = (char*)malloc(game_map_m * sizeof(char));
    for (i = 0; i < game_map_n; i++)
        for (j = 0; j < game_map_m; j++)
            used[i][j] = 0;
    max_queue_size = (2 * damage_radius + 1) * (2 * damage_radius + 1);
    queue = (struct koordinate_t*)malloc(max_queue_size
                                            * sizeof(struct koordinate_t));
    queue[0].x = players[player_number].x;
    queue[0].y = players[player_number].y;
    used[queue[0].y][queue[0].x] = 1;
    l = r = 0;
    do
    {
        x = queue[l].x;
        y = queue[l].y;
        try_add_to_queue(queue, used, local_map, &r, x - 1, y, used[y][x]);
        try_add_to_queue(queue, used, local_map, &r, x + 1, y, used[y][x]);
        try_add_to_queue(queue, used, local_map, &r, x, y - 1, used[y][x]);
        try_add_to_queue(queue, used, local_map, &r, x, y + 1, used[y][x]);
        if (local_map[y][x] >= '1' && local_map[y][x] <= '8' && l > 0)
            damage_from_blow(local_map[y][x] - '1', used[y][x] - 1, 
                             players, n_players, local_map);
        l++;
    } while (l <= r);
    free(queue);
    for (i = 0; i < game_map_n; i++)
        free(used[i]);
    free(used);
    return check_game_finish(players, n_players);
}

void get_ready_and_start_game(struct player_t* players, int n_players)
{
    int i;
    char t;
    for (i = 0; i < n_players; i++)
        read(players[i].socket, &t, sizeof(char));
    for (i = 0; i < n_players; i++)
        write(players[i].socket, "G", sizeof(char));
}

void* game_start(void* arg)
{
    struct pollfd* game_players;
    char** local_map;
    struct player_t* players;
    char* game_name;
    int number_of_players;
    int i, ind;
    int connected_players;
    int q = -1;
    ind = *((int*)arg);

    pthread_mutex_lock(&game_mutex);
    games[ind].state = RUNNING;
    players = games[ind].players;
    game_name = games[ind].game_name;
    number_of_players = games[ind].number_of_players;
    pthread_mutex_unlock(&game_mutex);

    local_map = malloc(game_map_n * sizeof(char*));
    for (i = 0; i < game_map_n; i++)
        local_map[i] = malloc(game_map_m * sizeof(char));
    init_map(local_map, players, number_of_players);
    game_players = malloc(number_of_players * sizeof(struct pollfd));
    connected_players = number_of_players;
    for (i = 0; i < number_of_players; i++)
    {
        game_players[i].fd = players[i].socket;
        game_players[i].events = POLLIN;
        write(players[i].socket, &i, sizeof(int));
        send_map(local_map, players[i].socket);
        send_players(players[i].socket, players, number_of_players);
        players[i].health = max_health;
    }

    get_ready_and_start_game(players, number_of_players);

    while (connected_players)
    {
        poll(game_players, number_of_players, -1);
        for (i = 0; i < number_of_players; i++)
            if (game_players[i].revents == POLLIN)
            {
                char decision;
                read(game_players[i].fd, &decision, sizeof(char));
                if (decision == 'Q')
                {
                    write(players[i].socket, "Q", sizeof(char));
                    return_to_menu(players[i].socket);
                    game_players[i].fd = -game_players[i].fd;
                    players[i].socket = -players[i].socket;
                    printf("Player %s disconnected from the game %s.\n", 
                            players[i].name, game_name);
                    connected_players--;
                    if (q == -1)
                    {
                        players[i].health = 0;
                        share_player_information(i, players, 
                                        number_of_players, local_map);
                        q = check_game_finish(players, number_of_players);
                    }
                }
                else if (q == -1)
                {
                    if (decision == 'L')
                    q = move(i, players, number_of_players, local_map, -1, 0);
                    else if (decision == 'R')
                    q = move(i, players, number_of_players, local_map, 1, 0);
                    else if (decision == 'U')
                    q = move(i, players, number_of_players, local_map, 0, -1);
                    else if (decision == 'D')
                    q = move(i, players, number_of_players, local_map, 0, 1);
                    else if (decision == 'B')
                    q = blow(i, players, number_of_players, local_map);
                }
            }
    }

    free(game_players);
    for (i = 0; i < game_map_n; i++)
        free(local_map[i]);
    free(local_map);
    pthread_mutex_lock(&game_mutex);
    games[ind].state = FINISHED;
    games[ind].n_winner = q;
    printf("Game %s finished. %s wins the round.\n", games[ind].game_name,
            players[q].name);
    pthread_mutex_unlock(&game_mutex);

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
    dup2(journal, 1); 
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
    close(journal);
    return 0;
}
