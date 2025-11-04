#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHTesp.h"
#include <Preferences.h>
#include <ArduinoJson.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Preferences preferences;

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
BLECharacteristic *pCharacteristic;
bool BLEdeviceConnected = false;

// SENSORES
const int sensor1 = 32; // HL-69 nº1
const int sensor2 = 34; // HL-69 nº2
const int sensor3 = 33; // HL-69 nº3
const int sensor4 = 35; // HL-69 nº4
const int DHTPIN = 4;   // DHT11
const int irrigationSalinityPin = 26;

const int RELAY = 5;

DHTesp dht;
void verifyPlantingBedId()
{
    preferences.begin("garden-monitor", false);
    plantingBedId = preferences.getString("plantingBedId", "");
    if(plantingBedId == ""){
            Serial.println("Nenhum UUID registrado, chamando API...");
            if (WiFi.status() == WL_CONNECTED)
            {
                HTTPClient http;
                http.begin(String(serverUrl) + "/register");
                http.addHeader("Content-Type", "application/json");
                int httpCode = http.POST("{}");
                Serial.println(httpCode);
                if (httpCode == 200)
                {
                    String payload = http.getString();
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, payload);
                    String plantingBedId = doc["esp32"]["id"];
                    preferences.putString("plantingBedId", plantingBedId);
                }
                else
                {
                    Serial.println("Falha na chamada HTTP: " + String(httpCode));
                }
                http.end();
            
        }
    }
    preferences.end();
}
void sendWifiListToApp()
{
    Serial.println("📡 Escaneando redes Wi-Fi...");

    int n = WiFi.scanNetworks();
    if (n == 0)
    {
        Serial.println("Nenhuma rede encontrada.");
        pCharacteristic->setValue("");
        pCharacteristic->notify();
        return;
    }

    String wifiList = "";
    for (int i = 0; i < n; ++i)
    {
        wifiList += WiFi.SSID(i);
        if (i < n - 1)
            wifiList += ";";
    }

    Serial.print("Redes encontradas: ");
    Serial.println(wifiList);

    pCharacteristic->setValue(wifiList.c_str());
    pCharacteristic->notify();
}
class NotificationCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic) override
    {
        std::string message = pCharacteristic->getValue();

        if (message == "CONN")
        {
            sendWifiListToApp();
            Serial.println("✅ Enviando lista de redes Wi-Fi...");
        }
        else if (message == "STTS")
        {
            WiFi.begin(ssid, password);
            Serial.print("Conectando ao WiFi...");
            if (WiFi.status() != WL_CONNECTED)
            {
                pCharacteristic->setValue("CONNECTED");
                pCharacteristic->notify();
            }
            else
            {
                pCharacteristic->setValue("DISCONNECTED");
                pCharacteristic->notify();
            }

            // Retorna status do wifi
        }
        else
        {
            if (message.length() < 5 || message.find(";") == std::string::npos)
                return; // evitar mensagens inválidas
            size_t delimiter = message.find(";");

            std::string receivedSSID = message.substr(0, delimiter);
            std::string receivedPass = message.substr(delimiter + 1);
            Serial.printf("SSID: %s, Password: %s\n", receivedSSID.c_str(), receivedPass.c_str());
            strncpy(ssid, receivedSSID.c_str(), sizeof(ssid) - 1);
            strncpy(password, receivedPass.c_str(), sizeof(password) - 1);
            ssid[sizeof(ssid) - 1] = '\0';
            password[sizeof(password) - 1] = '\0';
        }
    }
};

// Callbacks
class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        BLEdeviceConnected = true;
        Serial.println("✅ Dispositivo conectado!");
    }

    void onDisconnect(BLEServer *pServer) override
    {
        BLEdeviceConnected = false;
        Serial.println("❌ Dispositivo desconectado!");
        BLEDevice::startAdvertising();
    }
};
void connectionFlow()
{

    Serial.println("🚀 Iniciando BLE...");

    BLEDevice::init("Garden Monitor 🌱");

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());

    pCharacteristic->setCallbacks(new NotificationCallbacks());
    pService->start();

    // Iniciando o advertising <<<<<<<---------
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("Bluetooth ativo! aguardando conexão...");
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
            BLEDevice::stopAdvertising();
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
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{";
        jsonData += "\"plantingBedId\":\"" + plantingBedId + "\"";
        jsonData += ",\"salinity\":" + String(tensao); // TODO: recebe em tensão, converter sal-> backend?
        jsonData += "}";

        int httpResponseCode = http.POST(String(serverUrl) + "/irrigation-salinity" + jsonData);
        Serial.printf("plantingBedId: %s\n", plantingBedId.c_str());
        if (httpResponseCode == 200)
        {
        }
    }
}

int enviarDados()
{

    delay(10000); // espera estabilizar os sensores
    int umidade1 = analogRead(sensor1);
    int umidade2 = analogRead(sensor2);
    int umidade3 = analogRead(sensor3);
    int umidade4 = analogRead(sensor4);

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
                digitalWrite(RELAY, HIGH);
                Serial.println("Bomba acionada, esperando 30 segundos ...");
                delay(15000); // TODO: rega dinâmica, input do usuário
                Serial.println("coletando salinidade ...");
                enviarSensorSalinidade();
                delay(15000); 
                digitalWrite(RELAY, LOW);
                return 300000; // 5 minutos para próxima leitura
            }
            else{
                return 1800000; // 30 minutos para próxima leitura
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
    pinMode(RELAY, OUTPUT);

    Serial.println("Iniciando fluxo de conexão...");
    while (!tryConnectWiFi())
    {
        connectionFlow();
    }
    Serial.print("Conectado com IP: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    int delayTime = enviarDados();
    Serial.println("dados enviados");
    delay(delayTime); //espera dinâmica
}