#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#define _GNU_SOURCE  
#include <unistd.h>
#include <string.h>   
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <fcntl.h>
#include <search.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PC_buffer_len 10
#define Num_elem 1000000

typedef struct{
    int readers;
    bool writing;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} rw;


typedef struct {
    char **buffer_let;
    sem_t *data_items_let;
    sem_t *free_slots_let;
    int *nlet;
    int capolet;
} argomenti;


typedef struct{
    char **buffer_sc;
    sem_t *data_items_sc;
    sem_t *free_slots_sc;
    int *nsc;
    int caposc;
} argomenti2;

typedef struct{
    rw *readWriterArgs;
    pthread_mutex_t *mutex;
    int *cindex;
    char **buffer_let;
    sem_t *data_items_let;
    sem_t *free_slots_let;
    
    FILE *logfile;
    pthread_mutex_t *mutex_log;
} arglet;

typedef struct{
    rw *readWriterArgs;
    pthread_mutex_t *mutex;
    int *cindex;
    char **buffer_sc;
    sem_t *data_items_sc;
    sem_t *free_slots_sc;
    ENTRY **testa_lista_entry;
    int *entry_totali;
} argsc;

typedef struct {
 int *entry_totali;
 rw *readWriterArgs;
 ENTRY **testa_lista_entry;
 int pipe_sigterm[2];
 
} args_gestore;


typedef struct {
 int valore;
 ENTRY *next;
} coppia;

//Funzioni per la gestione della tabella hash
ENTRY *crea_entry(char *s);
void distruggi_entry(ENTRY *e);
void distruggi_entry_safe(ENTRY *e);
void *dealloca_hash(ENTRY *testa);
void *aggiungi(char *s, int *entry_totali, ENTRY **testa_lista_entry);
int conta(char *s);

//Funzioni per implementare il paradigma lettori/scrittori
void rw_init(rw *z);
void *read_lock(rw *v);
void *read_unlock(rw *v);
void *write_lock(rw *v);
void *write_unlock(rw *v);

char *strtok_r(char *str, const char *delim, char **saveptr);

ssize_t readn(int fd, void *ptr, size_t n);
