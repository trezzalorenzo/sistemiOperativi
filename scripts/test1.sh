#!/bin/bash

echo "Test1:"; 
echo -e "numero di thread nel pool= 1;\nMemoria massima = 134217728;\nFile massimi = 10000;\nSocket = storage_sock;\nfile di log = log_file;" > config.txt

valgrind --leak-check=full --log-file=valgrind-out.txt ./server &
#./server &
pid=$!
sleep 2s

./client -p -f storage_sock -t 200 -w ./storage -r ./storage/file1 -d ./letti 
./client -p -f storage_sock -t 200 -l ./storage/file1 -u ./storage/file1 -c ./storage/file1
./client -p -f storage_sock -t 200 -W ./storage1/info1 -D ./espulsi
./client -p -f storage_sock -t 200 -R n=1 -d ./letti
./client -h

sleep 1s

kill -s SIGHUP $pid
wait $pid
