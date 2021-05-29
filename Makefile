
cupthor: cupThor.cpp
	g++ $< -o $@ -std=c++17 -lpistache -lcrypto -lssl -lpthread
