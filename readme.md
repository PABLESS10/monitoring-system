Sistema de Monitoreo
Este proyecto implementa un sistema de monitoreo con dos componentes principales:

- Un monitor escrito en C++ que consulta métricas desde una base de datos MySQL.
- Un servidor REST escrito en Go que expone las métricas y permite verificar el estado de salud del servicio.

Requisitos previos

- Linux (Ubuntu 20.04 o superior recomendado)
- g++ (C++17 compatible)
- libmysqlcppconn (libmysqlcppconn-dev)
- Go (versión 1.14 o superior)
- MySQL Server (local o remoto)

Para instalar dependencias en Ubuntu ejecute:

sudo apt update  
sudo apt install g++ libmysqlcppconn-dev golang-go mysql-server
Compilar y ejecutar en Linux

1. Clonar el repositorio o copiar los archivos al servidor Linux.
2. Configurar la base de datos:
   Crear la base de datos y usuario en MySQL:
   CREATE DATABASE monitoring;  
   CREATE USER 'monitor'@'%' IDENTIFIED BY 'TuPassword123';  
   GRANT ALL PRIVILEGES ON monitoring.* TO 'monitor'@'%';  
   FLUSH PRIVILEGES;
   Actualizar config.json con los datos de conexión si es necesario.

3. Compilar el monitor en C++:
   g++ -std=c++17 -o monitor monitor.cpp -lmysqlcppconn -lpthread

4. Compilar el servidor en Go:
   go build -o server main.go

5. Ejecutar el servidor:
   ./server
   El servidor escuchará en el puerto configurado (por defecto 8080).
Probar el servicio

1. Probar el endpoint /health:
   curl http://localhost:8080/health
   Respuesta esperada:
   {"status": "UP"}

2. Probar el endpoint /metrics:
   curl http://localhost:8080/metrics
   Devuelve las métricas actuales obtenidas desde la base de datos.
Probar recarga dinámica
El servicio detecta automáticamente cambios en el archivo config.json sin necesidad de reiniciar el servidor.

1. Editar config.json y modificar, por ejemplo, los umbrales de temperatura.
2. Guardar el archivo.
3. En el log del servidor se debe mostrar un mensaje indicando que la configuración fue recargada dinámicamente.
4. Consultar nuevamente /metrics para verificar que se aplicaron los nuevos valores.
Docker (opcional)
Para ejecutar con Docker y Docker Compose:
docker-compose up --build
Esto levantará un contenedor con MySQL y otro con el monitor y servidor REST.
El servicio estará disponible en http://localhost:8080


Autor
Nombre: Juan Pablo Castro Dorantes
Proyecto: Sistema de monitoreo con C++ y Go

