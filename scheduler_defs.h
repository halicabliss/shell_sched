#ifndef SCHEDULER_DEFS_H
#define SCHEDULER_DEFS_H

#include <sys/types.h>

// chave para a fila de mensagens com makefile
#define MSG_QUEUE_KEY_PATH "Makefile"
#define MSG_QUEUE_KEY_ID 'S'

#define QUANTUM_MS 100

// tipos de mensagem (para o campo mtype)
#define MSG_TYPE_TO_SCHEDULER 1 // shell -> escalonador

// estrutura da mensagem
struct sched_msg {
    long mtype;           
    pid_t client_pid;     
    char command[512];    
};

#endif