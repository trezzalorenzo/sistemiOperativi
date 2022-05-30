#if !defined(API_H)
#define API_H
#include <unistd.h>
#include<stdio.h>
#include "ops.h"
#include "conn.h"

void attivaStampe();
char* readFileBytes(const char *name, long* filelen);
int WriteFilefromByte(const char* name, char* text, long size, const char* dirname);
int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(char* sockname);
int openFile(const char* pathname,int flags);
int writeFile(const char* pathname,const char* dirname);
int lockFile(const char* pathname);
int unlockFile(const char* pathname);
int closeFile(const char* pathname);
int readFile(const char* pathname,void**buf,size_t* size);
int appendToFile(const char* pathname,void*buf,size_t size,const char* dirname);
int removeFile(const char* pathname);
int readNFiles(int n,const char* dirname);
#endif
