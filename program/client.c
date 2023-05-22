#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define PORT 32454
#define SERVER_IP "127.0.0.1"

char* field;
char* opponentField;

void printField(int N) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            printf("%c", field[i * N + j]);
        }
        printf("\n");
    }
}

void printOpponentField(int N) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            printf("%c", opponentField[i * N + j]);
        }
        printf("\n");
    }
}

void fillOponentField(int N, int mortarsCount) {
    opponentField = (char*)malloc(N * N * sizeof(char));
    for (int i = 0; i < N * N; ++i) {
        opponentField[i] = '?';
    }
    
    // Заполняем поле.
    for (int i = 0; i < mortarsCount; ++i) {
        while (1) {
            int i = rand() % N;
            int j = rand() % N;
            
            // В этой ячейке уже есть орудие.
            if (opponentField[i * N + j] == '1') {
                continue;
            }
            
            opponentField[i * N + j] = '1';
            break;
        }
    }
}

void fillField(int N, int mortarsCount) {
    field = (char*)malloc(N * N * sizeof(char));
    for (int i = 0; i < N * N; ++i) {
        field[i] = '0';
    }
    
    // Заполняем поле.
    for (int i = 0; i < mortarsCount; ++i) {
        while (1) {
            int i = rand() % N;
            int j = rand() % N;
            
            // В этой ячейке уже есть орудие.
            if (field[i * N + j] == '1') {
                continue;
            }
            
            field[i * N + j] = '1';
            break;
        }
    }
}

int shoot(int N) {
    while (1) {
        int i = rand() % N;
        int j = rand() % N;
        
        // Уже стреляли и уничтожили.
        if (opponentField[i * N + j] == 'X') {
            continue;
        }
        
        // Уже стреляли и промахнулись.
        if (opponentField[i * N + j] == '.') {
            continue;
        }
        
        return (i * N + j);
    }
}

bool checkLose(int N) {
    for (int i = 0; i < N * N; ++i) {
        if (field[i] == '1') {
            return false;
        }
    }
    
    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("you need to enter 2 arguments: 1) size of the battlefield; 2) count of the mortars.");
        exit(0);
    }
    
    printf("Клиент успешно запущен!\n");
    
    int N = atoi(argv[1]); // Размер поля боя.
    
    int mortarsCount = atoi(argv[2]); // Количество минометов.
    srand(time(NULL)); // для рандомной генерации.
    
    fillField(N, mortarsCount);
    fillOponentField(N, mortarsCount);
    
    printf("Поле успешно сгенерировано!\n");
    printField(N);
    
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[32];
    
    // Создание сокета
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    
    // Преобразование IP-адреса из текстового вида в бинарный
    if (inet_pton(AF_INET, SERVER_IP, &(serverAddr.sin_addr)) <= 0) {
        perror("Ошибка преобразования IP-адреса");
        exit(EXIT_FAILURE);
    }

    // Подключение к серверу
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Ошибка подключения");
        exit(EXIT_FAILURE);
    }
    
    int bufferSize = 32;
    char* buf = (char*)malloc(bufferSize * sizeof(char));

    // Преобразование чисел в строковый формат и сохранение их в массив char
    int result = snprintf(buf, bufferSize, "%d %d", N, mortarsCount);
    if (result < 0 || result >= bufferSize) {
        close(clientSocket);
        printf("Ошибка при форматировании строкового переданных вами чисел (N и MortarsCount)!\n");
        exit(EXIT_FAILURE);
    }
    
    // Отправка сообщения на сервер
    if (send(clientSocket, buf, strlen(buf), 0) < 0) {
        close(clientSocket);
        perror("Ошибка при отправке данных о созданном поле серверу!");
        exit(EXIT_FAILURE);
    }
    
    int bytesReceived = recv(clientSocket, buffer, 32, 0);
    if (bytesReceived == -1) { // Ошибка при получении данных.
        close(clientSocket);
        printf("Ошибка при получении данных от сервера!\n");
        exit(EXIT_FAILURE);
    }
    
    if (bytesReceived == 0)
    {
        printf("Соединение с сервером было разорвано(сервер был закрыт)!!!");
        close(clientSocket);
        return 0;
    }
    
    bool isAttack = false;
    if (buffer[0] == '0') {
        isAttack = true;
    }
    
    if (isAttack) {
        printf("Игра началась, хожу первым.\n");
    } else {
        printf("Игра началась, хожу вторым.\n");
    }
    
    while (1) {
        if (isAttack) {
            int coordsToShoot = shoot(N);
            printf("Стреляю по координатам:  (%d, %d).\n", coordsToShoot / N, coordsToShoot - (coordsToShoot / N) * N);
            sprintf(buffer, "%d", coordsToShoot);
            if (send(clientSocket, buffer, 32, 0) == -1) {
                close(clientSocket);
                printf("Ошибка при отправке сообщения!");
                break;
            }
            
            memset(buffer, 0, 32);
            int bytesReceived = recv(clientSocket, buffer, 32, 0);
            if (bytesReceived == -1) { // Ошибка при получении данных.
                close(clientSocket);
                printf("Ошибка при получении данных от сервера!\n");
                exit(EXIT_FAILURE);
            }
            
            if (bytesReceived == 0) {
                printf("Соединение с сервером было разорвано(сервер был закрыт)!!!");
                close(clientSocket);
                break;
            }
            
            if (buffer[0] == 'D') {
                printf("Соперник покинул игру, ПОБЕДА!!!");
                close(clientSocket);
                break;
            }
            
            if (buffer[0] == 'F') {
                printf("Соперник был уничтожен выстрелом, ПОБЕДА!!!");
                close(clientSocket);
                break;
            }
            
            if (buffer[0] == '.') {
                printf("Промах!\n");
            } else if (buffer[0] == 'X') {
                printf("Уничтожил орудие!\n");
            }
            
            opponentField[coordsToShoot] = buffer[0]; // Сохраняем данные о выстреле.
            isAttack = false;
        } else {
            memset(buffer, 0, 32);
            int bytesReceived = recv(clientSocket, buffer, 32, 0);
            if (bytesReceived == -1) { // Ошибка при получении данных.
                close(clientSocket);
                printf("Ошибка при получении данных от сервера!\n");
                exit(EXIT_FAILURE);
            }
            
            if (bytesReceived == 0) {
                printf("Соединение с сервером было разорвано(сервер был закрыт)!!!");
                close(clientSocket);
                break;
            }
            
            if (buffer[0] == 'D') {
                printf("Соперник покинул игру, ПОБЕДА!!!");
                close(clientSocket);
                break;
            }
            
            int coords = atoi(buffer);
            memset(buffer, 0, 32);
            if (field[coords] == '0') {
                sprintf(buffer, "%c", '.');
            } else if (field[coords] == '1') {
                sprintf(buffer, "%c", 'X');   
            }
            
            field[coords] = '.';
            if (checkLose(N)) {
                memset(buffer, 0, 32);
                sprintf(buffer, "%c", 'F');
            }
            
            if (send(clientSocket, buffer, 32, 0) == -1) {
                close(clientSocket);
                printf("Ошибка при отправке сообщения!");
                break;
            }
            
            if (checkLose(N)) {
                printf("Противник побежил, уничтожив последнюю клетку: (%d, %d).", coords / N, coords - (coords / N) * N);
                break;
            } else {
                printf("Противник выстрелил по клетке: (%d, %d).", coords / N, coords - (coords / N) * N);
            }
            
            isAttack = true;
        }
    }
    
    close(clientSocket);
    return 0;
}

