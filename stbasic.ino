#include <Servo.h>
int ldr1Pin = A0;  // LDR 1 (Levi senzor)
int ldr2Pin = A1;  // LDR 2 (Desni senzor)
int panelPin = A2;
int servoPin = 8;  // Servo motor pin
Servo myservo;     // Pravi servo objekat
int ldr1Value = 0;

int ldr2Value = 0; 
int servoPos = 90; // Pocetna pozicija (moze se menjati ali utice na if funkciju
void setup() {
  myservo.attach(servoPin); // Prikljuci servo pinu 8
  pinMode(ldr1Pin, INPUT);  // LDR1 ulaz
  pinMode(ldr2Pin, INPUT);  // LDR2 ulaz 
  myservo.write(servoPos);  // Pocetna pozicija serva
  Serial.begin(9600);       // Serijska kom, upali serial monitor 
}
int threshold = 20; // tolerancija

void loop() {
  ldr1Value = analogRead(ldr1Pin);
  ldr2Value = analogRead(ldr2Pin);

  int diff = ldr1Value - ldr2Value;

  int raw = analogRead(panelPin); 
  float voltage = raw * (5.0 / 1023.0); // pretva`ra se u napon
  float panelVoltage = voltage * 2; // mnozi se napon jer se deli sa dva
  
  Serial.print("LDR1: ");
  Serial.print(ldr1Value);
  Serial.print(" LDR2: ");
  Serial.print(ldr2Value);
  Serial.print(" V: ");
  Serial.println(panelVoltage); // println zbog novog reda
  delay(100); 

  if (abs(diff) > threshold) {
    if (diff < 0) {
      //ako je LDR2 jači pomeri panel udesno
      if (servoPos < 180) { // moze se smanjiti ugao za 5 do 10 stepeni if (servoPos < 170) ...
        servoPos += 1;
        myservo.write(servoPos);
      }
    } else {
      // ako je LDR1 jači pomeri panel u levo
      if (servoPos > 0) { //if (servoPos > 10) ...
        servoPos -= 1;
        myservo.write(servoPos);
      }
    }
  }

  delay(100);
}
