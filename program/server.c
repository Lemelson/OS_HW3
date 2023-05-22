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

#define PORT 32454
#define MAX_CLIENTS 100
#define MAX_VISITORS 100

struct ClientInfo
{
    int n; // Размер поля.
    int mortarsCount; // Количество пушек.
    int socket;
};

int totalGamesCount = 0;
int* visitors; // Массив наблюдателей, храню сокеты.
struct ClientInfo clients[MAX_CLIENTS]; // Массив клиентов.
pthread_t threads[MAX_CLIENTS / 2];
sem_t semaphore;

bool isConnectionAlive(int socket) {
    // Создание набора файловых дескрипторов для проверки готовности чтения
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);

    // Установка таймаута на ноль, чтобы функция select сразу вернула результат
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Выполнение проверки готовности чтения
    int result = select(socket + 1, &readfds, NULL, NULL, &timeout);
    if (result == -1) {
        perror("Ошибка при вызове select");
        return false;  // Обработка ошибки...
    }

    return !FD_ISSET(socket, &readfds);
}

void updateVisitorsInfo() {
    // Убираем вышедших клиентов и закрываем соединения.
    for (int i = 0; i < MAX_VISITORS; ++i) {
        if (visitors[i] == -1) {
            continue;
        }
        
        if (!isConnectionAlive(i)) {
            close(visitors[i]);
            visitors[i] = -1;
        }
    }
    
    // Отправляем данные наблюдателям.
    char str[32];
    sprintf(str, "%d", totalGamesCount);
    for (int i = 0; i < MAX_VISITORS; ++i) {
        if (visitors[i] == -1) {
            continue;
        }
        
        send(visitors[i], str, 32, 0);
    }
}


bool isEmpty(int id) {
    return (clients[id].socket == -1);
}

void *clientHandler(void *arg) {
    int clientId = *((int*)arg);
    srand(time(NULL)); // для генерации рандомного числа.
    
    int attackInd = clientId - (rand() % 2); // Случайно выбираем, кто ходит первым.
    int defendInd = attackInd + 1;
    if (attackInd % 2 == 1) {
        defendInd = attackInd - 1;
    }
    
    send(clients[attackInd].socket, "0", strlen("0"), 0); // Первый ход.
    send(clients[defendInd].socket, "1", strlen("0"), 0); // Второй ход.
    
    bool isGameFinished = false;
    char buf[32]; // Храним ответ страны.
    while (1) {
        // Получение данных от клиента
        memset(buf, 0, 32);
        int bytesReceived = recv(clients[attackInd].socket, buf, 32, 0);
        if (bytesReceived == -1) { // Ошибка при получении данных.
            printf("В игре со странами-клиентами: (%d, %d) произошла ошибка при передаче данных!\n", defendInd, attackInd);
            break;
        }
        
        if (bytesReceived == 0) { // Ошибка при получении данных.
            memset(buf, 0, 32);
            printf("Клиент с (id : %d) вышел из комнаты: %d!\n", attackInd, attackInd / 2);
            sprintf(buf, "%c", 'D');
        }
        
        if (send(clients[defendInd].socket, buf, 32, 0) == -1) { // Ошибка при получении данных.
            memset(buf, 0, 32);
            sprintf(buf, "%c", 'D');
            send(clients[attackInd].socket, buf, 32, 0);
            printf("В игре со странами-клиентами: (%d, %d) произошла ошибка при передаче данных!\n", defendInd, attackInd);
            break;
        }
        
        if (buf[0] == 'D') { // Игра завершилась, клиент вышел из игры.
            break;
        }
        
        memset(buf, 0, 32);
        bytesReceived = recv(clients[defendInd].socket, buf, 32, 0);
        if (bytesReceived == -1) { // Ошибка при получении данных.
            printf("В игре со странами-клиентами: (%d, %d) произошла ошибка при передаче данных!\n", defendInd, attackInd);
            break;
        }
        
        if (bytesReceived == 0) {
            memset(buf, 0, 32);
            printf("Клиент с (id : %d) вышел из комнаты: %d!\n", defendInd, defendInd / 2);
            sprintf(buf, "%c", 'D'); // Добавляем сигнал о выходе соперника (D-DISCONNECT).
        }
        
        if (send(clients[attackInd].socket, buf, 32, 0) == -1) { // Ошибка при получении данных.
            printf("В игре со странами-клиентами: (%d, %d) произошла ошибка при передаче данных!\n", defendInd, attackInd);
            recv(clients[attackInd].socket, buf, 32, 0);
            memset(buf, 0, 32);
            sprintf(buf, "%c", 'D');
            send(clients[attackInd].socket, buf, 32, 0);
            break;
        }
        
        if (buf[0] == 'F' || buf[0] == 'D') {
            break;
        }
        
        // Передаем ход другой стране-клиенту.
        int swapBuf = attackInd;
        attackInd = defendInd;
        defendInd = swapBuf;
        
        sleep(1); // Задеоржка перед следующим выстрелом.
    }
    
    sem_wait(&semaphore);
    
    printf("В комнате: %d завершилась игра!\n", defendInd / 2);
    fflush(stdout);
    --totalGamesCount;
    updateVisitorsInfo();
    
    close(clients[attackInd].socket);
    close(clients[defendInd].socket);
    
    clients[attackInd].n = -1;
    clients[attackInd].mortarsCount = -1;
    clients[attackInd].socket = -1;
    
    clients[defendInd].n = -1;
    clients[defendInd].mortarsCount = -1;
    clients[defendInd].socket = -1;
    
    sem_post(&semaphore);
    
    pthread_exit(NULL);
}

// Находит свободную игровую комнату.
int findEmptyRoom(int n, int k) {
    // Если клиент вышел, то удаляем его из лобби.
    for (int i = 0; i < MAX_CLIENTS - 1; i += 2) {
        if (!isEmpty(i) && isEmpty(i + 1)) {
            if (!isConnectionAlive(clients[i].socket)) {
                printf("Прошлый клиент вышел из комнаты: %d, так что комната будет пересоздана.\n", i / 2);
                close(clients[i].socket); // Закрываем соединение.
                clients[i].n = -1;
                clients[i].mortarsCount = -1;
                clients[i].socket = -1;
            }
        }
    }
    
    for (int i = 0; i < MAX_CLIENTS - 1; i += 2) {
        if (clients[i].n == n && clients[i].mortarsCount == k && isEmpty(i + 1)) {
            return (i + 1); // Заходим к ожидающей стране-клиенту.
        }
    }
    
    for (int i = 0; i < MAX_CLIENTS; i += 2) {
        if (isEmpty(i)) {
            return i; // Заходим в пустую комнату.
        }
    }
    
    return -1; // Комната не была найдена.
}

void initClients() {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].n = -1;
        clients[i].mortarsCount = -1;
        clients[i].socket = -1;
    }
}

int findPosForVisitor() {
    for (int i = 0; i < MAX_VISITORS; ++i) {
        if (visitors[i] == -1) {
            return i;
        }
    }
    
    return -1;
}

void handleSIGPIPE(int signum) {
    // Обработка сигнала SIGPIPE
    printf("Клиент отключился, обработано...\n");
}


void handleSIGINT(int signal) {
    // Закрываем все клиентские сокеты.
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (isEmpty(i)) {
            continue;
        }
        
        close(clients[i].socket);
    }
    
    for (int i = 0 ; i < MAX_VISITORS; ++i) {
        if (visitors[i] == -1) {
            continue;
        }
        
        close(visitors[i]);
    }
    
    // Завершаем все потоки.
    for (int i = 0; i < MAX_CLIENTS / 2; ++i) {
        pthread_cancel(threads[i]);
    }
    
    // Уничтожаем семафор. 
    sem_destroy(&semaphore);
    
    exit(0); // Завершаем программу.
}

int main() {
    
    // Установка обработчика сигнала SIGINT на закрытие сервера
    signal(SIGINT, handleSIGINT);
    signal(SIGPIPE, handleSIGPIPE);
    
    sem_init(&semaphore, 0, 1);
    visitors = (int*)malloc(MAX_VISITORS * sizeof(int)); // Создание массива посетителей.
    for (int i = 0; i < MAX_VISITORS; ++i) {
        visitors[i] = -1;
    }
    
    initClients();
    
    int serverSocket;
    struct sockaddr_in serverAddr;
    
    // Создание сокета
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    
    // Привязка сокета к указанному порту
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Ошибка привязки сокета");
        exit(EXIT_FAILURE);
    }
    
    
    // Ожидание подключения клиентов
    if (listen(serverSocket, 5) < 0)
    {
        perror("Ошибка ожидания подключений");
        exit(EXIT_FAILURE);
    }


    printf("Сервер успешно запущен и ожидает подключений:\n");
    
    int clientSocket;
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    
    
    // Сервер работает бесконечно, для закрытия сервера необходимо просто закрыть его.
    while (1) {
        // Принятие нового подключения от клиента
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen)) < 0)
        {
            perror("Ошибка при принятии подключения\n");
            continue;
        }

        char buf[32];
        int bytesReceived = recv(clientSocket, buf, 32, 0);
        if (bytesReceived <= 0) {
            printf("Ошибка при подключении очередной страны-клиента!");
        }
        
        char *token;
        token = strtok(buf, " ");
        int n = atoi(token);
    
        token = strtok(NULL, " ");
        int k = atoi(token);
        
        // Передача таких значений означает, что подключаемый клиент - наблюдатель.
        if (n == 0 && k == 0) {
            int newPos = findPosForVisitor();
            if (newPos == -1) {
                printf("Сервер отклонил подключение подключение(превышено максимальное число наблюдателей на сервере)!\n");
                close(clientSocket);
                continue;
            }
            
            printf("Наблюдатель успешно подключен!");
            
            char str[32];
            sprintf(str, "%d", totalGamesCount);
            
            send(clientSocket, str, 32, 0); // Отправляем сообщение о текущем количестве игровых сессий.
            visitors[newPos] = clientSocket;
            continue;
        }
        
        sem_wait(&semaphore);
        int serverAns = findEmptyRoom(n, k);
        sem_post(&semaphore);
        
        if (serverAns == -1) {
            printf("Сервер отклонил подключение подключение(нет свободных комнат)!\n");
            close(clientSocket);
            continue;
        }
        
        clients[serverAns].n = n;
        clients[serverAns].mortarsCount = k;
        clients[serverAns].socket = clientSocket;
        if (serverAns % 2 == 0) {
            printf("Сервер успешно принял новое подключение, создана комната: %d!\n", serverAns / 2);
            printf("В комнату добавлен ожидающий клиент - (id : %d, N : %d, K: %d)!\n", serverAns, n, k);
            continue;
        }
        
        printf("Сервер успешно принял новое подключение к комнате: %d!\n", serverAns / 2);
        printf("В комнату добавлен клиент - (id : %d, N : %d, K: %d)!\n", serverAns, n, k);
        printf("В комнате: %d началась игра!\n", serverAns / 2);
        ++totalGamesCount;
        updateVisitorsInfo();
        
        if (pthread_create(&threads[serverAns / 2], NULL, clientHandler, (void *)&serverAns) != 0)
        {
            perror("Ошибка при создании потока");
            exit(EXIT_FAILURE);
        }
    }
    
    sem_destroy(&semaphore);
    return 0;
}

