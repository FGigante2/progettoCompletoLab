
# Progetto Completo Laboratorio II - Filippo Gigante

Il progetto consiste nella realizzazione di un archivio che memorizza stringhe dentro una tabella hash. Le stringhe vengono mandate dai client al server con una comunicazione via socket. Il server a sua volta manda le stringhe processate all'archivio, che ha il compito di inserirle in una tabella hash o contare il valore associato ad esse.

## **SERVER**

- **Modulo Argparse** : tramite questo modulo il server riceve 4 possibili parametri da linea di comando, 3 facoltativi (<code>-r , -w , -v</code>) e uno obbligatorio (<code>max_t</code>). <code>action=store_true</code> del parametro -v indica che può assumere valore True o False, se lo specifico da linea di comando, il parametro ha valore True e lancio archivio con valgrind.

- **ThreadPoolExecutor** : Questa classe, associata al modulo concurrent.futures, mi permette di usare il valore <code>max_t</code> passato da linea di comando come numero massimo di thread che il server può avviare contemporaneamente.

- **Lock** : La lock è usata nella gestione delle connessioni A e B. Viene utilizzata per inviare atomicamente lunghezza della sequenza e la sequenza stessa in capolet o caposc, dato che quest'ultime sono pipe condivise da tutti i thread del server.

- **recv_all** : la funzione <code>recv_all</code> garantisce che il server riceva esattamente i bytes che specifico nella sua chiamata. In questo modo sono sicuro che il dato ricevuto sia corretto e non abbia subito frammentazione durante l'invio nella connessione network level.

- **Parte Progetto Completo** : Il server, dopo che ha ricevuto la sequenza di lunghezza 0 da connessioni di tipo B, invia al client2 il numero di sequenze ricevute. Questo è memorizzato in una variabile locale <code>sequenze_ricevute</code> che è incrementata di 1 ogni volta che il server riceve una sequenza da quella connessione. Il client2 termina la connessione quando riceve il valore e risulta corretto.

## **CLIENT**

*Entrambi i client sono stati scritti in C per avere omogeneità tra i due codici.*

- **Client2** : Ogni thread del client ha una variabile locale <code>sequenze_inviate</code> per verificare se le sequenze ricevute dal server corrispondono a quelle inviate dal client. Il client2 utilizza le funzioni <code>readn</code> e <code>writen</code> per garantire l'invio di tutti i byte e per evitare frammentazione dei valori durante l'invio dei dati network level.

## **ARCHIVIO**

*Il file <code>archivio.c</code> contiene : main, capi, lettori e scrittori e il thread gestore dei segnali con gli handler associati, le funzioni dedicate alla tabella hash e ai readers/writers e tutte le strutture dati sono contenute nel file <code>funzioni.c</code> e nel corrispondente header <code>funzioni.h</code>*

- **Gestione delle stringhe** : la funzione <code>crea_entry()</code> utilizza come chiave il puntatore della stringa che gli scrittori hanno trovato sul buffer condiviso, in questo modo evito di creare un ulteriore duplicato della stringa.

- **strtok_r** : Per la tokenizzazione delle sequenze è necessaria la versione _r di strtok perché capolet e caposc possono trovarsi a tokenizzare le stringhe contemporaneamente. Usando strtok, capolet e caposc utilizzerebbero lo stesso buffer statico per la tokenizzazione e questo porterebbe ad errori.

- **distruggi_entry_safe** : Questa funzione dealloca tutta la memoria associata alla entry tranne la chiave, in questo modo dopo che i thread lettori chiamano la funzione <code>conta</code> possono scrivere la stringa sul file lettori.log. Dopo la scrittura sul file libero la memoria della della stringa.

- **dealloca_hash** : La funzione prende in input la testa della lista e dealloca tutti gli elementi. Questa funzione viene invocata alla fine del main (solo dopo aver ricevuto SIGTERM) e nell'handler di SIGUSR1. Quando chiamo questa funzione non ci sono altri thread che hanno accesso alla tabella, né in scrittura né in lettura.

- **Gestione dei segnali** : Il main blocca tutti i segnali, quindi tutti i thread che vengono creati ereditano questo blocco. L'unico thread che riceve i segnali è il thread gestore, che si mette in attesa con la sigwait() su una sigmask che contiene tutti i segnali.

- **SIGINT** : L'handler di sigint utilizza esclusivamente funzioni async-signal-safe e converte manualmente l'intero (<code>entry_totali</code>) in stringa. Successivamente stampa la stringa su stderr , uso <code>write(2, ... , ...)</code> nel formato "Entries : n ".

- **SIGUSR1** : L'handler di SIGUSR1 ottiene l'accesso in scrittura alla tabella hash utilizzando la funzione <code>write_lock()</code>. Successivamente, dealloca tutti gli elementi presenti attraverso <code>dealloca_hash()</code> e <code>hdestroy()</code>. Infine rilascia l'accesso in scrittura utilizzando <code>write_unlock()</code>. L'handler accede a dati che sono condivisi da tutto il programma.Tuttavia una volta ottenuto l'accesso in scrittura, si può essere certi di essere l'unico a poter accedere a tali dati. Questo approccio garantisce che la gestione del segnale avvenga in modo sicuro, evitando problemi di race condition ed evitando conflitti.

- **SIGTERM** : L'handler blocca tutti i segnali (Impedendo quindi che si mettano in coda sulla sigwait) con <code>pthread_sigmask(SIG_BLOCK, &mask, NULL);</code>. Per rendere l'handler il più veloce possibile ho implementato una comunicazione via pipe senza nome tra main e handler di SIGTERM. Dopo che il main fa la join su tutti i thread tranne tgestore (faccio anche la join di lettori e scrittori per garantire che il numero di entry distinte sia corretto), converte l'intero <code>entry_totali</code> in stringa attraverso la funzione <code>sprintf()</code> e lo scrive sulla pipe senza nome <code>pipe_sigterm</code>, successivamente il main si mette in attesa del tgestore con una join. A questo punto l'handler di SIGTERM riceve direttamente il numero di entry in formato stringa dalla pipe e lo stampa su stdout, infine fa terminare il thread gestore dei segnali. A questo punto il main prosegue e dealloca tutta la tabella hash (Anche gli elementi interni) e il programma termina. 
