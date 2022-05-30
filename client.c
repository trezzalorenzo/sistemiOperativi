#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <time.h>
#include "ops.h"
#include "conn.h"

#include "api.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


#define SOCK_NAME "./storage_sock"
int sockfd;

//variabili globali da settare opportunamente
int debug=0;
char* SOCKNAME=NULL;
long sleeptime=0;

//definizione lista per memorizzare i comandi in input da linea di comando
typedef struct ARGS{
	int opt;
	char* args;
} cmd_args;

struct argomento{
	cmd_args info;
	struct argomento *next;
};
typedef struct argomento ElementoDiArgomenti;

//dove gli argomenti da linea di comando sono salvati
ElementoDiArgomenti* listaArgomenti=NULL;

 //cartelle
 char* fileEspulsi;
 char* fileSalvati;


void inserisciCodaArg(ElementoDiArgomenti**lista,int opt,char* args){
	ElementoDiArgomenti *new=safe_malloc(sizeof(ElementoDiArgomenti));
	new->info.opt=opt;
	new->info.args=safe_malloc(sizeof(char)*strlen(args));
	strncpy((new->info.args),args,strlen(args));
	new->next=NULL;
	if(*lista==NULL){
		*lista=new;
	}else{
		ElementoDiArgomenti* cor=*lista;
		while(cor->next!=NULL){
			cor=cor->next;
		}
		cor->next=new;
	}

}

void printCommand(){
	fprintf(stderr,"----------------------\n");
	ElementoDiArgomenti* cor= listaArgomenti;
	while(cor!=NULL){
		fprintf(stderr,"operazione:%d con argomento:%s\n",cor->info.opt,cor->info.args);
		cor=cor->next;
	}
	fprintf(stderr,"fine argomenti\n\n");
}
void printHelper()
{
	printf("-h -> stampa l'helper\n");
	printf("-f filename -> specifica il nome file\n");
	printf("-w dirname[,n = 0]-> invia n file al server,se n=0 vengono inviati tutti i nella cartella\n");
	printf("-W file1[,file2] -> lista di file da inviare al server, sperati dalla virgola\n");
	printf("-D dirname -> cartella in cui salvare i file espulsi,usabile solo se è stato usato l'opzione -w o -W se non specificata i file vengono persi;\n");
	printf("-r file1[,file2]-> lista di file da leggere dal server,separati da una virgola\n");
	printf("-R [n = 0] -> legge n file dal server,se n non è specificato o è zero allora vengono letti tutti i file\n");
	printf("-d dirname -> cartella in cui memorizzare i file letti,se non specificata,i file vengono persi\n");
	printf("-t -> tempo in millisecondi tra le richieste,se non specificata è 0\n");
	printf("-c file1[, file2]-> lista di file da rimuovere dal server\n");
	printf("-l file1[,file2] -> lista di file su cui chiamare la lock\n");
	printf("-u file1[,file2] -> lista di file su cui rilasciare la lock\n");
	printf("-p -> attiva le stampe di debug\n");
}

int add_current_folder(char** pathname, char* name)
{
	char* folder = safe_malloc(strlen(name) + 1);
	*pathname = safe_malloc(4 + strlen(name));

	strncpy(*pathname, "./", 3);
	strncpy(folder, name, strlen(name) + 1);
	strncat(*pathname, folder, strlen(folder));
	free(folder);
	return 0;
}

char* cwd() 
{
	char* buf = safe_malloc(MAX_SIZE*sizeof(char));

	if(getcwd(buf, MAX_SIZE) == NULL) 
	{
		perror("cwd during getcwd");
		free(buf);
		return NULL; 
	}
	
	return buf;
}
int verificaComandi(){
	int isW=0;
	int isR=0;
	int isd=0;
	int isD=0;
	ElementoDiArgomenti* cor=listaArgomenti;
	char* dir1=NULL;
	char* dir2=NULL;
	while(cor!=NULL){
		//se ho incontrato -w,-W
		if(cor->info.opt==1 || cor->info.opt==2){
			isW=1;
		}
		//se ho incontrato -r o -R
		if(cor->info.opt==4 || cor->info.opt==5){
			isR=1;
		}
		//se ho incontrato -D
		if(cor->info.opt==3){
			isD=1;
			dir1=safe_malloc(sizeof(char)*strlen(cor->info.args));
			strncpy(dir1,cor->info.args,strlen(cor->info.args));
		}
		//se ho incontrato -d
		if(cor->info.opt==6){
			isd=1;
			dir2=safe_malloc(sizeof(char)*strlen(cor->info.args));
			strncpy(dir2,cor->info.args,strlen(cor->info.args));
		}
		cor=cor->next;
	}
	
	//verifico le propedeuticità
	if(!isW && isD){
		fprintf(stderr,"l'opzione -D deve essere usata con -w o -W\n");
		free(dir1);
		free(dir2);
		return -1;
	}
	if(!isR && isd)
	{
		fprintf(stderr,"l'opzione -d deve essere usata con -r o -R\n");
		free(dir1);
		free(dir2);
		return -1;
	}

	if(isD){
		char* dirbuf1;
		if(dir1[0]=='.' && dir1[1]=='/'){
			dir1 +=2;
			dirbuf1=cwd();
			dirbuf1=strncat(dirbuf1, "/", 2);
			dirbuf1=strncat(dirbuf1, dir1, strlen(dir1));
		}
		fileEspulsi=safe_malloc(strlen(dirbuf1));
		strncpy(fileEspulsi,dirbuf1,strlen(dirbuf1));
		if(debug) fprintf(stderr,"i file espulsi saranno salvati in %s\n",fileEspulsi);
	}
	if(isd){
		char* dirbuf2;
		if(dir2[0]=='.' && dir2[1]=='/'){
			dir2 +=2;
			dirbuf2=cwd();
			dirbuf2=strncat(dirbuf2, "/", 2);
			dirbuf2=strncat(dirbuf2, dir2, strlen(dir2));
		}
		fileSalvati=safe_malloc(strlen(dirbuf2));
		strncpy(fileSalvati,dirbuf2,strlen(dirbuf2));
		if(debug) fprintf(stderr,"i file espulsi saranno salvati in %s\n",fileSalvati);		
	}
	return 0;

}
int isdot (const char dir[])
{
	int l = strlen(dir);
	if((l > 0 && dir[l - 1] == '.')) return 1;
	return 0;
}



int write_from_dir_find (const char* dir, long* n) 
{

	//enetering directory
	if(chdir(dir) == -1) 
	{ 
		//error while entering directory, finished reading all directories 
		return 0; 
	}

	DIR *d;

	//opening directory
	if((d = opendir(".")) == NULL) 
	{ 
		//error while entering directory, finished reading all directories
		return -1;
	} 
	else {

		struct dirent *file;

		// reading all files
		// setting errno before readdir, to distinguish between two case in which it returns NULL
		// 1. an error occurred, set errno
		// 2. end of directory, no more files to read
		while((errno = 0, file = readdir(d)) != NULL) {

			struct stat statb;

			//getting stats
			if(stat(file->d_name, &statb) == -1) 
			{ 
				print_error("Error during stat of %s\n", file->d_name);
				return -1;
			}

			//if a directory is found...
			if(S_ISDIR(statb.st_mode)) {
				//...and it's not the same directory or parent directory...
				if(!isdot(file->d_name)) {
					//...call recursively
					if(write_from_dir_find(file->d_name, n) != 0) {
						// when I'm finished, go back to previous directory
						if (chdir("..") == -1) 
						{
							print_error("Impossible to got back to parent directory\n");
							return -1;
						}
					}
				}
			// the file is not a directory
			} else {
					if(*n == 0) return 1;
					char* buf = cwd();
					buf = strncat(buf, "/", 2);
					buf = strncat(buf, file->d_name, strlen(file->d_name));
					if (buf == NULL) return -1;

					//printf("n: %ld", *n);
					if(debug) fprintf(stderr,"Writing file:%s\n", buf);

					//open the file, if success	
					int ritornoOpen;	
					if((ritornoOpen=openFile(buf, 1)) == 12) 
					{ 
						if(lockFile(buf)==SRV_OK){
							int prova;
							if((prova=writeFile(buf, fileEspulsi)) == 0) 
							{
								*n = *n - 1;
							} 
						}
						unlockFile(buf);
						

						//else return 1;
					}
					
					//else return 1;

					free(buf);
			}
		}
		// error, print
		if (errno != 0) perror("readdir");
		closedir(d);
	}

	return 1;
}

int execute_command(){
	while(listaArgomenti!=NULL){
		
		//fprintf(stderr, "\nESECUZIONE di %d\n",listaArgomenti->info.opt );
		usleep(sleeptime*1000);
		switch(listaArgomenti->info.opt){
			case 0:
			{
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);
				break;
			}
			case 1: //-w
			{
				int i=0;
				long n=-1;
				char* dirname;
				char* token;
				char* buf;
				token=strtok(listaArgomenti->info.args,",");
				while(token!=NULL){
					if(i==0){
						if(token[0]=='.' && token[1]=='/'){
							token+=2;
							buf=cwd();
							buf=strncat(buf,"/",2);
							buf=strncat(buf,token,strlen(token));							
						}
						dirname=safe_malloc(strlen(buf));
						strncpy(dirname,buf,strlen(buf));
						i++;
					}
					else{

						char* item=strtok(token,"=");
						item=strtok(NULL,"=");
						isNumber(item,&n);	
					} 
					token=strtok(NULL, ",");
				}
				if(debug){
					if(n>0) fprintf(stdout,"scrittura di %ld file",n);
					else fprintf(stdout,"scrittura di tutti i file");
					fprintf(stdout," dalla cartella %s\n",dirname);
				}
				if(n==0) n=-1;
				char* temporaneo=cwd();
				if(write_from_dir_find(dirname,&n)<0){
					return -1;
				}
				chdir(temporaneo);
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);				
				break;
			}
			case 2: //-W
			{
				
				char*token;
				char*buf;
				token=strtok(listaArgomenti->info.args,",");
				while(token!=NULL){
					if(token[0]=='.' && token[1]=='/'){
						token +=2;
						buf=cwd();
						buf=strncat(buf,"/",2);
						buf=strncat(buf,token,strlen(token));
					}
					else{
						buf=safe_malloc(strlen(token));
						strncpy(buf,token, strlen(token));
					}
					if(debug) fprintf(stdout,"scrittura del file:%s\n",buf);
					if((openFile(buf,1))!=0){
						if(lockFile(buf)==SRV_OK){
							if(writeFile(buf,fileEspulsi)!=0){
								fprintf(stdout,"errore in write lato client");
								return -1;
							}
						}
					}
					unlockFile(buf);
					token=strtok(NULL,",");
				}
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);						
				break;
			}
			case 3: //-D
			{
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);						
				break;
			}
			case 4: //-r
			{

				char*token;
				char*buf;
				void* contenuto=NULL;
				size_t dimensione=0;
				//int returnReadFile=-1;
				token=strtok(listaArgomenti->info.args,",");
				while(token!=NULL){
					int returnReadFile=-1;
					if(token[0]=='.' && token[1]=='/'){
						token +=2;
						buf=cwd();
						buf=strncat(buf,"/",2);
						buf=strncat(buf,token,strlen(token));
					}
					else{
						buf=safe_malloc(strlen(token));
						strncpy(buf,token, strlen(token));
					}
					if(debug) fprintf(stdout,"lettura del file:%s\n",buf);
					
					if(lockFile(buf)==SRV_OK){
						returnReadFile=readFile(buf,&contenuto,&dimensione);
					}
					if(returnReadFile!=-1){
						//salvo il file nella cartella
						if(fileSalvati!=NULL){
							char *p;
							p=strrchr(token,'/');
							++p;
							if((WriteFilefromByte(p,(char*)contenuto,dimensione,fileSalvati))==-1){
								return -1;
							}
						}
					}
					token=strtok(NULL,",");
				}

				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);						
				//creare file nella cartella
				break;				
			}
			case 5: //-R
			{
				//segnalino
				long leg;
				isNumber(listaArgomenti->info.args,&leg);

				readNFiles(leg,fileSalvati);
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);						
				break;
			}
			case 6: //-d
			{
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);						
				break;
			}
			case 7://-l
			{
				char*token;
				char*buf;
				token=strtok(listaArgomenti->info.args,",");
				while(token!=NULL){
					if(token[0]=='.' && token[1]=='/'){
						token +=2;
						buf=cwd();
						buf=strncat(buf,"/",2);
						buf=strncat(buf,token,strlen(token));
					}
					else{
						buf=safe_malloc(strlen(token));
						strncpy(buf,token, strlen(token));
					}
				if(debug) fprintf(stdout,"richiesta di lock sul file:%s\n",buf);
				if(lockFile(buf)!=SRV_OK){
					fprintf(stderr,"errore nell'acquisizione della lock sul file %s\n",buf);
				}
				token=strtok(NULL,",");
				}
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);
				break;
			}

			case 8://-u
			{
				char*token;
				char*buf;
				token=strtok(listaArgomenti->info.args,",");
				while(token!=NULL){
					if(token[0]=='.' && token[1]=='/'){
						token +=2;
						buf=cwd();
						buf=strncat(buf,"/",2);
						buf=strncat(buf,token,strlen(token));
					}
					else{
						buf=safe_malloc(strlen(token));
						strncpy(buf,token, strlen(token));
					}
				if(debug) fprintf(stdout,"richiesta di unlock sul file:%s\n",buf);
				if(unlockFile(buf)!=SRV_OK){
					fprintf(stderr,"errore nel togliere la lock sul file %s\n",buf);
				}
				token=strtok(NULL,",");
				}
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);
				break;
			}
			case 9://-c
			{
				char*token;
				char*buf;
				
				token=strtok(listaArgomenti->info.args,",");
				while(token!=NULL){
				
					if(token[0]=='.' && token[1]=='/'){
						token +=2;
						buf=cwd();
						buf=strncat(buf,"/",2);
						buf=strncat(buf,token,strlen(token));
				

					}
					else{
						buf=safe_malloc(strlen(token));
						strncpy(buf,token, strlen(token));
					}
				if(debug) fprintf(stdout,"richiesta di cancellazione file:%s\n",buf);
				if(removeFile(buf)!=SRV_OK){
					fprintf(stderr,"errore errore nella rimozione del file %s\n",buf);
				}

				token=strtok(NULL,",");
				}
				ElementoDiArgomenti* aux=listaArgomenti;
				listaArgomenti=listaArgomenti->next;
				free(aux);
				break;
			}	

		}

	}
	return 0;
}

int main(int argc,char *argv[]){

	struct timespec abstime;
	int opt;
	listaArgomenti=NULL;
	while((opt=getopt(argc,argv,"hf:w:W:D:r:R:d:t:l:u:c:p"))!=-1){
 		switch(opt){
 			case 'h':{
 				printHelper();
 				exit(EXIT_SUCCESS);
 				break;
 			}
 			case 'f':{

 				if(add_current_folder(&SOCKNAME,optarg)==-1){
 					errno=-1;
 					perror("Errore -f");
 					exit(EXIT_FAILURE);
 				}
 				if((clock_gettime(CLOCK_REALTIME,&abstime))==-1){
 					errno=-1;
 					perror("Errore -f");
 					exit(EXIT_FAILURE); 					
 				}
 				abstime.tv_sec+=2;
 				if(debug) fprintf(stdout,"apertura connessione con il socket: %s\n",SOCKNAME);
 				if((openConnection(SOCKNAME,1000,abstime))==-1){
 					errno=ECONNREFUSED;
 					perror("Errore -f");
 					exit(EXIT_FAILURE);
 				}
 				else if(debug) fprintf(stdout,"connessione aperta con successo\n");
 				break;
 			}
 			case 'w':{
 				
 				inserisciCodaArg(&listaArgomenti,1,optarg);
 				break;
 			}
 			case 'W':{
 				inserisciCodaArg(&listaArgomenti,2,optarg);
 				break;	
 			}
 			case 'D':{
 				inserisciCodaArg(&listaArgomenti,3,optarg);
 				break;
 			}
 			case 'r':{
 				inserisciCodaArg(&listaArgomenti,4,optarg);
 				break;
 			}
 			case 'R':{

 				char*token=optarg;
				token=strchr(optarg,'=');
 				//token=strchr(token,1);
				token++;
				
				long daLeggere;
				if((isNumber(token,&daLeggere))==1){
					printf(" errore parametro -R, %s inserire un numero dopo '='\n", optarg);
					return EXIT_FAILURE;
 				}

 				inserisciCodaArg(&listaArgomenti,5,token);
                break;
 			}
 			case 'd':{
 				inserisciCodaArg(&listaArgomenti,6,optarg);
 				break;
 			}
 			case 't':{
 				if((isNumber(optarg,&sleeptime))==1){
					printf(" errore parametro -t, %s non è un numero\n", optarg);
					return EXIT_FAILURE;
 				}
 				if(debug) fprintf(stdout,"tempo tra le richieste impostato a %ld\n",sleeptime);
 				break;
 			}
 			case 'l':{
 				inserisciCodaArg(&listaArgomenti,7,optarg);
 				break;
 			}
 			case 'u':{
 				inserisciCodaArg(&listaArgomenti,8,optarg);
 				break;
 			}
 			case 'c':{
 				inserisciCodaArg(&listaArgomenti,9,optarg);
 				break;
 			}
 			case 'p':{
 				printf("stampe di debug attivate\n");
 				attivaStampe();
 				debug=1;
 				break;
 			}
 			case ':':{
 				printf("l'opzione ha bisogno di un valore\n");
 				break;
 			}
 			case '?':{
				printf("operazione %c non riconosicuta\n",optopt);
				break; 				
 			}
 		}

 		usleep(sleeptime*1000);
 	}
 	if(debug){
 		printCommand();
 	}
 	if(verificaComandi()!=0){
 		errno=-1;
 		perror("errore nei parametri passati da linea di comando\n");
 		exit(EXIT_FAILURE);
 	}
 	execute_command();
 	if(debug) fprintf(stdout,"richieste effettuate,chiusura connessione in corso\n");
 	sleep(2);
 	if(closeConnection(SOCKNAME)!=0){
 		perror("errore nella chiusura della connession con il server");
 		exit(EXIT_FAILURE);
 	}
 	if(debug) fprintf(stdout,"connessione chiusa\n");
 	
 
 	return 0;
 }