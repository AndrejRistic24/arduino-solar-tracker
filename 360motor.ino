//=============================================================================
// SOLAR TRACKER
//=============================================================================

#include <Servo.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>


//-----------------------------------------------------------------------------
// PIN DEFINICIJE
//-----------------------------------------------------------------------------
const int LDR1_PIN   = A0;   // Levi LDR senzor
const int LDR2_PIN   = A1;   // Desni LDR senzor
const int PANEL_PIN  = A2;   // Solarni panel 
const int SERVO_PIN  = 8;
const int BUTTON_PIN = 2;
const int HALL_PIN   = 4;

// RTC (DS1302): DAT, CLK, RST
const int RTC_DAT = 6;
const int RTC_CLK = 5;
const int RTC_RST = 7;


//-----------------------------------------------------------------------------
// KONSTANTE I PRAGOVI/TOLERANCIJE
//-----------------------------------------------------------------------------
// Servo
const int STOP_SIGNAL = 1490;   // PWM vrednost za "motor stoji"

// LDR pragovi
const int FORWARD_THRESHOLD = 120;    // ispod = "balansirano"
const int IDLE_THRESHOLD    = 220;   // iznad = izlazak iz IDLE u TRACKING

// Tajmeri (ms)
const unsigned long T_CLOCK              = 20;     // brzina petlje za tracking
const unsigned long PRINT_INTERVAL       = 500;    // brzina ispisa
const unsigned long STABLE_TIME          = 8000;   // koliko mora biti u IDLE pre SLEEP
const unsigned long SLEEP_INTERVAL       = 8000;   // koliko spava pre SCAN-a
const unsigned long HALL_STOP_TIME       = 500;    // pauza nakon udara magneta
const unsigned long STUCK_TIMEOUT        = 2000;   // zaglavljen pre backoff
const unsigned long POST_BACKOFF_TIMEOUT = 400;    // trajanje trackinga nakon backoffa
const unsigned long SCAN_MIN_TIME        = 500;    // ignorise hall/peak na pocetku faze
const unsigned long SLEEP_OFF_HALL       = 180;    // pomeranje sa halla pre sleep-a

// Serial
const long SERIAL_BAUD = 9600;


//-----------------------------------------------------------------------------
// ENUMS I TIPOVI
//-----------------------------------------------------------------------------
enum State     { TRACKING, IDLE, SLEEP, SCAN };
enum Move      { MOVE_LEFT, STOP, MOVE_RIGHT };
enum ScanPhase { SCAN_POSITIONING, SCAN_CALIBRATION, SCAN_BACK_TO_FIRST };


//-----------------------------------------------------------------------------
// HARDWARE OBJEKTI
//-----------------------------------------------------------------------------
Servo myservo;
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);


//-----------------------------------------------------------------------------
// STATE VARIJABLE
//-----------------------------------------------------------------------------

// --- Glavni state machine ---
State currentState = TRACKING;
ScanPhase scanPhase;

// --- Senzori ---
int   ldr1Value = 0;
int   ldr2Value = 0;
float filteredLdr1 = 0;
float filteredLdr2 = 0;

// --- Kretanje motora ---
Move lastMovement     = STOP;
Move backoffDirection = STOP;
Move scanDirection    = MOVE_RIGHT;
Move sleepMagnetSide  = STOP;     // pamti sa koje strane je bio hall pred sleep

// --- Hall senzor ---
bool hallDetected      = false;
bool previousHallState = false;
bool hallEdge          = false;
bool blockLeft         = false;
bool blockRight        = false;
unsigned long hallStopUntil = 0;

// --- Stuck / backoff ---
unsigned long stuckStart = 0;
bool backingOff  = false;
bool backoffUsed = false;          // jedan backoff po sleep ciklusu

// --- Post-backoff prozor ---
bool postBackoffPending = false;   // ceka hallStop pre aktivacije
bool postBackoffWindow  = false;
unsigned long postBackoffStart = 0;

// --- SCAN ---
unsigned long scanStartTime        = 0;
unsigned long calibrationStartTime = 0;
unsigned long firstPeakTime        = 0;
unsigned long returnDuration       = 0;
float firstPeakVoltage  = 0;
int   peaksFound        = 0;
bool  wasAboveTolerance = true;

// --- Tajmeri ---
unsigned long lastAction = 0;
unsigned long lastPrint  = 0;
unsigned long idleStart  = 0;
unsigned long sleepStart = 0;

// --- Korisnicki interfejs ---
bool   paused = false;
bool   lastButtonState = HIGH;
String command = "";

///////////////////////////////////////////////////////////////////////////
void setup() {

  myservo.writeMicroseconds(STOP_SIGNAL);  // Pocetna pozicija serva (od 1460 do 1524 stoji)
  myservo.attach(SERVO_PIN);                // Prikljuci servo pinu 8

  
  pinMode(LDR1_PIN, INPUT);                 // LDR1 ulaz
  pinMode(LDR2_PIN, INPUT);                 // LDR2 ulaz 
  pinMode(BUTTON_PIN, INPUT_PULLUP);        // Taster
  pinMode(HALL_PIN, INPUT_PULLUP);
  previousHallState = (digitalRead(HALL_PIN) == LOW); 

  Rtc.Begin();
  //Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));

  filteredLdr1 = analogRead(LDR1_PIN);
  filteredLdr2 = analogRead(LDR2_PIN);

  Serial.begin(SERIAL_BAUD);       // Serijska kom, upali serial monitor 
}

  /////////////////////// SENZORI  /////////////////////////////

void readSensors() {

  int raw1 = analogRead(LDR1_PIN);
  int raw2 = analogRead(LDR2_PIN);

  filteredLdr1 = 0.8 * filteredLdr1 + 0.2 * raw1;
  filteredLdr2 = 0.8 * filteredLdr2 + 0.2 * raw2;

  ldr1Value = filteredLdr1;
  ldr2Value = filteredLdr2;
}
////////////////////////  MOTOR  ///////////////////////////////
void moveServo()
{
  int change = 40;
  if (backingOff)
  {
    lastMovement = backoffDirection;
    if (backoffDirection == MOVE_LEFT)
    myservo.writeMicroseconds(STOP_SIGNAL - change);
    else if (backoffDirection == MOVE_RIGHT)
    myservo.writeMicroseconds(STOP_SIGNAL + change);
    return;
  }

  int curr_diff = ldr1Value - ldr2Value;
  int abs_curr_diff = abs(curr_diff);

  Move movement;

  ///////// odredjivanje smera //////////

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

  //////////// odredjivanje brzine ////////////

  int offset;

  if(abs_curr_diff > 400)
  {
    offset = 38;
  }
  else if (abs_curr_diff > 180)
  {
    offset = 38;
  }
   else 
  {
    offset = 38;
  }

  if (movement != STOP) lastMovement = movement;

  if (millis() < hallStopUntil)
  {
    myservo.writeMicroseconds(STOP_SIGNAL);
    return;
  }

  if (movement == MOVE_LEFT  && blockLeft)  movement = STOP;
  if (movement == MOVE_RIGHT && blockRight) movement = STOP;
  ////// upravljanje servom //////////

  switch(movement)
  {
    case MOVE_LEFT:
    myservo.writeMicroseconds(STOP_SIGNAL - offset);
    break;

    case MOVE_RIGHT:
    myservo.writeMicroseconds(STOP_SIGNAL + offset);
    break;

    case STOP:
    myservo.writeMicroseconds(STOP_SIGNAL);
    break;
  }
}

////////////////////////// SP ///////////////////////////////////////////////
 float readPanelVoltage() 
 {
    int raw = analogRead(PANEL_PIN); 
    float voltage = raw * (5.0 / 1023.0); // pretvara se u napon
    return voltage * 2; // zbog naponskog delilnika koji ga je podelio na dva 
 }

//////////// SKLANJANJE SA MAGNETA PRE SLEEP-a ///////////////////////
void moveFromHall()
{
  int change = 50;
  if (blockLeft)
  {
    sleepMagnetSide = MOVE_LEFT;
    myservo.writeMicroseconds(STOP_SIGNAL + change);  // ovo je u desno 
    delay(SLEEP_OFF_HALL);
    myservo.writeMicroseconds(STOP_SIGNAL);
  }
  else if (blockRight)
  {
    sleepMagnetSide = MOVE_RIGHT;
    myservo.writeMicroseconds(STOP_SIGNAL - change);  // ovo je u levo
    delay(SLEEP_OFF_HALL);
    myservo.writeMicroseconds(STOP_SIGNAL);
    Serial.println("MOVED OFF MAGNET TO THE LEFT BEFORE SLEEP");
  }
  else
  {
    sleepMagnetSide = STOP;
  }
}

 //////////// UPISIVANJE KOMANDI ZA STATE-OVE //////////////////// 
 void readCommands() 
 {
    if (Serial.available()) 
    {
      command = Serial.readStringUntil('\n');
      command.trim();

      if (command == "c1") 
      {
        currentState = TRACKING;
        myservo.attach(SERVO_PIN);
        Serial.println("Activating TRACKING MODE");
      }
      else if (command == "c2") 
      {
        currentState = IDLE;
        myservo.attach(SERVO_PIN);
        Serial.println("Activating IDLE STATE");
      }
      else if (command == "c3") 
      {
        moveFromHall();
        currentState = SLEEP;
        myservo.writeMicroseconds(STOP_SIGNAL);
        Serial.println("Activating SLEEP MODE");
      }
       else if (command == "c4")
      {
        currentState = SCAN;
        scanPhase = SCAN_POSITIONING;
        scanStartTime = millis();
        peaksFound = 0;
        firstPeakVoltage = 0;
        firstPeakTime = 0;
        scanDirection = MOVE_RIGHT;  // faza 1 uvek ide u desno
        myservo.attach(SERVO_PIN);
        Serial.println("Activating SCAN MODE");
      } 
      else if (command == "reset")
      {
        blockLeft  = false;
        blockRight = false;
        Serial.println("Hall limits cleared");
      }
  
    }
  }

/////////////////////////////////////////////////////////////////////
//=============================================================================
// TASTER (pause/resume)
//=============================================================================
void handleButton()
{
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && currentButtonState == LOW)
  {
    paused = !paused;

    if (paused)
    {
      Serial.println("SYSTEM PAUSED");
    }
    else
    {
      Serial.println("SYSTEM RESUMED");
    }

    delay(200);  // debounce
  }
  lastButtonState = currentButtonState;
}

//=============================================================================
// HALL SENZOR (edge detekcija + auto-block)
//=============================================================================
void handleHall()
{
  hallDetected = (digitalRead(HALL_PIN) == LOW);
  hallEdge = (hallDetected && !previousHallState);
  previousHallState = hallDetected;

  if (hallEdge)
  {
    if (backingOff)
    {
      backingOff = false;
      postBackoffPending = true;  // window pocinje tek kad hallStop istekne
      Serial.println("BACKOFF COMPLETE - OTHER MAGNET REACHED");
    }
    if (lastMovement == MOVE_LEFT)  blockLeft  = true;
    if (lastMovement == MOVE_RIGHT) blockRight = true;
    hallStopUntil = millis() + HALL_STOP_TIME;
    Serial.println("MAGNET DETECTED - LIMIT REACHED");
  }

  if (!hallDetected)
  {
    blockLeft  = false;
    blockRight = false;
  }
}

//=============================================================================
// IDLE state
//=============================================================================
void handleIdle(unsigned long now)
{
  readSensors();
  int curr_diff = ldr1Value - ldr2Value;
  int abs_curr_diff = abs(curr_diff);


  ////////// iz IDLE u TRACKING /////////////
  if (abs_curr_diff > IDLE_THRESHOLD) 
  {
    currentState = TRACKING;
    myservo.attach(SERVO_PIN);
  }
  ///////// iz IDLE u SLEEP ///////////////
  else if (now - idleStart >= STABLE_TIME) 
  {
    moveFromHall();
    myservo.writeMicroseconds(STOP_SIGNAL);
    sleepStart = now;
    currentState = SLEEP;
    backoffUsed = false;   // reset - backoff se moze ponovo iskoristiti u sledecem ciklusu
    backingOff = false;
    postBackoffWindow = false;
    postBackoffPending = false;
    Serial.println("Activating SLEEP MODE");
  }
}

//=============================================================================
// SLEEP state
//=============================================================================
void handleSleep(unsigned long now)
{
  if (now - sleepStart >= SLEEP_INTERVAL)
  {
    myservo.attach(SERVO_PIN);
    scanPhase = SCAN_POSITIONING;
    scanStartTime = now;
    peaksFound = 0;
    firstPeakVoltage = 0;
    firstPeakTime = 0;
    scanDirection = MOVE_RIGHT;
    currentState = SCAN;
    Serial.println("Activating SCAN MODE");
  }
}

//=============================================================================
// SCAN state
//=============================================================================
void handleScan(unsigned long now)
{
  readSensors();
  float currentVoltage = readPanelVoltage();
  int abs_diff = abs(ldr1Value - ldr2Value);
  int change = 50;

  ////////////// FAZA 1 - POZICIONIRANJE - do hall senzora ////////////
  if (scanPhase == SCAN_POSITIONING)
  {
    if (scanDirection == MOVE_RIGHT) 
    {  
      myservo.writeMicroseconds(STOP_SIGNAL + change);
    }
    else
    {
      myservo.writeMicroseconds(STOP_SIGNAL - change);
    }
    lastMovement = scanDirection;

    // kada dodje do halla, prelazi u kalibraciju
    if (hallEdge)
    {
      myservo.writeMicroseconds(STOP_SIGNAL);
      // okreni smer
      scanDirection = (scanDirection == MOVE_RIGHT) ? MOVE_LEFT : MOVE_RIGHT;
      scanPhase = SCAN_CALIBRATION;
      calibrationStartTime = now;
      peaksFound = 0;
      wasAboveTolerance = (abs_diff >= FORWARD_THRESHOLD);
      Serial.println("- STARTING CALIBRATION");
    }
  }
  ////////////// FAZA 2 - KALIBRACIJA ////////////
  else if (scanPhase == SCAN_CALIBRATION)
  {
    if (scanDirection == MOVE_RIGHT) 
    {  
      myservo.writeMicroseconds(STOP_SIGNAL + change);
    }
    else
    {
      myservo.writeMicroseconds(STOP_SIGNAL - change);
    }
    lastMovement = scanDirection;

    bool inTolerance = (abs_diff < FORWARD_THRESHOLD); // ako stalno govori jedan peak mozda promeniti toleranciju (problem je i polozaj LDRa)

    // detekcija ulaska u peak
    // ignorise prvi SCAN_MIN_TIME period (zbog hall stop i da se motor zaista pokrene)
    if ((now - calibrationStartTime > SCAN_MIN_TIME) && wasAboveTolerance && inTolerance)
    {
      peaksFound++;
      if (peaksFound == 1)
      {
        firstPeakVoltage = currentVoltage;
        firstPeakTime = now;
        Serial.print("PEAK 1 = ");
        Serial.print(firstPeakVoltage);
        Serial.println(" V");
      }
      else if (peaksFound == 2)
      {
        float secondPeakVoltage = currentVoltage;
        myservo.writeMicroseconds(STOP_SIGNAL);
        Serial.print("PEAK 2 = ");
        Serial.print(secondPeakVoltage);
        Serial.println(" V");

        if (secondPeakVoltage > firstPeakVoltage)
        {
          Serial.println("PEAK 2 IS BETTER => STAYING");
          currentState = TRACKING;
        }
        else
        {
          // vrati se na poziciju prvog peaka
          returnDuration = now - firstPeakTime;
          scanStartTime = now;
          scanPhase = SCAN_BACK_TO_FIRST;
          Serial.println("PEAK 1 IS BETTER => RRETURNING");
        }
      }
      wasAboveTolerance = false; // izasli iz tolerance zone tek kad diff opet bude veci
    }
    else if (!inTolerance)
    {
      wasAboveTolerance = true;  // izvan tolerancije, spreman za detekciju sledeceg peaka
    }

    // fallback: ako udari u drugi magnet a nismo nasli 2 peaka, idi u TRACKING
    if (hallEdge && (now - calibrationStartTime > SCAN_MIN_TIME))
    {
      myservo.writeMicroseconds(STOP_SIGNAL);
      Serial.print("CALIBRATION REACHED HALL WITH ");
      Serial.print(peaksFound);
      Serial.println(" PEAKS => GOING TO TRACKING");
      currentState = TRACKING;
    }
  }
  ////////////// FAZA 3 (ne mora da postoji) - POVRATAK NA PRVU VREDNOST ////////////
  else if (scanPhase == SCAN_BACK_TO_FIRST)
  {
    Move returnDir = (scanDirection == MOVE_RIGHT) ? MOVE_LEFT : MOVE_RIGHT;
    if (returnDir == MOVE_RIGHT)
    {
      myservo.writeMicroseconds(STOP_SIGNAL + change);
    }
    else
    {
      myservo.writeMicroseconds(STOP_SIGNAL - change);
    }
    lastMovement = returnDir;

    if (now - scanStartTime >= returnDuration)
    {
      myservo.writeMicroseconds(STOP_SIGNAL);
      currentState = TRACKING;
      Serial.println("ARRIVED AT FIRST PEAK => TRACKING");
    }
  }
}

//=============================================================================
// TRACKING state
//=============================================================================
void handleTracking(unsigned long now)
{
  if (now - lastAction >= T_CLOCK) 
  {
    lastAction = now;
    RtcDateTime nowRtc = Rtc.GetDateTime();
    readSensors();   
    int curr_diff = ldr1Value - ldr2Value;
    int abs_curr_diff = abs(curr_diff);
    float panelVoltage = readPanelVoltage();
    moveServo();

    Move demandedMovement = STOP;
    if (abs_curr_diff > FORWARD_THRESHOLD)
    {
      if (curr_diff > 0)
      {
        demandedMovement = MOVE_LEFT;
      }
      else
      {
        demandedMovement = MOVE_RIGHT;
      }     
    }

    bool demandingBlocked = 
      (demandedMovement == MOVE_LEFT  && blockLeft) ||
      (demandedMovement == MOVE_RIGHT && blockRight);

    if (demandingBlocked && !backingOff && !backoffUsed)
    {
      if (stuckStart == 0)
      {
        stuckStart = now;
      }
      else if (now - stuckStart > STUCK_TIMEOUT)
      {
        if (blockLeft)
        {
          backoffDirection = MOVE_RIGHT;
        }
        else
        {
          backoffDirection = MOVE_LEFT;
        }
        backingOff = true;
        backoffUsed = true;
        stuckStart = 0;
        Serial.println("STUCK - BACKING OFF UNTIL OTHER MAGNET");
      }
    }
    else if (!demandingBlocked)
    {
      stuckStart = 0;
    }

    if (now - lastPrint >= PRINT_INTERVAL) 
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
      Serial.print(" V: ");
      Serial.println(panelVoltage);
    }
  
    ///////// Tek kada hallStop istekne se aktivira period rada "post backoffa" ///////
    if (postBackoffPending && now >= hallStopUntil)
    {
      postBackoffPending = false;
      postBackoffWindow = true;
      postBackoffStart = now;
    }

    ///////// iz TRACKING U IDLE ///////////////

    if (!backingOff && abs_curr_diff < FORWARD_THRESHOLD) 
    {
      currentState = IDLE;
      myservo.writeMicroseconds(STOP_SIGNAL);
      idleStart = now;
      postBackoffWindow = false;  // nasao dobru poziciju, prekid window-a
      postBackoffPending = false;
    }
    /////////ako se posle backoffa za kratak period ne nadje dobra pozicija///////
    else if (postBackoffWindow && (now - postBackoffStart > POST_BACKOFF_TIMEOUT))
    {
      postBackoffWindow = false;
      moveFromHall();
      myservo.writeMicroseconds(STOP_SIGNAL);
      sleepStart = now;
      currentState = SLEEP;
      backoffUsed = false;
      Serial.println("NO GOOD POSITION FOUND AFTER BACKOFF - ENTERING SLEEP MODE");
    }
  }
}

/////////////////////////////////////////////////////////////////////
void loop() {
  
  readCommands();
  handleButton();
  if (paused)
  {
    myservo.writeMicroseconds(STOP_SIGNAL);
    return;
  }

  unsigned long now = millis();
  handleHall();

 switch(currentState) 
 {
  
  /////////////////////////////////////////////
  case TRACKING:
  {
    handleTracking(now);
    break;
  }
  ///////////////////////////////////////////////////
  case IDLE:
  {
    handleIdle(now);
    break;
  }

  /////////////////////////////////////////////////
  case SLEEP:
  {
    handleSleep(now);
    break;
  }

  case SCAN:
  {
    handleScan(now);
    break;
  }
 }
}