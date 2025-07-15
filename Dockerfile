FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependencias: compilador C++, librerías, curl, golang
RUN apt-get update && apt-get install -y \
    g++ \
    libmysqlcppconn-dev \
    libpthread-stubs0-dev \
    curl \
    golang-go \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar todo el código (monitor.cpp, main.go, config.json, etc)
COPY . .

# Compilar el monitor (C++ binary)
# Añadimos la opción -I./include para que g++ busque también en esa carpeta
RUN g++ -std=c++17 -I./include -o monitor monitor.cpp -lmysqlcppconn -lpthread

# Compilar el servidor Go
RUN go build -o server main.go

EXPOSE 8080

# Ejecutar el servidor Go que llama a monitor internamente
CMD ["./server"]
