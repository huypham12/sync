#include "../daemon/include/state_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 5

void* thread_func(void* arg) {
    int thread_id = *(int*)arg;
    char filename[32];
    sprintf(filename, "file_%d.txt", thread_id);
    
    // Test set state thread-safe
    sm_set_state(filename, STATE_NETWORK);
    printf("[Thread %d] Set %s to STATE_NETWORK\n", thread_id, filename);
    
    // Simulate some work
    usleep(100000); // 100ms
    
    // Test get state thread-safe
    FileState state = sm_get_state(filename);
    if (state == STATE_NETWORK) {
         printf("[Thread %d] Get %s state OK (NETWORK)\n", thread_id, filename);
    } else {
         printf("[Thread %d] Error! Get state failed\n", thread_id);
    }
    
    // Test remove / reset state
    sm_set_state(filename, STATE_NONE);
    printf("[Thread %d] Removed/Reset %s state to NONE\n", thread_id, filename);
    
    free(arg);
    return NULL;
}

int main() {
    printf("=== BẮT ĐẦU TEST STATE MANAGER (MULTITHREADING) ===\n");
    
    sm_init();
    
    pthread_t threads[NUM_THREADS];
    
    // Khởi tạo nhiều luồng cùng ghi và đọc từ state manager đồng thời
    for (int i = 0; i < NUM_THREADS; i++) {
        int* arg = malloc(sizeof(int));
        *arg = i;
        pthread_create(&threads[i], NULL, thread_func, arg);
    }
    
    // Đợi các luồng hoàn tất
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    sm_destroy();
    
    printf("=== KẾT THÚC TEST ===\n");
    return 0;
}
