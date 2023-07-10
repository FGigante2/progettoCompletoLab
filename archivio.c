#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#define _GNU_SOURCE   // permette di usare estensioni GNU
#include <unistd.h>
#include <string.h>   // funzioni per stringhe
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
} argomenti;

typedef struct{
    char **buffer_sc;
    sem_t *data_items_sc;
    sem_t *free_slots_sc;
    int *nsc;
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
 int valore;
 ENTRY *next;
} coppia;

ENTRY *crea_entry(char *s){
	ENTRY *e = malloc(sizeof(ENTRY));
	if(e == NULL){
		perror("Errore malloc entry");
	}
	
	e->key = s;
	//e->key = strdup(s);
	e->data = malloc(sizeof(coppia));
	
	//in `data` metto la coppia che deve essere usata per memorizzare la linked list
	coppia *c = (coppia *)e->data;
	c->valore = 1;
	c->next = NULL;
	
	return e;
}

void distruggi_entry(ENTRY *e)
{
  free(e->key); 
  free(e->data); 
  free(e);
}

void distruggi_entry_safe(ENTRY *e){

free(e->data);
free(e);
}



void *dealloca_hash(ENTRY *testa){

	ENTRY *attuale = testa; //prendo la testa della lista
	
	while(attuale != NULL){  //finche non finisco la lista
	
		ENTRY *prossima  = ( (coppia *)attuale->data )-> next; //mi salvo la prossima entry
	
		free(attuale->key);		//libero la memoria della stringa
		coppia *c = (coppia*)attuale->data;
		//libero la memoria della coppia
		free(c);
		free(attuale);
		
	
		attuale = prossima;
	}
	return NULL;
}


void *aggiungi(char *s, int *entry_totali, ENTRY **testa_lista_entry){
	//creo la entry associata alla stringa
	ENTRY *e = crea_entry(s);
	//controlla se la entry e nella tabella
	ENTRY *r = hsearch(*e, FIND);
	if(r == NULL){
		//aumento il numero di entry
		*(entry_totali)+=1;
		
		//devo inserire r con la ENTRY
		r = hsearch(*e, ENTER);
		
		// ho messo la ENTRY nella hash table
		printf("Chiave creata : %s valore %d\n", r->key, *((int *)r->data));
		
		//aggiorno la linked list
		coppia *c = (coppia *)e->data;
		c->next = *(testa_lista_entry);
		*(testa_lista_entry) = e;
		
		
	}
	else{
		//elemento gia presente, incremento di uno 'data'
		printf("Chiave gia trovata: %s\n", r->key);
		coppia *c = (coppia *)r->data;
	 	c->valore += 1;
	 	distruggi_entry(e);
	}
	
	return NULL;	
}

int conta(char *s){
	//creo una entry con la stringa che ho preso dal buffer, la entry mi serve per fare la search nella tabella hash,
	// in entrambi i casi devo distruggere la entry e restituire solo il valore
	ENTRY *e = crea_entry(s);
	
	ENTRY *r = hsearch(*e, FIND);
	if(r == NULL){
		// se non e` presente la stringa nella tabella hash
		distruggi_entry_safe(e);
		return 0;
	}
	else {
		int valore = ((coppia*)r->data) -> valore;
		distruggi_entry_safe(e);
		return valore;
	}		
}


void rw_init(rw *z)
{
  z->readers = 0;
  z->writing = false;
  pthread_cond_init(&z->cond,NULL);
  pthread_mutex_init(&z->mutex,NULL);
}

void *read_lock(rw *v){
    pthread_mutex_lock(&v->mutex);

        while(v->writing == true){
        	pthread_cond_wait(&v->cond, &v->mutex);
        }
        v->readers++;

    pthread_mutex_unlock(&v->mutex);
    return NULL;
}
void *read_unlock(rw *v){
    assert(v->writing == false);
    assert(v->readers > 0); //ci dobbiamo essere almeno noi che stiamo sempre leggendo
    pthread_mutex_lock(&v->mutex);
        v->readers--;
        if(v->readers == 0){
            pthread_cond_signal(&v->cond);
        }
    pthread_mutex_unlock(&v->mutex);
	return NULL;
}
void *write_lock(rw *v){
    pthread_mutex_lock(&v->mutex);
        while(v->readers > 0 || v->writing == true){
            pthread_cond_wait(&v->cond, &v->mutex);
        }
        v->writing = true;
    pthread_mutex_unlock(&v->mutex);
    return NULL;

}
void *write_unlock(rw *v){
    pthread_mutex_lock(&v->mutex);
        v->writing = false;
        pthread_cond_broadcast(&v->cond);
    pthread_mutex_unlock(&v-> mutex);
    return NULL;

}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (str == NULL)
        str = *saveptr;

    str += strspn(str, delim);

    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }

    char *token = str;
    str = strpbrk(token, delim);

    if (str == NULL)
        *saveptr = strchr(token, '\0');
    else {
        *str = '\0';
        *saveptr = str + 1;
    }

    return token;
}

ssize_t readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n;
   while (nleft > 0) {
     if((nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount read so far */
     } else if (nread == 0) break; /* EOF */
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft); /* return >= 0 */
}

void *lettore(void *v){
    
    arglet *args = (arglet *)v;
    char *parola_letta;

    while(true){
        //leggo le parole mandate da capolet all'interno del buffer buffer_let
        sem_wait(args->data_items_let);
        pthread_mutex_lock(args->mutex);        
        	parola_letta = args->buffer_let[*(args->cindex) % PC_buffer_len];
        	*(args->cindex) += 1;     	
        pthread_mutex_unlock(args->mutex);
        sem_post(args->free_slots_let);
        
        
        //se non e` il valore di terminazione invoco la funzione conta()
        if(atoi(parola_letta) != -1){
        read_lock(args->readWriterArgs);
        int n = conta(parola_letta);
        printf("Il valore letto nella tabella hash : %d\n", n);
        read_unlock(args->readWriterArgs);
        
        
        //scrivo il valore letto nel logfile
        pthread_mutex_lock(args->mutex_log);
        	fprintf(args->logfile , "%s %d\n", parola_letta, n);
        	fflush(args->logfile);
        pthread_mutex_unlock(args->mutex_log);
        
        free(parola_letta);
                
        } else break;
        
        	}       	           
       pthread_exit(NULL);
}
	
	

void *scrittore(void *v){
    argsc *args = (argsc *)v;
    char* parola_letta;
    while(true){
        //leggo le parole inviate da caposc nel buffer buffer_sc
        sem_wait(args->data_items_sc);
        pthread_mutex_lock(args->mutex);
        	parola_letta = args->buffer_sc[*(args->cindex) % PC_buffer_len];
        	*(args->cindex) += 1;
        pthread_mutex_unlock(args->mutex);
        sem_post(args->free_slots_sc);
     	
     	//se ho letto una stringa valida, la aggiungo alla tabella hash
     	if(atoi(parola_letta) != -1){    	
     	write_lock(args->readWriterArgs);	
     	aggiungi(parola_letta, args->entry_totali, args->testa_lista_entry);
     	write_unlock(args->readWriterArgs);
     	//free(parola_letta);
     	} else break;
    		}
    pthread_exit(NULL);
}

void *caposc(void *v){
    printf("caposc \u00e8 partito sta leggendo dalla pipe\n");
    int fd2 = open("caposc",O_RDONLY);
     if(fd2 < 0){
        perror("errore apertura pipe caposc");
        exit(0);
     }
    argomenti2 *args = (argomenti2 *)v;
    char *delim = ".,:; \n\r\t";
    int pindex = 0;
    char *val_terminazione = "-1";

     while(true){

        //dati per la strtok_r
        char line2[2048];
        char *ptr = NULL;

        //ricevo la lunghezza
        short len2;
        ssize_t e = read(fd2, &len2, sizeof(len2));
        if(e == 0){
        break;
        }
           
        //leggo la linea
        ssize_t t = readn(fd2, line2, ntohs(len2));
        if(t == 0)
            break;
        
        //aggiungo il carattere di terminazione stringa
        line2[t] = '\0';

        //tokenizzo la parola , copio il token e lo scrivo su buffer_sc
       	char *portion = strtok_r(line2, delim, &ptr);
       	while(portion != NULL){

            char *parola = strdup(portion);
            //scriviamo la parola sul buffer condiviso
            sem_wait(args->free_slots_sc);
            	args->buffer_sc[pindex++ % PC_buffer_len] = parola;
            	printf("caposc : %s\n",parola);
            sem_post(args->data_items_sc);
            
       	    portion = strtok_r(NULL,delim, &ptr);
       	    
       			      }
       		}
       	      	
       	//mando i valori di terminazione agli scrittori
       	for(int i=0;i< *(args->nsc) ; i++){
        	sem_wait(args->free_slots_sc);
       			args->buffer_sc[pindex++ % PC_buffer_len] = val_terminazione;       			
       		sem_post(args->data_items_sc);
        }
    close(fd2);
    pthread_exit(NULL);
}

void *capolet(void *v){

    printf("capolet \u00e8 partito sta leggendo dalla pipe\n");
    
    int fd = open("capolet", O_RDONLY);
    if(fd < 0){
        perror("errore apertura pipe capolet\n");
        exit(0);
    }
        
    char *delim = ".,:; \n\r\t";
    char *val_terminazione = "-1";
    argomenti *args = (argomenti *)v;
    int pindex = 0;
    
    
    while(true){

        char line[2048];
        char* ptr = NULL;

        short len;
        ssize_t e = read(fd, &len, sizeof(len));
        if(e == 0){
        	break;
        }
         
        //leggo la linea
        ssize_t t = read(fd ,line, ntohs(len));
        if(t == 0){
        break;
        }
           
        line[t] = '\0';
                                   
        
        //tokenizzo la linea e la invio al buffer condiviso
       	char *portion = strtok_r(line, delim,&ptr);
       	
       	while(portion != NULL){
       		char *parola = strdup(portion);
       		//metto il duplicato nel buffer
       		sem_wait(args->free_slots_let);
       			printf("capolet : %s\n", parola);
       			args->buffer_let[pindex++ % PC_buffer_len] = parola;
       		sem_post(args->data_items_let);
       		portion = strtok_r(NULL,delim,&ptr);
       		
       		
       	}
       	}
        
        //mando i valori di terminazione ai lettori
        for(int i=0;i<*(args->nlet) ; i++){
        	sem_wait(args->free_slots_let);
       			args->buffer_let[pindex++ % PC_buffer_len] = val_terminazione;      			
       		sem_post(args->data_items_let);
        }               
        close(fd);
        pthread_exit(NULL);
}


void sigusr1_handler(rw *args, ENTRY **testa){

	
	write_lock(args);
	write(1,"parto con lista nuova", 22);
	dealloca_hash(*(testa));
	hdestroy();
	hcreate(Num_elem);
	*(testa) = NULL;
	write_unlock(args);

}

void sigterm_handler(int entry_totali, ENTRY **testa){
	
	
   
    
    char entryStr[16];  
    int len = 0;

    //Converto il numero in stringa
    if (entry_totali == 0) {
        entryStr[0] = '0';
        len = 1;
    } else {
        int i = 0;
        while (entry_totali > 0) {
            entryStr[i] = '0' + (entry_totali % 10);
            entry_totali /= 10;
            i++;
        }

        len = i;

        //Ho processato le cifre nel verso opposto, quindi faccio reverse() della stringa
        for (int j = 0; j < len / 2; j++) {
            char temp = entryStr[j];
            entryStr[j] = entryStr[len - j - 1];
            entryStr[len - j - 1] = temp;
        }
    }
    
    
    //Scrivo i il valore trovato su stderr
    write(2, "Entry totali tabella hash:", 27);
    write(2, entryStr, len);
    write(2, "\n", 1);

  //dealloco la tabella hash
   dealloca_hash(*(testa));
   hdestroy();
   
  
  //faccio terminare il programma (scrivo sulla pipe al main)
  
	}



void sigint_handler(int entry_totali){
    //Creo una stringa di 16 caratteri per contenere il numero totale di entry (max 16 cifre)
    char entryStr[16];  
    int len = 0;

    //Converto il numero in stringa
    if (entry_totali == 0) {
        entryStr[0] = '0';
        len = 1;
    } else {
        int i = 0;
        while (entry_totali > 0) {
            entryStr[i] = '0' + (entry_totali % 10);
            entry_totali /= 10;
            i++;
        }

        len = i;

        //Ho processato le cifre nel verso opposto, quindi faccio reverse() della stringa
        for (int j = 0; j < len / 2; j++) {
            char temp = entryStr[j];
            entryStr[j] = entryStr[len - j - 1];
            entryStr[len - j - 1] = temp;
        }
    }
    
    
    //Scrivo i il valore trovato su stderr
    write(2, "Entry totali tabella hash:", 27);
    write(2, entryStr, len);
    write(2, "\n", 1);

}



typedef struct {
 int *entry_totali;
 rw *readWriterArgs;
 ENTRY **testa_lista_entry;
 
 
 int pipe_sigterm[2];
 
} args_gestore;



void* tgestore_thread(void *v){
	
	
	
	args_gestore *args = (args_gestore *)v;
	
	
	/*
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	*/
	
	/*
       struct sigaction sa;
       sigaction(SIGINT,NULL , &sa);
       sa.sa_handler = sigint_handler;       
       sigaction(SIGINT, &sa, NULL);
       */
	sigset_t mask;
    	sigfillset(&mask);
	int signal;
	
	while(true){

	int s = sigwait(&mask, &signal);
	if(s != 0){
		perror("Errore sigwait");
	}
	
	  printf("Il thread gestore ha ricevuto il segnale %d\n", signal);
	  
	  if(signal == SIGINT){
	  	sigint_handler( *(args->entry_totali));
	  }
	  
	  if(signal == SIGTERM){
	  	
	  	char messaggio[5];
	  	
	  	ssize_t bytes = read((args->pipe_sigterm)[0], &messaggio, 4);
	  	
	  	if(bytes > 0){
	  	 messaggio[bytes] = '\0';
	  	 write(1, "Messaggio:", 11);
	  	 write(1, messaggio, 5);
	  	 write(1,"\n", 1);
	  	}
	  	
	  	close((args-> pipe_sigterm)[0]);
	  	
	  	sleep(5);
	  	
	  	/*
	  	write(1,"M V Term", 9);
	  	char messaggio2[] = "Miao";
	  	write((args->pipe_sigterm)[0], messaggio2, 5);

	  	*/
	  	
	  	write(1, "Ho fatto tutto ora termino", 27);
	  	
	  	pthread_exit(NULL);
	  }
	  
	  if(signal == SIGUSR1){
	  sigusr1_handler(args->readWriterArgs, args->testa_lista_entry);
	  }
	
	}
	
	return NULL;
}



int main(int argc , char *argv[]){

        assert(argc >2);
        
     //creo la tabella hash
     int hash_table = hcreate(Num_elem);
      if(hash_table == 0){
		perror("Errore creazione hash table");
		exit(1);
	}
  	
     //apro il file lettore.log
     FILE *logfile = fopen("lettori.log","w");
     if(logfile == NULL){
     	perror("ERRORE APERTURA LOGFILE");
     	exit(1);
     }
     
    int entry_totali = 0;
    ENTRY *testa_lista_entry = NULL;

    //GESTIONE DEI SEGNALI
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK,&mask,NULL);
    //il thread main blocca tutti i segnali, i thread erediteranno questo blocco ma il thread gestore li riattiva tutti

   struct sigaction sa_int;
   
   sigaction(SIGINT,NULL,&sa_int);
   sa_int.sa_handler = sigint_handler;
   sigaction(SIGINT,&sa_int,NULL);


    pthread_t capi[2];
    argomenti args[1];
    argomenti2 args2[1];
    
    pthread_t tgestore[1];
    args_gestore argsgestore[1];
    
   

    //numero lettori e numero scrittori
    int nlet = atoi(argv[1]);
    int nsc = atoi(argv[2]);
    
    //dati per i lettori e scrittori
    pthread_t let[nlet];
    pthread_t sc[nsc];
    arglet arg_let[nlet];
    argsc arg_sc[nsc];
    
    
    //passo un pu
    
    pthread_mutex_t mu_let = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mu_sc = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mu_log = PTHREAD_MUTEX_INITIALIZER;
    int cindex_let = 0;
    int cindex_sc = 0;

    //inizializzo il buffer prodcons (comunicazione fra capi e scrittori/lettori)
    char *buffer_let[PC_buffer_len];
    char *buffer_sc[PC_buffer_len];
   
    //inizializzo i semafori
    sem_t free_slots_let, data_items_let, free_slots_sc, data_items_sc;
    sem_init(&free_slots_let, 0, PC_buffer_len);
    sem_init(&data_items_let, 0, 0);
    sem_init(&free_slots_sc,0, PC_buffer_len);
    sem_init(&data_items_sc,0, 0);

    //inizializzo dati per read/write condiviso
    rw readWriterArgs[1];
    rw_init(&readWriterArgs[0]);
    


    //dati per caposc
    args2[0].data_items_sc = &data_items_sc;
    args2[0].free_slots_sc = &free_slots_sc;
    args2[0].buffer_sc = buffer_sc; 
    args2[0].nsc = &nsc;
    
    //dati per capolet
    args[0].data_items_let = &data_items_let;
    args[0].free_slots_let = &free_slots_let;
    args[0].buffer_let = buffer_let;
    args[0].nlet = &nlet;
    
    //creo i thread lettori
    for(int i = 0 ; i < nlet; i++){
        arg_let[i].mutex = &mu_let;
        arg_let[i].cindex = &cindex_let;
        arg_let[i].buffer_let = buffer_let;
        arg_let[i].data_items_let = &data_items_let;
        arg_let[i].free_slots_let = &free_slots_let;
        arg_let[i].readWriterArgs = &readWriterArgs[0];
        arg_let[i].logfile = logfile;
        arg_let[i].mutex_log = &mu_log;
        pthread_create(&let[i], NULL , lettore, arg_let+i);
    }
	
    //creo i thread scrittori
    for(int i = 0; i < nsc; i++){
        arg_sc[i].mutex = &mu_sc;
        arg_sc[i].cindex = &cindex_sc;
        arg_sc[i].buffer_sc = buffer_sc;
        arg_sc[i].data_items_sc = &data_items_sc;
        arg_sc[i].free_slots_sc = &free_slots_sc;
        arg_sc[i].readWriterArgs = &readWriterArgs[0];
        arg_sc[i].entry_totali = &entry_totali;
        arg_sc[i].testa_lista_entry = &testa_lista_entry;
        pthread_create(&sc[i], NULL , scrittore, arg_sc+i);
    }
    
    argsgestore[0].entry_totali = &entry_totali;
    argsgestore[0].readWriterArgs = &readWriterArgs[0];
    argsgestore[0].testa_lista_entry = &testa_lista_entry;
    
    
    

    //creo capolet e caposc, e tgestore
    pthread_create(&capi[0], NULL , capolet , &args[0]);
    pthread_create(&capi[1], NULL , caposc, &args2[0]);
    
    int pipe_sigterm[2];
    pipe(pipe_sigterm);
    argsgestore[0].pipe_sigterm[0] = pipe_sigterm[0];
    argsgestore[0].pipe_sigterm[1] = pipe_sigterm[1];
    
    
    pthread_create(&tgestore[0], NULL, tgestore_thread, &argsgestore[0]);
    
    //faccio la join di tutti i thread
    pthread_join(capi[0],NULL);
    pthread_join(capi[1],NULL);
    
    for(int i = 0 ; i< nlet ; i++){
    pthread_join(let[i],NULL);
    }
    
    for(int i = 0 ; i< nsc ; i++){
    pthread_join(sc[i],NULL);
    }
    
    
    //comunicazione con sigterm attraverso le pipe
    char main_to_gestore[] = "Ciao";
    write(pipe_sigterm[1], main_to_gestore, 5);
    
    
    //-------------------------------------------------------------------------------------------
   
   //char gestore_to_main[5];
   //while(gestore_to_main != "Miao"){
   
   /*
   printf("ok");
   ssize_t r = read(pipe_sigterm[0], &gestore_to_main, 5);
   if(r > 0){
   //gestore_to_main[r] = '\0';
   printf("Ho ricevuto %s ora devo terminare", gestore_to_main);
   }
   }
   */
   
   
   close(pipe_sigterm[1]);
   
    pthread_join(tgestore[0], NULL);
    
   
   
    dealloca_hash(testa_lista_entry);
    hdestroy();
    fclose(logfile);
    pthread_mutex_destroy(&mu_let);
    pthread_mutex_destroy(&mu_sc);
    pthread_mutex_destroy(&mu_log);
    
    
    sem_destroy(&data_items_let);
    sem_destroy(&free_slots_let);
    sem_destroy(&data_items_sc);
    sem_destroy(&free_slots_sc);
    printf("ho chiuso fino a qua e dovrei aver deallocato");
    
 return 0;
}