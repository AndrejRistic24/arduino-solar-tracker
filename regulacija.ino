#include <Servo.h>

int ldr1Pin = A0;  // LDR 1 (Levi senzor)
int ldr2Pin = A1;  // LDR 2 (Desni senzor)
int panelPin = A2;
int servoPin = 8;  // Servo motor pin

Servo myservo;     // Pravi servo objekat
int servoPos = 90; // Pocetna pozicija (moze se menjati ali utice na if funkciju


int ldr1Value = 0;
int ldr2Value = 0;
float filteredLdr1 = 0;
float filteredLdr2 = 0;
int threshold = 100; 
int stableCount = 0; // broj stabilnih merenja na pocetku
const int stableRequired = 1000; //broj stabilnih merenja potrebno za ulazak u sleep mode

String command = ""; // ZA UNOS KOMANDE U SERIAL MONITORU

// State-ovi
enum State {TRACKING,SLEEP}; // dva stanja samo 
State currentState = TRACKING; // pocetno stanje od pocetka 




// Definisanje tajmera (ako je state machine izbegavati delay)
unsigned long lastAction = 0; // pocetak merenja od prosle akcije 
const unsigned long trackingInterval = 5; // brzo očitavanje
const unsigned long printInterval = 1000;  // spor ispis
const unsigned long sleepInterval = 60000;
unsigned long sleepStart = 0;
unsigned long lastPrint = 0;

///////////////////////////////////////////////////////////////////////////
void setup() {
  
  myservo.attach(servoPin); // Prikljuci servo pinu 8
  myservo.write(servoPos);  // Pocetna pozicija serva
  
  pinMode(ldr1Pin, INPUT);  // LDR1 ulaz
  pinMode(ldr2Pin, INPUT);  // LDR2 ulaz 

  Serial.begin(9600);       // Serijska kom, upali serial monitor 
}



  /////////////////////// SENZORI  /////////////////////////////
/*void readSensors() {
  long sum1 = 0;
  long sum2 = 0;

  for (int i = 0; i < 5; i++) {
    sum1 += analogRead(ldr1Pin);
    sum2 += analogRead(ldr2Pin);
  }

  ldr1Value = sum1 / 5;
  ldr2Value = sum2 / 5;
}*/

void readSensors() {

  int raw1 = analogRead(ldr1Pin);
  int raw2 = analogRead(ldr2Pin);

  filteredLdr1 = 0.8 * filteredLdr1 + 0.2 * raw1;
  filteredLdr2 = 0.8 * filteredLdr2 + 0.2 * raw2;

  ldr1Value = filteredLdr1;
  ldr2Value = filteredLdr2;
}
////////////////////////  MOTOR  ///////////////////////////////
bool moveServo() {

  static unsigned long lastServoWrite = 0;
  int diff = ldr1Value - ldr2Value;
  int absDiff = abs(diff);

 
  //////// stabilno ////////////
  if (absDiff <= threshold) {
    stableCount++;
    return true;
  } 
  else {
    stableCount = 0;
  }

  /////// brzina zavisi od diff-a /////////
  int step = map(absDiff, threshold, 1023, 1, 4);

  step = constrain(step, 1, 4);

  // pomeranje
  if (diff < 0 && servoPos < 179) {
    servoPos += step;
  }
  else if (diff > 0 && servoPos > 1) {
    servoPos -= step;
  }
  if (millis() - lastServoWrite > 20) {
    myservo.write(servoPos);
    lastServoWrite = millis();
  }
  

  return false;
}

////////////////////////// SP ///////////////////////////////////////////////
 float readPanelVoltage() {
      int raw = analogRead(panelPin); 
      float voltage = raw * (5.0 / 1023.0); // pretvara se u napon
      return voltage * 2; // zbog naponskog delilnika koji ga je podelio na dva 
 }

//////////// UPISIVANJE KOMANDI ZA STATE-OVE ////////////////////
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
      stableCount = 0;
      myservo.detach();
      Serial.println("Activating SLEEP MODE");
    }
  
  }
}

/////////////////////////////////////////////////////////////////////
void loop() {
  
  handleSerial();
  unsigned long now = millis();

 switch(currentState) {
  
/////////////////////////////////////////////
 case TRACKING:
  if (now - lastAction >= trackingInterval) {

    lastAction = now;

    readSensors();

    bool stable = moveServo();

    if (stable && stableCount >= stableRequired) {

      Serial.println("Entering sleep mode");

      myservo.detach();

      sleepStart = now;

      currentState = SLEEP;

      stableCount = 0;
    }
  }

  if (now - lastPrint >= printInterval) {

    lastPrint = now;

    float panelVoltage = readPanelVoltage();
     int diff = ldr1Value - ldr2Value;
     
    Serial.print("diff: ");
    Serial.println(diff);
    
    Serial.print("LDR1: ");
    Serial.print(ldr1Value);

    Serial.print(" LDR2: ");
    Serial.print(ldr2Value);

    Serial.print(" Servo: ");
    Serial.print(servoPos);

    Serial.print(" Stable: ");
    Serial.print(stableCount / 10); 
    Serial.print("%"); // da bi bilo 100% max
    
    Serial.print(" V: ");
    Serial.println(panelVoltage);
  }

break;

///////////////////////////////////////////////////
   case SLEEP:
   if (now - lastAction >= trackingInterval) {
    lastAction = now;

   if(now - sleepStart >= sleepInterval) {
      myservo.attach(servoPin);
      currentState = TRACKING;
     }
   
   }
   break;
 }
}
