#!/bin/bash

#  @author Federico Germinario 545081                                 
#  @file script.sh                                                    
#  @brief script richiesto   
#  
#  Si dichiara che il contenuto di questo file e' in ogni sua parte opera
#  originale dell'autore                                      


if [[ $# != 2 ]]; then
    echo "usa $0 file_configurazione tempo" 1>&2
    exit -1
fi

# Scorro i parametri d'ingresso
for p in "$@"
do
    if [ "$p" == "--help" ]; then  
        echo "usa $0 file_configurazione tempo" 1>&2
        exit 1
    fi
done

# Controllo che il primo parametro sia un file
if [ ! -f $1 ]; then
    echo "$1 non esiste" 1>&2
    exit -1
fi

# Controllo che il secondo parametro sia un intero >= 0
if [ $2 -lt 0 ]; then
    echo "$2 non è un intero positivo" 1>&2
    exit -1
fi

pathFile=$1
t=$2

# Estrazione DirName dal file
DirName=$(grep -v '#' $pathFile | grep DirName)  # DirName  = /tmp/chatty
DirName=${DirName#*"="}                          # Elimino da DirName la piu corta occorenza iniziale di "="
DirName=${DirName//[[:blank:]]/}                 # Elimino tutto lo spazio rimasto

cd $DirName

# Se t > 0 archivio in DirName.tar.gz tutti i files presenti in DirName piu vecchi di t minuti
if [ $t -gt 0 ]; then
   
    #Parametri tar  c: creare un nuovo archivio v: modalita verbosa f: nome archivio da creare
    #Parametri rm   f: force  d: rimuove cartelle vuote  r: recursive
    find . -mmin +$t -exec tar -cvf dirname.tar {} + | xargs rm -fdr 
    echo "Script eseguito"                                                                  
    exit 1
fi

# Se t = 0 stampo la lista dei file presenti in DirName
if [ $t -eq 0 ]; then
    echo "File contenuti in $DirName:"
    for f in "$DirName"/*; 
    do
        if [ -f $f ]; then  # Controllo che sia un file
            basename "$f"
        fi
    done
    echo "Script eseguito"  
    exit 1
fi
