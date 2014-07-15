#define _POISIX_C_SOURCE 200112L

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <curses.h>

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

void client_work(int server_socket)
{
    char name[61];
    printf("Enter your name(60 letters max): ");
    fgets(name, 60, stdin);
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
    client_work(server_socket);
    return 0;
}
