src = $(wildcard *.cpp) # space:match any character default
obj = $(patsubst %.cpp, %.o, $(src)) # not {}

ALL:client server
client:pclient.o 
	g++ pclient.o  -o client
	
server:pserver.o 
	g++ pserver.o  -o server -lpthread


pclient.o:pclient.cpp  
	g++ -c pclient.cpp -o pclient.o

pserver.o:pserver.cpp  
	g++ -c pserver.cpp -o pserver.o



#g++ test.cpppp -o test


clean:
	rm -rf $(obj) client server

