#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHTesp.h"
#include <ArduinoJson.h>

// WIFI

char ssid[32] = "NETMIG-2.4G-ferreira_Ext";
char password[32] = "phantomcore1052";
// const char *serverUrl = "http://56.124.43.144:3000/";
// const char *serverUrl = "http://192.168.100.189:3000/";
const char *serverUrl = "http://18.230.39.195:3000/";

IPAddress local_IP(192, 168, 100, 253); // IP desejado pro ESP32
IPAddress gateway(192, 168, 100, 1);    // Gateway (geralmente IP do roteador ou repetidor)
IPAddress subnet(255, 255, 255, 0);     // Máscara de sub-rede padrão
IPAddress primaryDNS(8, 8, 8, 8);       // DNS opcional
IPAddress secondaryDNS(8, 8, 4, 4);     // DNS opcional

// Variaveis globais
String plantingBedId = "5d30626c-6855-4462-8b8a-f9226b19e70f";

// SENSORES
const int sensor1 = 32; // HL-69 nº1
const int sensor2 = 34; // HL-69 nº2
const int sensor3 = 33; // HL-69 nº3
const int sensor4 = 35; // HL-69 nº4
const int DHTPIN = 4;   // DHT11
const int irrigationSalinityPin = 26;

const int WaterRELAY = 5;
const int Hl69RELAY = 16;

DHTesp dht;
bool tryConnectWiFi()
{
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
    int attempts = 0;
    Serial.print("Conectando ao WiFi...");

    while (attempts < 3)
    {
        WiFi.begin(ssid, password);
        Serial.print(".");
        delay(5000);
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("\nWiFi conectado!");
            return true;
        }
        attempts++;
    }
    Serial.println("\nFalha ao conectar ao WiFi");
    return false;
}

void enviarSensorSalinidade()
{
    WiFi.disconnect(true); // desconecta pra liberar o pino 26
    const int leitura = analogRead(irrigationSalinityPin);
    const float tensao = (leitura / 4095.0) * 3.3;
    Serial.print(" | Tensao: ");
    Serial.print(tensao, 2);
    Serial.println(" V");

    if (tryConnectWiFi())
    {
        HTTPClient http;
        http.begin(String(serverUrl) + "irrigation-salinity"); // <-- URL completa
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        jsonData += ",\"salinity\":" + String(tensao);
        jsonData += "}";

        int httpResponseCode = http.POST(jsonData);
        Serial.printf("plantingBedId: %s\n", plantingBedId.c_str());
        if (httpResponseCode == 200)
        {
        }
    }
}

void enviarDados()
{
    digitalWrite(Hl69RELAY, HIGH);
    delay(1000); // aguarda estabilização dos sensores HL-69
    int umidade1 = analogRead(sensor1);
    int umidade2 = analogRead(sensor2);
    int umidade3 = analogRead(sensor3);
    int umidade4 = analogRead(sensor4);
    digitalWrite(Hl69RELAY, LOW);
    
    delay(2000); // aguarda estabilização do DHT11
    TempAndHumidity dhtData = dht.getTempAndHumidity();
    DHTesp::DHT_ERROR_t status = dht.getStatus();

    Serial.print(status);

    Serial.printf("🌡️ Temp: %.1f°C  💧 Umid: %.1f%%\n", dhtData.temperature, dhtData.humidity);

    Serial.println("=== Leitura dos Sensores ===");
    Serial.printf("Umidade Solo 1: %d\n", umidade1);
    Serial.printf("Umidade Solo 2: %d\n", umidade2);
    Serial.printf("Umidade Solo 3: %d\n", umidade3);
    Serial.printf("Umidade Solo 4: %d\n", umidade4);
    Serial.printf("Temperatura: %.1f °C\n", dhtData.temperature);
    Serial.printf("Umidade Ar: %.1f %%\n", dhtData.humidity);

    if (tryConnectWiFi())
    {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        jsonData += ",\"sensor1\":" + String(umidade1);
        jsonData += ",\"sensor2\":" + String(umidade2);
        jsonData += ",\"sensor3\":" + String(umidade3);
        jsonData += ",\"sensor4\":" + String(umidade4);
        jsonData += ",\"airTemperature\":" + String(dhtData.temperature);
        jsonData += ",\"airUmidity\":" + String(dhtData.humidity);
        jsonData += "}";

        int httpResponseCode = http.POST(jsonData);
        Serial.printf("plantingBedId: %s\n", plantingBedId.c_str());
        if (httpResponseCode == 200)
        {
            String response = http.getString();
            Serial.printf("POST enviado! Código: %d\nResposta: %s\n", httpResponseCode, response.c_str());
            if (response == "true")
            {
                digitalWrite(WaterRELAY, HIGH);
                Serial.println("Bomba acionada, esperando 30 segundos ...");
                delay(15000); // TODO: rega dinâmica, input do usuário
                Serial.println("coletando salinidade ...");
                enviarSensorSalinidade();
                delay(15000);
                digitalWrite(WaterRELAY, LOW);
                // return 300000; // 5 minutos para próxima leitura
            }
            else
            {
                Serial.println("irrigação não autorizada");
                // return 1800000; // 30 minutos para próxima leitura
            }
        }
        else
        {
            Serial.printf("Erro ao enviar POST: %d\n", httpResponseCode);
        }
        http.end();
    }
    else
    {
        Serial.println("WiFi desconectado!");
    }
    Serial.println("=== Fim da Leitura ===");
}

void setup()
{
    Serial.begin(115200);
    dht.setup(DHTPIN, DHTesp::DHT11);
    pinMode(WaterRELAY, OUTPUT);
     pinMode(Hl69RELAY, OUTPUT);
}

void loop()
{
    enviarDados();
    Serial.println("dados enviados");
    delay(1800000); 
}