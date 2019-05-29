while true;
do
	cp infcroya.txt "logs/infcroya-$(date +"%d-%m-%y-%r").txt"
	cp core "logs/core-$(date +"%d-%m-%y-%r")"
	./server
done
