#!/usr/bin/python3
import socket, struct, threading, argparse, concurrent.futures, logging, os,stat,time,subprocess,signal

lock = threading.Lock()
Max_sequence_length = 2048
PORT = 53563
HOST = "127.0.0.1"

#creo il logger dedicato al server
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
handler = logging.FileHandler('server.log')
logger.addHandler(handler)

def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 1024))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks
 
def gestisci_client(conn,addr,fd,fd2):
  with conn:
    #print(f"Nuova connessione da {addr}")
    #devo ricevere il tipo di comunicazione
    tipo = recv_all(conn,1).decode()
    if(tipo == "A"):
        gestione_a(conn,fd)
    elif(tipo == "B"):
        gestione_b(conn,fd2)
 
def gestione_b(conn,fd2):

  bytes_totali = 0
  sequenze_ricevute = 0
  
  while(1):
    #ricevo la lunghezza della linea
    data = recv_all(conn,2)
    lunghezza = struct.unpack('!h',data)[0]
      
    if(lunghezza == 0):
      logger.info(f"Connessione di tipo B | Byte scritti nella pipe caposc : {bytes_totali}")
      data3 = recv_all(conn,1)
      sequenze_ricevute += 1
      conn.sendall(struct.pack("!i" , sequenze_ricevute))
      break
      
    #ricevo la linea
    data2 = recv_all(conn,lunghezza)
    
    lock.acquire()
    os.write(fd2, data)
    os.write(fd2,data2)
    lock.release()
    
    
    #invio la lunghezza alla pipe (formato short)
    #-------------------------------
    bytes_totali+=2
    bytes_totali += len(data2)
    sequenze_ricevute += 1  
    #print(data2)

def gestione_a(conn,fd):

  bytes_totali = 0
  
  #ricevo la lunghezza della linea	
  data = recv_all(conn,2)
  lunghezza = struct.unpack('!h',data)[0]
  
  #ricevo la linea
  data1 = recv_all(conn,lunghezza)
  
  #invio atomicamente lunghezza della sequenza e sequenza --------------------------
  lock.acquire()
  os.write(fd,data)
  os.write(fd,data1)
  lock.release()
  
  #aggiungo i byte totali inviati da scrivere sul file di log ----------------------
  bytes_totali += len(data1)
  bytes_totali += 2
 
  
  logger.info(f"Connessione di tipo A | Bytes scritti nella pipe capolet : {bytes_totali}")
 
def main(host=HOST,port=PORT): 
  ADDR = (host,port)
    #creo capolet e caposc se non esistono nella directory corrente
  current_path = os.getcwd()
  caposc_path = current_path +  '/caposc'
  capolet_path = current_path + '/capolet'

  if(not os.path.exists(caposc_path)):
    os.mkfifo(caposc_path)

  if(not os.path.exists(capolet_path)):
    os.mkfifo(capolet_path)

  #apro la pipe verso i programmi C
  fd = os.open('capolet', os.O_WRONLY)
  fd2 = os.open('caposc', os.O_WRONLY)
  
  with socket.socket(socket.AF_INET , socket.SOCK_STREAM) as server : 
    #collego il socket all'indirizzo
    try:
        print("[WAITING] : il server \u00e8 in attesa di connessioni")
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(ADDR)
        server.listen()
        with concurrent.futures.ThreadPoolExecutor(max_workers= args.max_t) as executor:
          while True:
            conn, addr = server.accept()    
            thread = threading.Thread(target = gestisci_client , args = (conn, addr,fd,fd2)) 
            thread.start()
                                                                                        
    except KeyboardInterrupt: #quando ricevo un'interruzione chiudo il server
      pass

    print("[SHUTDOWN DEL SERVER]")
    os.unlink(caposc_path)
    os.unlink(capolet_path)
    server.shutdown(socket.SHUT_RDWR)
    os.kill(p.pid,signal.SIGTERM)
    

if __name__ == '__main__':
    #Controllo dei valori passati da riga di comando
  parser = argparse.ArgumentParser(prog = 'server.py',
      description = "---------------------------------------\n"
                    "Server che riceve sequenze dai client e le scrive su pipe condivisa da capolet e caposc",
      formatter_class=argparse.RawTextHelpFormatter,
      epilog='---------------------------------------')
  parser.add_argument('max_t', type=int,                                      help='Un intero positivo che d\u00e0 al server il massimo numero di thread che pu\u00f2 avviare contemporaneamente')
  parser.add_argument('-r',  type=int, default= 3,                            help= 'Numero thread lettori (default = 3)')
  parser.add_argument('-w',  type=int, default=3,                              help= 'Numero thread scrittori (default = 3)')
  parser.add_argument('-v',  action='store_true',                             help='Utilizzo -v per eseguire "archivio.c" con valgrind')
  
  args = parser.parse_args()

  assert args.max_t > 0
  if(args.v == True):
    p = subprocess.Popen(["valgrind","--leak-check=full",
                          "--show-leak-kinds=all" ,
                          "--log-file=valgrind-%p.log",
                          "--track-origins=yes",
                          "./xarchivio", f"{args.r}",f"{args.w}"])
                          
  else:
    p = subprocess.Popen(["./archiviopatch", f"{args.r}",f"{args.w}"])
    
  main()
