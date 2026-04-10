#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <time.h>

// WIFI
char ssid[32] = "ssid";
char password[32] = "password";
// const char *serverUrl = "http://56.124.43.144:3000/";
// const char *serverUrl = "http://192.168.100.112:3000/";
const char *serverUrl = "http://12.123.123.123:3000/";
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


constexpr int WaterRELAY = 5;

WiFiClient client;

DHTesp dht;

void selectChannel(int ch)
{
    digitalWrite(S0, bitRead(ch, 0));
    digitalWrite(S1, bitRead(ch, 1));
    digitalWrite(S2, bitRead(ch, 2));
    digitalWrite(S3, bitRead(ch, 3));
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
            return true;
        }
        attempts++;
    }
    Serial.println("Falha ao conectar ao WiFi");
    return false;
}
int readChannel(int ch)
{
    selectChannel(ch);
    delay(1000);
    analogRead(SIG); // descarta leitura
    // delay(500);
    int sig = analogRead(SIG);
    Serial.printf("Canal %d: %d\n", ch, sig);
    return sig;
}
// void printSensores()
// {
//     digitalWrite(MOSFET, HIGH);
//     delay(2000);
//     int umidade1 = readChannel(0);
//     int umidade2 = readChannel(1);
//     int umidade3 = readChannel(2);
//     int umidade4 = readChannel(3);
//     digitalWrite(MOSFET, LOW);
//     syslog.log("=========================");
// }
void enviadrDadosAr()
{
    if (tryConnectWiFi())
    {
        HTTPClient http;
        http.begin(String(serverUrl) + "air");
        http.addHeader("Content-Type", "application/json");
        dht.getTempAndHumidity(); // discard
        delay(1000);
        TempAndHumidity dhtData = dht.getTempAndHumidity();
        DHTesp::DHT_ERROR_t status = dht.getStatus();

        Serial.printf("🌡️ Temp: %.1f°C  💧 Umid: %.1f%%\n", dhtData.temperature, dhtData.humidity);
        Serial.printf("Temperatura: %.1f °C\n", dhtData.temperature);
        Serial.printf("Umidade Ar: %.1f %%\n", dhtData.humidity);

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        jsonData += ",\"airTemperature\":" + String(dhtData.temperature);
        jsonData += ",\"airHumidity\":" + String(dhtData.humidity);
        jsonData += "}";

        int httpResponseCode = http.POST(jsonData);
        Serial.printf("plantingBedId: %s\n", plantingBedId.c_str());
        if (httpResponseCode == 200)
        {
            String response = http.getString();
            Serial.printf("POST enviado! Código: %d\nResposta: %s\n", httpResponseCode, response.c_str());
        }
    }
}
void enviarDadosSensores()
{

    if (tryConnectWiFi())
    {
        Serial.printf("WiFi status: %d", WiFi.status());
        HTTPClient http;
        http.begin(String(serverUrl) + "soil");
        WiFiClient client;
        http.addHeader("Content-Type", "application/json");

        // canteiro 1
        int umidade1 = readChannel(0);
        int umidade2 = readChannel(1);
        int umidade3 = readChannel(2);
        int umidade4 = readChannel(3);

        // // canteiro 2
        // int umidade5 = readChannel(4);
        // int umidade6 = readChannel(5);
        // int umidade7 = readChannel(6);
        // int umidade8 = readChannel(7);

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        jsonData += ",\"sensor1\":" + String(umidade1);
        jsonData += ",\"sensor2\":" + String(umidade2);
        jsonData += ",\"sensor3\":" + String(umidade3);
        jsonData += ",\"sensor4\":" + String(umidade4);
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
// void testemultiplexador()
// {
//     while (true)
//     {
//         syslog.log("Digite o número do canal (0-15): ");
//         while (Serial.available() == 0)
//         {
//         }
//         int canal = Serial.parseInt();

//         for (int i = 0; i < 5; i++)

//         {
//             int valor = readChannel(canal);

//             syslog.log("Canal %d: %d\n", canal, valor);
//             delay(300);
//         }
//     }
// }
// void testarmultiplexador2()
// {
//     for (int ch = 0; ch < 4; ch++)
//     {
//         int valor = readChannel(ch);
//         delay(1000);
//         syslog.log("Canal %d: %d\n", ch, valor);
//     }
// }

// void verificarSensores()
// {
//     time_t now = time(nullptr);
//     struct tm *timeinfo = localtime(&now);

//     if (timeinfo->tm_hour == 9)
//     {
//        int irrigacao = enviarDadosSensores();
//     }
//     else if (timeinfo->tm_hour == 18)
//     {
//         enviarDadosSensores();
//     }

//     digitalWrite(MOSFET, HIGH);
//     delay(2000);
//     for (int ch = 0; ch < 16; ch++)
//     {
//         int valor = readChannel(ch);
//         syslog.log("Canal %d: %d\n", ch, valor);
//     }
//     digitalWrite(MOSFET, LOW);
// }

void setup()
{
    Serial.begin(115200);
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);
    pinMode(WaterRELAY, OUTPUT);

    analogReadResolution(12);
    analogSetPinAttenuation(SIG, ADC_11db);
    if (tryConnectWiFi())
    {
        delay(2000);
        configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov", "a.st1.ntp.br");

        struct tm timeinfo;
        bool time = false;
        while(!time){
            if (!getLocalTime(&timeinfo))
            {
                Serial.printf("Falha ao obter o tempo, tentando novamente...\n");
                delay(2000);
                return;
            }
            time = true;
            
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