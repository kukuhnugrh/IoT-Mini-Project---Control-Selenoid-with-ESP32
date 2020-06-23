#include <WiFi.h>
#include <MQTT.h>
#include <SPI.h>
#include <MFRC522.h>

#define timeSeconds 10

const char ssid[] = "YOUR WIFI NAME";
const char pass[] = "YOUR PASSWORD";

const int RST_PIN = 22; // Reset pin
const int SS_PIN = 21; // Slave select pin //MISO
const int relayPin1 = 15;
const int relayPin2 = 4;
const int motionSensor = 5;
const int analogSound = 32;

int pirState = LOW;
int clap = 0;
long detection_range_start = 0;
long detection_range = 0;
boolean status_lights = false;

String pesan="Access Denied";

WiFiClient net;
MQTTClient client;
MFRC522 mfrc522(SS_PIN, RST_PIN);

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("Selenoid", "YOUR MQTT USERNAME", "YOUR MQTT PASSWORD")) { //client_id, username, password
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  client.subscribe("/setSystem");
  client.subscribe("/selenoid");
  client.subscribe("/clapon");
}

void cardReader(){
  String uid;
  String temp;
  for(int i=0;i<4;i++){
    if(mfrc522.uid.uidByte[i]<0x10){
      temp = "0" + String(mfrc522.uid.uidByte[i],HEX);
    }else {
      temp = String(mfrc522.uid.uidByte[i],HEX);
    } 
    if(i==3){
      uid =  uid + temp;
    }else {
      uid =  uid + temp+ " ";
    }
  }
  Serial.println("UID "+uid);
  String grantedAccess = "77 ab 4d 34"; //Akses RFID yang ditunjuk
  grantedAccess.toLowerCase();

  if (uid == grantedAccess) {
     if(pesan == "Access Denied"){
      Serial.println("Access Granted");
      client.publish("/getSystem", "1");
      pesan = "Access Granted";
     }
  }else{
    Serial.println("Access Denied");
    client.publish("/getSystem", "0");
    pesan = "Access Denied";
    client.publish("/relay", "Tertutup");
    client.publish("/lockstatus", "0");
    client.publish("/lampu", "Mati");
    client.publish("/clapstatus", "0");
  }
  Serial.println("\n");
  mfrc522.PICC_HaltA();
}

void messageReceived(String &topic, String &payload) {
  if(topic =="/setSystem"){
    if(payload == "1"){
      pesan = "Access Granted";
    }else{
      pesan = "Access Denied";
      client.publish("/relay", "Tertutup");
      client.publish("/lockstatus", "0");
      client.publish("/lampu", "Mati");
      client.publish("/clapstatus", "0");
    }
  }
  if(pesan == "Access Granted"){
    if(topic =="/selenoid"){
      if(payload == "1"){
        client.publish("/relay", "Terbuka");
        digitalWrite(relayPin1, HIGH);
      }else{
        client.publish("/relay", "Tertutup");
        digitalWrite(relayPin1, LOW);
      }
    }else if(topic =="/clapon"){
      if(payload == "1"){
        client.publish("/lampu", "Menyala");
        digitalWrite(relayPin2, HIGH);
      }else{
        client.publish("/lampu", "Mati");
        digitalWrite(relayPin2, LOW);
      }
    }
    Serial.println("incoming: " + topic + " - " + payload);
  }
}

void pirSensor(){
  if(digitalRead(motionSensor)){
    client.publish("/relay", "Terbuka");
    client.publish("/lockstatus", "1");
    digitalWrite(relayPin1, HIGH);
  }else{
    client.publish("/relay", "Tertutup");
    client.publish("/lockstatus", "0");
    digitalWrite(relayPin1, LOW);
  }
}

void soundSensor(){
  
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, pass);

  pinMode(relayPin1, OUTPUT);
  digitalWrite(relayPin1, LOW);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin2, LOW);
  pinMode(motionSensor, INPUT);
  pinMode(analogSound, INPUT);

  while (!Serial); // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
 
  client.begin("broker.shiftr.io", net);
  client.onMessage(messageReceived);
  connect();

  client.publish("/getSystem", "0");
  client.publish("/relay", "Tertutup");
  client.publish("/lockstatus", "0");
  client.publish("/lampu", "Mati");
  client.publish("/clapstatus", "0");
}

void loop() {
  client.loop();
  delay(100);
  
  if (!client.connected()) {
    connect();
  }

  if(pesan == "Access Granted"){
    pirSensor();
    float Analog = analogRead(analogSound);
    if(Analog>1000){
      if(clap == 0){
        detection_range = millis();
        detection_range_start = detection_range;
        clap++;
      }else if (clap > 0 && millis()-detection_range >= 50){
        detection_range = millis();
        clap++;
      }
    }
    Serial.println (Analog);
    if (millis()-detection_range_start >= 400){
      if (clap == 2){
        if (!status_lights){
          status_lights = true;
          client.publish("/lampu", "Menyala");
          client.publish("/clapstatus", "1");
          digitalWrite(relayPin2, HIGH);
        }else if (status_lights){
          status_lights = false;
          client.publish("/lampu", "Mati");
          client.publish("/clapstatus", "0");
          digitalWrite(relayPin2, LOW);
        }
       }
       clap = 0;
    } 
  }

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
  return;
  }
   
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  cardReader();
}
