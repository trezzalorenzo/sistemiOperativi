#include<stdio.h>
#include "ops.h"
#include <unistd.h>
#include "conn.h"

int sockfd;
int stampeDebug = 0;

void attivaStampe(){
	stampeDebug=1;
}

char* readFileBytes(const char *name, long* filelen) 
{
	//fprintf(stderr, "dentro readbyte\n");
	FILE *file = NULL;

	if ((file = fopen(name, "rb")) == NULL) 
	{
		perror("ERROR: opening file");
		fclose(file);
		exit(EXIT_FAILURE);
	}

	//go to end of file
	if(fseek(file, 0, SEEK_END) == -1) 
	{
		perror("ERROR: fseek"); 
		fclose(file);
		exit(EXIT_FAILURE);
	} 

	long len = ftell(file); //current byte offset
	*filelen = len; //assigning the lenght to external value

	char* ret; //return string
	
	ret = safe_malloc(len);

	//starting at the beginning of the file
	if(fseek(file, 0, SEEK_SET) != 0) 
	{
		perror("ERROR: fseek");
		fclose(file); 
		free(ret);
		exit(EXIT_FAILURE);
	}

	int err;

	//fread must return the same number as the size passed
	if((err = fread(ret, 1, len, file)) != len)
	{
		perror("ERROR: fread");
		fclose(file); 
		free(ret);
		exit(EXIT_FAILURE);
	}

	fclose(file);
	return ret;
}

int WriteFilefromByte(const char* name, char* text, long size, const char* dirname) 
{

	FILE *fp1;
	char* fullpath = safe_malloc(strlen(dirname) + strlen(name) + 2);
	
	sprintf(fullpath,"%s/%s", dirname, name);
	fullpath[strlen(fullpath)+1] = '\0';
	//fprintf(stderr, "%s", fullpath);
	errno = 0;
	if((fp1 = fopen(fullpath, "wb")) == NULL) {
		fprintf(stderr, "errno:%d\n", errno);
		return -1;
	}

	if((fwrite((void*)text, sizeof(char), size, fp1)) != size) return -1;


	fclose(fp1);

	return 0;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
	errno = 0;
	int result = 0;
	
	//checking if parameters are correct
	if(sockname == NULL || msec < 0) { errno = EINVAL; return -1;}
	
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, sockname, strlen(sockname)+1);
	
	if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) { errno = -1; return -1;}

	struct timespec current_time;
	if((clock_gettime(CLOCK_REALTIME, &current_time)) == -1) return -1;
	
	//if the connection failed, retry
	while ((result = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) != 0 && abstime.tv_sec > current_time.tv_sec) {
		if((result = usleep(msec*1000)) != 0) return result;
		if((clock_gettime(CLOCK_REALTIME, &current_time)) == -1) return -1;
	}

	if(result != -1) return 0;

	return result;
}

int closeConnection(char* sockname){
	errno=0;
	msg* closeMsg=safe_malloc(sizeof(msg));
	closeMsg->op_type=END_COMMUNICATION;
	if(write(sockfd,closeMsg,sizeof(msg))<=0){
		errno=-1;
		perror("Errore invio messaggio in closeConnection");
		return -1;
	}
	op risposta;
	if(read(sockfd,&risposta,sizeof(op))<=0){
		errno=-1;
		perror("errore ricezione risposta in closeConnection");
		return -1;
	}
	close(sockfd);
	if(risposta==SRV_OK) return 0;
	return -1;
}

int openFile(const char* pathname,int flags){
	
	msg *openMsg=safe_malloc(sizeof(msg));
	openMsg->dimNome=strlen(pathname);
	openMsg->op_type=OPEN_FILE;
	strncpy(openMsg->nomeFile,pathname,strlen(pathname));
	openMsg->pid=getpid();
	openMsg->fd_con=sockfd;
	openMsg->flag=flags;
	
	/*if(write(sockfd,openMsg,sizeof(msg))!=0){
		errno=-1;
		perror("errore in invio comando in openFilen\n");
		return -1;
	}*/
	write(sockfd,openMsg,sizeof(msg));
	op returner;
	if(read(sockfd,&returner,sizeof(op))<=0){
		errno=-1;
		perror("errore in recezione risposta in openFilen\n");
		return -1;		
	}
	if(stampeDebug){
		fprintf(stdout,"open file con risposta:%d\n",returner);
	}
	return returner;
}


int writeFile(const char* pathname,const char* dirname){
	msg *writeMsg=safe_malloc(sizeof(msg));
	writeMsg->op_type=WRITE_FILE;
	errno=0;
	writeMsg->dimNome=strlen(pathname);
	strncpy(writeMsg->nomeFile,pathname,strlen(pathname));
	writeMsg->pid=getpid();
	writeMsg->fd_con=sockfd;
	
	long file_lenght;
	char* buf;

	if((buf=readFileBytes(pathname,&file_lenght))==NULL){
		errno=-1;
		perror("errore in readFileBytes\n");
		return -1;
	}
	memcpy(writeMsg->contenutoFile,buf,file_lenght);
	writeMsg->dimensione=file_lenght;
	write(sockfd,writeMsg,sizeof(msg));
	op returner;
	read(sockfd,&returner,sizeof(op));
	if(returner==SRV_FILE_EXPELLED){
		//leggo il numero di file espulsi
		int fileEspulsi;
		if(readn(sockfd,&fileEspulsi,sizeof(int))<=0){
			errno=-1;
			perror("errore nella lettura dei file espulsi");
			return -1;
		}
		
		while(fileEspulsi>0){
			msg *messaggio=safe_malloc(sizeof(msg));
			if(readn(sockfd,messaggio,sizeof(msg))<=0){
				errno=-1;
				perror("errore ricezione file espulso\n");
				return -1;
			}
			

			if(dirname!=NULL){
				//segnalino
				
				char* name;
				name=strrchr(messaggio->nomeFile,'/');
				++name;
				if((WriteFilefromByte(name,messaggio->contenutoFile,messaggio->dimensione,dirname))==-1){
					errno=-1;
					perror("errore in scrittura su file");
					return -1;
				}
			}
			free(messaggio);
			fileEspulsi--;
		}
	}
	return 0;
}

int lockFile(const char* pathname){
	msg* lockMsg=safe_malloc(sizeof(msg));
	lockMsg->dimNome=strlen(pathname);
	lockMsg->op_type=LOCK_FILE;
	strncpy(lockMsg->nomeFile,pathname,strlen(pathname));
	lockMsg->pid=getpid();
	lockMsg->fd_con=sockfd;
	write(sockfd,lockMsg,sizeof(msg));
	op returner;
	read(sockfd,&returner,sizeof(op));
	if(stampeDebug) fprintf(stdout,"lockFile con risposta %d\n",returner);
	return returner;
}

int unlockFile(const char* pathname){
	msg* unlockMsg=safe_malloc(sizeof(msg));
	unlockMsg->dimNome=strlen(pathname);
	unlockMsg->op_type=UNLOCK_FILE;
	strncpy(unlockMsg->nomeFile,pathname,strlen(pathname));
	unlockMsg->pid=getpid();
	unlockMsg->fd_con=sockfd;
	write(sockfd,unlockMsg,sizeof(msg));
	op returner;
	read(sockfd,&returner,sizeof(op));
	if(stampeDebug) fprintf(stdout,"unlockFile con risposta %d\n",returner);
	return returner;	
}

int closeFile(const char* pathname){
	msg* closeMsg=safe_malloc(sizeof(msg));
	/*backup*/	closeMsg->fd_con=sockfd;
	closeMsg->dimNome=strlen(pathname);
	closeMsg->op_type=CLOSE_FILE;
	strncpy(closeMsg->nomeFile,pathname,strlen(pathname));
	closeMsg->pid=getpid();
	write(sockfd,closeMsg,sizeof(msg));
	op returner;
	read(sockfd,&returner,sizeof(op));
	if(stampeDebug) fprintf(stdout,"closeFile con risposta %d\n",returner);
	return returner;
}

int readFile(const char* pathname,void**buf,size_t* size){

	msg* readMsg=safe_malloc(sizeof(msg));
	/*backup*/	readMsg->fd_con=sockfd;
	readMsg->dimNome=strlen(pathname);
	readMsg->op_type=READ_FILE;
	strncpy(readMsg->nomeFile,pathname,strlen(pathname));
	readMsg->pid=getpid();	
	write(sockfd,readMsg,sizeof(msg));
	
	op returner;
	read(sockfd,&returner,sizeof(op));
	if(stampeDebug) fprintf(stdout,"readFile con risposta %d\n",returner);
	//int dimensione;
	read(sockfd,size,sizeof(size_t));

	char* tmp=safe_malloc((*size));
	
	readn(sockfd,tmp,(*size));
	*buf=(void*)tmp;
	return returner;
}

int readNFiles(int n,const char* dirname){
	msg* readNMsg=safe_malloc(sizeof(msg));
	readNMsg->op_type=READ_FILE_N;
	/*backup*/ readNMsg->fd_con=sockfd;
	readNMsg->flag=n;
	write(sockfd,readNMsg,sizeof(msg));
	int ok=0;
	while(ok==0){
		msg *messaggio=safe_malloc(sizeof(msg));
		if(readn(sockfd,messaggio,sizeof(msg))<=0){
			errno=-1;
			perror("errore ricezione file espulso\n");
			return -1;
		}
		
		if(messaggio->flag==-1){
			ok=1;
			continue;
		}

		if(dirname!=NULL){
			char* name;
			name=strrchr(messaggio->nomeFile,'/');
			++name;

			if((WriteFilefromByte(name,messaggio->contenutoFile,messaggio->dimensione,dirname))==-1){
				errno=-1;
				perror("errore in scrittura su file");
				return -1;
			}
		}
	}
	int returner;
	return read(sockfd,&returner,sizeof(op));
}

int appendToFile(const char* pathname,void*buf,size_t size,const char* dirname){
	return 0;
}
int removeFile(const char* pathname){

	msg* removeMsg=safe_malloc(sizeof(msg));
	/*backup*/removeMsg->fd_con=sockfd;
	removeMsg->dimNome=strlen(pathname);
	removeMsg->op_type=REMOVE_FILE;
	strncpy(removeMsg->nomeFile,pathname,strlen(pathname));
	removeMsg->pid=getpid();
	write(sockfd,removeMsg,sizeof(msg));
	op returner;
	read(sockfd,&returner,sizeof(op));
	if(stampeDebug) fprintf(stderr, "removeFile con risposta %d\n",returner );
	return returner;
}