#if !defined(CONN_H)
#define CONN_H

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "ops.h"
#define MAX_NAME 1000
#define MAX_SIZE 1000

/*
 * \brief: struttura dati che definisce il messaggio scambiato tra client e server
 * \param dimNome: la lunghezza del nome del file
 * \param dimensione: la dimensoine del file
 * \param op_type: il tipo di operazione richiesto al server
 * \param nomeFile: nome del file
 * \param contenutoFile: il contenuto del file
 * \param pid: il processo che ha richiesto l'operazione
 * \param fd_conn: fd su cui avviene la comunicazione
 * \param flag: eventuali flag(alcune operazioni lo richiedono)
 */
typedef struct messaggio{
	int dimNome;
	long dimensione;
	op op_type;
	char nomeFile[MAX_NAME];
	char contenutoFile[MAX_SIZE];
	pid_t pid;
	long fd_con;
	int flag;
} msg;

struct msglista{
	msg info;
	struct  msglista* next;
};
typedef struct msglista ElementoDiMessaggio;

/*
 *\brief: ritorna la lunghezza della lista
 *\param lista:puntatore alla testa della lista
*/
static inline int lunghezzaListaMessaggi(ElementoDiMessaggio* lista){
	int counter=0;
	while(lista!=NULL){
		fprintf(stderr, "lunghezzaListaMessaggi");
		counter++;
		lista=lista->next;
	}
	return counter;
	
}
/*
 * \brief: inizializza la lista a null
 * \param lista:puntatore da inizializzare
*/
static inline void inizializzaListaMessaggi(ElementoDiMessaggio** lista){
	*lista = NULL;
}
/*
 * \brief: elimina il primo elemento della lista
 * \param lista: puntatore alla testa della lista
 * \param eliminato: puntatore per salvare il messaggio eliminato
*/
static inline void popMessage(ElementoDiMessaggio**lista,msg *eliminato){
	ElementoDiMessaggio* aux;
	if(*lista!=NULL){
		aux=*lista;
		*lista=(*lista)->next;
		*eliminato=aux->info;
		
		free(aux);
	}
}
/* \brief:inserisce un elemento in coda
 * \pram lista:puntatore alla testa della lista
 * \param messaggio:elemento da inserire
*/
static inline void pushMessage(ElementoDiMessaggio**lista,msg messaggio){
	ElementoDiMessaggio* ultimo;
	ElementoDiMessaggio* new=malloc(sizeof(ElementoDiMessaggio));
	new->info=messaggio;
	new->next=NULL;
	if(*lista==NULL){
		*lista=new;
	}
	else{
		ultimo=*lista;
		while(ultimo->next!=NULL){
			ultimo=ultimo->next;
		}
		ultimo->next=new;
	}
}
/*
 * \brief: dealloca la lista
 * \param lista:puntatore alla testa della lista
*/
static inline void liberaListaMessaggi(ElementoDiMessaggio* lista){
	while(lista!=NULL){
		ElementoDiMessaggio* cor=lista;
		lista=lista->next;
		free(cor);
	}
}
/*
 *\brief:stampa la lista dei messaggio
*/
static inline void stampaMessage(ElementoDiMessaggio*lista){
	while(lista!=NULL){
		printf("%s\n",(lista->info).nomeFile);
		lista=lista->next;	
	}
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
    return 1;
}
#endif

