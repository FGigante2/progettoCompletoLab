
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#define _GNU_SOURCE
#include <unistd.h> 
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

// host e port a cui connettersi
#define HOST "127.0.0.1"
#define PORT 53563
#define Max_sequence_length 2048

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

//funzioni readn e writen per leggere o scrivere nel socket
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
size_t writen(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nwritten;
 
   nleft = n;
   while (nleft > 0) {
     if((nwritten = write(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount written so far */
     } else if (nwritten == 0) break; 
     nleft -= nwritten;
     ptr   += nwritten;
   }
   return(n - nleft); /* return >= 0 */
}

void *gestionefile(void *v){

    char *nomefile = (char *)v;
    int sequenze_inviate = 0;

    FILE *f = fopen(nomefile , "r");
     if(f == NULL){
        printf("Errore nell'apertura del file %s \n, codice errore %d", nomefile, errno);
        exit(1);
    }

    //------------------------------------CREAZIONE SOCKET-----------------------------
    int inet_socket;
    inet_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(inet_socket < 0){
        perror("Errore nella creazione del socket");
        exit(1);
    };

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr(HOST);

    int stato_connessione = connect(inet_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    if(stato_connessione == -1){
        perror("C'\u00e8 stato un errore nella connessione con il socket");
        exit(1);
    }
    //---------------------------------FINE CREAZIONE SOCKET-----------------------------

    //mando il carattere "B" che specifica il tipo di connessione
    char tipo_connessione = 'B';
    writen(inet_socket, &tipo_connessione, 1);
   
    //leggo le linee del file
    char* linea = NULL;
    size_t size = 0;
        while(getline(&linea , &size, f) != -1){

        //controllo che la linea non superi la lunghezza massima
        assert(strlen(linea) <= Max_sequence_length);

        // mandiamo la lunghezza della prossima linea
        short dim = strlen(linea);
        short tmp = htons(dim);
        writen(inet_socket, &tmp, 2);

        //mandiamo la linea al server
        char buffer2[dim];
        strcpy(buffer2 , linea);
        writen(inet_socket, &buffer2, dim);
        sequenze_inviate++;  
        }
    free(linea); 
    fclose(f);
   
    //il thread ha finito di mandare tutte le linee del file

    //invio la sequenza di lunghezza 0 (terminazione)
    char sequenza_terminazione[] = "";
    short dim2 = 0;
    writen(inet_socket, &dim2, 2); //mando la lunghezza della linea
    writen(inet_socket, sequenza_terminazione, 1); //mando la stringa di lunghezza 0
    sequenze_inviate++;

    //il server mi manda il numero di sequenze che ha ricevuto
    int sequenze_ricevute_bytes;
    readn(inet_socket , &sequenze_ricevute_bytes , 4);
    int sequenze_ricevute = ntohl(sequenze_ricevute_bytes);
    assert(sequenze_ricevute == sequenze_inviate);
    
    close(inet_socket);  
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){

    if(argc == 1){
        perror("Errore: client1 deve ricevere almeno un file da linea di comando");
        exit(1);
    }

    //creo tanti thread quanti sono i file passati da linea di comando
    //ogni thread crea una connessione di tipo B e invia tutte le linee del file al server
    
    pthread_t id[argc - 1];
    for(int i = 0 ; i < argc - 1 ; i++){
        pthread_create(&id[i], NULL, gestionefile, argv[i+1] );
    }

    for(int i= 0; i < argc - 1; i++){
    pthread_join(id[i],NULL);
    }
    
    return 0;
}
