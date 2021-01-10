all:
	g++ -Wall -o serial_mon -std=c++11 main.cpp -pthread
clean:
	rm -f serial_mon
