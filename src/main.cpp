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
const uint64_t SLEEP_TIME = 30ULL * 60ULL * 1000000ULL;

String plantingBedId = "5d30626c-6855-4462-8b8a-f9226b19e70f";

IPAddress local_IP(192, 168, 100, 253); // IP desejado pro ESP32
IPAddress gateway(192, 168, 100, 1);    // Gateway (geralmente IP do roteador ou repetidor)
IPAddress subnet(255, 255, 255, 0);     // Máscara de sub-rede padrão
IPAddress primaryDNS(8, 8, 8, 8);       // DNS opcional
IPAddress secondaryDNS(8, 8, 4, 4); 
    // DNS opcional
constexpr int DHTPIN = 4;                   // DHT11
constexpr int irrigationSalinityPin = 26;
constexpr int WaterRELAY = 5;
constexpr int Hl69RELAY = 16;
constexpr int S0 = 18;
constexpr int S1 = 19;
constexpr int S2 = 21;
constexpr int S3 = 22;
constexpr int SIG = 34;

DHTesp dht;

void selectChannel(int channel)
{
    digitalWrite(S0, channel & 0x01);
    digitalWrite(S1, (channel >> 1) & 0x01);
    digitalWrite(S2, (channel >> 2) & 0x01);
    digitalWrite(S3, (channel >> 3) & 0x01);
}
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

void enviarDados()
{
    digitalWrite(Hl69RELAY, HIGH);
    delay(1000); // aguarda estabilização dos sensores HL-69
    digitalWrite(Hl69RELAY, LOW);

    delay(2000); // aguarda estabilização do DHT11
    TempAndHumidity dhtData = dht.getTempAndHumidity();
    DHTesp::DHT_ERROR_t status = dht.getStatus();

    Serial.print(status);

    Serial.printf("🌡️ Temp: %.1f°C  💧 Umid: %.1f%%\n", dhtData.temperature, dhtData.humidity);

    Serial.println("=== Leitura dos Sensores ===");
    // Serial.printf("Umidade Solo 1: %d\n", umidade1);
    // Serial.printf("Umidade Solo 2: %d\n", umidade2);
    // Serial.printf("Umidade Solo 3: %d\n", umidade3);
    // Serial.printf("Umidade Solo 4: %d\n", umidade4);
    Serial.printf("Temperatura: %.1f °C\n", dhtData.temperature);
    Serial.printf("Umidade Ar: %.1f %%\n", dhtData.humidity);

    if (tryConnectWiFi())
    {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        // jsonData += ",\"sensor1\":" + String(umidade1);
        // jsonData += ",\"sensor2\":" + String(umidade2);
        // jsonData += ",\"sensor3\":" + String(umidade3);
        // jsonData += ",\"sensor4\":" + String(umidade4);
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
void entrarEmDeepSleep()
{
    Serial.println("Sleep...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);
    Serial.begin(115200);

    dht.setup(DHTPIN, DHTesp::DHT11);

    pinMode(WaterRELAY, OUTPUT);
    pinMode(Hl69RELAY, OUTPUT);
    enviarDados();
    entrarEmDeepSleep();
}
