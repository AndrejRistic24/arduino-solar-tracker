#include <Servo.h>

#include <ThreeWire.h>
#include <RtcDS1302.h>

int ldr1Pin = A0;  // LDR 1 (Levi senzor)
int ldr2Pin = A1;  // LDR 2 (Desni senzor)
int panelPin = A2;
int servoPin = 8;  // Servo motor pin

Servo myservo;     // Pravi servo objekat
int servoPos = 90; // Pocetna pozicija (moze se menjati ali utice na if funkciju

ThreeWire myWire(6, 5, 7); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

int ldr1Value = 0;
int ldr2Value = 0;
float filteredLdr1 = 0;
float filteredLdr2 = 0;

//int threshold = 100; // tolerancija

// POKUSAJ HISTEREZISA
int enterThreshold = 110; 
int exitThreshold = 70;  

bool trackingActive = false;

int stableCount = 0; // broj stabilnih merenja na pocetku
const int stableRequired = 100; //broj stabilnih merenja potrebno za ulazak u sleep mode


String command = ""; // ZA UNOS KOMANDE U SERIAL MONITORU

// STATE-OVI
enum State {TRACKING,SLEEP}; // dva stanja samo 
State currentState = TRACKING; // pocetno stanje od pocetka 




// Definisanje tajmera (ako je state machine izbegavati delay)
unsigned long lastAction = 0; // pocetak merenja od prosle akcije 
const unsigned long trackingInterval = 15; // brzo očitavanje
const unsigned long printInterval = 1000;  // spor ispis
const unsigned long sleepInterval = 60000;
const unsigned long sleepPrintInterval = 5000; // utice na ispis posle komande za sleep
unsigned long sleepStart = 0;
unsigned long lastPrint = 0;

///////////////////////////////////////////////////////////////////////////
void setup() {
  
  filteredLdr1 = analogRead(ldr1Pin);
  filteredLdr2 = analogRead(ldr2Pin);

  Rtc.Begin();
  //Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));

  myservo.attach(servoPin); // Prikljuci servo pinu 8
  myservo.write(servoPos);  // Pocetna pozicija serva
  
  pinMode(ldr1Pin, INPUT);  // LDR1 ulaz
  pinMode(ldr2Pin, INPUT);  // LDR2 ulaz 

  Serial.begin(9600);       // Serijska kom, upali serial monitor 
}



  /////////////////////// SENZORI  /////////////////////////////
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
  /*if (absDiff <= threshold) {
    stableCount++;
    return true;
  } 
  else {
    stableCount = 0;
  } */


  ///////// pokusaj histerezis

  if (!trackingActive && absDiff > enterThreshold) {
    trackingActive = true;
    stableCount = 0;
  }

  if (trackingActive && absDiff < exitThreshold) {
    trackingActive = false;
  }

  ///// stabilan polozaj //////////////

  if (!trackingActive && absDiff < exitThreshold) {
    stableCount++;
  } 
  else if (!trackingActive) {
    stableCount = 0;
  }

  ///////  step i pomeranje  ////////////////

  int step = map(absDiff, enterThreshold, 1023, 1, 4);
  step = constrain(step, 1, 4);


  if (trackingActive) {

    if (diff < 0 && servoPos < 179) {
      servoPos += step;
    } 
    else if (diff > 0 && servoPos > 1) {
      servoPos -= step;
    }

    ////// ogranicenje za printanje podataka /////
    if (millis() - lastServoWrite > 20) {
      myservo.write(servoPos);
      lastServoWrite = millis();
    }
  }

  return (stableCount >= stableRequired);
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

  RtcDateTime nowRtc = Rtc.GetDateTime();

  int hour = nowRtc.Hour();
  int minute = nowRtc.Minute();

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

  /*if (hour >= 20 || hour < 6) {
  currentState = SLEEP;
  }
  else {
  currentState = TRACKING;
  }*/

  if (now - lastPrint >= printInterval) {

    lastPrint = now;

    float panelVoltage = readPanelVoltage();
     int diff = ldr1Value - ldr2Value;
     
    Serial.print("diff: ");
    Serial.print(diff);

    RtcDateTime nowRtc = Rtc.GetDateTime();

    Serial.print(" Time: ");

    Serial.print(nowRtc.Hour());
    Serial.print(":");
    Serial.print(nowRtc.Minute());  
    Serial.print(":");
    Serial.println(nowRtc.Second());
    
    Serial.print("LDR1: ");
    Serial.print(ldr1Value);

    Serial.print(" LDR2: ");
    Serial.print(ldr2Value);

    Serial.print(" Servo: ");
    Serial.print(servoPos);

    Serial.print(" Stable: ");
    Serial.print(stableCount); 
    Serial.print("%"); // da bi bilo 100% max
    
    Serial.print(" V: ");
    Serial.println(panelVoltage);
    }

    break;

    ///////////////////////////////////////////////////
   case SLEEP:

    myservo.detach();

    // print
    if (now - lastPrint >= sleepPrintInterval) {

    lastPrint = now;

    Serial.println("SLEEP MODE");
    } 

    // povratak u TRACKING
    if (now - sleepStart >= sleepInterval) {

    myservo.attach(servoPin);

    currentState = TRACKING;
    }

    break;
  }
}
