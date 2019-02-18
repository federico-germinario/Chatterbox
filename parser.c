/**
 * @file  parser.c
 * @author Federico Germinario 545081
 * 
 *  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 *  originale dell'autore
 * 
 */

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @function parsing
 * @brief Parsing file di configurazione
 *
 * @param path    path file da leggere
 * @param conf    puntatore alla struttura dati dove memorizzare i parametri letti
 *
 * @return 0 successo, -1 fallimento
 * 
 */
int parsing(char* path, struct serverConf *conf){
    FILE *fd;
    char line[MAX_LINESIZE];
    char param[MAX_LINESIZE];
    char val[MAX_LINESIZE];
    int valSize; 

    // Apertura file in sola lettura
    fd = fopen (path, "r");             
    if(fd == NULL){
        perror("fopen");
        return -1;
    }

    memset(line,  '\0', MAX_LINESIZE * sizeof(char));
    memset(param, '\0', MAX_LINESIZE * sizeof(char));
    memset(val,   '\0', MAX_LINESIZE * sizeof(char));

    // Lettura righe del file 
    while(fgets(line, MAX_LINESIZE, fd) != NULL){    
        if(strlen(line) > 1 && line[0] != '#'){   // La riga contine un parametro  
            sscanf(line, "%s = %s", param, val);
            valSize = strlen(val);
            if(strlen(param) == 0 && valSize != 0) continue;           // Es. "vuoto"  = 512

            //Parsing 
            if(strncmp(param, "UnixPath", strlen("UnixPath")) == 0){
                strncpy(conf->UnixPath, val, valSize + 1);
            }
            else if(strncmp(param, "DirName", strlen("DirName")) == 0){
                strncpy(conf->DirName, val, valSize + 1);
            }
            else if(strncmp(param, "StatFileName", strlen("StatFileName")) == 0){
                strncpy(conf->StatFileName, val, valSize + 1); 
            }
            else if(strncmp(param, "MaxConnections", strlen("MaxConnections")) == 0){
                conf->MaxConnections = strtol(val, NULL, 10);
            }
            else if(strncmp(param, "ThreadsInPool", strlen("ThreadsInPool")) == 0){
                conf->ThreadsInPool = strtol(val, NULL, 10);
            }
            else if(strncmp(param, "MaxMsgSize", strlen("MaxMsgSize")) == 0){
                conf->MaxMsgSize = strtol(val, NULL, 10);
            }
            else if(strncmp(param, "MaxFileSize", strlen("MaxFileSize")) == 0){
                conf->MaxFileSize = strtol(val, NULL, 10);
            }
            else if(strncmp(param, "MaxHistMsgs", strlen("MaxHistMsgs")) == 0){
                conf->MaxHistMsgs = strtol(val, NULL, 10);
            }
        }
    }
    fclose(fd);
    return 0;
}
