// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <pthread.h>

// #define PORT 8080
// #define BUFFER_SIZE 1024

// // Receiver thread function
// void *receive_messages(void *arg)
// {
//     int client_socket = *(int *)arg;
//     char buffer[BUFFER_SIZE];

//     while (1)
//     {
//         memset(buffer, 0, BUFFER_SIZE);
//         int n = read(client_socket, buffer, BUFFER_SIZE);
//         if (n <= 0)
//         {
//             printf("Client disconnected.\n");
//             return NULL;
//         }
//         printf("Client: %s", buffer);
//     }
//     return NULL;
// }

// // Handle each connected client
// void *handle_client(void *arg)
// {
//     int client_socket = *(int *)arg;
//     free(arg);

//     pthread_t recv_thread;
//     pthread_create(&recv_thread, NULL, receive_messages, &client_socket);
//     pthread_detach(recv_thread); // Clean up automatically

//     char buffer[BUFFER_SIZE];
//     while (1)
//     {
//         memset(buffer, 0, BUFFER_SIZE);
//         fgets(buffer, BUFFER_SIZE, stdin);
//         if (send(client_socket, buffer, strlen(buffer), 0) <= 0)
//         {
//             printf("Send failed.\n");
//             break;
//         }
//     }

//     close(client_socket);
//     return NULL;
// }

// int main()
// {
//     int server_fd, client_socket;
//     struct sockaddr_in address;
//     socklen_t addrlen = sizeof(address);

//     // Create socket
//     if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
//     {
//         perror("Socket failed");
//         exit(EXIT_FAILURE);
//     }

//     address.sin_family = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port = htons(PORT);

//     // Bind socket
//     if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
//     {
//         perror("Bind failed");
//         exit(EXIT_FAILURE);
//     }

//     // Listen
//     if (listen(server_fd, 5) < 0)
//     {
//         perror("Listen failed");
//         exit(EXIT_FAILURE);
//     }

//     printf("Server listening on port %d...\n", PORT);

//     // Accept multiple clients
//     while (1)
//     {
//         client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
//         if (client_socket < 0)
//         {
//             perror("Accept failed");
//             continue;
//         }

//         printf("Client connected.\n");

//         int *new_sock = malloc(sizeof(int));
//         *new_sock = client_socket;

//         pthread_t thread;
//         pthread_create(&thread, NULL, handle_client, new_sock);
//         pthread_detach(thread); // auto cleanup
//     }

//     close(server_fd);
//     return 0;
// }

/* select()-based concurrent TCP chat server
 * Features:
 *   - select() multiplexing (no per-client threads)
 *   - SO_REUSEADDR to avoid "Address already in use"
 *   - Hard max-client guard with informative rejection message
 *   - send_all() for reliable partial-send handling
 *   - Full IP + port logging for every client event
 *   - Server can send targeted messages via stdin: <id>:<message>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 5000
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024

typedef struct
{
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
} Client;

static Client clients[MAX_CLIENTS];
static int client_count = 0;

/* Send all bytes in buf; returns total sent or -1 on error. */
static int send_all(int fd, const char *buf, int len)
{
    int total = 0;
    while (total < len)
    {
        int n = send(fd, buf + total, len - total, 0);
        if (n <= 0)
            return -1;
        total += n;
    }
    return total;
}

/* Broadcast msg to every client except skip_fd (-1 = send to all). */
static void broadcast(const char *msg, int skip_fd)
{
    int len = (int)strlen(msg);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].fd == skip_fd)
            continue;
        if (send_all(clients[i].fd, msg, len) < 0)
            printf("Broadcast to client %d failed\n", i + 1);
    }
}

/* Remove client at index idx (swap-with-last). */
static void remove_client(int idx)
{
    printf("Client %d (%s:%d) disconnected\n",
           idx + 1, clients[idx].ip, clients[idx].port);
    close(clients[idx].fd);
    clients[idx] = clients[client_count - 1];
    client_count--;
}

int main(void)
{
    /* --- Create server socket --- */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    /* SO_REUSEADDR: allow immediate rebind after crash/restart */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        exit(1);
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        exit(1);
    }

    printf("Server started on port %d (max %d clients)\n", PORT, MAX_CLIENTS);
    printf("Send a message: <id>:<message>\n");

    /* --- Main select() loop --- */
    while (1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        /* Watch stdin for server console input */
        FD_SET(STDIN_FILENO, &read_fds);

        /* Watch the listening socket for new connections */
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        /* Watch every active client socket */
        for (int i = 0; i < client_count; i++)
        {
            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].fd > max_fd)
                max_fd = clients[i].fd;
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0)
        {
            if (errno == EINTR)
                continue; /* interrupted by signal, retry */
            perror("select");
            break;
        }

        /* --- New incoming connection --- */
        if (FD_ISSET(server_fd, &read_fds))
        {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
            if (new_fd < 0)
            {
                perror("accept");
            }
            else if (client_count >= MAX_CLIENTS)
            {
                /* Max-client guard: inform and close immediately */
                const char *full_msg = "Server full. Try again later.\n";
                send_all(new_fd, full_msg, (int)strlen(full_msg));
                close(new_fd);

                char rej_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, rej_ip, sizeof(rej_ip));
                printf("Rejected %s:%d (max clients reached)\n",
                       rej_ip, ntohs(client_addr.sin_port));
            }
            else
            {
                clients[client_count].fd = new_fd;
                clients[client_count].port = ntohs(client_addr.sin_port);
                inet_ntop(AF_INET, &client_addr.sin_addr,
                          clients[client_count].ip, INET_ADDRSTRLEN);
                client_count++;

                printf("Client %d connected  IP=%-15s  port=%d\n",
                       client_count,
                       clients[client_count - 1].ip,
                       clients[client_count - 1].port);

                /* Tell client its assigned ID so the browser can display it */
                char welcome[64];
                int wlen = snprintf(welcome, sizeof(welcome),
                                    "YOURID:%d\n", client_count);
                send_all(new_fd, welcome, wlen);
            }
        }

        /* --- Server console: send targeted message to a client --- */
        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            char line[BUFFER_SIZE];
            if (fgets(line, sizeof(line), stdin) != NULL)
            {
                /* Server broadcast: "*:message" or "all:message" */
                char bcast_msg[BUFFER_SIZE];
                int id;
                char msg[BUFFER_SIZE];
                if (sscanf(line, "*:%[^\n]", bcast_msg) == 1 ||
                    sscanf(line, "all:%[^\n]", bcast_msg) == 1)
                {
                    char formatted[BUFFER_SIZE + 32];
                    int flen = snprintf(formatted, sizeof(formatted),
                                        "BROADCAST (Server): %s\n", bcast_msg);
                    broadcast(formatted, -1);
                    printf("[Broadcast -> %d clients]: %s\n",
                           client_count, bcast_msg);
                }
                else if (sscanf(line, "%d:%[^\n]", &id, msg) == 2)
                {
                    if (id >= 1 && id <= client_count)
                    {
                        char formatted[BUFFER_SIZE + 20];
                        int flen = snprintf(formatted, sizeof(formatted),
                                            "Server: %s\n", msg);
                        if (send_all(clients[id - 1].fd, formatted, flen) < 0)
                            printf("Send to client %d failed\n", id);
                        else
                            printf("To [%d] (%s:%d): %s\n",
                                   id,
                                   clients[id - 1].ip,
                                   clients[id - 1].port,
                                   msg);
                    }
                    else
                    {
                        printf("Invalid client ID: %d (active: %d)\n",
                               id, client_count);
                    }
                }
                else
                {
                    printf("Format: <id>:<message>  or  *:<message>\n");
                }
            }
        }

        /* --- Readable client sockets --- */
        for (int i = 0; i < client_count; i++)
        {
            if (!FD_ISSET(clients[i].fd, &read_fds))
                continue;

            char buffer[BUFFER_SIZE];
            int n = recv(clients[i].fd, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0)
            {
                /* n == 0: clean disconnect; n < 0: error */
                remove_client(i);
                i--; /* index shifted after swap-remove */
            }
            else
            {
                buffer[n] = '\0';
                printf("[%d] (%s:%d): %s",
                       i + 1, clients[i].ip, clients[i].port, buffer);

                /* Check for broadcast: "*:message" from a client */
                char bcast_msg[BUFFER_SIZE];
                int target_id;
                char dm_msg[BUFFER_SIZE];
                if (sscanf(buffer, "*:%[^\n]", bcast_msg) == 1)
                {
                    char formatted[BUFFER_SIZE + 32];
                    int flen = snprintf(formatted, sizeof(formatted),
                                        "BROADCAST from [%d]: %s\n", i + 1, bcast_msg);
                    broadcast(formatted, clients[i].fd); /* skip sender */
                    printf("[Broadcast] [%d] -> all: %s\n", i + 1, bcast_msg);
                    /* echo back to sender so they see their own broadcast */
                    char echo[BUFFER_SIZE + 32];
                    int elen = snprintf(echo, sizeof(echo),
                                        "BROADCAST (you -> all): %s\n", bcast_msg);
                    send_all(clients[i].fd, echo, elen);
                }
                /* Check if message is a client-to-client DM: "targetID:message" */
                else if (sscanf(buffer, "%d:%[^\n]", &target_id, dm_msg) == 2)
                {
                    if (target_id == i + 1)
                    {
                        const char *self_err = "Cannot send DM to yourself.\n";
                        send_all(clients[i].fd, self_err, (int)strlen(self_err));
                    }
                    else if (target_id >= 1 && target_id <= client_count)
                    {
                        char formatted[BUFFER_SIZE + 32];
                        int flen = snprintf(formatted, sizeof(formatted),
                                            "DM from [%d]: %s\n", i + 1, dm_msg);
                        if (send_all(clients[target_id - 1].fd, formatted, flen) < 0)
                            printf("DM relay to %d failed\n", target_id);
                        else
                            printf("DM [%d] -> [%d]: %s\n", i + 1, target_id, dm_msg);
                    }
                    else
                    {
                        char err[64];
                        int elen = snprintf(err, sizeof(err),
                                            "No client with ID %d.\n", target_id);
                        send_all(clients[i].fd, err, elen);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}