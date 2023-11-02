# Makefile for Writing Make Files Example
 
# *****************************************************
# Variables to control Makefile operation
 
CC = g++
CFLAGS = -g -pthread  -std=c++11
 
# ****************************************************
# Targets needed to bring the executable up to date
 
server: tsamgroup56.cpp utils.cpp
	$(CC) $(CFLAGS) -o tsamgroup56 tsamgroup56.cpp utils.cpp

client: client.cpp utils.cpp
	$(CC) $(CFLAGS) -o client client.cpp utils.cpp

ip: ip.cpp 
	$(CC) $(CFLAGS) -o ip ip.cpp

all: server client ip

