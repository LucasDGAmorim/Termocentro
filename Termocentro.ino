#include <PubSubClient.h>
#include <WiFi.h>
#include <PID_v1_bc.h>
#include <max6675.h>

// Defina os pinos para o MAX6675
int thermoDO = 19;  // MISO
int thermoCS = 5;   // Chip Select
int thermoCLK = 18; // Clock

// Defina os pinos para o controle da resistência
int outputPin = 4; // Saída para a resistência

// Configuração do Wi-Fi
const char* ssid = "WifiGuimares";
const char* password = "guimares@2024";

// Configuração do MQTT
const char* mqtt_server = "192.168.1.74"; // Endereço IP do broker MQTT
const int mqtt_port = 1883; // Porta padrão MQTT
const char* mqtt_topic = "termocentro_leitura"; // Tópico para publicar a temperatura
const char* mqtt_target_topic = "termocentro_target"; // Tópico para o Setpoint

WiFiClient espClient;
PubSubClient client(espClient);

// Parâmetros do MAX6675
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// Variáveis do PID
double Setpoint = 0.0, Input, Output;
double Kp = 0.8, Ki = 0.3, Kd = 1.0; // Ajuste inicial

// Instancia o objeto PID
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

// Variável para controle de tempo de publicação MQTT
unsigned long lastPublishTime = 0; // Armazena o último tempo de publicação
unsigned long publishInterval = 1000; // Intervalo de 1 segundo (1000 ms)

void setup_wifi() {
  delay(10);
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.println("Conectando ao MQTT...");
    if (client.connect("ESP32-Termocentro")) {
      Serial.println("Conectado ao MQTT");

      // Inscreve-se no tópico de Setpoint para alterar o valor
      client.subscribe(mqtt_target_topic);
    } else {
      Serial.print("Falha ao conectar. Estado: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// Função callback chamada quando uma nova mensagem chega no MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Verifica se a mensagem veio do tópico de Setpoint
  if (strcmp(topic, mqtt_target_topic) == 0) {
    payload[length] = '\0'; // Adiciona o terminador de string
    String message = String((char*)payload); // Converte o payload para String
    Setpoint = message.toDouble(); // Converte para double e atualiza o Setpoint
    Serial.print("Novo Setpoint recebido: ");
    Serial.println(Setpoint);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(outputPin, OUTPUT);

  // Inicializa o Wi-Fi e MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback); // Define a função de callback

  // Inicializa o PID
  myPID.SetMode(AUTOMATIC);
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  // Lê a temperatura do termopar
  Input = thermocouple.readCelsius();
  Serial.print("Temperatura: ");
  Serial.println(Input);

  // Controle On/Off com histerese
  if (Input >= Setpoint) {
    digitalWrite(outputPin, LOW); // Desliga a resistência
    Serial.println("Resistência DESLIGADA");
  } else if (Input <= Setpoint) {
    digitalWrite(outputPin, HIGH); // Liga a resistência
    Serial.println("Resistência LIGADA");
  }

  // Publica a temperatura a cada 1 segundo
  unsigned long currentTime = millis();
  if (currentTime - lastPublishTime >= publishInterval) {
    char payload[50];
    snprintf(payload, sizeof(payload), "{\"temperatura\": %.2f}", Input);
    client.publish(mqtt_topic, payload);
    Serial.println("Temperatura publicada no MQTT");
    lastPublishTime = currentTime; // Atualiza o último tempo de publicação
  }

  delay(200); // Intervalo de controle permanece em 200 ms
}
