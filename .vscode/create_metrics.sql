
CREATE TABLE IF NOT EXISTS metrics (
    id INT AUTO_INCREMENT PRIMARY KEY,
    metric_name VARCHAR(100) NOT NULL,
    metric_value FLOAT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO metrics (metric_name, metric_value) VALUES
('cpu_usage', 35.5),
('memory_usage', 60.2),
('disk_io', 125.8),
('network_latency', 20.4),
('temperature', 55.3);
