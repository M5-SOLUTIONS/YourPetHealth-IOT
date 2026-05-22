#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL  6

const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port      = 1883;
const char* topic_dados    = "fiap/vet/pet01/dados";
const char* topic_alerta   = "fiap/vet/sala01/alerta";
const char* topic_cadastro = "fiap/vet/cadastro";

const int LED_ALERTA  = 26; 
const int LED_SISTEMA = 27;  
#define   DHTPIN 15
#define   DHTTYPE DHT22

const char* pet_nome  = "Rex";
const char* pet_tipo  = "Cachorro";
const char* pet_raca  = "Labrador";
const char* pet_tutor = "João Silva";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient   espClient;
PubSubClient client(espClient);

bool temperaturaDesconfortavel(float temp, const char* tipo) {
  if (strcmp(tipo, "Cachorro") == 0) return (temp < 18.0 || temp > 24.0);
  if (strcmp(tipo, "Gato")     == 0) return (temp < 20.0 || temp > 25.0);
  if (strcmp(tipo, "Coelho")   == 0) return (temp < 16.0 || temp > 21.0);
  return (temp < 18.0 || temp > 26.0);
}

String gerarMensagemAlerta(float temp, const char* tipo) {
  if (temp > 24.0)
    return "Sala MUITO QUENTE para " + String(tipo) + "! Ligue o ar-condicionado.";
  else
    return "Sala MUITO FRIA para " + String(tipo) + "! Aumente a temperatura.";
}

void setup_wifi() {
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    String clientId = "ESP32Vet-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" Conectado!");
      digitalWrite(LED_SISTEMA, HIGH); 

      StaticJsonDocument<256> cad;
      cad["id"]    = "pet01";
      cad["nome"]  = pet_nome;
      cad["tipo"]  = pet_tipo;
      cad["raca"]  = pet_raca;
      cad["tutor"] = pet_tutor;
      char bufCad[256];
      serializeJson(cad, bufCad);
      client.publish(topic_cadastro, bufCad, true);

    } else {
      Serial.print("Falhou. rc=");
      Serial.println(client.state());
      digitalWrite(LED_SISTEMA, LOW);
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_ALERTA,  OUTPUT);
  pinMode(LED_SISTEMA, OUTPUT);
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) reconnect_mqtt();
  client.loop();

  float temp = dht.readTemperature();

  if (isnan(temp)) {
    Serial.println("Erro ao ler DHT22!");
    return;
  }

  bool desconfortavel = temperaturaDesconfortavel(temp, pet_tipo);
  digitalWrite(LED_ALERTA, desconfortavel ? HIGH : LOW);

  StaticJsonDocument<256> doc;
  doc["id"]             = "pet01";
  doc["nome"]           = pet_nome;
  doc["tipo"]           = pet_tipo;
  doc["temp_ambiente"]  = temp;
  doc["desconfortavel"] = desconfortavel;

  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(topic_dados, buffer);
  Serial.println(buffer);

  if (desconfortavel) {
    String msg = gerarMensagemAlerta(temp, pet_tipo);

    StaticJsonDocument<200> alerta;
    alerta["sala"]     = "Sala 01";
    alerta["pet"]      = pet_nome;
    alerta["tipo"]     = pet_tipo;
    alerta["temp"]     = temp;
    alerta["mensagem"] = msg;

    char bufAlert[200];
    serializeJson(alerta, bufAlert);
    client.publish(topic_alerta, bufAlert);
    Serial.println("ALERTA: " + msg);
  }

  delay(10000);
}