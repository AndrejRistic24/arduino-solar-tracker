#include <Servo.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

int ldr1Pin = A0;  // LDR 1 (Levi senzor)
int ldr2Pin = A1;  // LDR 2 (Desni senzor)
int panelPin = A2;
int servoPin = 8;  // Servo motor pin

Servo myservo;     // Pravi servo objekat
int servoPos = 90; // Pocetna pozicija motora

ThreeWire myWire(6, 5, 7); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

int ldr1Value = 0;
int ldr2Value = 0;
float filteredLdr1 = 0;
float filteredLdr2 = 0;
const int FORWARD_THRESHOLD = 50;
int IDLE_THRESHOLD = 90;

bool servoAttached = true;

String command = ""; // ZA UNOS KOMANDE U SERIAL MONITORU

// State-ovi
enum State {TRACKING, IDLE, SLEEP}; // dva stanja samo 
State currentState = TRACKING; // pocetno stanje od pocetka 

// Stanja pomeranja 
enum Move {MOVE_LEFT, STOP, MOVE_RIGHT};      


// Definisanje tajmera (ako je state machine izbegavati delay)
const unsigned long t_clock = 20; // brzo očitavanje
const unsigned long sleepInterval = 120000;
unsigned long idleStart = 0;
const unsigned long stableTime = 5000;
unsigned long lastAction = 0; // pocetak merenja od prosle akcije 
unsigned long sleepStart = 0;
unsigned long lastPrint = 0;
const unsigned long printInterval = 500;  // spor ispis

///////////////////////////////////////////////////////////////////////////
void setup() {
  
  myservo.attach(servoPin); // Prikljuci servo pinu 8
  myservo.write(servoPos);  // Pocetna pozicija serva
  
  pinMode(ldr1Pin, INPUT);  // LDR1 ulaz
  pinMode(ldr2Pin, INPUT);  // LDR2 ulaz 

  Rtc.Begin();
  //Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));

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
void moveServo()
{
  int curr_diff = ldr1Value - ldr2Value;
  int abs_curr_diff = abs(curr_diff);

  Move movement;

  ////// određivanje smera //////
  if (abs_curr_diff <= FORWARD_THRESHOLD) 
  {
    movement = STOP;
  }
  else if (curr_diff > 0) 
  {
    movement = MOVE_LEFT;
  }
  else 
  {
    movement = MOVE_RIGHT;
  }

  ////// određivanje brzine //////
  int step;

  if (abs_curr_diff > 400) {
    step = 5;
  }
  else if (abs_curr_diff > 180) {
    step = 2;
  }
  else {
    step = 1;
  }

  ////// pomeranje //////
  switch(movement) 
  {
    case MOVE_LEFT:
      servoPos -= step;
      break;
    case MOVE_RIGHT:
      servoPos += step;
      break;
    case STOP:
      return;
  }

  servoPos = constrain(servoPos, 1, 179); // granice 
  myservo.write(servoPos);
}

////////////////////////// SP ///////////////////////////////////////////////
 float readPanelVoltage() 
 {
    int raw = analogRead(panelPin); 
    float voltage = raw * (5.0 / 1023.0); // pretvara se u napon
    return voltage * 2; // zbog naponskog delilnika koji ga je podelio na dva 
 }

 //////////// UPISIVANJE KOMANDI ZA STATE-OVE //////////////////// 
 void handleSerial() 
 {
    if (Serial.available()) 
    {
      command = Serial.readStringUntil('\n');
      command.trim();

      if (command == "c1") 
      {
        currentState = TRACKING;
        myservo.attach(servoPin);
        Serial.println("Activating TRACKING MODE");
      }
      else if (command == "c2") 
      {
        currentState = IDLE;
        myservo.attach(servoPin);
        Serial.println("Activating IDLE STATE");
      }
      else if (command == "c3") 
      {
        currentState = SLEEP;
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
  int second = nowRtc.Second();

 switch(currentState) {
  
  /////////////////////////////////////////////
  case TRACKING:

  if (now - lastAction >= t_clock) 
  {
    lastAction = now;
    readSensors();   
    int curr_diff = ldr1Value - ldr2Value;
    int abs_curr_diff = abs(curr_diff);
    float panelVoltage = readPanelVoltage();
    moveServo();

    if (now - lastPrint >= printInterval) 
    {
      lastPrint = now;

      Serial.print("curr_diff: ");
      Serial.print(curr_diff);
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
      Serial.print(" V: ");
      Serial.println(panelVoltage);
    }
  
    ///////// iz TRACKING U IDLE ///////////////

    if (abs_curr_diff < FORWARD_THRESHOLD) 
    {
      currentState = IDLE;
      myservo.detach();
      idleStart = now;
    }
  }

  break;

  ///////////////////////////////////////////////////
  case IDLE:
  readSensors();
  int curr_diff = ldr1Value - ldr2Value;
  int abs_curr_diff = abs(curr_diff);


 ////////// iz IDLE u TRACKING /////////////
  if (abs_curr_diff > IDLE_THRESHOLD) 
  {
    currentState = TRACKING;
    myservo.attach(servoPin);
  }
  ///////// iz IDLE u SLEEP ///////////////
  else if (now - idleStart >= stableTime) 
  {
    myservo.detach();
    sleepStart = now;
    currentState = SLEEP;
    Serial.println("Activating SLEEP MODE");
  }
  break;

  /////////////////////////////////////////////////
  case SLEEP:

  if (now - sleepStart >= sleepInterval) // iz SLEEP-a u TRACKING
  {
    myservo.attach(servoPin);
    currentState = TRACKING;
  }
  break;
 }
}
