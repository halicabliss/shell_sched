#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>

#include "scheduler_defs.h" // definicoes e chave para msg

// estruturas

typedef struct process_node {
    pid_t pid;
    int priority;
    time_t start_time;
    int context_switches;
    struct process_node* next;
} process_node_t;

typedef struct finished_process {
    pid_t pid;
    double turnaround;
    struct finished_process* next;
} finished_process_t;

finished_process_t* g_history_head = NULL;
finished_process_t* g_history_tail = NULL;

#define MAX_QUEUES 3
process_node_t* g_queue_heads[MAX_QUEUES] = { NULL, NULL, NULL };
process_node_t* g_queue_tails[MAX_QUEUES] = { NULL, NULL, NULL };

volatile pid_t g_current_process_pid = 0;
process_node_t* g_current_process_node = NULL;
int g_msg_queue_id = -1;
int g_num_queues = 0;



void enqueue_at_end(process_node_t* node) {
    int queue_idx = node->priority - 1;
    node->next = NULL;
    if (g_queue_tails[queue_idx] == NULL) {
        g_queue_heads[queue_idx] = node;
        g_queue_tails[queue_idx] = node;
    } else {
        g_queue_tails[queue_idx]->next = node;
        g_queue_tails[queue_idx] = node;
    }
}
void enqueue_at_front(process_node_t* node) {
    int queue_idx = node->priority - 1; 
    node->next = g_queue_heads[queue_idx];
    g_queue_heads[queue_idx] = node;
    if (g_queue_tails[queue_idx] == NULL) {
        g_queue_tails[queue_idx] = node;
    }
}
process_node_t* dequeue_next_process() {
    for (int i = 0; i < g_num_queues; i++) {
        if (g_queue_heads[i] != NULL) {
            process_node_t* node = g_queue_heads[i];
            g_queue_heads[i] = node->next;
            if (g_queue_heads[i] == NULL) {
                g_queue_tails[i] = NULL;
            }
            node->next = NULL;
            return node;
        }
    }
    return NULL; 
}

process_node_t* remove_process_from_queue(pid_t pid) {
    for (int i = 0; i < g_num_queues; i++) {
        process_node_t* current = g_queue_heads[i];
        process_node_t* prev = NULL;
        
        while (current != NULL) {
            if (current->pid == pid) {
                // Remove da fila
                if (prev == NULL) {
                    g_queue_heads[i] = current->next;
                } else {
                    prev->next = current->next;
                }
                if (g_queue_tails[i] == current) {
                    g_queue_tails[i] = prev;
                }
                
                return current;
            }
            prev = current;
            current = current->next;
        }
    }
    return NULL;
}

void schedule() {
    if (g_current_process_pid != 0 && g_current_process_node != NULL) {
        kill(g_current_process_pid, SIGSTOP);
        enqueue_at_end(g_current_process_node);
        g_current_process_pid = 0;
        g_current_process_node = NULL;
    }
    process_node_t* next_process = dequeue_next_process();
    if (next_process != NULL) {
        next_process->context_switches++;
        g_current_process_pid = next_process->pid;
        g_current_process_node = next_process;
        kill(g_current_process_pid, SIGCONT); 
    } else {
        printf("[Scheduler] Nenhuma processo na fila. CPU ociosa.\n");
        g_current_process_pid = 0;
        g_current_process_node = NULL;
    }
}

void handle_exec(char* command_args, pid_t client_pid) {
    char command[64];
    int priority;
    sscanf(command_args, "%63s %d", command, &priority);

    if (priority < 1 || priority > g_num_queues) {
        fprintf(stderr, "[Scheduler] Erro: Prioridade %d inválida.\n", priority);
        return;
    }
    

    printf("[Scheduler] Recebido EXEC, prioridade: %d\n", priority);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) { 
        raise(SIGSTOP); 
        execlp(command, command, NULL);
        perror("execlp");
        exit(1);
    }

    printf("[Scheduler] Novo processo criado: %d\n", pid);

    process_node_t* new_node = (process_node_t*)malloc(sizeof(process_node_t));
    if (new_node == NULL) { // Checagem de alocação
        fprintf(stderr, "Erro de alocação de memória!\n");
        kill(pid, SIGKILL); // Mata o filho
        return;
    }
    new_node->pid = pid;
    new_node->priority = priority;
    new_node->next = NULL;
    new_node->context_switches = 0;
    new_node->start_time = time(NULL);
    

    // add ao final da fila
    enqueue_at_end(new_node);
    
    int current_priority = 999; 
    if (g_current_process_pid != 0 && g_current_process_node != NULL) {
        current_priority = g_current_process_node->priority;
    }

    if (priority < current_priority) {
        printf("[Scheduler] PREEMPÇÃO: Novo processo %d (P%d) é mais prioritário.\n",
               new_node->pid, new_node->priority);
        
        
        if (g_current_process_pid != 0) {
            // pausa o processo atual
            kill(g_current_process_pid, SIGSTOP);
            
            // coloca ele no inicio da fila (interrupção)
            enqueue_at_front(g_current_process_node);
            
            g_current_process_pid = 0;
            g_current_process_node = NULL;
        }
        
        schedule();
    }
}

void handle_list(pid_t client_pid) {
    printf("\n");
    printf("[Scheduler] Recebido LIST\n");
    
    struct sched_msg reply;
    reply.mtype = client_pid;
    reply.client_pid = getpid();
    
    char response_buffer[2048] = ""; 
    char temp_buffer[256];

    if (g_current_process_pid != 0) {
        time_t now = time(NULL);
        double elapsed = difftime(now, g_current_process_node->start_time);
        
        snprintf(temp_buffer, sizeof(temp_buffer),
            ">>> [ CPU ] EM EXECUÇÃO:\n"
            "    PID: %d | Prioridade: %d | Tempo: %.1fs | Trocas: %d\n",
            g_current_process_pid, g_current_process_node->priority, elapsed, g_current_process_node->context_switches);
        strcat(response_buffer, temp_buffer);
    } else {
        strcat(response_buffer, "\n>>> [ CPU ] EM EXECUÇÃO:\n    [ OCIOSA ]\n");
    }

    
    for (int i = 0; i < g_num_queues; i++) {
        snprintf(temp_buffer, sizeof(temp_buffer), "\n>>> FILA DE PRIORIDADE %d:\n    ", i + 1);
        strcat(response_buffer, temp_buffer);
        
        process_node_t* current = g_queue_heads[i];
        if (current == NULL) {
            strcat(response_buffer, "[ VAZIA ]\n");
        } else {
            while(current != NULL) {
                snprintf(temp_buffer, sizeof(temp_buffer), "[%d (T:%d)] -> ", current->pid,current->context_switches);
                strcat(response_buffer, temp_buffer);
                current = current->next;
            }
            strcat(response_buffer, "NULL\n");
        }
    }
    
    strcat(response_buffer, "\n");

    
    snprintf(reply.command, sizeof(reply.command), "%s", response_buffer);
    if (msgsnd(g_msg_queue_id, &reply, sizeof(reply.command), 0) < 0) {
        perror("msgsnd reply");
    }
}

void handle_exit() {
    printf("[Scheduler] Recebido EXIT. Encerrando...\n");
    time_t end_time = time(NULL);

    if (g_current_process_pid != 0) {
        kill(g_current_process_pid, SIGKILL);
        waitpid(g_current_process_pid, NULL, 0); // Colhe o filho
        double turnaround = difftime(end_time, g_current_process_node->start_time);
        printf("Processo %d (rodando) morto. Turnaround: %.2f seg\n", g_current_process_pid, turnaround);
        free(g_current_process_node);
    }

    
    for (int i = 0; i < g_num_queues; i++) {
        process_node_t* current = g_queue_heads[i];
        while (current != NULL) {
            kill(current->pid, SIGKILL);
            waitpid(current->pid, NULL, 0); 
            double turnaround = difftime(end_time, current->start_time);
            printf("Processo %d (fila P%d) morto. Turnaround: %.2f seg\n", 
                   current->pid, i + 1, turnaround);

            process_node_t* temp = current;
            current = current->next;
            free(temp);
        }
    }

    // remove a fila de mensagens
    if (msgctl(g_msg_queue_id, IPC_RMID, NULL) < 0) {
        perror("msgctl IPC_RMID");
    }

    printf("[Scheduler] Encerrado.\n");
    exit(0);
}

void sigalrm_handler(int sig) {
    if (g_current_process_pid == 0) {
        return; 
    }
    schedule();
}

void sigchld_handler(int sig) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[Scheduler] Filho %d terminou.\n", pid);

        if (pid == g_current_process_pid) {
            time_t end_time = time(NULL);
            double turnaround = difftime(end_time, g_current_process_node->start_time);
            printf("Processo %d concluído. Turnaround: %.2f s. Trocas de Contexto: %d\n", pid, turnaround, g_current_process_node->context_switches);

            free(g_current_process_node);
            g_current_process_pid = 0;
            g_current_process_node = NULL;
            
            schedule(); 
        } else {
        process_node_t* node = remove_process_from_queue(pid);
        
        if (node != NULL) {
            time_t end_time = time(NULL);
            double turnaround = difftime(end_time, node->start_time);
            
            printf("Processo %d terminou INESPERADAMENTE na fila.\n", pid);
            printf("Turnaround parcial: %.2f segundos\n", turnaround);
            
            free(node);
        } else {
            printf("Processo %d terminou mas não foi encontrado!\n", pid);
            }
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <num_filas>\n", argv[0]);
        return 1;
    }
    g_num_queues = atoi(argv[1]);
    if (g_num_queues < 1 || g_num_queues > MAX_QUEUES) {
        fprintf(stderr, "Erro: Número de filas deve ser entre 1 e %d\n", MAX_QUEUES);
        return 1;
    }

    printf("[Scheduler] Iniciado. Filas: %d. PID: %d\n", g_num_queues, getpid());
    
    key_t key = ftok(MSG_QUEUE_KEY_PATH, MSG_QUEUE_KEY_ID);
    if (key == -1) { perror("ftok"); return 1; }
    
    g_msg_queue_id = msgget(key, 0666); // pega a fila criada pelo shell_sched
    if (g_msg_queue_id < 0) {
        perror("msgget (o shell_sched deve criar primeiro)");
        return 1;
    }

    signal(SIGALRM, sigalrm_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, handle_exit);     

    // quantum
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = QUANTUM_MS * 1000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = QUANTUM_MS * 1000;
    
    if (setitimer(ITIMER_REAL, &timer, NULL) < 0) {
        perror("setitimer"); return 1;
    }

    
    struct sched_msg rx_msg;
    while (1) {
        ssize_t result = msgrcv(g_msg_queue_id, &rx_msg, sizeof(rx_msg.command), MSG_TYPE_TO_SCHEDULER, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue; }

            perror("msgrcv");
            break;
        }

        if (strncmp(rx_msg.command, "EXEC", 4) == 0) {
            handle_exec(rx_msg.command + 5, rx_msg.client_pid);
        } else if (strcmp(rx_msg.command, "LIST") == 0) {
            handle_list(rx_msg.client_pid);
        } else if (strcmp(rx_msg.command, "EXIT") == 0) {
            handle_exit();
            break;
        }
    }
    return 0;
}