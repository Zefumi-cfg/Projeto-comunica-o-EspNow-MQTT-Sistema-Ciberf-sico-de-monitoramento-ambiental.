#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>
#include <ArduinoJson.h> 

// === CONFIGURAÇÕES ===
#define ID "10-2"
#define DHTPIN 4
#define DHTTYPE DHT11
#define LED_PIN 2
#define INTERVALO_BASE_MS   60000
#define INTERVALO_RANDOM_MS 5000   
#define MAX_IDS_VISTOS      20     

// Configuração da Serial de Comunicação (Se quiser usar pinos dedicados)
// Por padrão, usaremos a Serial0 (Pinos TX=1 e RX=3 conectados ao outro ESP)
#define SERIAL_COM Serial2

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

String idsVistos[MAX_IDS_VISTOS];
int    idxIdsVistos = 0;

unsigned long ultimoEnvio = 0;
unsigned long intervaloAtual = INTERVALO_BASE_MS;
int contadorMensagens = 0;

DHT dht(DHTPIN, DHTTYPE);

void piscarLED(int tempo = 50) {
  digitalWrite(LED_PIN, HIGH);
  delay(tempo);
  digitalWrite(LED_PIN, LOW);
}

String macParaString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool jaVisto(const String& id) {
  for (int i = 0; i < MAX_IDS_VISTOS; i++) {
    if (idsVistos[i] == id) return true;
  }
  return false;
}

void registrarID(const String& id) {
  idsVistos[idxIdsVistos] = id;
  idxIdsVistos = (idxIdsVistos + 1) % MAX_IDS_VISTOS;
}

void sortearProximoIntervalo() {
  long aleatorio = random(-INTERVALO_RANDOM_MS, INTERVALO_RANDOM_MS);
  intervaloAtual = INTERVALO_BASE_MS + aleatorio;
}

void enviarTextoESPNow(const String& textoJson, bool piscarLongo = false) {
  esp_now_send(broadcastAddress, (const uint8_t *)textoJson.c_str(), textoJson.length());
  piscarLED(piscarLongo ? 300 : 50);
}

void enviarACK(const char* idDestino) {
  StaticJsonDocument<128> doc;
  doc["tipo"] = "ack";
  doc["idDestino"] = idDestino;
  doc["idAck"] = ID;

  String jsonString;
  serializeJson(doc, jsonString);
  enviarTextoESPNow(jsonString, false);
}

// Modificado para espelhar o envio local na Serial TX
void montarEEnviarInternos() {
  float temperatura = dht.readTemperature();
  float umidade     = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade)) {
    return;
  }

  contadorMensagens++;

  StaticJsonDocument<256> doc;
  doc["tipo"] = "data";
  doc["id"] = ID;
  doc["temp"] = (int)temperatura;
  doc["umid"] = (int)umidade;
  doc["count"] = contadorMensagens;

  String jsonString;
  serializeJson(doc, jsonString); 

  // 1. Envia via wireless na rede de borda
  String chave = String(ID) + "-" + String(contadorMensagens);
  registrarID(chave);
  enviarTextoESPNow(jsonString, true);
  
  // 2. ENVIAR VIA TX/RX PARA O GATEWAY (Adiciona quebra de linha \n no final)
  SERIAL_COM.println(jsonString);

  sortearProximoIntervalo();
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

// Modificado para que mensagens recebidas de vizinhos também sigam para o Gateway
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  char buffer[len + 1];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0'; 
  String jsonRecebido = String(buffer);

  StaticJsonDocument<256> doc;
  DeserializationError erro = deserializeJson(doc, jsonRecebido);
  if (erro) return;

  String tipo = doc["tipo"] | "";

  if (tipo == "ack") return;

  if (tipo == "data") {
    String idOrigem = doc["id"] | "";
    int count = doc["count"] | 0;
    
    enviarACK(idOrigem.c_str());

    String chave = idOrigem + "-" + String(count);
    if (jaVisto(chave)) return;
    registrarID(chave);

    // Repassa via rádio se não for seu
    if (idOrigem != ID) {
      enviarTextoESPNow(jsonRecebido, false);
      
      // ENVIA VIA TX/RX PARA O GATEWAY (O dado do vizinho também sobe para a nuvem!)
      SERIAL_COM.println(jsonRecebido);
    }
  }
}

void setup() {
  // Inicializa a serial compartilhada para log e comunicação TX/RX
  SERIAL_COM.begin(115200, SERIAL_8N1, 16, 17);
  randomSeed(analogRead(0));  
  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  for (int i = 0; i < 3; i++) { piscarLED(200); delay(200); }

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);

  ultimoEnvio = millis();
  sortearProximoIntervalo();
}

void loop() {
  unsigned long agora = millis();
  if (agora - ultimoEnvio >= intervaloAtual) {
    montarEEnviarInternos();
    ultimoEnvio = agora;
  }
  delay(100);
}
          