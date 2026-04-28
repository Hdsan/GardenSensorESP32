#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
// WIFI

char ssid[] = WIFI_SSID;
char password[] = WIFI_PASS;

const char *serverUrl = SERVER_URL;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

String plantingBedId = "5d30626c-6855-4462-8b8a-f9226b19e70f";

IPAddress local_IP(192, 168, 100, 251); // IP desejado pro ESP32
IPAddress gateway(192, 168, 100, 1);    // Gateway
IPAddress subnet(255, 255, 255, 0);     // mascara de sub-rede padrão
IPAddress primaryDNS(8, 8, 8, 8);       // dns opcional
IPAddress secondaryDNS(8, 8, 4, 4);

constexpr int S0 = 16;
constexpr int S1 = 17;
constexpr int S2 = 18;
constexpr int S3 = 19;
constexpr int SIG = 32;

constexpr int DHTPIN = 4;

constexpr int WaterRELAY = 5;

WiFiClient client;

DHTesp dht;


int readMedian(int channel)
{
    // reduz ruido
    const int samples = 5;
    int values[samples];

    for (int i = 0; i < samples; i++)
    {
        values[i] = analogRead(channel);
        delay(100);
    }

    for (int i = 0; i < samples - 1; i++)
    {
        for (int j = i + 1; j < samples; j++)
        {
            if (values[j] < values[i])
            {
                int temp = values[i];
                values[i] = values[j];
                values[j] = temp;
            }
        }
    }
    Serial.printf("Mediana do canal %d: %d\n", channel, values[2]);
    return values[2];
}
void amostraDeDados()
{

    HTTPClient http;
    http.begin(String(serverUrl) + "sample");
    WiFiClient client;
    http.addHeader("Content-Type", "application/json");

    dht.getTempAndHumidity(); // descarta primeira leitura pra estabilizar
    delay(1000);
    TempAndHumidity dhtData = dht.getTempAndHumidity();
    DHTesp::DHT_ERROR_t status = dht.getStatus();
    Serial.print(status);

    Serial.printf("🌡️ Temp: %.1f°C  💧 Umid: %.1f%%\n", dhtData.temperature, dhtData.humidity);
    Serial.printf("Temperatura: %.1f °C\n", dhtData.temperature);
    Serial.printf("Umidade Ar: %.1f %%\n", dhtData.humidity);
    int umidade1 = readMedian(32);
    int umidade2 = readMedian(33);
    int umidade3 = readMedian(34);
    int umidade4 = readMedian(35);

    String jsonData = "{";
    jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
    jsonData += ",\"sensor1\":" + String(umidade1);
    jsonData += ",\"sensor2\":" + String(umidade2);
    jsonData += ",\"sensor3\":" + String(umidade3);
    jsonData += ",\"sensor4\":" + String(umidade4);
    jsonData += ",\"air_temperature\":" + String(dhtData.temperature);
    jsonData += ",\"air_humidity\":" + String(dhtData.humidity);
    jsonData += "}";
      int httpResponseCode = http.POST(jsonData);

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
        Serial.print("WiFi status: ");
        Serial.println(WiFi.status());
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("WiFi conectado!");
            amostraDeDados();
            return true;
        }
        attempts++;
    }
    Serial.println("Falha ao conectar ao WiFi");
    return false;
}

void enviarDadosSensores()
{

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("WiFi status: %d", WiFi.status());
        HTTPClient http;
        http.begin(String(serverUrl) + "soil");
        WiFiClient client;
        http.addHeader("Content-Type", "application/json");

        // canteiro 1
        int umidade1 = readMedian(32);
        int umidade2 = readMedian(33);
        int umidade3 = readMedian(34);
        int umidade4 = readMedian(35);

        dht.getTempAndHumidity(); // descarta primeira leitura pra estabilizar
        delay(1000);
        TempAndHumidity dhtData = dht.getTempAndHumidity();
        DHTesp::DHT_ERROR_t status = dht.getStatus();
        Serial.print(status);

        Serial.printf("🌡️ Temp: %.1f°C  💧 Umid: %.1f%%\n", dhtData.temperature, dhtData.humidity);
        Serial.printf("Temperatura: %.1f °C\n", dhtData.temperature);
        Serial.printf("Umidade Ar: %.1f %%\n", dhtData.humidity);

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        jsonData += ",\"sensor1\":" + String(umidade1);
        jsonData += ",\"sensor2\":" + String(umidade2);
        jsonData += ",\"sensor3\":" + String(umidade3);
        jsonData += ",\"sensor4\":" + String(umidade4);
        jsonData += ",\"air_temperature\":" + String(dhtData.temperature);
        jsonData += ",\"air_humidity\":" + String(dhtData.humidity);
        jsonData += "}";

        int httpResponseCode = http.POST(jsonData);
        if (httpResponseCode == 200)
        {
            String response = http.getString();
            if (response.length() > 0)
            {
                Serial.printf("POST enviado! Código: %d\nResposta: %s\n", httpResponseCode, response.c_str());
                float tempoBomba = response.toFloat();
                Serial.printf("Tempo bomba: %.2f segundos\n", tempoBomba);
                if (tempoBomba > 0)
                {
                    digitalWrite(WaterRELAY, HIGH);
                    Serial.printf("Bomba acionada\n");
                    delay(tempoBomba * 1000);
                    digitalWrite(WaterRELAY, LOW);
                }
                else
                {
                    Serial.printf("irrigação não autorizada\n");
                    return;
                }
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
        Serial.printf("WiFi desconectado!\n");
        return;
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(32, INPUT);
    pinMode(33, INPUT);
    pinMode(34, INPUT);
    pinMode(35, INPUT);
    
    pinMode(WaterRELAY, OUTPUT);
    Serial.begin(115200);
    dht.setup(DHTPIN, DHTesp::DHT11);

    if (tryConnectWiFi())
    {
        delay(2000);
        configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov", "a.st1.ntp.br");

        struct tm timeinfo;
        bool time = false;
        while (!time)
        {
            if (!getLocalTime(&timeinfo))
            {
                Serial.printf("Falha ao obter o tempo, tentando novamente...\n");
                delay(2000);
            }
            else
            {
                time = true;
            }
        }

        int h = timeinfo.tm_hour;
        int m = timeinfo.tm_min;
        int s = timeinfo.tm_sec;

        bool horarioMultiplo = (h % 3 == 0);

        int proximaHora = ((h / 3) + 1) * 3;
        if (proximaHora >= 24)
            proximaHora = 0;

        long agora = h * 3600 + m * 60 + s;
        long alvo = proximaHora * 3600;

        long sleepSec = alvo - agora;
        if (sleepSec <= 0)
        {
            sleepSec += 24 * 3600;
        }

        if (horarioMultiplo || h == 0)
        {
            Serial.printf("Hora permitida-> enviando dados\n");
            enviarDadosSensores();

            Serial.printf("Sincronizando para o próximo ciclo alinhado\n");
        }
        else
        {
            Serial.printf("Fora de hora -> aguardando\n");
        }

        Serial.printf("Agora: %02d:%02d:%02d\n", h, m, s);
        Serial.printf("Proximo alvo: %02d:00:00\n", proximaHora);
        Serial.printf("Dormindo por %ld segundos\n", sleepSec);

        WiFi.disconnect(true);
        esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
        esp_deep_sleep_start();
    }
    else
    {
        Serial.printf("Falha ao conectar ao WiFi\n");
        return;
    }
}
void loop()
{
}