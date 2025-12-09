#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Servo.h>

// --- PIN DEFINITION ---
#define ECHO_PIN D6
#define TRIG_PIN D5
#define SERVO_PIN D4
#define RELAY_KURAS D1
#define RELAY_ISI D2

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo feederServo;

const int JAM_PAKAN = 14;
const int LAMA_PAKAN_MS = 3000;
const int LAMA_KURAS_HARI = 7;
const float JARAK_PENUH_CM = 5.0;
const float JARAK_KURAS_MIN_CM = 20.0;
const float AMBANG_BATAS_ULTRASONIC = 50.0;

bool isFedToday = false;
bool isCleanCycleActive = false;
long lastCleanTimestamp = 0;

unsigned long previousMillisLCD = 0;
const long intervalLCD = 1000;
unsigned long previousMillisKuras = 0;
const long intervalKurasCheck = 60000;

const int POS_TUTUP = 0;
const int POS_BUKA = 120;

enum SystemMode {
  NORMAL,
  PAKAN_COUNTDOWN,
  PAKAN_ACTIVE,
  KURAS_COUNTDOWN,
  KURAS_ACTIVE,
  ISI_ACTIVE
};

SystemMode currentMode = NORMAL;

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distanceCm = duration * 0.034 / 2;
  
  if (distanceCm > AMBANG_BATAS_ULTRASONIC) {
    return AMBANG_BATAS_ULTRASONIC; 
  }
  return distanceCm;
}

void displayLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

String secondsToHMS(long seconds) {
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  
  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", h, m, s);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_KURAS, OUTPUT);
  pinMode(RELAY_ISI, OUTPUT);
 
  digitalWrite(RELAY_KURAS, LOW);
  digitalWrite(RELAY_ISI, LOW);

  feederServo.attach(SERVO_PIN);
  feederServo.write(POS_TUTUP);

  Wire.begin(D3, D7);
  
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
    while (1);
  }
  
  lcd.init();
  lcd.backlight();
  displayLCD("Booting Sistem...", "Tunggu 5 detik");
  delay(5000); 

  if (lastCleanTimestamp == 0) {
    lastCleanTimestamp = rtc.now().unixtime();
  }
}

void loop() {
  unsigned long currentMillis = millis();
  DateTime now = rtc.now();

  if (currentMillis - previousMillisLCD >= intervalLCD) {
    previousMillisLCD = currentMillis;
    updateDisplay(now);
  }

  checkFeeder(now);

  if (currentMillis - previousMillisKuras >= intervalKurasCheck) {
    previousMillisKuras = currentMillis;
    checkCleanCycle(now);
  }

  executeRoutine();
}

void checkFeeder(DateTime now) {
  if (now.hour() == JAM_PAKAN && !isFedToday && currentMode == NORMAL) {
    currentMode = PAKAN_COUNTDOWN;
    isFedToday = true;
  }

  if (now.hour() == 0 && isFedToday) {
    isFedToday = false;
  }
}

void checkCleanCycle(DateTime now) {
  if (currentMode == NORMAL) {
    long elapsedDays = (now.unixtime() - lastCleanTimestamp) / 86400L; // 86400 detik dalam sehari

    if (elapsedDays >= LAMA_KURAS_HARI) {
      currentMode = KURAS_COUNTDOWN;
    }
  }
}

void executeRoutine() {
  static unsigned long actionStartTime = 0;
  float currentDistance;
  
  switch (currentMode) {

    case PAKAN_COUNTDOWN:
      if (millis() - actionStartTime >= 10000) {
        currentMode = PAKAN_ACTIVE;
        actionStartTime = millis(); // Reset waktu untuk durasi servo
      }
      break;

    case PAKAN_ACTIVE:
      feederServo.write(POS_BUKA);
      if (millis() - actionStartTime >= LAMA_PAKAN_MS) {
        feederServo.write(POS_TUTUP);
        currentMode = NORMAL;
      }
      break;

    case KURAS_COUNTDOWN:
      if (millis() - actionStartTime >= 60000) {
        currentMode = KURAS_ACTIVE;
        digitalWrite(RELAY_KURAS, HIGH);
      }
      break;

    case KURAS_ACTIVE:
      currentDistance = measureDistance();

      if (currentDistance >= JARAK_KURAS_MIN_CM) {
        digitalWrite(RELAY_KURAS, LOW);
        delay(5000);
        currentMode = ISI_ACTIVE;
        digitalWrite(RELAY_ISI, HIGH);
      }
      break;

    case ISI_ACTIVE:
      currentDistance = measureDistance();
    
      if (currentDistance <= JARAK_PENUH_CM) {
        digitalWrite(RELAY_ISI, LOW); // OFF
        lastCleanTimestamp = rtc.now().unixtime();
        currentMode = NORMAL;
      }
      break;

    case NORMAL:
      actionStartTime = millis();
      break;
  }
}

void updateDisplay(DateTime now) {
  char timeBuffer[17];
  char line2Buffer[17];
  
  sprintf(timeBuffer, "%02d/%02d %02d:%02d:%02d", now.day(), now.month(), now.hour(), now.minute(), now.second());

  switch (currentMode) {
    
    case NORMAL: {
      long elapsedSeconds = now.unixtime() - lastCleanTimestamp;
      long secondsPerWeek = LAMA_KURAS_HARI * 86400L;
      int sisaHari = LAMA_KURAS_HARI - (elapsedSeconds / 86400L);
      
      long detikTotalSaatIni = now.hour() * 3600 + now.minute() * 60 + now.second();
      long detikTargetPakan = JAM_PAKAN * 3600;
      long detikSisaPakan = detikTargetPakan - detikTotalSaatIni;
      if (detikSisaPakan < 0) {
        detikSisaPakan += 86400L;
      }
      
      if (detikSisaPakan < 3600) {
        sprintf(line2Buffer, "Pakan: %s", secondsToHMS(detikSisaPakan).c_str());
      } else {
        sprintf(line2Buffer, "Kuras: %d Hari Lagi", sisaHari);
      }
      break;
    }
    
    case PAKAN_COUNTDOWN:
      sprintf(line2Buffer, "Buka dlm %d detik", 10 - (int)((millis() - actionStartTime) / 1000));
      sprintf(timeBuffer, "Pakan Otomatis");
      break;
      
    case PAKAN_ACTIVE:
      sprintf(line2Buffer, "Servo Berjalan");
      sprintf(timeBuffer, "Pakan Otomatis");
      break;

    case KURAS_COUNTDOWN:
      sprintf(line2Buffer, "Mulai dlm %d detik", 60 - (int)((millis() - actionStartTime) / 1000));
      sprintf(timeBuffer, "SIKLUS KURAS:");
      break;

    case KURAS_ACTIVE:
      sprintf(line2Buffer, "Level: %.1f cm", measureDistance());
      sprintf(timeBuffer, "KURAS Berjalan!");
      break;
      
    case ISI_ACTIVE:
      sprintf(line2Buffer, "Target: %.1f cm", JARAK_PENUH_CM);
      sprintf(timeBuffer, "ISI AIR Berjalan!");
      break;
  }

  lcd.setCursor(0, 0);
  lcd.print(timeBuffer);
  lcd.setCursor(0, 1);
  lcd.print(line2Buffer);
}
