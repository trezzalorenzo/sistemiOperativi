#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/select.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

#include "ops.h"
#include "conn.h"
#include "HashLFU.h"

#define MAX_BUF 2048
#define CONFIG_FL "./config.txt"
#define TAB_SIZE 10
#define MAXBACKLOG 10000

/*-----VARIABILI DI CONFIGURAZIONE-----*/

//numeri di threadworkers
long NUM_THREAD_WORKERS;

//byte letti dal file di configurazione
long MAX_MEMORY_TOT;
pthread_mutex_t memt_lock = PTHREAD_MUTEX_INITIALIZER;

//file letti dal file di configurazione
long MAX_NUM_FILES_TOT;
pthread_mutex_t filet_lock = PTHREAD_MUTEX_INITIALIZER;

//per memorizzare il nome della socket
char* SOCKET_NAME = NULL;

//per memorizzare il nome del file di log
char* LOG_NAME = NULL;

// elimino int daEliminare=0;

//riferimento al file di log
FILE* log_file = NULL;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

/*-----VARIABILI AUSILIARIE-----*/

//byte attualmente memorizzati nel server
long MAX_MEMORY_MB;
pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

//file attualmente memorizzati nel server
long MAX_NUM_FILES;
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

//memorizza il massimo numero di file salvati durante l'esecuzione del server
long MAX_FILES_MEMORIZED = 0;
pthread_mutex_t max_file_lock = PTHREAD_MUTEX_INITIALIZER;

//memorizza il massimo numero di byte letti durante l'esecuzione del server
long MAX_MEMORY_EVER = 0;
pthread_mutex_t max_mem_lock = PTHREAD_MUTEX_INITIALIZER;

//memorizza i file espulsi durante l'esecuzione del server
long EXPELLED_COUNT = 0;
pthread_mutex_t exp_lock = PTHREAD_MUTEX_INITIALIZER;

/*-----STRUTTURE DATI-----*/

//tabella hash per memorizzare i file
hash cacheMemory;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

//coda per le richieste dei client
ElementoDiMessaggio* client_requests;
pthread_mutex_t cli_req = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_list = PTHREAD_COND_INITIALIZER;

//lista per memorizzare i file espulsi
ElementoDiMessaggio* expelled_files;
pthread_mutex_t expfiles_lock = PTHREAD_MUTEX_INITIALIZER;

//array per memorizzare i riferimenti ai thread worker
pthread_t* thread_ids = NULL;

/*-----VARIABILI UTILI PER LA SELECT-----*/

//insieme di file descriptor attivi
fd_set set; 
pthread_mutex_t set_lock = PTHREAD_MUTEX_INITIALIZER;

//file descriptor massimo
int fd_max = 0;
pthread_mutex_t max_lock = PTHREAD_MUTEX_INITIALIZER;

/*-----VARIABILI PER LA GESTIONE DEI SEGNALI-----*/

volatile sig_atomic_t fast_stop = 0;
volatile sig_atomic_t slow_stop = 0;
volatile sig_atomic_t signal_stop = 0;
pthread_t signal_handler;
sigset_t signal_mask;


/*-----FUNZIONI AUSILIARIE-----*/

/**
 * \brief: ritorna il valore massimo tra gli fd in uso
 * \param: insieme di file descriptor
 * \param: file descriptor massimo
 */
int updateMax(fd_set set, int fdmax)
{
    for(int i = (fdmax-1); i >= 0; i--){
    	if (FD_ISSET(i, &set)) return i;
    	}
    	assert(1 == 0);
    	
    	return -1;
}

/**
 * \brief: invia una risposta e eventuali file espulsia al client
 * \param connfd: il file descriptor dove avviene la comunicazione
 * \param op_type: la risposta da inviare
 * \param ok_write: usato per identificare se la precedente operazione era una scrittura,per poter inviare eventuali file espulsi
 */

int report_ops(long connfd, op op_type, int ok_write)
{
	/*debug*/ //fprintf(stderr,"risposta:%d\n",op_type);
	LOCK(&log_lock);
	fprintf(log_file, "\nCLIENT %ld risultato  %d\n", connfd, op_type);
	fflush(log_file);
	UNLOCK(&log_lock);

	//invio il risultato al client
	if (writen(connfd, &op_type, sizeof(op)) <= 0)
	{
		perror("ERROR: write report ops");
		return -1;
	}
	
	//se c'è stata una operazione di write precedentemente
	if(ok_write)
	{
		//chiamo la lock sulla lista dei file da mandare
		LOCK(&expfiles_lock);
		int dimensione=0;
		ElementoDiMessaggio*aux=expelled_files;
		if(expelled_files!=NULL){
			while(expelled_files!=NULL){
				dimensione++;
				expelled_files=expelled_files->next;
			}
		}
		expelled_files=aux;

		//invia il numero di file che verranno spediti

		if (writen(connfd, &dimensione, sizeof(int)) <= 0)
		{ 
			perror("ERROR: write sending expelled files"); 
			return -1;
		}
		
		//finche la lista messaggi espulsi contiene elementi
		while (dimensione > 0)
		{	
			msg* cur_msg = safe_malloc(sizeof(msg));
			popMessage(&expelled_files, cur_msg);	
			if (writen(connfd, cur_msg, sizeof(msg)) <= 0)
			{
				perror("ERROR: write sending expelled files"); 
				return -1;
			}
			dimensione--;
			free(cur_msg);
		}
		UNLOCK(&expfiles_lock);
	}
	return 0;
}

/**
 * \brief: apre un file esistente o crea un file nuovo pronto per essere scritto
 * \param connfd: il file descriptor su cui avviene la comunicazione
 * \param name: il nome del file su cui operare
 * \param flag: il flag dell'operazione
 * \param pid: l'id del processo che richiede l'operazione
 */
int open_file_svr(long connfd, char* name, int flag, pid_t pid)
{


	int chiave=funzioneHash(name,TAB_SIZE);
	
	if(isPresentInHash(&cacheMemory,name)==0) //se il file è gia presente in hash
	{
		ElementoDiFile* current = getFileFromHash(&cacheMemory, name);
		fileServer message=current->info;
		if(flag == 1) //ho provato a creare un file già presente
		{
			return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
		}

		//se il file è già lockato 
		if(message.isLocked == 0 || (message.isLocked == 1 && message.whoLock == pid))
		{	
			// se il fie è chiuso lo apro
			if(message.isOpen == 1)
			{ 
				//apro il file
				openFileLista(cacheMemory.key[chiave],message.nomeFile);

				LOCK(&log_lock);
				fprintf(log_file, "OpenFile %s\nFlag %d\n", name, flag);
				fflush(log_file);
				UNLOCK(&log_lock);
				//se il flag O_LOCK è stato impostao ma il file è gia aperto

				if(flag == 2 && message.isLocked != 1)
				{
					LOCK(&log_lock);
					fprintf(log_file, "OpenLock %s\n", name);
					fflush(log_file);
					UNLOCK(&log_lock);
					lockFile(&cacheMemory.key[chiave],&message,pid);
				} 
	
			} 
			else 
			{
				return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
			} // file gia esistente e aperto
		} 
		else
		{
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		} //file lockato da un altro processo		
	}
	else
	{
		if(!flag)
		{
			//tentativo di creare senza O_LOCK
			return report_ops(connfd, SRV_NOK, 0); 
		} 
		else
		{	
			
			//pronto per la scrittura
			LOCK(&log_lock);
			fprintf(log_file, "Create file %s\n", name);
			fflush(log_file);
			UNLOCK(&log_lock);
			fileServer tmp;

			/*(tmp).nomeFile = safe_malloc(strlen(name)*sizeof(char)+1 );
			strncpy((tmp).nomeFile, name, strlen(name) );
			(tmp).isLocked = UNLOCKED;
			//creo il file aperto
			(tmp).isOpen = OPEN;
			(tmp).numeroDiUtilizzi = 0;
			(tmp).contenuto = safe_malloc(MAXCONTENT*sizeof(char));
			(tmp).dimensioneFile = strlen((tmp).contenuto);
			*/
			creaFile(&tmp,name);
			
			//O_CREATE && O_LOCK
			if(flag == 3)
			{
				LOCK(&log_lock);
				fprintf(log_file, "OpenLock %s\n", name);
				fflush(log_file);
				UNLOCK(&log_lock);
				HashInsert(&cacheMemory, tmp);
				ElementoDiFile* tmp=getFileFromHash(&cacheMemory,name);
				lockFile(&cacheMemory.key[chiave],&tmp->info,pid);
				return report_ops(connfd, SRV_READY_FOR_WRITE, 0); 
			}
			
			
			HashInsert(&cacheMemory, tmp);
			
			report_ops(connfd, SRV_READY_FOR_WRITE, 0);
			return 0;
		}
	}
	return report_ops(connfd, SRV_OK, 0);
}

/*
 * \brief: determina e esegue l'operazione richiesta
 * \param connfd: il file descriptor su cui avviene la comunicazione
 * \param info: messaggio che contiene tutte le info utili per l'esecuzione
 */

int cmd(int connfd, msg* info) 
{
	/*debug*/ //fprintf(stderr,"operazione richiesta:%d | ",info->op_type);
	switch(info->op_type) 
	{
		case OPEN_FILE:
		{
    		LOCK(&cache_lock);
			int ret = open_file_svr(connfd, info->nomeFile, info->flag, info->pid);
			UNLOCK(&cache_lock);
			return ret;
			break;
		}
		case CLOSE_FILE: 
		{
			LOCK(&cache_lock);
			int tmp=closeFile(cacheMemory.key[funzioneHash(info->nomeFile,TAB_SIZE)], info->nomeFile);
			UNLOCK(&cache_lock);
			
			LOCK(&log_lock);
			fprintf(log_file, "CloseFile %s\n", info->nomeFile);
			fflush(log_file);
			UNLOCK(&log_lock);

			report_ops(connfd, SRV_FILE_LOCKED, 0);
			return tmp;
			break;
		}
		case READ_FILE:	
		{
			int chiave=funzioneHash(info->nomeFile,TAB_SIZE);
			//char* tmp_buf=NULL;
			size_t tmp_size = 0;
			LOCK(&cache_lock);
			ElementoDiFile* aux=getPuntatoreAFile(cacheMemory.key[chiave],info->nomeFile);

			lockFile(&cacheMemory.key[chiave],&aux->info,info->pid);

			if(aux!=NULL && aux->info.whoLock==info->pid && aux->info.isLocked==LOCKED && aux->info.isOpen==OPEN){
			tmp_size=aux->info.dimensioneFile;
			//fprintf(stderr, "\n---size:%ld\n", tmp_size);
			//char *tmp_buf=safe_malloc(sizeof(tmp_size)+1);
			//memcpy(tmp_buf, aux->info.contenuto, tmp_size);
			//strncpy(tmp_buf,aux->info.contenuto,tmp_size);
			aggiorna(&cacheMemory.key[chiave],info->nomeFile);
			UNLOCK(&cache_lock);
			//invia il codice per segnalare il successo
			LOCK(&log_lock);
			fprintf(log_file, "Read\tfile letto %s\ngrande %ld\n ", aux->info.nomeFile, aux->info.dimensioneFile);
			fflush(log_file);
			UNLOCK(&log_lock);
		
			if(report_ops(connfd, SRV_OK, 0) == -1) return -1;
			//invia la dimensione del contenuto
			if(writen(connfd, &aux->info.dimensioneFile, sizeof(long)) <= 0){
				perror("errore read file");
				return -1;
			}
			//invia il contenuto

			int prova;
			if((prova=writen(connfd, aux->info.contenuto, tmp_size)) <= 0){
				perror("ERROR: write read file");
				return -1;
			}
			//backup
			//free(tmp_buf);
			}
			else
			{
			//backup
			//free(tmp_buf);
			return report_ops(connfd, -1, 0);
			}
			break;
		}

		case READ_FILE_N: 
		{
			LOCK(&cache_lock);
			
			int i=0;
			int fileDaLeggere=info->flag;
			//finche non ho letto tutti i file oppure non ho letto i file richiesti
			while(i<cacheMemory.dimensione-1 && fileDaLeggere!=0){
				if(cacheMemory.key[i]!=NULL){
					ElementoDiFile* aux=cacheMemory.key[i];
					while(aux!=NULL){
						msg* tem_msg=safe_malloc(sizeof(msg));
						tem_msg->dimNome=strlen(aux->info.nomeFile);
						tem_msg->dimensione=strlen(aux->info.contenuto);
						tem_msg->flag=0;
						strncpy(tem_msg->nomeFile,aux->info.nomeFile,tem_msg->dimNome);
						strncpy(tem_msg->contenutoFile,aux->info.contenuto,tem_msg->dimensione);
						if(writen(connfd,tem_msg,sizeof(msg))<=0){
							perror("errore write file");
							return -1;
						}
						LOCK(&log_lock);
						fprintf(log_file, "Read\tfile letto %s\ngrande %ld\n ", aux->info.nomeFile, aux->info.dimensioneFile);
						fflush(log_file);
						UNLOCK(&log_lock);
						aux=aux->next;
						fileDaLeggere--;
						free(tem_msg);
					}
				}
				i++;
			}
			msg* terminatore=safe_malloc(sizeof(msg));
			terminatore->flag=-1;
			
			if(writen(connfd, terminatore, sizeof(msg)) <= 0){
				perror("errore read file");
				return -1;
			}
			UNLOCK(&cache_lock);
			free(terminatore);
			return report_ops(connfd,SRV_OK,0);
			break;
		}

		case WRITE_FILE:
		{

			int chiave=funzioneHash(info->nomeFile,TAB_SIZE);

			//int eliminato=0;
			LOCK(&cache_lock);
			LOCK(&memt_lock);
			LOCK(&mem_lock);
			LOCK(&file_lock);
			if(isPresentInHash(&cacheMemory,info->nomeFile)!=0){
				report_ops(connfd,SRV_FILE_NOT_FOUND,0);
			}else{
			
				ElementoDiFile* aux=getFileFromHash(&cacheMemory,info->nomeFile);

				//se è stato lockato da altri al di fuori di me
				if(aux->info.isLocked==LOCKED && aux->info.whoLock!=info->pid){
					report_ops(connfd,SRV_FILE_LOCKED,0);
				}
				//il file è già stato scritto
				else if(aux->info.dimensioneFile>0){
					report_ops(connfd,SRV_FILE_ALREADY_PRESENT,0);
				}
				//se il file è chiuso
				else if(aux->info.isOpen==CLOSE){
					report_ops(connfd,SRV_FILE_CLOSED,0);
				}
				//se tento la scrittura di un file piu grande della memoria massima gestibile
				else if(MAX_MEMORY_TOT<info->dimensione){
					return report_ops(connfd, SRV_MEM_FULL, 0);
				}

				else if((MAX_NUM_FILES==0) || (MAX_MEMORY_MB<info->dimensione)){
					int tempo=aux->info.numeroDiUtilizzi;
					aux->info.numeroDiUtilizzi=100;	
						while(MAX_MEMORY_MB<info->dimensione || MAX_NUM_FILES==0){
							
							ElementoDiFile* espuls=deleteLFU(&cacheMemory);
							MAX_MEMORY_MB=MAX_MEMORY_MB+espuls->info.dimensioneFile;
							MAX_NUM_FILES++;
							fileServer tmp=espuls->info;
							//da tipo file a tipo messaggio
							msg* espulso=safe_malloc(sizeof(msg));
							espulso->dimNome=strlen(tmp.nomeFile);
							strncpy(espulso->nomeFile,tmp.nomeFile,espulso->dimNome);
							espulso->dimensione=strlen(tmp.contenuto);
							strncpy(espulso->contenutoFile,tmp.contenuto,espulso->dimensione);
							LOCK(&exp_lock);
							EXPELLED_COUNT++;
							UNLOCK(&exp_lock);
							LOCK(&expfiles_lock);

							pushMessage(&expelled_files,*espulso);
							LOCK(&log_lock);
							fprintf(log_file, "Capacity Miss\tfile espulso %s\t memoria liberata %ld\n", espulso->nomeFile,espulso->dimensione);
							fflush(log_file);
							UNLOCK(&log_lock);	
							UNLOCK(&expfiles_lock);
						}
					aux->info.numeroDiUtilizzi=tempo;
				}

				//backup else{
					//ho spazio di memorizzazione
					MAX_MEMORY_MB=MAX_MEMORY_MB-info->dimensione;
					MAX_NUM_FILES--;
					if((MAX_NUM_FILES_TOT - MAX_NUM_FILES) > MAX_FILES_MEMORIZED){
						MAX_FILES_MEMORIZED = MAX_NUM_FILES_TOT - MAX_NUM_FILES;
					}

					else if((MAX_MEMORY_TOT - MAX_MEMORY_MB) > MAX_MEMORY_EVER){	
						MAX_MEMORY_EVER = MAX_MEMORY_TOT - MAX_MEMORY_MB;
					}
					LOCK(&log_lock);	 
					fprintf(log_file, "WriteFile\tfile scritto %s\nmemoria occupata %ld\n ", info->nomeFile, info->dimensione);
					fflush(log_file);
					UNLOCK(&log_lock);
					//ElementoDiFile*auxo=getFileFromHash(&cacheMemory,info->nomeFile);
					//fprintf(stderr,"appendToFile:%d\n",appendToFile(cacheMemory.key[chiave], &aux->info,info->contenutoFile, info->pid));
					appendToFile(&cacheMemory.key[chiave], &aux->info,info->contenutoFile, info->pid);
					
					report_ops(connfd,SRV_FILE_EXPELLED,1);
			/*backup}*/}

			
			UNLOCK(&memt_lock);
			UNLOCK(&file_lock);
			UNLOCK(&mem_lock);
			UNLOCK(&cache_lock);

			return 0;
			break;
		}
		case APPEND_FILE:
		{
			LOCK(&cache_lock);
			if(isPresentInHash(&cacheMemory,info->nomeFile)!=0){
				report_ops(connfd,SRV_FILE_NOT_FOUND,0);
			}else{
			//	ElementoDiFile* aux=getFileFromHash(&cacheMemory,info->nomeFile);
				if(MAX_MEMORY_MB<info->dimensione){
					while(MAX_MEMORY_MB<info->dimensione){
						ElementoDiFile* tmp= deleteLFU(&cacheMemory);
						MAX_MEMORY_MB=MAX_MEMORY_MB-tmp->info.dimensioneFile;
					}
					
				}

			return report_ops(connfd,SRV_FILE_EXPELLED,1);
			}
			break;
		}
		case REMOVE_FILE:
    	{	

    		LOCK(&cache_lock);
    		int chiave=funzioneHash(info->nomeFile,TAB_SIZE);
			if(isPresentInHash(&cacheMemory,info->nomeFile)!=0){
				report_ops(connfd,SRV_FILE_NOT_FOUND,0);
			}else{
				ElementoDiFile* aux=getFileFromHash(&cacheMemory,info->nomeFile);
				LOCK(&log_lock);
				fprintf(log_file, "Remove\tfile rimosso %s\n ", info->nomeFile);
				fflush(log_file);				
				UNLOCK(&log_lock);

				LOCK(&mem_lock);
				MAX_MEMORY_MB +=aux->info.dimensioneFile;
				UNLOCK(&mem_lock);
				LOCK(&file_lock);
				MAX_NUM_FILES++;
				UNLOCK(&file_lock);
				eliminaFree(&cacheMemory.key[chiave],info->nomeFile);
				report_ops(connfd,SRV_OK,0);
			}
			UNLOCK(&cache_lock);
			return 0;
			break;
		}
		case LOCK_FILE:
		{
			
			LOCK(&cache_lock);
			
			int chiave=funzioneHash(info->nomeFile,TAB_SIZE);


			ElementoDiFile* aux=getPuntatoreAFile(cacheMemory.key[chiave],info->nomeFile);

			/*aux->info.isLocked = LOCKED;
			aux->info.isOpen = OPEN;
			aux->info.whoLock = info->pid;			
*/
			fileServer t=aux->info;
			int returner=lockFile(&cacheMemory.key[chiave],&t,info->pid);
//			int returner=0;
			
			if(returner==0){
			
				report_ops(connfd,SRV_OK,0);
				LOCK(&log_lock);
				fprintf(log_file, "lockFile %s\n", info->nomeFile);
				fflush(log_file);
				UNLOCK(&log_lock);
			}
			UNLOCK(&cache_lock);
			if(returner==1){
				report_ops(connfd,SRV_FILE_LOCKED,0);
			}
			if(returner==-1){
				report_ops(connfd,SRV_FILE_NOT_FOUND,0);
			}


			return returner;
			break;
		}
		case UNLOCK_FILE:
		{
			
			LOCK(&cache_lock);
			
			int chiave=funzioneHash(info->nomeFile,TAB_SIZE);
			ElementoDiFile* aux=getPuntatoreAFile(cacheMemory.key[chiave],info->nomeFile);
			int returner=unlockFile(&cacheMemory.key[chiave],&aux->info);
			if(returner==0){
				report_ops(connfd,SRV_OK,0);
				LOCK(&log_lock);
				fprintf(log_file, "UnlockFile %s\n", info->nomeFile);
				fflush(log_file);
				UNLOCK(&log_lock);
			}
			UNLOCK(&cache_lock);
			if(returner==1){
				report_ops(connfd,SRV_FILE_LOCKED,0);
			}
			if(returner==-1){
				report_ops(connfd,SRV_FILE_NOT_FOUND,0);
			}
			return returner;
			break;
		}
		case END_COMMUNICATION:
		{

			return report_ops(connfd, SRV_OK, 0);
			return 0;
			break;
		}
		default: 
		{
			fprintf(stderr, "Command not found SRV\n");
			return -1;
		}
	}

	return 0;
}

/**
 * \brief: task dei thread worker
 */
void* getMSG(void* arg)
{
	
	LOCK(&log_lock);
	fprintf(log_file, "\nStartedThread %ld\n", pthread_self());
	fflush(log_file);
	UNLOCK(&log_lock);

	//finchè non ricevo il segnale di terminare immediatamente
	while(!fast_stop)
	{
		//se è stato ricevuto il segnale di stop lento e tutte le richieste sono state servite,esco dal while
		if(slow_stop && client_requests == NULL)
		{
			LOCK(&log_lock);
			fprintf(log_file, "SIGHUP Finished serving all requests\n");
			fflush(log_file);
			UNLOCK(&log_lock);
			break;
		}	

		msg* operation = safe_malloc(sizeof(msg));

		//in attesa di un messaggio nella coda
		LOCK(&cli_req);
		WAIT(&wait_list, &cli_req);

		//se non è stato richiesto un fast stop e la coda delle richieste non è vuota allora
		if(!fast_stop && client_requests!= NULL)
		{
			//effettuo una pop per ottenere una richiesta
			popMessage(&client_requests,operation);
			UNLOCK(&cli_req);

			LOCK(&log_lock);
			fprintf(log_file, "\nTHREAD %ld\nServing request from client %ld\n", pthread_self(), operation->fd_con);
			fflush(log_file);
			UNLOCK(&log_lock);

			//eseguo l'operazione richiesta
			cmd(operation->fd_con, operation);
			//se il client non ha richiesta la chiusura della connessione, viene reinserito nel fdset
			if(operation->op_type != 20)
			{
				LOCK(&set_lock);
				FD_SET(operation->fd_con, &set);
				UNLOCK(&set_lock);
			}
			else
			{
				//l'operazione è una close connection
				LOCK(&log_lock);
				fprintf(log_file, "Closing connection\nClient %ld", operation->fd_con);
				fflush(log_file);
				UNLOCK(&log_lock);
				close(operation->fd_con);
				//aggiorno fd massimo
				if (operation->fd_con == fd_max) fd_max = updateMax(set, fd_max);
			}
		}
		else
		{
			UNLOCK(&cli_req);
		}	

	/*backup*/	free(operation);
	}
	
	fflush(stdout);
	pthread_exit(NULL);
}

/**
 * \brief: task del thread che gestisce i segnali
 */
void* signalhandler(void* arg)
{
	
	int signal = -1;

	//il task termina se ho ricevuto un segnale
	while(!signal_stop)
	{
		//wait aspettando un segnale
		sigwait(&signal_mask, &signal);
		//aggiorno il log file

		LOCK(&log_lock);
		fprintf(log_file, "\nricevuto un segnale\n");
		fflush(log_file);
		UNLOCK(&log_lock);

		//segno che ho ricevuto un segnale per uscire dal loop
		signal_stop = 1;

		//se il segnale è una richiesta di fast stop
		if (signal == SIGINT || signal == SIGQUIT)
        {
        	//aggiorno il file di log
			LOCK(&log_lock);
			fprintf(log_file, "SIGINT or SIGQUIT\n");
			fflush(log_file);
			UNLOCK(&log_lock);

			//setto a true la variabile volatile che si occupa di segnalare ai thread workers
			//di fermarsi e al thread main di non accettare piu nuove connessioni
			fast_stop = 1;

			//risveglia tutti i thread bloccati sulla variabile condizione associata alla coda richieste
			pthread_cond_broadcast(&wait_list);

			//chiusura delle connessioni e svuotamento della lista
			ElementoDiMessaggio* cor=client_requests;
			while(cor!=NULL){
				close(cor->info.fd_con);
				ElementoDiMessaggio*tmp=cor;
				cor=cor->next;
				free(tmp);
			}
			//chiusura dei thread
			for (int i=0; i<NUM_THREAD_WORKERS; i++)
			{
				pthread_join(thread_ids[i], NULL);
			}
		}

		//slow stop
		if(signal == SIGHUP) 
		{
			LOCK(&log_lock);
			fprintf(log_file, "SIGHUP\n");
			fflush(log_file);
			UNLOCK(&log_lock);

			slow_stop = 1;
			
			pthread_cond_broadcast(&wait_list);

			//attesa della terminazione dei thread worker
			for(int i = 0; i < NUM_THREAD_WORKERS; i++)
			{
				pthread_join(thread_ids[i], NULL);
			}
		}
	}
	pthread_exit(NULL);
}

/**
 * \brief: parsing del file di configurazione
 */
void configParsing() 
{
	FILE *config_input = NULL;
	
	char* buffer = safe_malloc(MAX_BUF);

	if ((config_input = fopen(CONFIG_FL, "r")) == NULL) 
	{
		perror("ERROR: opening config file");
		fclose(config_input);
		exit(EXIT_FAILURE);
	} 

	int count = 0;

	while(fgets(buffer, MAX_BUF, config_input) != NULL) 
	{

		char* comment;

		if ((comment = strchr(buffer, '#')) != NULL) 
		{ 
			continue;
		}

		char* semicolon;
		CHECK_EQ_EXIT((semicolon = strchr(buffer, ';')), NULL, "formattazione errata:atteso ; alla fine di ogni riga\n");
				
		*semicolon = '\0';

		char* equals;
		CHECK_EQ_EXIT((equals = strchr(buffer, '=')), NULL, "formattazione errata atteso formato: <name> = <value>;\n");

		++equals;

		switch (count)
		{

			case 0:
				if(isNumber(equals,  &NUM_THREAD_WORKERS) != 0) {
					fprintf(stderr, "formattazione errata:la prima riga deve essere il numero di thread workers\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}			
			case 1:
				if(isNumber(equals, &MAX_MEMORY_MB) != 0) {
					fprintf(stderr, "formattazione errata:la seconda riga deve essere il numero di bytes memorizzabili\n");
					free(buffer);
					exit(EXIT_FAILURE);
				} else {MAX_MEMORY_TOT = MAX_MEMORY_MB;}
			case 2:
				if(isNumber(equals, &MAX_NUM_FILES) != 0) {
					fprintf(stderr, "formattazione errata:la terza riga deve essere il massimo numero di file memorizzabili\n");
					free(buffer);
					exit(EXIT_FAILURE);
				} else {MAX_NUM_FILES_TOT = MAX_NUM_FILES;}
			case 3:
				while((*equals) != '\0' && isspace(*equals)) ++equals;
				if((*equals) == '\0') {
					fprintf(stderr, "formattazione errata:la quarta riga deve essere il nome del sockt\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
				strncpy(SOCKET_NAME, equals, MAX_BUF);
			case 4:
				while((*equals) != '\0' && isspace(*equals)) ++equals;
				if((*equals) == '\0') 
				{
					fprintf(stderr, "formattazione errata:la quinta riga deve essere il nome del file di log\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
				strncpy(LOG_NAME, equals, MAX_BUF);
		}

		count++;
		
	}

	fclose(config_input);
	free(buffer);
	
}

/**
 * \brief: crea il file di log in modalità scrittura salvando il riferimento nella variabile
 *			globale log_file
 */
void log_create()
{
	
	char* log_path = safe_malloc(7 + strlen(LOG_NAME));
	sprintf(log_path, "./%s.txt", LOG_NAME);
	
	log_file = fopen(log_path, "w");

	if(log_file != NULL) 
	{
		fprintf(log_file, "LOG FILE\n");
		fflush(log_file);
	}
	
	free(log_path);

	return;
}

int main (int argc, char* argv[]) 
{
	//maschero i segnali
	int err = 0;
	//memorizzare la vecchia maschera
    sigset_t oldmask;
    SYSCALL_EXIT("sigemptyset", err, sigemptyset(&signal_mask), "ERROR: sigemptyset", "");
    SYSCALL_EXIT("sigaddset", err, sigaddset(&signal_mask, SIGHUP), "ERROR: sigaddset", "");
    SYSCALL_EXIT("sigaddset", err, sigaddset(&signal_mask, SIGINT), "ERROR: sigaddset", "");
    SYSCALL_EXIT("sigaddset", err, sigaddset(&signal_mask, SIGQUIT), "ERROR: sigaddset", "");

    //applico la maschera
    SYSCALL_EXIT("pthread_sigmask", err, pthread_sigmask(SIG_SETMASK, &signal_mask, &oldmask), "ERROR: pthread_sigmask", "");

    //attivo il thread che si occupa della gestione dei segnali
    SYSCALL_EXIT("pthread_create", err, pthread_create(&signal_handler, NULL, &signalhandler, NULL), "ERROR: pthread_create", "");  

	// alloca spazio per il socket name e il nome del file di log
	SOCKET_NAME = safe_malloc(MAX_BUF);
	LOG_NAME = safe_malloc(MAX_BUF);

	// parsing del file di configurazione
	configParsing();

	// creazione del file di log
	log_create(LOG_NAME);

	fprintf(log_file, "\nSERVER CREATO\nnumero di thread workers %ld\nmassima memoria usabile %ld\nmemoria ancora disponibile %ld\nmassimo numero di file %ld\nSocket Name %s\nLog file name %s\n\n", 
		NUM_THREAD_WORKERS, MAX_MEMORY_MB, MAX_MEMORY_TOT, MAX_NUM_FILES, SOCKET_NAME, LOG_NAME);
	fflush(log_file);

	//inizializza cache
	hashInizializza(&cacheMemory, TAB_SIZE);

	fprintf(log_file, "hash table inizializzata con successo\n");
	fflush(log_file);

	//inizializza liste
	//client_requests = safe_malloc(sizeof(ElementoDiMessaggio));
	inizializzaListaMessaggi(&client_requests);

	//expelled_files = safe_malloc(sizeof(ElementoDiMessaggio));
	inizializzaListaMessaggi(&expelled_files);

	//inizializzazione socket name per accettare connessioni
	unlink(SOCKET_NAME);

	int fd_skt; //file descriptor socket
	int fd_sel; //indice per verificare i risultati della select
	int fd_con; //I/O socket con client

	fd_set rdset; //set di file descriptor in attesa

	// creazione ed attivazione thread

	CHECK_EQ_EXIT((thread_ids = (pthread_t*) calloc(NUM_THREAD_WORKERS, sizeof(pthread_t))), NULL, "errore calloc threads");

	int res = 0;

	for(int i = 0; i < NUM_THREAD_WORKERS; i++) 
	{
		if((res = pthread_create(&(thread_ids[i]), NULL, &getMSG, NULL) != 0)) 
		{ 
			perror("ERROR: threads init");
			exit(EXIT_FAILURE);
		}
	}
	
	//inizializzazione socket
	SYSCALL_EXIT("socket", fd_skt, socket(AF_UNIX, SOCK_STREAM, 0), "errore socket", "");
	fprintf(log_file, "\n connesso al socket %s\n", SOCKET_NAME);

	//socket address
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME)+1);

 	//preparazione socket
	int result;
    SYSCALL_EXIT("bind", result, bind(fd_skt, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "errore bind", "");
    SYSCALL_EXIT("listen", result, listen(fd_skt, MAXBACKLOG), "errore listen", "");
    
	//aggiornamento fd massimo
	if(fd_skt > fd_max) fd_max = fd_skt;

    //preparazione dei set utili per la select
    FD_ZERO(&set);
    FD_ZERO(&rdset);
    FD_SET(fd_skt, &set);

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1;

	int print_count = 0;
	
	while(!slow_stop && !fast_stop)  
	{      
		rdset = set; //salvataggio del set in uno temporaneo
		//+1 perchè ho bisogno del numero di file descriptors attivi e non dell'indice massimo
		if (select(fd_max + 1, &rdset, NULL, NULL, &timeout) == -1)
		{
		    perror(" errore select");
			return EXIT_FAILURE;
		} 
		else { 
			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++)
			{
				//accetto una nuova connessione
			    if (FD_ISSET(fd_sel, &rdset))
				{
					if (fd_sel == fd_skt)
					{ 
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "errore accept", "");	
						FD_SET(fd_con, &set);  // aggiungo il file descriptor al set
						
						// aggiornamento del massimo
						if(fd_con > fd_max) fd_max = fd_con;  
						if(print_count == 0)
						{
							fprintf(log_file, "\nconnesso con il client %d\n", fd_con);
							fflush(log_file);
							print_count++;
						}
						continue;
					}
					fd_con = fd_sel;

					msg* request=safe_malloc(sizeof(msg));
								
					if (readn(fd_con, request, sizeof(msg))<=0) 
					{
						perror("errore lettura messaggio");
						return EXIT_FAILURE;
					}
					
					//eliminazione del file descriptor dal set,sarà aggiunto in seguito per altre
					//eventuali richieste
					FD_CLR(fd_con, &set);

					request->fd_con = fd_con;
					//aggiungo alla lista richieste e segnalo che non è piu vuota
					LOCK(&cli_req);
					pushMessage(&client_requests,*request);
					free(request);
					pthread_cond_signal(&wait_list);
					UNLOCK(&cli_req);
 					
										
		   		}
			}
		}
    }
    fprintf(stderr,"MAX_MEMORY_MB: %ld\nMAX_MEMORY_TOT: %ld\nMAX_NUM_FILES:%ld\nMAX_NUM_FILES_TOT:%ld\n",MAX_MEMORY_MB,MAX_MEMORY_TOT,MAX_NUM_FILES,MAX_NUM_FILES_TOT);
    fprintf(stderr,"MAX_FILES_MEMORIZED:%ld MAX_MEMORY_EVER:%ld EXPELLED_COUNT:%ld\n",MAX_FILES_MEMORIZED, MAX_MEMORY_EVER, EXPELLED_COUNT);
	//join del signal handler
	pthread_join(signal_handler, NULL);
	HashPrint(&cacheMemory);
	LOCK(&log_lock);
	fprintf(log_file, "\nEXPELLED_COUNT %ld\n", EXPELLED_COUNT);
	fprintf(log_file, "MAX_MEMORY_EVER %ld\n", MAX_MEMORY_EVER);
	fprintf(log_file, "MAX_FILES_MEMORIZED %ld\n", MAX_FILES_MEMORIZED);
	fflush(log_file);
	UNLOCK(&log_lock);	

	close(fd_skt);
	unlink(SOCKET_NAME);

	fclose(log_file);

    free(SOCKET_NAME);
	free(LOG_NAME);


	while(client_requests!=NULL){
		ElementoDiMessaggio* aux=client_requests;
		client_requests=client_requests->next;

		free(aux);
	}
	while(expelled_files!=NULL){
		ElementoDiMessaggio* aux = expelled_files;
		expelled_files=expelled_files->next;

		free(aux);
	}
	free(thread_ids);

	for(size_t i=0;i< cacheMemory.dimensione;i++){
		deallocaLista(&(cacheMemory.key[i]));
	}
	free(cacheMemory.key);
	pthread_exit(NULL);
}
