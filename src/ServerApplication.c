#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 256
#define IPV4 AF_INET
#define TCP SOCK_STREAM
#define ALL_IPS INADDR_ANY
#define address_in struct sockaddr_in

/* Parâmetros de conexão */
int PORT;
int NUM_CONNECTIONS;

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
int open_socket;
unsigned int number_of_requests = 0;
pthread_mutex_t mutex_connection_info;

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
 *
 */
void* slave(void* ignored)
{   
    while(1) {
        pthread_mutex_lock(&mutex_request);
        while(request_available == 0) {
            pthread_cond_wait(&condition_request, &mutex_request);
        }
        request_available = 0;
        pthread_mutex_unlock(&mutex_request);

        pthread_mutex_lock(&mutex_connection_info);
        ++number_of_requests;
        int current_connection = open_socket;
        printf("(pthread %lx) request %d from connection %d.\n", pthread_self(), number_of_requests, current_connection);
        pthread_mutex_unlock(&mutex_connection_info);

        unsigned int number_of_messages = 1;
        char buffer[BUFFER_SIZE];
        while(1) {
            memset(buffer, 0, BUFFER_SIZE - 1);
            if(read(current_connection, buffer, BUFFER_SIZE) < 0) {
                perror("Couldn't read from socket.");
                pthread_exit(NULL);
            }
            if(strcmp(exit_message, buffer) == 0 || strcmp(exitn_message, buffer) == 0) {
                printf("Client (%d) disconnected.\n", current_connection);
                printf("(pthread %lx) is ready to accept new connections!\n", pthread_self());
                break;
            }
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
 *
 */
void* master(void* ignored)
{
    int socket_file_descriptor = socket(IPV4, TCP, 0);
    if(socket_file_descriptor < 0) {
        perror("Couldn't open socket.");
    }

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
    listen(socket_file_descriptor, NUM_CONNECTIONS);

    address_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    while(1) {
        int new_connection = accept(
            socket_file_descriptor,
            (struct sockaddr*) &client_address,
            &client_address_length);
        if(new_connection < 0) {
            perror("Couldn't accept new connection.");
            exit(0);
        }

        pthread_mutex_lock(&mutex_connection_info);
        open_socket = new_connection;
        request_available = 1;
        pthread_mutex_unlock(&mutex_connection_info);

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
    
    printf("> number of slave threads: ");
    scanf("%u", &NUM_THREADS);

    char raw_port[4];
    printf("> port number desired: ");
    scanf("%s", raw_port);
    PORT = atoi(raw_port);

    printf("> number of maximum client connections: ");
    scanf("%d", &NUM_CONNECTIONS);
}

/*
 * Ponto de execução do programa:
 * - recebe parâmetros iniciais;
 * - inicializa mutex de request e de number;
 * - cria as threads slave e a thread listener.
 * - após término da master, libera memória
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