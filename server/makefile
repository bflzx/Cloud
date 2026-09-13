server:main.cpp http_server.hpp
	g++ $^ -o $@ -std=c++20 -lboost_system -lpthread -ljsoncpp -lcrypto -lmysqlclient

.PHONY:clean
clean:
	rm -f server
