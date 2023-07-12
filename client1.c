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

// host e port a cui connettersi
#define HOST "127.0.0.1"
#define PORT 53563
#define Max_sequence_length 2048

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

int main(int argc, char *argv[]){

    if(argc != 2){
        printf("Client1 deve ricevere un file da linea di comando, usare : 'client1 nomefile' ");
        exit(1);
    }
    
    //apro il file
    FILE *f1 = fopen(argv[1], "r");
    if(f1 == NULL){
        printf("Errore nell'apertura del file %s \n, codice errore dsfasdfa :%d", argv[1], errno);
        exit(1);
    }

    //per ogni riga del file creiamo una nuova connessione, uso getline

    //leggo le linee del file
    char* linea = NULL;
    size_t size = 0;
        while(getline(&linea , &size, f1) != -1){
        
    //creo una nuova connessione, mi connetto al socket e mando il carattere "A"
    //creo il socket
    int inet_socket;
    inet_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(inet_socket < 0){
        perror("Errore nella creazione del socket");
        exit(1);
    };

    //specifico l'indirizzo e la porta del socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr(HOST);

    //mi connetto al socket 
    int stato_connessione = connect(inet_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    if(stato_connessione == -1){
        perror("C'\u00e8 stato un errore nella connessione con il socket");
        exit(1);
    }

    //mando il carattere "A" che specifica il tipo di connessione
    char tipo_connessione = 'A';
    writen(inet_socket, &tipo_connessione, 1);

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
    close(inet_socket);
    }

    fclose(f1);
    free(linea);

    return 0;
}
