#!/bin/bash

echo "Test3:"; 
echo -e "numero di thread nel pool= 8;\nMemoria massima = 33554432;\nFile massimi = 100;\nSocket = storage_sock;\nfile di log = log_file;" > config.txt

TEST_TIME=3
./server &

SERVER_PID=$!
export SERVER_PID

bash -c "sleep ${TEST_TIME} && kill -s SIGINT ${SERVER_PID}" &
TIMER_PID=$!

pids=()
for i in {0..10}; do
	bash -c "./scripts/helperTest3.sh ${i}" &
	pids+=($!)
	sleep=0.1
done

wait ${SERVER_PID}
wait ${TIMER_PID}

for i in "${pids[@]}"; do
	kill -s SIGKILL ${i}
	wait ${i}
done

kill -s SIGKILL $(pidof client)
exit 0
