#include "funzioni.h"

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
        if(strcmp(parola_letta , "-1") != 0){
        read_lock(args->readWriterArgs);
        	int n = conta(parola_letta);       
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
     	if(strcmp(parola_letta,"-1") != 0){    	
     	write_lock(args->readWriterArgs);	
     	aggiungi(parola_letta, args->entry_totali, args->testa_lista_entry);
     	write_unlock(args->readWriterArgs);
     	
     	} else break;
    		}
    pthread_exit(NULL);
}

void *caposc(void *v){

    argomenti2 *args = (argomenti2 *)v;
    char *delim = ".,:; \n\r\t";
    int pindex = 0;
    char *val_terminazione = "-1";
    int caposc = args->caposc;

     while(true){

        //dati per la strtok_r
        char line2[2048];
        char *ptr = NULL;

        //ricevo la lunghezza
        short len2;
        ssize_t e = read(caposc, &len2, sizeof(len2));
        if(e == 0){
        break;
        }
           
        //leggo la linea
        ssize_t t = readn(caposc, line2, ntohs(len2));
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
    close(caposc);
    pthread_exit(NULL);
}

void *capolet(void *v){
      
    char *delim = ".,:; \n\r\t";
    char *val_terminazione = "-1";
    argomenti *args = (argomenti *)v;
    int pindex = 0;
    int capolet = args->capolet;
    
    
    while(true){
        char line[2048];
        char* ptr = NULL;
        short len;
        
        ssize_t e = read(capolet, &len, sizeof(len));
        if(e == 0){
        	break;
        }
         
        //leggo la linea
        ssize_t t = read(capolet ,line, ntohs(len));
        if(t == 0){
        break;
        }
           
        line[t] = '\0';
                                  
        //Tokenizzo la linea
       	char *portion = strtok_r(line, delim,&ptr);
       	
       	while(portion != NULL){
       		char *parola = strdup(portion);
       		//Metto la copia nel buffer
       		sem_wait(args->free_slots_let);
       			args->buffer_let[pindex++ % PC_buffer_len] = parola;
       		sem_post(args->data_items_let);
       		portion = strtok_r(NULL,delim,&ptr);   		
            }
            
       	}
     //Mando i valori di terminazione ai lettori
     for(int i=0;i<*(args->nlet) ; i++){
       sem_wait(args->free_slots_let);
       	 args->buffer_let[pindex++ % PC_buffer_len] = val_terminazione;      			
       sem_post(args->data_items_let);
        }               
   close(capolet);
   pthread_exit(NULL);
}

void sigusr1_handler(rw *args, ENTRY **testa, int *entry_totali){
	write_lock(args);
	dealloca_hash(*(testa));
	hdestroy();
	hcreate(Num_elem);
    *(entry_totali) = 0;
	*(testa) = NULL;

	write_unlock(args);
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
   ssize_t a =  write(2, "Entries : ", 11);
   ssize_t b =  write(2, entryStr, len);
   ssize_t c =  write(2, "\n", 1);
}

void sigterm_handler(int pipe_sigterm[2]){

    //leggo prima la lunghezza 
    char entry_totali[20];
    ssize_t bytes = read(pipe_sigterm[0], entry_totali, sizeof(entry_totali));
    if(bytes == 0) {
    	perror("Errore pipe sigterm");
    	exit(1);
    }
    entry_totali[bytes] = '\0';
    close(pipe_sigterm[0]);
    write(1, "Entries : ", 11);
    write(1, entry_totali, strlen(entry_totali));
	  	
    pthread_exit(NULL);
}

void* tgestore_thread(void *v){

 args_gestore *args = (args_gestore *)v;
	
 sigset_t mask;
 sigfillset(&mask);
 int signal;
	
 while(true){
 	int s = sigwait(&mask, &signal);
	if(s != 0){
		perror("Errore sigwait");
		  }
		  
 	switch(signal){
 	
 	case SIGINT:
	  	sigint_handler(*(args->entry_totali));
	  	break;
	  	
	case SIGTERM:
	   	sigterm_handler(args->pipe_sigterm);
	  	break;
	  	
	case SIGUSR1:
	  	sigusr1_handler(args->readWriterArgs, args->testa_lista_entry, args->entry_totali);
	  	break;

	}
 }

 return NULL;

} 
 
int main(int argc , char *argv[]){

     if(argc != 3){
     	printf("Uso : %s r w | r e w sono interi positivi" , argv[0]);
     }
     
    //Numero lettori e numero scrittori
    int nlet = atoi(argv[1]);
    int nsc = atoi(argv[2]);
    
    //Apro le pipe capolet e caposc in scrittura
    int fd = open("capolet", O_RDONLY);
    if(fd < 0){
        perror("Errore apertura pipe capolet\n");
        exit(0);
    }
      
    int fd2 = open("caposc",O_RDONLY);
     if(fd2 < 0){
        perror("Errore apertura pipe caposc");
        exit(0);
     }
    
     //Creo la tabella hash
     int hash_table = hcreate(Num_elem);
      if(hash_table == 0){
		perror("Errore creazione tabella hash");
		exit(1);
	}
  	
     //Apro il file lettore.log
     FILE *logfile = fopen("lettori.log","w");
     if(logfile == NULL){
     	perror("Errore apertura logfile");
     	exit(1);
     }
     
    int entry_totali = 0;
    ENTRY *testa_lista_entry = NULL;

    //Il thread principale blocca tutti i segnali, tutti i thread che creo piu avanti erediteranno questo blocco
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK,&mask,NULL);
    
    //creo un array di pthread_t di due capi e un tgestore
    pthread_t capi[2];
    pthread_t tgestore[1];
    
    //Creo le strutture dati per i capi e tgestore
    argomenti args_capolet[1];
    argomenti2 args_caposc[1];
    args_gestore argsgestore[1];
    
    //dati per i lettori e scrittori
    pthread_t let[nlet];
    pthread_t sc[nsc];
    arglet arg_let[nlet];
    argsc arg_sc[nsc];
    
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
    
    //Creo la pipe per la comunicazione main->sigterm_handler
    int pipe_sigterm[2];
    pipe(pipe_sigterm);
    
    //inizializzo dati per caposc
    args_caposc[0].data_items_sc = &data_items_sc;
    args_caposc[0].free_slots_sc = &free_slots_sc;
    args_caposc[0].buffer_sc = buffer_sc; 
    args_caposc[0].nsc = &nsc;
    args_caposc[0].caposc = fd2;
    
    //inizializzo dati per capolet
    args_capolet[0].data_items_let = &data_items_let;
    args_capolet[0].free_slots_let = &free_slots_let;
    args_capolet[0].buffer_let = buffer_let;
    args_capolet[0].nlet = &nlet;
    args_capolet[0].capolet = fd;
    
    //inizializzo dati per tgestore
    argsgestore[0].entry_totali = &entry_totali;
    argsgestore[0].readWriterArgs = &readWriterArgs[0];
    argsgestore[0].testa_lista_entry = &testa_lista_entry;
    argsgestore[0].pipe_sigterm[0] = pipe_sigterm[0];
    argsgestore[0].pipe_sigterm[1] = pipe_sigterm[1];
    
    //Creo i thread lettori
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
    
    //Creo capolet e caposc, e tgestore
    pthread_create(&capi[0], NULL , capolet , &args_capolet[0]);
    pthread_create(&capi[1], NULL , caposc, &args_caposc[0]);
    pthread_create(&tgestore[0], NULL, tgestore_thread, &argsgestore[0]);
    
    //Faccio la join di tutti i thread
    pthread_join(capi[0],NULL);
    pthread_join(capi[1],NULL);
    
    for(int i = 0 ; i< nlet ; i++){
    pthread_join(let[i],NULL);
    }
    
    for(int i = 0 ; i< nsc ; i++){
    pthread_join(sc[i],NULL);
    }
     
    //Comunicazione con sigterm attraverso la pipe "pipe_sigterm"
    char main_to_gestore[20];
    sprintf(main_to_gestore, "%d", entry_totali);
    write(pipe_sigterm[1], main_to_gestore, strlen(main_to_gestore));
    close(pipe_sigterm[1]);
    
    pthread_join(tgestore[0], NULL);  //Mi blocco qua finche non finisce handler_sigterm
    
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
    
 return 0;
}
