#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 256

/* Parâmetros definidos no console */
unsigned int NUM_THREADS;
unsigned int MASTER_SLEEP_MICROSEC;

/* Variáveis de requisições */
pthread_mutex_t mutex_request;
pthread_cond_t condition_request;

/* Variáveis de resultados */
unsigned int number_of_requests = 0;
pthread_mutex_t mutex_number;

/*
 * Threads slave consomem notificações da thread master,
 * de forma que uma única processa o request e printa no
 * terminal o novo número de requisições feitas até o momento.
 */
void* slave(void* ignored)
{   
    while(1) {
        pthread_mutex_lock(&mutex_request);
        pthread_cond_wait(&condition_request, &mutex_request);
        pthread_mutex_unlock(&mutex_request);

        pthread_mutex_lock(&mutex_number);
        number_of_requests++;
        printf("(pthread %lx) request %d\n", pthread_self(), number_of_requests);
        pthread_mutex_unlock(&mutex_number);
    }
    perror("Unexpected behaviour.");
    return NULL;
}


/*
 * Thread master notifica threads slave disponíveis que
 * uma nova requisição fake ocorreu.
 */
void* master(void* ignored)
{
    while(1) {
        usleep(MASTER_SLEEP_MICROSEC);
        pthread_cond_signal(&condition_request);
    }
    perror("Unexpected behaviour.");
    return NULL;
}

/* Cria a thread master */
void intialize_master()
{
    pthread_t master_thread;
    if(pthread_create(&master_thread, NULL, master, (void*) NULL) != 0)
        perror("Couldn't create pthread.");
    if(pthread_join(master_thread, NULL))
        perror("Couldn't join pthread.");
}

/* Cria todas as threads slave */
void initialize_slave_threads(pthread_t* slaves)
{
    uint i;
    for(i = 0; i < NUM_THREADS; i++)
        if(pthread_create(&slaves[i], NULL, slave, (void*) NULL) != 0)
            perror("Couldn't create pthread.");
}

/* Inicializa todos parâmetros pela leitura no terminal */
void set_parameters()
{
    printf("You need to set some parameters before the execution.\n");
    
    printf("> number (bigger than 0) of slave threads: ");
    scanf("%u", &NUM_THREADS);

    printf("> time, in microseconds (10^-6, recommended 800000) to fake a new request: ");
    scanf("%u", &MASTER_SLEEP_MICROSEC);
}

/*
 * Ponto de execução do programa:
 * - recebe parâmetros iniciais;
 * - inicializa mutex de request e de number;
 * - cria as threads slave e a thread listener.
 */
int main()
{
    set_parameters();
    
    pthread_mutex_init(&mutex_request, NULL);
    pthread_mutex_init(&mutex_number, NULL);
    pthread_cond_init(&condition_request, NULL);

    pthread_t slaves[NUM_THREADS];
    initialize_slave_threads(slaves); 
    intialize_master();  
}