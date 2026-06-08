
#include <WiFi.h>
#include <ArduinoJson.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// wi fi pra conectar
#define WLAN_SSID       "Babau"
#define WLAN_PASS       "12345678"

// configurações do adafruit
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "" 
///usuário adafruit (tirei meu nome né)
#define AIO_KEY         ""
///chave adafruit (tirei a minha OBVIO)

// serial dde escuta que vai vr do primeiro esp
#define SERIAL_IN Serial2

// inicialização wifi e mqtt
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// feeds lá do AdaFruit
Adafruit_MQTT_Publish feed_temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/esp32-10-2-temp");
Adafruit_MQTT_Publish feed_umid = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/esp32-10-2-umid");

// conexão estável com o broker MQTT
void conectarMQTT() {
  int8_t ret;
  if (mqtt.connected()) return;

  Serial.print("Conectando ao Adafruit IO... ");
  while ((ret = mqtt.connect()) != 0) { 
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println(" Tentando novamente em 5 segundos...");
       mqtt.disconnect();
       delay(5000);
  }
  Serial.println("conectado!!!");
}

void setup() {
  // inicializando na mesma frequência do primeiro
  SERIAL_IN.begin(115200);
  delay(10);

  SERIAL_IN.begin(115200, SERIAL_8N1, 16, 17);


  // conexão wifi
  Serial.println();
  Serial.print("Conectando-se a rede: ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado");
  Serial.print("Endereço IP: "); Serial.println(WiFi.localIP());
}

void loop() {
  // garante conexãi estável
  conectarMQTT();
  
  // verifica se tem a string Json dispnível
  if (SERIAL_IN.available() > 0) {
    // percorre a string pra encontrar a quebra de linha enviada pelo primeiro esp
    String dadosSeriais = SERIAL_IN.readStringUntil('\n');
    dadosSeriais.trim(); // limpa espaço, trim no caso é tipo aparar, cortar

    if (dadosSeriais.length() > 0) {
      StaticJsonDocument<256> doc;
      DeserializationError erro = deserializeJson(doc, dadosSeriais);

      if (!erro) {
        String tipo = doc["tipo"] | "";
        String idDispositivo = doc["id"] | "";

        // para processar pacotes do tipo Data
        if (tipo == "data") {
          int temperatura = doc["temp"] | 0;
          int umidade = doc["umid"] | 0;

          Serial.print("📥 Processando dados do Dispositivo ID: ");
          Serial.println(idDispositivo);

          // roteamento com o ID reebido
          if (idDispositivo == "10-2") {
            Serial.print("📤 Postando no Adafruit IO -> Temp: "); Serial.print(temperatura);
            Serial.print("°C | Umid: "); Serial.print(umidade); Serial.println("%");

            // coloca os valores nos feeds
            feed_temp.publish(temperatura);
            feed_umid.publish(umidade);
          } 
          else {
            Serial.println("ID de dispositivo desconhecido recebido pelo barramento.");
          }
        }
      } else {
        Serial.print("erro ao decodificar json Serial: ");
        Serial.println(erro.f_str());
      }
    }
  }
  
  // Ping periódico obrigatório na API do Adafruit IO para manter o socket aberto
  if(!mqtt.ping()) {
    mqtt.disconnect();
  }
  
  delay(50);
}
