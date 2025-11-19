/*
 * -----------------------------------------------------------------------------
 * PROJETO: Shell para escalonamento preemptivo-interrupt com prioridades
 * -----------------------------------------------------------------------------
 *
 * AMBIENTE DE DESENVOLVIMENTO:
 * Sistema Operacional: Linux 5.15.167.4-microsoft-standard-WSL2
 * Versão do Compilador: gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
 *
 * AUTORES:
 * - Emanuel de Oliveira Barbosa - 211010403
 * - Luiz Felipe Ducat - 231035400
 * - Pedro Arthur de Moura Neves - 211055352
 * -----------------------------------------------------------------------------
 */

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
    
    // criar a chave
    key_t key = ftok(MSG_QUEUE_KEY_PATH, MSG_QUEUE_KEY_ID);
    if (key == -1) {
        perror("ftok");
        return;
    }

    // criar a fila de mensagens
    g_msg_queue_id = msgget(key, IPC_CREAT | 0666);
    if (g_msg_queue_id < 0) {
        perror("msgget (criar)");
        return;
    }
    printf("Fila de mensagens criada (ID: %d).\n", g_msg_queue_id);

    // iniciar o processo user_scheduler
    g_scheduler_pid = fork();
    if (g_scheduler_pid < 0) {
        perror("fork");
        return;
    }

    if (g_scheduler_pid == 0) { 
        // prepara os argumentos para o escalonador
        char num_queues_str[10];
        snprintf(num_queues_str, sizeof(num_queues_str), "%d", num_of_queues);
        
        execlp("./user_scheduler", "user_scheduler", num_queues_str, NULL);
        
        perror("execlp user_scheduler");
        exit(1);
    }
    
    printf("Escalonador iniciado com PID %d, Filas: %d.\n", g_scheduler_pid, num_of_queues);
}

void send_command_to_scheduler(const char* command) {
    if (g_scheduler_pid == 0) {
        fprintf(stderr, "Erro: O escalonador não está em execução. Use 'create_user_scheduler'.\n");
        return;
    }

    struct sched_msg msg;
    msg.mtype = MSG_TYPE_TO_SCHEDULER; 
    msg.client_pid = getpid();         
    strncpy(msg.command, command, sizeof(msg.command) - 1);
    msg.command[sizeof(msg.command) - 1] = '\0';

    if (msgsnd(g_msg_queue_id, &msg, sizeof(msg.command), 0) < 0) {
        perror("msgsnd");
    }
}

void execute_process(const char* command, int priority) {
    if (priority < 1 || priority > 3) {
        fprintf(stderr, "Erro: Prioridade deve ser entre 1 e %d\n", 3);
        return;
    }
    char command_buffer[128];
    snprintf(command_buffer, sizeof(command_buffer), "EXEC %s %d", command, priority);
    send_command_to_scheduler(command_buffer);
}

void list_scheduler() {
    send_command_to_scheduler("LIST");
    
    struct sched_msg reply;
    
    
    if (msgrcv(g_msg_queue_id, &reply, sizeof(reply.command), getpid(), 0) < 0) {
        perror("msgrcv (resposta do list)");
    } else {
        printf("%s", reply.command);
    }
}

void exit_scheduler() {
    if (g_scheduler_pid == 0) return;
    
    send_command_to_scheduler("EXIT");
    
    waitpid(g_scheduler_pid, NULL, 0); 
    printf("Escalonador (PID: %d) terminou.\n", g_scheduler_pid);
    g_scheduler_pid = 0;
    
    
}


int main() {
    char cmd_buffer[MAX_CMD_LENGTH];
    char arg1_buffer[MAX_CMD_LENGTH];
    char arg2_buffer[MAX_CMD_LENGTH];
    char line[MAX_LINE_LENGTH];
    
    while (1) {
        printf("\n");
        printf("> shell_sched: ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            // (EOF)
            if (g_scheduler_pid != 0) {
                exit_scheduler();
            }
            break;
        }
        
        line[strcspn(line, "\n")] = '\0';
        int argc = sscanf(line, "%49s %49s %49s", cmd_buffer, arg1_buffer, arg2_buffer);
        if (argc == 0) continue;

        // execute command
        if (strcmp(cmd_buffer, "create_user_scheduler") == 0) {
            if (argc == 2) create_user_scheduler(atoi(arg1_buffer));
            else fprintf(stderr, "Uso: %s <numero_de_filas>\n", cmd_buffer);

        } else if (strcmp(cmd_buffer, "execute_process") == 0) {
            if (argc == 3) {
                execute_process(arg1_buffer, atoi(arg2_buffer)); // arg1: comando, arg2: prioridade
            }
            else fprintf(stderr, "Uso: %s <comando> <prioridade>\n", cmd_buffer);
        
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