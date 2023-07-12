#include "funzioni.h"

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
	//Controlla se la entry e nella tabella
	ENTRY *r = hsearch(*e, FIND);
	if(r == NULL){
		//Aumento il numero totale di entry
		*(entry_totali)+=1;
		
		//Inserisco la entry
		r = hsearch(*e, ENTER);
		
		//aggiorno la linked list
		coppia *c = (coppia *)e->data;
		c->next = *(testa_lista_entry);
		*(testa_lista_entry) = e;
			
	}
	else{
		//elemento già presente, incremento di uno il valore associato alla chiave
		coppia *c = (coppia *)r->data;
	 	c->valore += 1;
	 	distruggi_entry(e);
	}
	
	return NULL;	
}

int conta(char *s){
	//Creo una entry con la stringa presa dal buffer condiviso dai lettori
	ENTRY *e = crea_entry(s);
	
	//Cerco la entry nella tabella
	ENTRY *r = hsearch(*e, FIND);
	
	if(r == NULL){
		// Se non e`presente 
		distruggi_entry_safe(e);
		return 0;
	}
	else {
		//Se e` presente ritorno il valore associato alla stringa
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
    assert(v->readers > 0); 
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
