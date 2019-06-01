while true;
do
	mv infcroya.txt "logs/infcroya-$(date +"%d-%m-%y-%r").txt"
	mv core "logs/core-$(date +"%d-%m-%y-%r")"
	./server
done
