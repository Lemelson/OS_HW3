#include <stdio.h>
#include <pthread.h>

int sharedValue = 0;

void* threadFunction(void* arg) {
    // Изменение значения глобальной переменной
    sharedValue = 42;

    pthread_exit(NULL);
}

int main() {
    pthread_t thread;
    
    printf("sharedValue: %d\n", sharedValue);
    pthread_create(&thread, NULL, threadFunction, NULL);
    pthread_join(thread, NULL);

    printf("sharedValue: %d\n", sharedValue);

    return 0;
}

