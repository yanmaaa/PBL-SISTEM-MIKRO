#include <Wire.h> 
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include "RTClib.h" 
#include <Servo.h>

const int TRIG_PIN = D5;
const int ECHO_PIN = D6;
const int SERVO_PIN = D8;
const int PUMP1_PIN = D0;
const int PUMP2_PIN = D7;

hd44780_I2Cexp lcd; 
RTC_DS3231 rtc;
Servo myservo;

int readUltrasonicDistance(); 
void runCountdown(const char* actionName); 
void checkSchedule(DateTime now, int distance); 
void feedFish();
void startKuras();
void controlKurasAir(int distance);
void displayModeStatus();
void displayTimeAndDate(DateTime now);
void displayNormalStatus(int distance);
void serialEvent();

const int JARAK_AIR_HABIS_CM = 12;
const int JARAK_AIR_PENUH_CM = 3;

bool isKurasActive = false;
bool isDemoMode = false;
bool pakanDoneToday = false;
String inputString = "";

void setup() {
  Serial.begin(115200);
  inputString.reserve(10); 

  pinMode(PUMP1_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, LOW);
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP2_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(); 
  if (!rtc.begin()) {
    Serial.println("Tidak dapat menemukan sensor RTC");
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // UNTUK SETTING WAKTU PERTAMA KALI

  myservo.attach(SERVO_PIN);
  myservo.write(0);
  lcd.begin(16, 2); 
  lcd.print("Setup Complete!");
  delay(1500);
  lcd.clear();
  Serial.println("System Ready. Ketik 'ganti' untuk beralih mode.");
  Serial.print("Mode Saat Ini: ");
  Serial.println(isDemoMode ? "DEMO" : "ASLI (RTC)");
}

void loop() {
  DateTime now = rtc.now();
  int distance = readUltrasonicDistance(); 
  serialEvent();

  if (isKurasActive) {
    controlKurasAir(distance);
    pakanDoneToday = true; 
  } else {
    checkSchedule(now, distance);
    displayModeStatus();
    displayTimeAndDate(now);
    displayNormalStatus(distance);
  }
  delay(500); 
}

void serialEvent() {
  while (Serial.available()) {
  char inChar = (char)Serial.read();
  inputString += inChar;

    if (inChar == '\n') {
      inputString.trim(); 
      if (inputString.equalsIgnoreCase("ganti")) {
        isDemoMode = !isDemoMode; 
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("MODE BERGANTI!");
        lcd.setCursor(0, 1);
        lcd.print(isDemoMode ? "-> DEMO MODE" : "-> MODE ASLI");
        Serial.print("Mode Diubah ke: ");
        Serial.println(isDemoMode ? "DEMO" : "ASLI (RTC)");
        delay(3000);
        lcd.clear();
      }
      inputString = ""; 
      }
  }
}

void checkSchedule(DateTime now, int distance) {
    bool isFeedTimeRTC = (now.hour() == 14 && now.minute() == 0 && now.second() == 0);
    bool isFeedTimeDemo = (now.second() == 10); 
    
    if ((isDemoMode && isFeedTimeDemo) || (!isDemoMode && isFeedTimeRTC)) {
        if (!pakanDoneToday) {
            runCountdown("PAKAN IKAN");
            feedFish();
            pakanDoneToday = true; 
        }
    }
    if (now.hour() != 14) { 
        pakanDoneToday = false; 
    }

    bool isKurasTimeRTC = (now.dayOfTheWeek() == 0 && now.hour() == 10 && now.minute() == 0 && now.second() == 0);
    bool isKurasTimeDemo = (now.second() == 20); 

    if ((isDemoMode && isKurasTimeDemo) || (!isDemoMode && isKurasTimeRTC)) {
        runCountdown("KURAS AIR");
        startKuras();
    }
}

void runCountdown(const char* actionName) {
    lcd.clear();
    for (int i = 5; i >= 1; i--) {
        lcd.setCursor(0, 0);
        lcd.print(actionName);
        lcd.print("...");
        
        lcd.setCursor(0, 1);
        lcd.print("MULAI DALAM ");
        lcd.print(i);
        lcd.print(" DETIK!");
        
        Serial.print(actionName);
        Serial.print(" dalam: ");
        Serial.println(i);
        
        delay(1000);
    }
    lcd.clear();
    Serial.println("Aksi Dimulai!");
}

void feedFish() {
  Serial.println("Pakan Diberikan!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MEMBERI PAKAN!");
  myservo.write(90);
  delay(500);
  myservo.write(0);
  lcd.clear(); 
}

void startKuras() {
  if (!isKurasActive) {
    Serial.println("Kuras Dimulai! Pompa 1 ON.");
    isKurasActive = true;
    digitalWrite(PUMP1_PIN, HIGH);
    digitalWrite(PUMP2_PIN, LOW);
    lcd.clear();
  }
}

void controlKurasAir(int distance) {
  if (digitalRead(PUMP1_PIN) == HIGH && distance >= JARAK_AIR_HABIS_CM) {
  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, HIGH);
  Serial.println("Air Habis (>=12cm). TRANSISI -> Isi Air (Pompa 2 ON).");
  }
  else if (digitalRead(PUMP2_PIN) == HIGH && distance <= JARAK_AIR_PENUH_CM) {
    digitalWrite(PUMP2_PIN, LOW);
    isKurasActive = false;
    Serial.println("Air Penuh (<=3cm). Siklus Selesai.");

    lcd.clear();
    lcd.setCursor(0, 0); 
    lcd.print("SIKLUS SELESAI!");
    delay(3000); 
  }
  lcd.setCursor(0, 0); 
  if (digitalRead(PUMP1_PIN) == HIGH) {
    lcd.print("MENGURAS AIR...");
  } else if (digitalRead(PUMP2_PIN) == HIGH) {
    lcd.print("ISI AIR...");
  } else {
    lcd.print("PROSES SELESAI"); 
  }
  lcd.setCursor(0, 1); 
  lcd.print("Jarak: "); lcd.print(distance); lcd.print(" cm");
  lcd.print("            ");
}

int readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

void displayModeStatus() {
  lcd.setCursor(12, 0);
  lcd.print(isDemoMode ? "DEMO" : "RTC ");
}

void displayTimeAndDate(DateTime now) {
  if (isKurasActive) return;
  lcd.setCursor(0, 0);

  lcd.print(now.hour() < 10 ? "0" : "");
  lcd.print(now.hour());
  lcd.print(":");
  lcd.print(now.minute() < 10 ? "0" : "");
  lcd.print(now.minute());
  lcd.print(":");
  lcd.print(now.second() < 10 ? "0" : "");
  lcd.print(now.second());
  lcd.print(" ");
  lcd.print(now.day() < 10 ? "0" : "");
  lcd.print(now.day());
}

void displayNormalStatus(int distance) {
  if (isKurasActive) return;
  
  lcd.setCursor(0, 1);
  lcd.print("Jarak Air: ");
  
  if (distance <= 0 || distance > 300) {
    lcd.print("...");
  } else {
    lcd.print(distance);
    lcd.print("cm");
  }
  lcd.print(" (P1/P2 OFF)"); 
  lcd.print("   ");
}
