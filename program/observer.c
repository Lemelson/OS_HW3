#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 32454
#define SERVER_IP "127.0.0.1"

void handleSIGPIPE(int signum) {
    // Чтобы прога не падала при отключении сервера.
}

int main()
{
    signal(SIGPIPE, handleSIGPIPE);
    
    int clientSocket;
    struct sockaddr_in serverAddr;
    
    // Создание сокета
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    // Преобразование IP-адреса из текстового в бинарный формат
    if (inet_pton(AF_INET, SERVER_IP, &(serverAddr.sin_addr)) <= 0)
    {
        perror("Ошибка преобразования адреса");
        exit(EXIT_FAILURE);
    }

    // Подключение к серверу
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Ошибка подключения");
        exit(EXIT_FAILURE);
    }
    
    if (send(clientSocket, "0 0", strlen("0 0"), 0) < 0) {
        close(clientSocket);
        perror("Ошибка при отправке данных о созданном поле серверу!");
        exit(EXIT_FAILURE);
    }
    
    bool first = true;
    while (1) {
        char buf[32];
        int receivedBytes = recv(clientSocket, buf, 32, 0);
        if (receivedBytes == -1) { // Ошибка при получении данных.
            close(clientSocket);
            printf("Ошибка при получении данных от сервера!\n");
            exit(EXIT_FAILURE);
        }
        
        if (receivedBytes == 0) {
            printf("Соединение с сервером было разорвано(сервер был закрыт)!!!");
            close(clientSocket);
            break;
        }
        
        if (first) {
            printf("Наблюдатель: успешно подключен\n");
            first = false;
        }
        
        printf("Текущее количество активных игр: %d", atoi(buf));
        fflush(stdout);
    }

    return 0;
}

