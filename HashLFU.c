#include<stdio.h>
#include<stdlib.h>
#include "lista.h"
#include "ops.h"
#define O_LOCK 0
#define O_CREATE 1

/*
 * \brief: implementazione di una hashtable
 * \param key: array di puntatori ad elementi di tipo ElementoDiFile
 * \param dimensione: dimensione della tabella
 * \param fileInChache: elementi salvati nella struttura dati
 */
typedef struct Hashtable{
	//array di elementi
	ElementoDiFile** key;
	int dimensione;
	int fileInChache;
} hash;
/*
 * \brief:inizializza una hashmap allocando spazio per le liste di trabocco
 * \param tab:tabella da inizializzare
 * \param size:numero di chiave della hashtable
 */
void hashInizializza(hash* tab,int size){
	tab->key=safe_malloc(sizeof(ElementoDiFile)*size);
	for(int i=0;i<size;i++){
		inizializza(&(tab->key[i]));
	}
	tab->dimensione=size;
	tab->fileInChache=0;
}
/*
 * \brief: funzione hash che data una stringa ne calcola il valore numerico ed esegure il modulo
 * \param chiave: stringa su cui calcolare il valore hash
 * \param modulo: valore del modulo
 */
size_t funzioneHash(char* chiave,int modulo){
	int valoreChiave=0;
	for(int i=0;i<strlen(chiave);i++){
		valoreChiave=valoreChiave+chiave[i];
	}
	return valoreChiave%modulo;
}
/*
 * \brief: inserisce un elemento nella hashtable
 * \param table: tabella su cui lavorare
 * \param file: elemento da inserire
 */
void HashInsert(hash* table,fileServer file ){
	
	int index=funzioneHash(file.nomeFile, table->dimensione);
	
	inserisciTesta(&(table->key[index]), file);
	table->fileInChache++;
}

/*
 * \brief:stampa a schermo la tabella
 * \param table:tabella da stampare
 */
void HashPrint(hash* table){
	for(size_t i=0;i< table->dimensione;i++){
		printf("%ld:",i);
		stampaList(table->key[i]);
	}
}

/*
 * \brief:ritorna 0 se l'elemento è presente nella hashtable,1 altrimenti
 * \param table: riferimento alla hashmap
 * \param nomeFile: file su cui lavorare
 */
int isPresentInHash(hash* table,char *nomeFile){

	int chiave=funzioneHash(nomeFile, table->dimensione);


	return isPresent(table->key[chiave], nomeFile);
}

ElementoDiFile* getFileFromHash(hash* table,char* nomeFile){
	if(isPresentInHash(table, nomeFile)==0){
	int chiave=funzioneHash(nomeFile, table->dimensione);
	return getPuntatoreAFile((table->key)[chiave], nomeFile);
	}
	return NULL;
}

/*
 * \brief: elimina un file dalla hash,secondo la politica LFU, ritornandolo
 * \param table: riferimento alla hashmap
*/
ElementoDiFile* deleteLFU(hash* table){
	//determino il minimo
	int min=10000;
	int bucket=-1;
	for(int i=0;i<table->dimensione;i++){
		if(table->key[i]!=NULL){
			if(table->key[i]->info.numeroDiUtilizzi<min){
				min=table->key[i]->info.numeroDiUtilizzi;
				bucket=i;
			}
		}
	}	
	
	ElementoDiFile*aux=table->key[bucket];
	ElementoDiFile*returner=safe_malloc(sizeof(ElementoDiFile));
	returner->info.numeroDiUtilizzi=aux->info.numeroDiUtilizzi;
	returner->info.dimensioneFile=aux->info.dimensioneFile;
	returner->info.nomeFile=safe_malloc(strlen(aux->info.nomeFile));
	
	returner->info.contenuto=safe_malloc(strlen(aux->info.contenuto));
	strncpy(returner->info.nomeFile,aux->info.nomeFile,strlen(aux->info.nomeFile));
	strncpy(returner->info.contenuto,aux->info.contenuto,strlen(aux->info.contenuto));
	returner->next=NULL;
	
	eliminaFree(&table->key[bucket],aux->info.nomeFile);
	
	return returner;

}


int openFile(hash *table, char *pathname, int flags, pid_t idProcesso) {
	fileServer file;
	int key=funzioneHash(pathname, table->dimensione);
	//se il file già esiste

	//il è stato impostato il flag o_create allora
	if (flags == O_CREATE) {
		//se l'elemento è presente in cache torno con errore

		if (isPresentInHash(table, pathname) == 0) {
			return -1;
		}
		//se l'elemento non è presente in cache allora crea un file
		else {
			//viene creato un file e inserito in hash
			creaFile(&file, pathname);
			HashInsert(table, file);

			return 0;
		}
	}
	if(flags==O_LOCK){
		//se l'elemento è presente in cache
		if(isPresentInHash(table, pathname) == 0){
			//invoco la lock sul file
			ElementoDiFile* aux= getFileFromHash(table, pathname);
			return lockFile(&(table->key[key]), &aux->info, idProcesso);
		}else{
			return -1;
		}
	}
	if(flags==(O_CREATE | O_CREATE)){
		//se è presente in cache
		if(isPresentInHash(table, pathname)==0){
			//invoco la lock sul file
			ElementoDiFile* aux= getFileFromHash(table, pathname);
			return lockFile(&(table->key[key]), &aux->info, idProcesso);
		}else{
			//il file viene creato e lockato
			creaFile(&file,pathname);
			HashInsert(table, file);
			ElementoDiFile* aux= getFileFromHash(table, pathname);
			return lockFile(&(table->key[key]), &aux->info, idProcesso);
		}
	}
	return -1;
}
int appendInHash(){
	return 0;
}
