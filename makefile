# Makefile

.PHONY: all build clean docker-up docker-down

all: build

build:
	# Compilar monitor C++
	g++ -std=c++17 -o monitor monitor.cpp -lmysqlcppconn -lpthread
	# Compilar servidor Go
	go build -o server main.go

clean:
	rm -f monitor server

docker-up:
	docker-compose up --build -d

docker-down:
	docker-compose down
