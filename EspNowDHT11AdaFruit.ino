
#include <WiFi.h>
#include <ArduinoJson.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// === CONFIGURAÇÕES WI-FI ===
#define WLAN_SSID       "Babau"
#define WLAN_PASS       "12345678"

// === CONFIGURAÇÕES ADAFRUIT IO ===
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "" 
///Preencher com seu usuário do Adafruit
#define AIO_KEY         ""
///Preencher com sua chave Adafruit

// Definição da Serial de escuta (RX) vinda do primeiro ESP32
#define SERIAL_IN Serial2

// Inicialização do cliente Wi-Fi e MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// === MAPEAMENTO DE FEEDS DO ADAFRUIT IO ===
// IMPORTANTE: Crie estes feeds exatamente com esses nomes textuais no seu Adafruit IO
Adafruit_MQTT_Publish feed_temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/esp32-10-2-temp");
Adafruit_MQTT_Publish feed_umid = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/esp32-10-2-umid");

// Função para garantir conexão estável com o Broker MQTT
void conectarMQTT() {
  int8_t ret;
  if (mqtt.connected()) return;

  Serial.print("🌐 Conectando ao Adafruit IO... ");
  while ((ret = mqtt.connect()) != 0) { 
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("🔄 Tentando novamente em 5 segundos...");
       mqtt.disconnect();
       delay(5000);
  }
  Serial.println("✅ Conectado com Sucesso!");
}

void setup() {
  // Inicializa a Serial na mesma velocidade do primeiro chip
  SERIAL_IN.begin(115200);
  delay(10);

  SERIAL_IN.begin(115200, SERIAL_8N1, 16, 17);


  // Conexão Wi-Fi Local convencional
  Serial.println();
  Serial.print("📶 Conectando-se a rede: ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi Conectado!");
  Serial.print("Endereço IP: "); Serial.println(WiFi.localIP());
}

void loop() {
  // Garante conexões ativas antes de processar dados
  conectarMQTT();
  
  // Verifica se há uma string JSON completa disponível na Serial RX
  if (SERIAL_IN.available() > 0) {
    // Lê a string até encontrar a quebra de linha '\n' enviada pelo primeiro chip
    String dadosSeriais = SERIAL_IN.readStringUntil('\n');
    dadosSeriais.trim(); // Limpa espaços residuais

    if (dadosSeriais.length() > 0) {
      StaticJsonDocument<256> doc;
      DeserializationError erro = deserializeJson(doc, dadosSeriais);

      if (!erro) {
        String tipo = doc["tipo"] | "";
        String idDispositivo = doc["id"] | "";

        // Garante que só processará pacotes do tipo 'data'
        if (tipo == "data") {
          int temperatura = doc["temp"] | 0;
          int umidade = doc["umid"] | 0;

          Serial.print("📥 Processando dados do Dispositivo ID: ");
          Serial.println(idDispositivo);

          // Roteamento inteligente baseado no ID recebido por fio
          if (idDispositivo == "10-2") {
            Serial.print("📤 Postando no Adafruit IO -> Temp: "); Serial.print(temperatura);
            Serial.print("°C | Umid: "); Serial.print(umidade); Serial.println("%");

            // Publica nos feeds correspondentes
            feed_temp.publish(temperatura);
            feed_umid.publish(umidade);
          } 
          else {
            Serial.println("⚠️ ID de dispositivo desconhecido recebido pelo barramento.");
          }
        }
      } else {
        Serial.print("❌ Erro ao decodificar JSON Serial: ");
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
