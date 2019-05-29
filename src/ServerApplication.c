#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <limits.h>

#define BUFFER_SIZE 256
#define IPV4 AF_INET
#define TCP SOCK_STREAM
#define ALL_IPS INADDR_ANY
#define address_in struct sockaddr_in

/* Parâmetros de conexão */
int PORT;

/* Número de threads slaves */
unsigned int NUM_THREADS;

/* Resposta das threads slaves */
const char pre_answer[] = "I've got your ";
const char pos_answer[] = " message.";

/* Mensagens de término do programa */
const char exit_message[] = "exit";
const char exitn_message[] = "exit\n";

/* Variáveis de requisições */
int request_available = 0;
pthread_mutex_t mutex_request;
pthread_cond_t condition_request;

/* Variáveis compartilhadas */
unsigned int number_of_requests = 0;
pthread_mutex_t mutex_connection_info;

/* Fila de requests */
TAILQ_HEAD(tailhead, entry) head;

struct entry {
    int open_socket;
    TAILQ_ENTRY(entry) entries;
};

/* Adiciona request na fila */
void add_request_to_queue(int connection)
{
    struct entry *element = malloc(sizeof(struct entry));
    if(element) {
        element->open_socket = connection;
    }
    TAILQ_INSERT_TAIL(&head, element, entries);
}

/*
 * Consome um request da fila e caso a fila fique vazia,
 * atualiza condição de request disponível.
 */
int get_request_from_queue()
{
    pthread_mutex_lock(&mutex_connection_info);
    ++number_of_requests;
    struct entry* next_request = TAILQ_FIRST(&head);
    TAILQ_REMOVE(&head, next_request, entries);
    if(TAILQ_EMPTY(&head)) {
        pthread_mutex_lock(&mutex_request);
        request_available = 0;
        pthread_mutex_unlock(&mutex_request);
    }
    printf("(pthread %lx) request %d from connection %d.\n",
        pthread_self(),
        number_of_requests,
        next_request->open_socket);
    pthread_mutex_unlock(&mutex_connection_info);
    return next_request->open_socket;
}

/*
 * Faz uma manipulação de char[] e o número de mensagens
 * até o momento para construir a resposta do servidor.
 */
char* build_message(unsigned int number_of_messages)
{
    char number_of_messages_string[32];
    sprintf(number_of_messages_string, "%d", number_of_messages);
    char* answer_message = malloc(
        strlen(pre_answer) +
        strlen(number_of_messages_string) + 
        strlen(pos_answer) +
        1);
    strcpy(answer_message, pre_answer);
    strcat(answer_message, number_of_messages_string);
    strcat(answer_message, pos_answer);
    strcat(answer_message, "\0");
    return answer_message;
}

/*
 * Comportamento das threads slave:
 * - espera a condição da variável request_available ser diferente de 0,
 * o que indica que há um request a ser tratado;
 * - consome o request da fila de requests;
 * - troca mensagens com o cliente;
 * - fecha conexão quando cliente digitar 'exit'.
 */
void* slave(void* ignored)
{   
    while(1) {
        // enquanto não houver requests disponíveis, fico em espera
        pthread_mutex_lock(&mutex_request);
        while(request_available == 0) {
            pthread_cond_wait(&condition_request, &mutex_request);
        }        
        pthread_mutex_unlock(&mutex_request);

        // consome requisição da fila
        int current_connection = get_request_from_queue();
        unsigned int number_of_messages = 1;
        char buffer[BUFFER_SIZE];
        while(1) {
            // limpa buffer e lê do cliente
            memset(buffer, 0, BUFFER_SIZE - 1);
            if(read(current_connection, buffer, BUFFER_SIZE) < 0) {
                perror("Couldn't read from socket.");
                pthread_exit(NULL);
            }

            // se o cliente mandou 'exit', fecha sua conexão e sai do loop
            if(strcmp(exit_message, buffer) == 0 || strcmp(exitn_message, buffer) == 0) {
                printf("Client (%d) disconnected.\n", current_connection);
                printf("(pthread %lx) is ready to accept new connections!\n", pthread_self());
                break;
            }

            // constrói mensagem e manda pro cliente
            char* answer_message = build_message(number_of_messages);
            printf("Client (%d): %s\n", current_connection, buffer);
            if(write(current_connection, answer_message, strlen(answer_message)) < 0) {
                perror("Couldn't write to socket.");
                pthread_exit(NULL);
            }    
            free(answer_message);
            number_of_messages++;
        }
        close(current_connection);
    }
    perror("Unexpected behaviour.\n");
    pthread_exit(NULL);
}

/*
 * Comportamento da thread master:
 * - configura socket para ouvir conexões e deixa fila de request vazia;
 * - espera até aceitar uma conexão;
 * - adiciona requisição na fila para ser tratada pelas slaves;
 * - satisfaz condição de aceite de request, ou seja, libera uma thread
 * slave para que ela trate o request.
 */
void* master(void* ignored)
{
    // configura socket IPV4 e TCP
    int socket_file_descriptor = socket(IPV4, TCP, 0);
    if(socket_file_descriptor < 0) {
        perror("Couldn't open socket.");
    }

    // configura endereço, porta e dá o bind do socket
    address_in server_address;
    bzero((char*) &server_address, sizeof(server_address));
    server_address.sin_family = IPV4;
    server_address.sin_addr.s_addr = ALL_IPS;
    server_address.sin_port = htons(PORT);    
    int binding = bind(
        socket_file_descriptor,
        (struct sockaddr*) &server_address,
        sizeof(server_address));
    if(binding < 0) {
        perror("Couldn't bind socket configuration on address.");
        exit(0);
    }

    // coloca socket para ouvir até INT_MAX conexões
    listen(socket_file_descriptor, INT_MAX);

    TAILQ_INIT(&head);
    address_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    while(1) {
        // aceita nova conexão
        int new_connection = accept(
            socket_file_descriptor,
            (struct sockaddr*) &client_address,
            &client_address_length);
        if(new_connection < 0) {
            perror("Couldn't accept new connection.");
            exit(0);
        }

        // adiciona requisição para fila e atualiza condição de requisição disponível
        pthread_mutex_lock(&mutex_connection_info);
        add_request_to_queue(new_connection);
        printf("Added new request to Queue.\n");
        request_available = 1;
        pthread_mutex_unlock(&mutex_connection_info);

        // libera uma thread slave para tratar requisição
        pthread_cond_signal(&condition_request);
    }
    perror("Unexpected behaviour.\n");
    pthread_exit(NULL);
}

/* Cria a thread master */
void intialize_master_thread()
{
    pthread_t master_thread;
    if(pthread_create(&master_thread, NULL, master, (void*) NULL) != 0)
        perror("Couldn't create pthread.\n");
    if(pthread_join(master_thread, NULL))
        perror("Couldn't join pthread.\n");
}

/* Cria todas as threads slave */
void initialize_slave_threads(pthread_t* slaves)
{
    uint i;
    for(i = 0; i < NUM_THREADS; ++i)
        if(pthread_create(&slaves[i], NULL, slave, (void*) NULL) != 0)
            perror("Couldn't create pthread.\n");
}

/* Inicializa todos parâmetros pela leitura no terminal */
void set_parameters()
{
    printf("You need to set some parameters before the execution.\n");
    
    char raw_port[4];
    printf("> port number desired: ");
    scanf("%s", raw_port);
    PORT = atoi(raw_port);

    printf("> number of slave threads: ");
    scanf("%u", &NUM_THREADS);
}

/*
 * Ponto de execução do servidor:
 * - recebe parâmetros iniciais;
 * - inicializa mutex da condição de request disponível e da fila de requests;
 * - cria as threads slave e a thread listener;
 * - após término da master, libera memória.
 */
int main()
{
    set_parameters();
    
    pthread_mutex_init(&mutex_request, NULL);
    pthread_mutex_init(&mutex_connection_info, NULL);
    pthread_cond_init(&condition_request, NULL);

    pthread_t slaves[NUM_THREADS];
    initialize_slave_threads(slaves); 
    intialize_master_thread();

    pthread_mutex_destroy(&mutex_request);
    pthread_mutex_destroy(&mutex_connection_info);
    pthread_cond_destroy(&condition_request);
}
