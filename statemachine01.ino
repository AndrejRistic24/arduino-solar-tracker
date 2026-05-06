#include <Servo.h>
int ldr1Pin = A0;  // LDR 1 (Levi senzor)
int ldr2Pin = A1;  // LDR 2 (Desni senzor)
int panelPin = A2;
int servoPin = 8;  // Servo motor pin

Servo myservo;     // Pravi servo objekat
int servoPos = 90; // Pocetna pozicija (moze se menjati ali utice na if funkciju

int ldr1Value = 0;
int ldr2Value = 0;
int threshold = 20; // tolerancija 
int stableCount = 0; // broj stabilnih merenja na pocetku
const int stableRequired = 5; //broj stabilnih merenja potrebno za ulazak u sleep mode

String command = ""; // ZA UNOS KOMANDE U SERIAL MONITORU

// Definisanje state-ova
enum State {TRACKING,SLEEP}; // tri stanja, moze i dva ali je ovako prirodniji prelaz
State currentState = TRACKING; // pocetno stanje od pocetka 


// Definisanje tajmera (ako je state machine izbegavati delay)
unsigned long lastAction = 0; // pocetak merenja od prosle akcije 
const unsigned long trackInterval = 15000; // period rada (sekunde x 1000)


// Definisanje napona zbog promene state-ova
//float lastVoltage = 0;
//float voltageThreshold = 0.1;

void setup() {
  
  myservo.attach(servoPin); // Prikljuci servo pinu 8
  myservo.write(servoPos);  // Pocetna pozicija serva
  
  pinMode(ldr1Pin, INPUT);  // LDR1 ulaz
  pinMode(ldr2Pin, INPUT);  // LDR2 ulaz 

  Serial.begin(9600);       // Serijska kom, upali serial monitor 
}



  // SVE FUNKCIJE OVDE IDU 
void readSensors() {
  ldr1Value = analogRead(ldr1Pin);
  ldr2Value = analogRead(ldr2Pin);
}

bool moveServo() {
  int diff = ldr1Value - ldr2Value;

    Serial.print("diff: ");
    Serial.println(diff);

  if (abs(diff) > threshold) {
    if (diff < 0 && servoPos < 175) { //ako je LDR2 jači pomeri panel udesno
      servoPos += 2;
      }
    else if (diff > 0 && servoPos > 5) {
      servoPos -= 2;
      }
      myservo.write(servoPos);
      return false;
  }
      else {
      stableCount++;
      return true;
    }
  } 

 float readPanelVoltage() {
      int raw = analogRead(panelPin); 
      float voltage = raw * (5.0 / 1023.0); // pretvara se u napon
      return voltage * 2; // zbog naponskog delilnika koji ga je podelio na dva 
 }

 void handleSerial() {
  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "c1") {
      currentState = TRACKING;
      myservo.attach(servoPin);
      Serial.println("Activating TRACKING MODE");
    }
    else if (command == "c2") {
      currentState = SLEEP;
      myservo.detach();
      Serial.println("Activating SLEEP MODE");
    }
  
  }
}

void loop() {
  
  handleSerial();
  unsigned long now = millis();

 switch(currentState) {

  case TRACKING:
  if (now - lastAction >= 500) { // brzo merenje dok traži poziciju
    lastAction = now;

    myservo.attach(servoPin);

    readSensors();
    bool stable = moveServo();

    float panelVoltage = readPanelVoltage();
    //lastVoltage = panelVoltage;

    Serial.print("LDR1: ");
    Serial.print(ldr1Value);
    Serial.print(" LDR2: ");
    Serial.print(ldr2Value);
    Serial.print(" stableCount: ");
    Serial.print(stableCount);
    Serial.print(" V: ");
    Serial.println(panelVoltage);

    //  posle 5 stabilnih citanja prelazi u sleep mode
    if (stable && stableCount >= stableRequired) {
      Serial.println("Entering sleep mode");
      myservo.detach();
      currentState = SLEEP;
      stableCount = 0;
    }
  }
  break;


   case SLEEP:
   if (now - lastAction >= trackInterval) {
    lastAction = now;

    /* float newVoltage = readPanelVoltage();

     Serial.print("Sleep check V: ");
     Serial.println(newVoltage);

     if(abs(newVoltage - lastVoltage) > voltageThreshold) {
      myservo.attach(servoPin);
      currentState = TRACKING;
     } */
   }
   break;
 }
}
