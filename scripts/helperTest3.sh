folder_index=$1

client_launched=0

while true; do
	./client -f storage_sock -t 0 -W ./storage/file1 -W ./storage/file2  -W ./storage/file3 -c ./storage/file1 -c ./storage/file2  
	./client -f storage_sock -t 0 -W ./storage/info1 -W ./storage/info2  -W ./storage/info3 -c ./storage/info1 -c ./storage/info2
	client_launched=$(($client_launched+3))
done
sleep 0.5

exit 0
