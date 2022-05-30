#!/bin/bash

#function that centers the text and puts in between two lines
center() 
{
    printf '=%.0s' $(seq 1 $(tput cols))
    echo "$1" | sed  -e :a -e "s/^.\{1,$(tput cols)\}$/ & /;ta" | tr -d '\n' | head -c $(tput cols)
    printf '=%.0s' $(seq 1 $(tput cols)) | sed 's/^ //'
}

# variables to save stats
n_read=0
tot_br=0

n_write=0
tot_bw=0

n_lock=0
n_olock=0
n_unlock=0
n_close=0

max_mem=0
max_file=0
tot_exp=0

n_threds=0
n_requests=0

max_con=0

#if the log file exists
if [ -e "../log_file.txt" ]; then

    #counting occurrences of key words
    n_read=$(grep "Read" "../log_file.txt" | wc -l)
    n_write=$(grep "WriteFile" "../log_file.txt" | wc -l)

    n_lock=$(grep "lockFile" "../log_file.txt" | wc -l)
    n_olock=$(grep "Create" "../log_file.txt" | wc -l)
    n_unlock=$(grep "UnlockFile" "../log_file.txt" | wc -l)    
    n_close=$(grep "CloseFile" "../log_file.txt" | wc -l)
    max_con=$(grep "connesso" "../log_file.txt" | wc -l)

    #taking values from stats variables
    max_file=$(grep -e "MAX_FILES_MEMORIZED" "../log_file.txt" | cut -c 21- )
    tot_exp=$(grep -e "EXPELLED_COUNT" "../log_file.txt" | cut -c 16- )

    #counting the number of threads that were activated
    n_threads=$(grep -e "StartedThread" "../log_file.txt" | wc -l)

    #sum of all the bytes that were written

    for i in $(grep -e "grande" "../log_file.txt" | cut -c 8- ); do   
        tot_bw=$tot_bw+$i;
    done
    #obtaining sum
    tot_bw=$(bc <<< ${tot_bw})

    #sum of all the bytes that were written

    for i in $(grep -e "memoria occupata" "../log_file.txt" | cut -c 18- ); do   
        tot_br=$tot_br+$i;
    done

    #obtaining sum
    tot_br=$(bc <<< ${tot_br})
    #printing values
    center "SERVER STATS"

    echo " "

    enable -n echo

    echo "Numero di read: ${n_read}"

    echo -n "Media di byte letti: "
    #calculating the mean value and printing it
    if [ ${n_read} != 0 ]; then
        echo "scale=0; ${tot_br} / ${n_read}" | bc -l
    fi

    echo "Numero di write: ${n_write}"
    
    echo -n "Media dei byte scritti:" 
    #calculating the mean value and printing it
    if [ ${n_write} != 0 ]; then
        echo "scale=0; ${tot_bw} / ${n_write}" | bc -l
    fi

    echo "Numero di lock: ${n_lock}"
    echo "Numero di open lock: ${n_olock}"
    echo "Numero di unlock: ${n_unlock}"
    echo "Numero di close: ${n_close}"
    echo "Memoria massima memorizzata: ${max_mem}"
    echo "File massimi memorizzati: ${max_file}"
    echo "File espulsi dalla politica di rimpiazzo: ${tot_exp}"

    for i in $(grep "Thread worker" "../log_file.txt" | cut -c 15- ); do  
        n_requests=$(grep "$i" "../log_file.txt" | wc -l )
        echo "$n_requests richieste processate dal thread $i"
    done

    echo "connessioni massime: ${max_con}"

    echo " "
    center "END"
           
else
    echo "il log file non è presente"
fi
