package main

import (
    "encoding/json"
    "fmt"
    "io/ioutil"
    "log"
    "net/http"
    "os/exec"
)

type Config struct {
    DatabaseHost     string `json:"database_host"`
    DatabasePort     int    `json:"database_port"`
    DatabaseUser     string `json:"database_user"`
    DatabasePassword string `json:"database_password"`
    DatabaseName     string `json:"database_name"`
    QueryInterval    int    `json:"query_interval"`
    RestPort         int    `json:"rest_port"`
    Thresholds       map[string]Threshold `json:"thresholds"`
}

type Threshold struct {
    Min int `json:"min"`
    Max int `json:"max"`
}

func loadConfig(path string) (*Config, error) {
    data, err := ioutil.ReadFile(path)
    if err != nil {
        return nil, err
    }
    var config Config
    err = json.Unmarshal(data, &config)
    if err != nil {
        return nil, err
    }
    return &config, nil
}

func metricsHandler(w http.ResponseWriter, r *http.Request) {
    out, err := exec.Command("./monitor").Output()
    if err != nil {
        http.Error(w, fmt.Sprintf("Error ejecutando monitor: %v", err), http.StatusInternalServerError)
        return
    }
    w.WriteHeader(http.StatusOK)
    w.Write(out)
}

func healthHandler(w http.ResponseWriter, r *http.Request) {
    w.WriteHeader(http.StatusOK)
    w.Write([]byte(`{"status": "UP"}`))
}

func main() {
    config, err := loadConfig("config.json")
    if err != nil {
        log.Fatalf("Error cargando config.json: %v", err)
    }
    fmt.Printf("Conectando a base de datos %s en %s:%d...\n", config.DatabaseName, config.DatabaseHost, config.DatabasePort)

    http.HandleFunc("/metrics", metricsHandler)
    http.HandleFunc("/health", healthHandler)

    port := fmt.Sprintf(":%d", config.RestPort)
    fmt.Printf("API Server escuchando en %s\n", port)
    log.Fatal(http.ListenAndServe(port, nil))
}
