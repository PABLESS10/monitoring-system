#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <unistd.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fstream>             
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Threshold {
    int min;
    int max;
};

struct MonitoringResult {
    int alerts = 0;
    int rowsProcessed = 0;
    std::string lastCheckTime;
};

std::mutex resultMutex;
MonitoringResult lastCheckResult;

int queryInterval = 5;
int serverPort = 8080;
const int BUFFER_SIZE = 1024;

struct DBConfig {
    std::string host;
    std::string user;
    std::string password;
    std::string database;
} dbConfig;

std::map<std::string, Threshold> thresholds = {
    {"Temperature", {10, 50}},
    {"Pressure", {20, 55}},
    {"Humidity", {10, 70}},
    {"CPU_Usage", {0, 70}},
    {"Memory_Usage", {0, 70}}
};

void logMessage(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    std::time_t tnow = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&tnow);

    std::ostringstream timestamp;
    timestamp << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    std::cout << "{\"level\":\"" << level << "\","
              << "\"message\":\"" << message << "\","
              << "\"timestamp\":\"" << timestamp.str() << "\"}"
              << std::endl;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tnow = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&tnow);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

bool loadConfig(const std::string& filename) {
    try {
        std::ifstream configFile(filename);
        if (!configFile.is_open()) {
            logMessage("ERROR", "No se pudo abrir el archivo de configuración: " + filename);
            return false;
        }

        json configJson;
        configFile >> configJson;

        // Validar que cada campo obligatorio existe y no es null
        if (!configJson.contains("mysql_host") || configJson["mysql_host"].is_null() ||
            !configJson.contains("mysql_user") || configJson["mysql_user"].is_null() ||
            !configJson.contains("mysql_password") || configJson["mysql_password"].is_null() ||
            !configJson.contains("mysql_db") || configJson["mysql_db"].is_null()) {
            logMessage("ERROR", "Faltan campos obligatorios en el archivo de configuración.");
            return false;
        }

        dbConfig.host = configJson["mysql_host"].get<std::string>();
        dbConfig.user = configJson["mysql_user"].get<std::string>();
        dbConfig.password = configJson["mysql_password"].get<std::string>();
        dbConfig.database = configJson["mysql_db"].get<std::string>();

        // Campos opcionales
        if (configJson.contains("query_interval") && !configJson["query_interval"].is_null()) {
            queryInterval = configJson["query_interval"].get<int>();
        }
        if (configJson.contains("rest_port") && !configJson["rest_port"].is_null()) {
            serverPort = configJson["rest_port"].get<int>();
        }

        // Leer thresholds si existen
        if (configJson.contains("thresholds") && !configJson["thresholds"].is_null()) {
            thresholds.clear();
            for (const auto& [name, values] : configJson["thresholds"].items()) {
                thresholds[name] = {
                    values["min"].get<int>(),
                    values["max"].get<int>()
                };
            }
        }

        logMessage("INFO", "Archivo de configuración cargado correctamente.");
        return true;
    } catch (const std::exception& e) {
        logMessage("ERROR", std::string("Error al leer config.json: ") + e.what());
        return false;
    }
}

void checkDatabase() {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> con(driver->connect(dbConfig.host, dbConfig.user, dbConfig.password));
        con->setSchema(dbConfig.database);

        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT name, value FROM metrics"));

        MonitoringResult tempResult;
        int rowsProcessed = 0;

        while (res->next()) {
            std::string name = res->getString("name");
            int value = res->getInt("value");

            auto threshold = thresholds.find(name);
            if (threshold != thresholds.end()) {
                if (value < threshold->second.min || value > threshold->second.max) {
                    std::ostringstream alert;
                    alert << "Métrica fuera de rango: " << name << " = " << value;
                    logMessage("WARNING", alert.str());
                    tempResult.alerts++;
                }
            }

            rowsProcessed++;
        }

        std::lock_guard<std::mutex> lock(resultMutex);
        tempResult.rowsProcessed = rowsProcessed;
        tempResult.lastCheckTime = getCurrentTimestamp();
        lastCheckResult = tempResult;

        logMessage("INFO", "Processed " + std::to_string(rowsProcessed) + " rows.");
    } catch (sql::SQLException& e) {
        logMessage("ERROR", std::string("Database error: ") + e.what());
    }
}

void monitoringThread() {
    while (true) {
        checkDatabase();
        std::this_thread::sleep_for(std::chrono::seconds(queryInterval));
    }
}

void sendHttpResponse(int clientSock, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;

    std::string response = oss.str();
    send(clientSock, response.c_str(), response.size(), 0);
}

void apiServerThread() {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        logMessage("ERROR", "Failed to create socket");
        return;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(serverPort);

    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        logMessage("ERROR", "Failed to bind socket");
        close(serverSock);
        return;
    }

    if (listen(serverSock, 5) < 0) {
        logMessage("ERROR", "Failed to listen on socket");
        close(serverSock);
        return;
    }

    logMessage("INFO", "API Server listening on port " + std::to_string(serverPort));

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock < 0) {
            logMessage("ERROR", "Failed to accept client connection");
            continue;
        }

        char buffer[BUFFER_SIZE] = {0};
        int received = recv(clientSock, buffer, BUFFER_SIZE - 1, 0);
        if (received <= 0) {
            close(clientSock);
            continue;
        }

        std::string request(buffer, received);

        if (request.find("GET /status") == 0 || request.find("GET /health") == 0 || request.find("GET /metrics") == 0) {
            MonitoringResult currentResult;
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                currentResult = lastCheckResult;
            }

            std::ostringstream json;
            json << "{"
                 << "\"alerts\":" << currentResult.alerts << ","
                 << "\"last_check\":\"" << currentResult.lastCheckTime << "\","
                 << "\"rows_processed\":" << currentResult.rowsProcessed
                 << "}";

            sendHttpResponse(clientSock, json.str());
        } else {
            const char* notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(clientSock, notFound, strlen(notFound), 0);
        }

        close(clientSock);
    }

    close(serverSock);
}

int main() {
    if (!loadConfig("config.json")) {
        logMessage("ERROR", "No se pudo cargar el archivo de configuración.");
        return 1;
    }

    std::thread monitorThread(monitoringThread);
    std::thread serverThread(apiServerThread);

    monitorThread.join();
    serverThread.join();

    return 0;
}
