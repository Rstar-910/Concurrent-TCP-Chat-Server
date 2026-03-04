// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <pthread.h>

// #define PORT 8080
// #define SIZE 1024

// int sock;

// void *receive_messages(void *arg)
// {
//     char buffer[SIZE];
//     while (1)
//     {
//         memset(buffer, 0, SIZE);
//         int n = read(sock, buffer, SIZE);
//         if (n <= 0)
//         {
//             printf("Server disconnected.\n");
//             close(sock);
//             exit(0);
//         }
//         printf("Server: %s", buffer);
//     }
//     return NULL;
// }

// int main()
// {
//     struct sockaddr_in serv_addr;
//     char buffer[SIZE];

//     sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0)
//     {
//         perror("Socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

//     if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
//     {
//         perror("Connection failed");
//         exit(EXIT_FAILURE);
//     }

//     printf("Connected to server. Start chatting!\n");
//     pthread_t recv_thread;
//     pthread_create(&recv_thread, NULL, receive_messages, NULL);
//     pthread_detach(recv_thread);

//     while (1)
//     {
//         memset(buffer, 0, SIZE);
//         fgets(buffer, SIZE, stdin);
//         send(sock, buffer, strlen(buffer), 0);
//     }

//     close(sock);
//     return 0;
// }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 5000
#define BUFFER_SIZE 1024

void *sender(void *arg)
{
    int fd = *(int *)arg;
    char buffer[BUFFER_SIZE];
    while (1)
    {
        if (fgets(buffer, sizeof(buffer), stdin) != NULL)
        {
            send(fd, buffer, strlen(buffer), 0);
        }
    }
    return NULL;
}

void *receiver(void *arg)
{
    int fd = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[n] = '\0';
        printf("%s", buffer);
    }
    printf("Server disconnected\n");
    exit(0);
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket failed");
        exit(1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect failed");
        exit(1);
    }

    printf("Connected to chatroom\n");

    pthread_t send_thread, recv_thread;
    pthread_create(&send_thread, NULL, sender, &fd);
    pthread_create(&recv_thread, NULL, receiver, &fd);

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);
    close(fd);
    return 0;
}