#define _POISIX_C_SOURCE 200112L

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ncurses.h>
#include <poll.h>
#include <unistd.h>

#define INMENU 2001
#define INLOBBIE 2002
#define INGAME 2003
#define EXIT 2004

#define MAXTIMESPAN 30

int state = INMENU;

struct game_t
{
    char* game_name;
    int max_players_number;
    int server_number;
};

struct game_t* games = NULL;
char** players = NULL;
int number_of_games = 0;
int number_of_players = 0;

int min(int a, int b)
{
    return a < b ? a : b;
}

int open_connection(char* address, u_int16_t user_port)
{
    u_int16_t port_number;
    struct sockaddr_in addr;
    struct hostent* host;
    int server_socket;
    if (user_port < 0 || user_port >= 65536)
    {
        printf("Invalid port number.\n");
        exit(1);
    }
    port_number = htons(user_port);
    addr.sin_family = AF_INET;
    addr.sin_port = port_number;
    host = gethostbyname(address);
    if (host == NULL)
    {
        printf("Invalid server address.\n");
        exit(1);
    }
    memcpy(&addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(server_socket, (struct sockaddr*)&addr,
                sizeof(addr)) == -1)
    {
        printf("Unable to connect to server.\n");
        exit(1);
    }
    return server_socket;
}

void print_message_center(WINDOW* win, int line, char* buf)
{
    mvwaddstr(win, line, (win->_maxx - strlen(buf)) / 2, buf);
    wrefresh(win);
}

void read_fake_addition(int socket)
{
    int a, b, d;
    char* c;
    read(socket, &a, sizeof(int));
    read(socket, &b, sizeof(int));
    c = malloc(b * sizeof(char));
    read(socket, c, b * sizeof(char));
    read(socket, &d, sizeof(int));
    free(c);
}

void read_fake_deletion(int socket)
{
    int a;
    read(socket, &a, sizeof(int));
}

void wait_server_msgs_menu(struct pollfd* server)
{
    char action;
    int socket = server->fd;
    if (poll(server, 1, MAXTIMESPAN))
    {
        read(socket, &action, sizeof(char));
        if (action == 'A')
        {
            struct game_t game;
            int len;
            read(socket, &game.server_number, sizeof(int));
            read(socket, &len, sizeof(int));
            game.game_name = malloc(len * sizeof(char));
            read(socket, game.game_name, len * sizeof(char));
            read(socket, &game.max_players_number, sizeof(int));
            games = realloc(games, (++number_of_games) 
                                    * sizeof(struct game_t));
            games[number_of_games - 1] = game;
        }
        else if (action == 'D')
        {
            int number, i, j;
            read(socket, &number, sizeof(int));
            for (i = 0; i < number_of_games 
                        && games[i].server_number != number; i++);
            if (i < number_of_games)
            {
                number_of_games--;
                for (j = i; j < number_of_games; j++)
                    games[j] = games[j + 1];
            }
        }
    }
}

void reset_structures()
{
    if (players != NULL)
        free(players);
    if (games != NULL)
        free(games);
    number_of_games = 0;
    number_of_players = 0;
    players = NULL;
    games = NULL;
}

void wait_server_msgs_lobbie(struct pollfd* server)
{    
    char action;
    int socket = server->fd;
    if (poll(server, 1, MAXTIMESPAN))
    {
        read(socket, &action, sizeof(char));
        if (action == 'A')
            read_fake_addition(socket);
        else if (action == 'D')
            read_fake_deletion(socket);
        else if (action == '+')
        {
            int len;
            read(socket, &len, sizeof(int));
            players = realloc(players, (++number_of_players) * sizeof(char*));
            players[number_of_players - 1] = malloc(len * sizeof(char));
            read(socket, players[number_of_players - 1], len * sizeof(char));
        }
        else if (action == 'C')
        {
            state = INMENU;
            reset_structures();
        }
        else if (action == 'S')
        {
            state = INGAME;
            reset_structures();
        }
    }
}

void print_menu(WINDOW* menu_win, int highlight, int menu_height)
{
    int i, start, finish;
    char* message;
    print_message_center(stdscr, LINES / 10, "Choose game or host your own");
    print_message_center(stdscr, LINES / 10 + 2, 
            "Use arrow keys to move, space to select, ESC to exit");
    wclear(menu_win);
    box(menu_win, 0, 0);
    start = highlight - highlight % menu_height;
    finish = min(start + menu_height - 1, number_of_games);
    for (i = start; i <= finish; i++)
    {
        if (highlight == i)
            wattron(menu_win, A_REVERSE);
        if (i == 0)
            message = "*** Host Game ***";
        else
            message = games[i - 1].game_name;
        print_message_center(menu_win, i - start + 1, message); 
        if (highlight == i)
            wattroff(menu_win, A_REVERSE);
    }
    refresh();
}

void print_lobbie(WINDOW* lobbie_win, char* game_name, int max_number,int host)
{
    char buf[20];
    int i;
    print_message_center(stdscr, LINES / 8, game_name);
    sprintf(buf, "%d / %d", number_of_players, max_number);
    print_message_center(stdscr, LINES / 8 + 2, buf);
    print_message_center(stdscr, LINES - 6, 
            "Press ESC to leave lobbie. This will cause canceling the game");
    if (host)
        print_message_center(stdscr, LINES - 4, 
                             "Press space to start the game");
    box(lobbie_win, 0, 0);
    for (i = 0; i < number_of_players; i++)
        print_message_center(lobbie_win, i + 1, players[i]);
    refresh();
}

void read_keys_lobbie(int socket, int host)
{
   int c;
   c = getch();
   if (c != ERR)
       switch(c)
       {
           case 27:
               write(socket, "C", sizeof(char));
               break;
           case ' ':
                if (host)
                    write(socket, "S", sizeof(char));
                break;
       }
}

void game_start(int socket)
{
    int c;
    clear();
    print_message_center(stdscr, LINES / 2, "Game will be here soon");
    print_message_center(stdscr, LINES / 2 + 2, "Press ESC to return to menu");
    refresh();
    while ((c = getch()) != 27)
    {}
    write(socket, "D", sizeof(char));
    state = INMENU;
}

void game_lobbie(int socket, char* name, char* game_name, 
                 int max_number, int host)
{
    struct pollfd server;
    WINDOW* lobbie_win;
    clear();
    halfdelay(1);
    server.fd = socket;
    server.events = POLLIN;
    state = INLOBBIE;
    players = malloc(sizeof(char*));
    players[0] = name;
    number_of_players = 1;
    lobbie_win = newwin(max_number + 2, 60, 
                        (LINES - max_number - 2) / 2,
                        (COLS - 60) / 2);
    while (state == INLOBBIE)
    {
        print_lobbie(lobbie_win, game_name, max_number, host);
        read_keys_lobbie(socket, host);
        wait_server_msgs_lobbie(&server);
    }
    if (state == INGAME)
       game_start(socket); 
    else
    {
        clear();
        print_message_center(stdscr, LINES / 2, "Game was canceled");
        refresh();
        sleep(1);
    }
}

void hosting_game_menu(char* name, int socket)
{
    char gname[61];
    char message[150];
    int max_number;
    clear();
    echo();
    print_message_center(stdscr, LINES / 3, 
                         "Enter name of game (60 letters max):");
    mvgetnstr(LINES / 2, COLS / 2 - 5, gname, 60);
    clear();
    print_message_center(stdscr, LINES / 3, 
                            "Enter number of players (8 max):");
    mvscanw(LINES / 2, COLS / 2, "%d", &max_number);
    if (max_number > 8)
        max_number = 8;
    sprintf(message, "%s~H~%s~%d~", name, gname, max_number);
    write(socket, message, (strlen(message) + 1) * sizeof(char));
    noecho();
    game_lobbie(socket, name, gname, max_number, 1);
}

void try_to_connect(struct game_t game, char* name, int socket)
{
    char message[150];
    char answer;
    int number = game.server_number;
    char* game_name = game.game_name;
    int max_number = game.max_players_number;
    sprintf(message, "%s~C~%d~", name, number);
    write(socket, message, (strlen(message) + 1) * sizeof(char));
    do
    {
        read(socket, &answer, sizeof(char));
        if (answer == 'A')
            read_fake_addition(socket);
        else if (answer == 'D')
            read_fake_deletion(socket);
        else if (answer == '+')
            game_lobbie(socket, name, game_name, max_number, 0);
        else if (answer == '-')
            print_message_center(stdscr, LINES - 2, "Unable to connect to game");
    } while (answer != '+' && answer != '-');
}

void leave_server(int socket, char* name)
{
    char message[80];
    sprintf(message, "%s~D~", name);
    write(socket, message, (strlen(message) + 1) * sizeof(char));
}

void read_keys_menu(int* highlight, char* name, int socket)
{
    int c;
    c = getch();
    if (c != ERR)
        switch(c)
        {
            case KEY_UP:
                (*highlight)--;
                if ((*highlight) < 0)
                    *highlight = 0;
                break;
            case KEY_DOWN:
                (*highlight)++;
                if ((*highlight) > number_of_games)
                    *highlight = number_of_games;
                break;
            case ' ':
                if (*highlight == 0)
                    hosting_game_menu(name, socket);
                else
                    try_to_connect(games[*highlight - 1], 
                                    name, socket);
                break;
            case 27:
                state = EXIT;
                leave_server(socket, name);
                break;
        }
}

void choice_menu(char* name, int server_socket)
{
    WINDOW* menu_win;
    struct pollfd server;
    int menu_height = LINES / 2 - 2;
    int highlight = 0;
    halfdelay(1);
    server.fd = server_socket;
    server.events = POLLIN;
    clear();
    menu_win = newwin(LINES / 2, COLS / 2, LINES / 4, COLS / 4);
    while (state != EXIT)
    {
        wait_server_msgs_menu(&server);
        if (highlight > number_of_games)
            highlight = number_of_games;
        print_menu(menu_win, highlight, menu_height);
        read_keys_menu(&highlight, name, server_socket);
    }
}

void client_work(int server_socket)
{
    char name[61];
    print_message_center(stdscr, LINES / 3, 
                            "Enter your name(60 letters max): ");
    echo();
    move(LINES / 2, COLS / 2 - 5);
    getnstr(name, 60);
    noecho();
    choice_menu(name, server_socket);
}

void init_curses()
{ 
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, true);
}

int main(int argc, char** argv)
{
    int server_socket;
    if (argc < 2)
    {
        printf("Server address expected\n");
        return 1;
    }
    if (argc < 3)
    {
        printf("Port number expected\n");
        return 1;
    }
    server_socket = open_connection(argv[1], atoi(argv[2]));
    init_curses();
    client_work(server_socket);
    endwin();
    reset_structures();
    return 0;
}
