#if !defined(HASHLFU_H)
#define HASHLFU_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "ops.h"
#include "lista.h"


typedef struct Hashtable{
	//array di elementi
	ElementoDiFile** key;
	int dimensione;
	int fileInChache;
} hash;

size_t funzioneHash(char* chiave,int modulo);
void hashInizializza(hash* tab,int size);
void HashInsert(hash* table,fileServer file );
void HashPrint(hash* table);
int isPresentInHash(hash* table,char *nomeFile);
ElementoDiFile* getFileFromHash(hash* table,char* nomeFile);
int openFile(hash *table, char *pathname, int flags, pid_t idProcesso);
int appendInHash();
ElementoDiFile* deleteLFU(hash* table);
#endif

