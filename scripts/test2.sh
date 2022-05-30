#!/bin/bash

echo "Test2:";
echo -e "numero di thread nel pool= 4;\nMemoria massima = 1000000;\nFile massimi = 10;\nSocket = storage_sock;\nfile di log = log_file;" > config.txt

./server &
pid=$!
sleep 2s

./client -p -f storage_sock -t 200 -w ./storage -D ./espulsi
./client -p -f storage_sock -t 200 -w ./storage1 -D ./espulsi

sleep 1s
kill -s SIGHUP $pid
wait $pid
