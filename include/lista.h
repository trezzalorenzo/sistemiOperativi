#define OPEN 0
#define CLOSE 1
#define LOCKED 0
#define UNLOCKED 1
#define MAXLENGHTNAME 200
#define MAXCONTENT 1000
#if !defined(LISTA_H)
#define LISTA_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ops.h"

/* \brief: definizione del tipo di dato che descrive lo stato di un file
 * \param numeroDiUtilizzi: numero di volte che il file è stato usato, parametro utile per la politica LFU
 * \param contenuto: contenuto del file
 * \param isOpen: 0 se il file è stato aperto, 1 altimenti
 * \param dimensioneFile: dimensione del file
 * \param isLocked: 0 se il file è lockato, 1 altimenti
 * \param whoLock: id del processo che ha richiesto la lock
 */

typedef struct file {
	int numeroDiUtilizzi;
	char *nomeFile;
	char *contenuto;
	int isOpen;
	long dimensioneFile;
	int isLocked;
	pid_t whoLock;
} fileServer;

/*
 * \brief: definizione di una lista
 * \param info: elemento corrente di tipo fileServer
 * \param elem: puntatore al prossimo elemento della lista
 */

struct elemento {
	fileServer info;
	struct elemento *next;
};
typedef struct elemento ElementoDiFile;

/*
 * \bief: inizializza la lista facendola puntare a null
 * \param lista: puntatore
 */
static inline void inizializza(ElementoDiFile **lista) {
	*lista = NULL;
}

/*
 * \brief: crea un elemento di tipo file aperto e senza lock
 * \return: 1 se il nome passato è null o la dimensione del nome è maggiore di quella accettata
 * \param nome: titolo del file
 * \param contenuto: contenuto del file
 */
static inline int creaFile(fileServer *file, char *nome) {
	if (nome == NULL || strlen(nome) > MAXLENGHTNAME) {
		return -1;
	}
	(*file).nomeFile = safe_malloc(strlen(nome)*sizeof(char)+1 );
	strncpy((*file).nomeFile, nome, strlen(nome) );
	(*file).isLocked = UNLOCKED;
	//creo il file aperto
	(*file).isOpen = OPEN;
	(*file).numeroDiUtilizzi = 0;
	(*file).contenuto = safe_malloc(MAXCONTENT*sizeof(char));
	(*file).dimensioneFile = strlen((*file).contenuto);
	return 0;
}

/*
 * \brief: stampa a schermo i valori di un determinato file
 * \param file: file da stampare
 */
static inline void stampaFile(fileServer file) {
	printf("il nome del file: ");
	printf("%s\n", file.nomeFile);
	printf("il contenuto: ");
	printf("%s\n", file.contenuto);
	printf("dimensione file: %ld\n", (file.dimensioneFile));
	if (file.isLocked == LOCKED) {
		printf("isLocked: LOCKED\n");
	}
	if (file.isLocked == UNLOCKED) {
		printf("isLocked: UNLOCKED\n");
	}
	if (file.isOpen == OPEN) {
		printf("isOpen: OPENED\n");
	}
	if (file.isOpen == CLOSE) {
		printf("isOpen: CLOSED\n");
	}
	printf("numero utilizzi: %d\n", file.numeroDiUtilizzi);
}

/*
 * \brief: stampa il nome e il numero di utilizi dei file nella lista
 * \param lista: puntatore alla testa della lista
 */
static inline void stampaList(ElementoDiFile *lista) {
	while (lista != NULL) {
		printf("%s:%d,%d->", lista->info.nomeFile, lista->info.numeroDiUtilizzi,lista->info.isLocked);
		lista = lista->next;
	}
	printf("\n");
}

/*
 * \brief: ritorna 0 se l'elemente è presente nella lista,-1 il nome file è null, 1 altrimenti
 * \param lista: puntatore alla testa della lista
 * \param nomeFile: nome del file da cercare
 */
static inline int isPresent(ElementoDiFile *lista, char *nomeFile) {
	ElementoDiFile*aux=lista;
	if (nomeFile == NULL) {
		return -1;
	}
	while (aux != NULL) {
		if (strncmp(aux->info.nomeFile, nomeFile, strlen(nomeFile)) == 0) {
			return 0;
		} else
			aux = aux->next;
	}
	return 1;
}

/*
 * \brief: dato un nome ritorna il puntatore al nodo
 * \param lista: puntatore alla testa
 * \param nomeFile:elemento da cercare
 */
static inline ElementoDiFile* getPuntatoreAFile(ElementoDiFile *lista,char *nomeFile) {

	ElementoDiFile*aux=lista;
	if (nomeFile != NULL) {
		while (aux != NULL) {
			if (strncmp(aux->info.nomeFile, nomeFile, strlen(nomeFile))== 0) {
				//lista->next = NULL;
				return aux;
			} else{

				aux = aux->next;
			}
		}
		return NULL;
	}
	return NULL;
}

/*
 * \brief: inserisce un nuovo elemento in testa
 * \param list: puntato alla testa della lista
 * \param file; elemento da aggiungere alla lista
 */

static inline void inserisciTesta(ElementoDiFile **lista, fileServer file) {
	ElementoDiFile *aux;
	ElementoDiFile *tmp=(*lista);
	aux = safe_malloc(sizeof(ElementoDiFile));
	aux->info = file;
	aux->next = NULL;
	(*lista) = aux;
	(*lista)->next=tmp;
}

/*
 * \brief: elimina un file dalla lista ritornandolo
 * \param lista: puntatore alla testa della lista
 * \param nomeFile: nome del file da cancellare
 * \return: null se i parametri sono errati o l'elemento non è presente
 */
static inline ElementoDiFile* elimina(ElementoDiFile **lista, char *nomeFile) {
	if (*lista == NULL || nomeFile == NULL) {
		return NULL;
	}
	if (strncmp((*lista)->info.nomeFile, nomeFile, strlen(nomeFile)) == 0) {
		ElementoDiFile *tmp = *lista;
		*lista = (*lista)->next;
		tmp->next = NULL;
		return tmp;
	} else {
		ElementoDiFile *aux = *lista;
		ElementoDiFile *cor = (*lista)->next;
		while (cor->next != NULL) {
			if (strncmp(cor->info.nomeFile, nomeFile, strlen(nomeFile)) == 0) {
				aux->next = cor->next;
				cor->next = NULL;
				return cor;
			} else {
				cor = cor->next;
				aux = aux->next;
			}
		}
		if (strncmp(cor->info.nomeFile, nomeFile, strlen(nomeFile)) == 0) {
			aux->next = NULL;
			cor->next = NULL;
			return cor;
		}
	}
	return NULL;
}

/*
 * \brief: elimina ed esefue la free di un nodo della lista
 * \param lista: puntatore alla testa della lista
 * \param nomeFile: nome del file
 *
 */
static inline void eliminaFree(ElementoDiFile **lista, char *nomeFile) {
	if (nomeFile != NULL) {
		ElementoDiFile *tmp = elimina(lista, nomeFile);
		free(tmp->info.contenuto);
		free(tmp->info.nomeFile);
		free(tmp);
	}
}
/*
 * \brief: inserisce un elemento in coda,funzione ausiliaria per aggiorna()
 * \param lista: puntatore alla testa della lista
 * \param file: elemento da inserire in fondo
 */
static inline void inserisciCoda(ElementoDiFile **lista, fileServer file) {
	if (*lista == NULL) {
		ElementoDiFile *l = safe_malloc(sizeof(ElementoDiFile));
		l->info = file;
		l->next = NULL;

		*lista = l;

	} else {
		ElementoDiFile *cor = *lista;
		while (cor->next != NULL) {
			cor = cor->next;
		}
		ElementoDiFile *aux = safe_malloc(sizeof(ElementoDiFile));
		aux->info = file;
		aux->next = NULL;
		cor->next = aux;

	}
}

/*
 * \brief: sposta l'elemento in coda e incrementa il numero di utilizzi
 * \param lista:puntatore alla testa della lista
 * \param nomeFile: nome del file su cui operare
 */
static inline void aggiorna(ElementoDiFile **lista, char *nomeFile) {
	if (nomeFile != NULL && *lista != NULL) {
		ElementoDiFile *tmp = elimina(lista, nomeFile);
		(tmp->info).numeroDiUtilizzi++;
		if(*lista==NULL){
			*lista=tmp;
		}
		else{
			ElementoDiFile* cor=*lista;
			while(cor->next!=NULL){
				cor=cor->next;
			}
			cor->next=tmp;
		}
		//inserisciCoda(lista, tmp->info);
	}
}

/*
 * \brief: se il file è gia stato aperto ritorna 1,se non è presente ritorna -1, 0 altimenti
 * \param lista: puntatore alla testa della lista
 * \param nomeFile: nome del file che bisogna aprire
 */
static inline int openFileLista(ElementoDiFile *lista, char *nomeFile) {
	if (nomeFile != NULL && lista != NULL) {
		if (isPresent(lista, nomeFile) == 0) {
			aggiorna(&lista, nomeFile);
			ElementoDiFile *tmp = getPuntatoreAFile(lista, nomeFile);
			if (tmp->info.isOpen == 1) {
				tmp->info.isOpen = 0;
				return 0;
			} else {
				return 1;
			}
		} else {
			return -1;
		}
	}
	return -1;
}

/*
 * \brief: torna 1 se il file non era aperto,-1 se il file non è presente, 0 altrimenti
 * \param lista:puntatore alla testa della lista
 * \param nomeFile:nome del file da aprire
 */
static inline int closeFile(ElementoDiFile *lista, char *nomeFile) {
	if (lista != NULL && nomeFile != NULL) {
		if (isPresent(lista, nomeFile) == 0) {
			aggiorna(&lista, nomeFile);
			ElementoDiFile *tmp = getPuntatoreAFile(lista, nomeFile);
			if (tmp->info.isOpen == 0) {
				tmp->info.isOpen = 1;
				return 0;
			} else {
				return 1;
			}
		} else {
			return -1;
		}
	}
	return -1;
}
/*
 * \brief: torna 1 se il file era già lockato,-1 se il file non esiste,0 altrimenti
 * \param lista:puntatore alla testa della lista
 * \param file: puntatore al file da bloccare
 * \param id: identificativo del client che effettua l'operazione
 */

static inline int lockFile(ElementoDiFile **lista,fileServer* file, pid_t id) {

	//controllo i parametri
	if (lista == NULL) {
		return -1;
	}
	//se il file è presente
	if (isPresent(*lista, file->nomeFile) == 0) {
		//aggiorno il numero di utilizzi
		aggiorna(lista, file->nomeFile);
		//se il file è stato gia lockato
		if (file->isLocked == LOCKED) {
			return 1;
		} else {
			file->isLocked = LOCKED;
			file->isOpen = OPEN;
			file->whoLock = id;
			return 0;
		}
	} else {
		return -1;
	}
}

/*
 * \brief: torna 1 se il file non aveva lock,-1 se non esiste,0 altrimenti
 * \param lista:puntatore alla testa della lista
 * \param nomeFile:puntatore al file a cui levare la lock
 */
static inline int unlockFile(ElementoDiFile **lista, fileServer* file) {
	if (*lista != NULL) {
		if (isPresent(*lista, file->nomeFile) == 0) {
			aggiorna(lista, file->nomeFile);
			if (file->isLocked == LOCKED) {
				file->isLocked = UNLOCKED;
				return 0;
			} else {
				return 1;
			}

		} else {
			return -1;
		}
	}
	return -1;
}

/*
 * \brief: dealloca lo spazio occupato dalla lista
 * \param lista: puntatore alla testa della lista
*/
static inline void deallocaLista(ElementoDiFile**lista){
	while(*lista!=NULL){
		ElementoDiFile*aux=*lista;
		*lista=(*lista)->next;
		free(aux->info.contenuto);
		free(aux->info.nomeFile);
		free(aux);
	}
}

/*
 * \brief ritorna 0 in caso di successo,-1 in caso di errore nei parametri,1 se il file non era stato aperto
 * \param lista:puntatore alla testa della lista
 * \param nomeFile:nome del file su cui operare
 * \param toAppen:stringa da aggiungere al file
 */
static inline int appendToFile(ElementoDiFile **lista, fileServer* file,char *toAppend, pid_t id) {
	if(((file->dimensioneFile)+strlen(toAppend))>=MAXCONTENT) return -1;
	if (*lista != NULL && toAppend != NULL) {
		if (isPresent(*lista, file->nomeFile) == 0) {
			aggiorna(lista, file->nomeFile);				
			if (file->isOpen == OPEN ) {
				//strncpy(file->contenuto,toAppend,strlen(toAppend));
				strncat(file->contenuto,toAppend,strlen(toAppend));
				file->dimensioneFile = strlen(file->contenuto);
				return 0;
			} else {
				return 1;
			}
		} else {
			return -1;
		}
	} else {
		return -1;
	}
}

#endif /*LISTA_H*/
