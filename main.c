// main.c (O Shell)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     
#include <sys/wait.h>   
#include <sys/ipc.h>    
#include <sys/msg.h>    

#include "scheduler_defs.h" //  cabeçalho compartilhado

#define MAX_CMD_LENGTH 50
#define MAX_LINE_LENGTH 128

// variáveis globais para o shell
int g_msg_queue_id = -1;
pid_t g_scheduler_pid = 0;

void create_user_scheduler(int num_of_queues) {
    if (g_scheduler_pid != 0) {
        fprintf(stderr, "Erro: O escalonador já está em execução (PID: %d).\n", g_scheduler_pid);
        return;
    }
    
    // 1. Criar a chave
    key_t key = ftok(MSG_QUEUE_KEY_PATH, MSG_QUEUE_KEY_ID);
    if (key == -1) {
        perror("ftok");
        return;
    }

    // 2. Criar a fila de mensagens (Note o IPC_CREAT)
    g_msg_queue_id = msgget(key, IPC_CREAT | 0666);
    if (g_msg_queue_id < 0) {
        perror("msgget (criar)");
        return;
    }
    printf("Fila de mensagens criada (ID: %d).\n", g_msg_queue_id);

    // 3. Iniciar o processo user_scheduler
    g_scheduler_pid = fork();
    if (g_scheduler_pid < 0) {
        perror("fork");
        return;
    }

    if (g_scheduler_pid == 0) { // --- Processo Filho (Scheduler) ---
        // Prepara os argumentos para o escalonador (ex: "./user_scheduler 2")
        char num_queues_str[10];
        snprintf(num_queues_str, sizeof(num_queues_str), "%d", num_of_queues);
        
        execlp("./user_scheduler", "user_scheduler", num_queues_str, NULL);
        
        // Se o execlp falhar
        perror("execlp user_scheduler");
        exit(1);
    }
    
    // --- Processo Pai (Shell) ---
    printf("Escalonador iniciado com PID %d, Filas: %d.\n", g_scheduler_pid, num_of_queues);
}

void send_command_to_scheduler(const char* command) {
    if (g_scheduler_pid == 0) {
        fprintf(stderr, "Erro: O escalonador não está em execução. Use 'create_user_scheduler'.\n");
        return;
    }

    struct sched_msg msg;
    msg.mtype = MSG_TYPE_TO_SCHEDULER; // Envia para o tipo 1 (o escalonador escuta)
    msg.client_pid = getpid();         // Diz a ele quem somos (nosso PID)
    strncpy(msg.command, command, sizeof(msg.command) - 1);
    msg.command[sizeof(msg.command) - 1] = '\0';

    if (msgsnd(g_msg_queue_id, &msg, sizeof(msg.command), 0) < 0) {
        perror("msgsnd");
    }
}

void execute_process(int priority) {
    char command_buffer[64];
    snprintf(command_buffer, sizeof(command_buffer), "EXEC %d", priority);
    send_command_to_scheduler(command_buffer);
}

void list_scheduler() {
    send_command_to_scheduler("LIST");
    
    // Agora, espera por uma resposta
    struct sched_msg reply;
    
    // Espera por uma mensagem do tipo "meu_pid"
    if (msgrcv(g_msg_queue_id, &reply, sizeof(reply.command), getpid(), 0) < 0) {
        perror("msgrcv (resposta do list)");
    } else {
        // PRÓXIMO PASSO: Quando o handle_list() no escalonador estiver pronto,
        // esta linha irá imprimir a lista completa e formatada.
        printf("--- Resposta do Escalador ---\n%s", reply.command);
    }
}

void exit_scheduler() {
    if (g_scheduler_pid == 0) return; // Nada a fazer
    
    send_command_to_scheduler("EXIT");
    
    // Espera o processo do escalonador terminar
    waitpid(g_scheduler_pid, NULL, 0); 
    printf("Escalonador (PID: %d) terminou.\n", g_scheduler_pid);
    g_scheduler_pid = 0;
    
    // A fila de mensagens é removida pelo *escalonador* no handle_exit() dele
}


int main() {
    char cmd_buffer[MAX_CMD_LENGTH];
    char arg_buffer[MAX_CMD_LENGTH];
    char line[MAX_LINE_LENGTH];
    
    while (1) {
        printf("> shell_sched: ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            // Se o usuário apertar Ctrl+D (EOF)
            if (g_scheduler_pid != 0) {
                exit_scheduler();
            }
            break;
        }
        
        line[strcspn(line, "\n")] = '\0';
        int argc = sscanf(line, "%49s %49s", cmd_buffer, arg_buffer);
        if (argc == 0) continue;

        // execute command
        if (strcmp(cmd_buffer, "create_user_scheduler") == 0) {
            if (argc == 2) create_user_scheduler(atoi(arg_buffer));
            else fprintf(stderr, "Uso: %s <numero_de_filas>\n", cmd_buffer);

        } else if (strcmp(cmd_buffer, "execute_process") == 0) {
            if (argc == 2) execute_process(atoi(arg_buffer));
            else fprintf(stderr, "Uso: %s <prioridade_do_processo>\n", cmd_buffer);
        
        } else if (strcmp(cmd_buffer, "list_scheduler") == 0) {
            list_scheduler();

        } else if (strcmp(cmd_buffer, "exit_scheduler") == 0) {
            exit_scheduler();
            break; // Sai do loop do shell

        } else {
            fprintf(stderr, "Erro: comando desconhecido\n");
        }
    }
    return 0;
}