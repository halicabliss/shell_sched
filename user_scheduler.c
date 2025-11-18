#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "scheduler_defs.h" // definicoes e chave para msg

// estruturas

typedef struct process_node {
    pid_t pid;
    int priority;
    // PRÓXIMO PASSO: Adicionar campo para turnaround
    // time_t start_time;
    struct process_node* next; // proximo no na fila
} process_node_t;

// definir as filas de prioridade assumindo no maximo 3 filas
#define MAX_QUEUES 3
process_node_t* g_queue_heads[MAX_QUEUES] = { NULL, NULL, NULL };
process_node_t* g_queue_tails[MAX_QUEUES] = { NULL, NULL, NULL };

volatile pid_t g_current_process_pid = 0;
process_node_t* g_current_process_node = NULL;
int g_msg_queue_id = -1;
int g_num_queues = 0;



// funcoes
// (enqueue_at_end, enqueue_at_front, dequeue_next_process, remove_process_from_queue)
// ... essas funções estão PRONTAS. Não precisam de alteração. ...

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
int remove_process_from_queue(pid_t pid) {
    for (int i = 0; i < g_num_queues; i++) {
        process_node_t* current = g_queue_heads[i];
        process_node_t* prev = NULL;
        while (current != NULL) {
            if (current->pid == pid) {
                if (prev == NULL) {
                    g_queue_heads[i] = current->next;
                } else {
                    prev->next = current->next;
                }
                if (g_queue_tails[i] == current) {
                    g_queue_tails[i] = prev;
                }
                free(current);
                return 1;
            }
            prev = current;
            current = current->next;
        }
    }
    return 0;
}

// ...
// A função schedule() está PRONTA. Não precisa de alteração.
// ...
void schedule() {
    if (g_current_process_pid != 0 && g_current_process_node != NULL) {
        printf("[Scheduler] Quantum estourou. Pausando %d (Prio %d)\n",
               g_current_process_pid, g_current_process_node->priority);
        kill(g_current_process_pid, SIGSTOP);
        enqueue_at_front(g_current_process_node);
        g_current_process_pid = 0;
        g_current_process_node = NULL;
    }
    process_node_t* next_process = dequeue_next_process();
    if (next_process != NULL) {
        printf("[Scheduler] Iniciando/Continuando processo %d (Prio %d)\n",
               next_process->pid, next_process->priority);
        g_current_process_pid = next_process->pid;
        g_current_process_node = next_process;
        kill(g_current_process_pid, SIGCONT); 
    } else {
        printf("[Scheduler] Nenhuma processo na fila. CPU ociosa.\n");
        g_current_process_pid = 0;
        g_current_process_node = NULL;
    }
}

// ...
// A função handle_exec() está quase PRONTA.
// ...
void handle_exec(char* command_args, pid_t client_pid) {
    int priority;
    sscanf(command_args, "%d", &priority);

    if (priority < 1 || priority > g_num_queues) {
        fprintf(stderr, "[Scheduler] Erro: Prioridade %d inválida.\n", priority);
        // TODO: Enviar resposta de erro ao shell
        return;
    }
    
    printf("[Scheduler] Recebido EXEC, prioridade: %d\n", priority);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) { // --- Processo Filho (Worker) ---
        raise(SIGSTOP); 
        execlp("./cpu_bound_loop", "cpu_bound_loop", NULL);
        perror("execlp cpu_bound_loop");
        exit(1);
    }

    // --- Processo Pai (Scheduler) ---
    printf("[Scheduler] Novo processo criado: %d\n", pid);

    // 1. Criar o nó
    process_node_t* new_node = (process_node_t*)malloc(sizeof(process_node_t));
    if (new_node == NULL) { // Checagem de alocação
        fprintf(stderr, "Erro de alocação de memória!\n");
        kill(pid, SIGKILL); // Mata o filho órfão
        return;
    }
    new_node->pid = pid;
    new_node->priority = priority;
    new_node->next = NULL;
    
    // PRÓXIMO PASSO: Salvar a hora de início para o turnaround
    // new_node->start_time = time(NULL);

    // 2. Adicionar o processo no FINAL da fila
    enqueue_at_end(new_node);
    
    // 3. Lógica de "Interrupção" (Preempção) - JÁ ESTÁ PRONTA
    int current_priority = 999; 
    if (g_current_process_pid != 0 && g_current_process_node != NULL) {
        current_priority = g_current_process_node->priority;
    }
    if (priority < current_priority) {
        printf("[Scheduler] PREEMPÇÃO: Novo processo %d (P%d) é mais prioritário.\n",
               new_node->pid, new_node->priority);
        schedule();
    }
}

void handle_list(pid_t client_pid) {
    printf("[Scheduler] Recebido LIST\n");
    
    // PRÓXIMOS PASSOS:
    // 1. Percorrer as filas de prioridade e montar a lista de PIDs.
    // 2. Montar uma string (char buffer[1024]) com as informações formatadas.
    //    (Dica: use sprintf() ou strcat() para construir a string)
    // 3. Enviar a string de volta para o shell (client_pid).
    
    // Exemplo de envio de resposta (substituir pela string montada):
    struct sched_msg reply;
    reply.mtype = client_pid; // Responde para o PID do cliente
    reply.client_pid = getpid(); // O scheduler é o cliente agora
    
    // Montagem da string de resposta (EXEMPLO SIMPLES)
    char response_buffer[512] = "";
    char temp_buffer[100];

    // Info do processo rodando
    if (g_current_process_pid != 0) {
        snprintf(temp_buffer, sizeof(temp_buffer), "Processo Rodando: %d (P%d)\n", 
                 g_current_process_pid, g_current_process_node->priority);
        strcat(response_buffer, temp_buffer);
    } else {
        strcat(response_buffer, "Processo Rodando: NENHUM (CPU Ociosa)\n");
    }

    // PRÓXIMO PASSO: Iterar pelas filas aqui
    for (int i = 0; i < g_num_queues; i++) {
        snprintf(temp_buffer, sizeof(temp_buffer), "Fila P%d: ", i + 1);
        strcat(response_buffer, temp_buffer);
        
        process_node_t* current = g_queue_heads[i];
        if (current == NULL) {
            strcat(response_buffer, "VAZIA\n");
        } else {
            while(current != NULL) {
                snprintf(temp_buffer, sizeof(temp_buffer), "[%d] -> ", current->pid);
                strcat(response_buffer, temp_buffer);
                current = current->next;
            }
            strcat(response_buffer, "NULL\n");
        }
    }

    // Copia a resposta final para a mensagem
    snprintf(reply.command, sizeof(reply.command), "%s", response_buffer);

    if (msgsnd(g_msg_queue_id, &reply, sizeof(reply.command), 0) < 0) {
        perror("msgsnd reply");
    }
}

void handle_exit() {
    printf("[Scheduler] Recebido EXIT. Encerrando...\n");
    
    // PRÓXIMOS PASSOS:
    // 1. Matar todos os processos filhos (workers) ainda vivos.
    //    - Iterar por g_queue_heads[0], [1], [2]... e dar kill(pid, SIGKILL)
    //    - Não esquecer de matar o g_current_process_pid também.
    //    - Dar free() em todos os nós (nodes).
    
    // 2. Calcular turnaround para os processos que não terminaram e imprimir.
    //    - Ex: double turnaround = difftime(time(NULL), node->start_time);
    
    // 3. Remover a fila de mensagens do sistema (JÁ FEITO)
    if (msgctl(g_msg_queue_id, IPC_RMID, NULL) < 0) {
        perror("msgctl IPC_RMID");
    }
    
    printf("[Scheduler] Encerrado.\n");
    exit(0);
}

// signal handlers
// ...
// O sigalrm_handler está PRONTO.
// ...
void sigalrm_handler(int sig) {
    if (g_current_process_pid == 0) {
        return; 
    }
    schedule();
}

// ...
// O sigchld_handler está quase PRONTO.
// ...
void sigchld_handler(int sig) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[Scheduler] Filho %d terminou.\n", pid);

        if (pid == g_current_process_pid) {
            // PRÓXIMO PASSO: Calcular e imprimir o turnaround aqui.
            // Ex: double turnaround = difftime(time(NULL), g_current_process_node->start_time);
            // printf("Processo %d terminou. Turnaround: %.2f seg\n", pid, turnaround);

            free(g_current_process_node);
            g_current_process_pid = 0;
            g_current_process_node = NULL;
            
            schedule(); // Chama o escalonador para por outro para rodar.
        
        } else {
            // Processo terminou mas não estava rodando (estava na fila)
            // Isso não deveria acontecer (pois ele se congela), mas é bom tratar.
            // A função remove_process_from_queue já dá free() no nó.
            
            // PRÓXIMO PASSO: Se quiséssemos turnaround aqui,
            // a 'remove_process_from_queue' teria que *retornar* o nó
            // antes de dar free(), para lermos o start_time.
            remove_process_from_queue(pid);
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

    // --- 1. Inicializar Filas de Prioridade ---
    // (Não precisa alocar, já usamos globais)

    // --- 2. Conectar à Fila de Mensagens ---
    key_t key = ftok(MSG_QUEUE_KEY_PATH, MSG_QUEUE_KEY_ID);
    if (key == -1) { perror("ftok"); return 1; }
    
    g_msg_queue_id = msgget(key, 0666); // Pega a fila criada pelo shell_sched
    if (g_msg_queue_id < 0) {
        perror("msgget (o shell_sched deve criar primeiro)");
        return 1;
    }

    // --- 3. Configurar Signal Handlers ---
    signal(SIGALRM, sigalrm_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, handle_exit);      // Ctrl+C no escalonador

    // --- 4. Configurar o Timer (Quantum) ---
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = QUANTUM_MS * 1000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = QUANTUM_MS * 1000;
    
    if (setitimer(ITIMER_REAL, &timer, NULL) < 0) {
        perror("setitimer"); return 1;
    }

    // --- 5. Loop Principal de Mensagens ---
    struct sched_msg rx_msg;
    while (1) {
        // Bloqueia esperando uma mensagem DO TIPO 1
        if (msgrcv(g_msg_queue_id, &rx_msg, sizeof(rx_msg.command), MSG_TYPE_TO_SCHEDULER, 0) < 0) {
            perror("msgrcv");
            break;
        }

        // Processa o comando recebido
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