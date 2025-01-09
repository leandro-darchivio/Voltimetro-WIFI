#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>

#define WIFI_SSID "vvvvvvvvvvvvvvvvvvvvO"
#define WIFI_PASSWORD "vvvvvvvvvvvvvvvvvvvvvvvvvv"

ESP8266WebServer server(80);
File dataFile;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Conectando a WiFi...");

  // Configurar IP estática
  IPAddress local_IP(192, 168, 0, 196);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Error al configurar IP estática");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConexión establecida.");
  Serial.print("Dirección IP asignada: ");
  Serial.println(WiFi.localIP());

  // Montar el sistema de archivos SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Error al montar SPIFFS");
    return;
  }

  // Eliminar el archivo CSV si existe
  if (SPIFFS.exists("/datos.csv")) {
    SPIFFS.remove("/datos.csv");
    Serial.println("Archivo CSV eliminado.");
  }

  // Crear un nuevo archivo CSV
  dataFile = SPIFFS.open("/datos.csv", "w");
  if (dataFile) {
    dataFile.println("Tiempo (s);Tension (V)"); 
    dataFile.close();
    Serial.println("Nuevo archivo CSV creado.");
  } else {
    Serial.println("Error al crear el archivo de datos");
  }

  // Ruta principal para visualizar el gráfico
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="es">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Tensión de batería: </title>
        <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        <style>
          body {
            background-color: #121212;
            color: #ffffff;
            font-family: Arial, sans-serif;
          }
          h1 {
            color: #ffffff;
          }
          button {
            background-color: #6200ea;
            color: white;
            padding: 10px 20px;
            font-size: 16px;
            border: none;
            cursor: pointer;
            border-radius: 5px;
          }
          button:hover {
            background-color: #3700b3;
          }
        </style>
      </head>
      <body>
        <h1>Tensión de batería <span id="currentValue">0.00</span> V</h1>
        <canvas id="a0Chart" width="400" height="160"></canvas>
        <a href="/descargar" download="datos.csv"><button>Descargar Datos</button></a>
        <script>
          const ctx = document.getElementById('a0Chart').getContext('2d');
          const a0Chart = new Chart(ctx, {
            type: 'line',
            data: {
              labels: [],
              datasets: [{
                label: 'Tensión (V)',
                data: [],
                borderColor: 'rgba(255, 0, 0, 1)', // Línea roja
                backgroundColor: 'rgba(255, 0, 0, 0.2)', // Fondo rojo translúcido
                borderWidth: 1
              }]
            },
            options: {
              scales: {
                x: { 
                  title: { display: true, text: 'Tiempo [s]' },
                  grid: {
                    color: 'rgba(169, 169, 169, 0.7)' // Líneas del grid verticales en gris
                  }
                },
                y: { 
                  title: { display: true, text: 'Tensión [V]' },
                  suggestedMin: 0,
                  suggestedMax: 3.3,
                  grid: {
                    color: 'rgba(169, 169, 169, 0.7)' // Líneas del grid horizontales en gris
                  }
                }
              },
              plugins: {
                legend: {
                  labels: {
                    color: '#ffffff' // Color de las leyendas
                  }
                }
              }
            }
          });

          function fetchData() {
            fetch('/data')
              .then(response => response.json())
              .then(data => {
                a0Chart.data.labels.push(data.time);
                a0Chart.data.datasets[0].data.push(data.value);
                if (a0Chart.data.labels.length > 20) {
                  a0Chart.data.labels.shift();
                  a0Chart.data.datasets[0].data.shift();
                }
                a0Chart.update();

                // Actualizar el valor actual de tensión
                document.getElementById('currentValue').innerText = data.value.toFixed(2);
              });
          }

          setInterval(fetchData, 1000);
        </script>
      </body>
      </html>
    )rawliteral";
    server.send(200, "text/html", html);
  });

  // Ruta para obtener los datos en tiempo real
  server.on("/data", HTTP_GET, []() {
    static unsigned long lastMillis = 0;
    static int seconds = 0;

    if (millis() - lastMillis >= 1000) {
      lastMillis = millis();
      int sensorValue = analogRead(A0);
      float voltage = sensorValue * (3.3 / 1023.0); // Conversión a voltios
      seconds++;

      // Guardar el dato en el archivo con ";" como delimitador
      dataFile = SPIFFS.open("/datos.csv", "a");
      if (dataFile) {
        dataFile.printf("%d;%.2f\n", seconds, voltage); // Usamos ";" como delimitador
        dataFile.close();
      }

      // Enviar datos al cliente
      String json = "{\"time\": \"" + String(seconds) + "\", \"value\": " + String(voltage, 2) + "}";
      server.send(200, "application/json", json);
    }
  });

  // Ruta para descargar el archivo de datos
  server.on("/descargar", HTTP_GET, []() {
    File file = SPIFFS.open("/datos.csv", "r");
    if (!file) {
      server.send(500, "text/plain", "Error al abrir el archivo de datos");
      return;
    }
    server.streamFile(file, "text/csv");
    file.close();
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();
}
