#include <Wire.h> 
#include <hd44780.h>           // Main header untuk LCD
#include <hd44780ioClass/hd44780_I2Cexp.h> // I2C Expander I/O class header
#include "RTClib.h" 
#include <Servo.h>

// --- DEKLARASI PIN ESP8266 ---
const int TRIG_PIN = D5;  // Pin Trigger Sensor Ultrasonik
const int ECHO_PIN = D6;  // Pin Echo Sensor Ultrasonik
const int SERVO_PIN = D8; // Pin Motor Servo
const int PUMP1_PIN = D0; // Pompa 1 (Kuras)
const int PUMP2_PIN = D7; // Pompa 2 (Isi Ulang)

// --- DEKLARASI OBJEK ---
hd44780_I2Cexp lcd; 
RTC_DS3231 rtc;      
Servo myservo;       

// --- PROTOTIPE FUNGSI KUSTOM ---
// WAJIB dideklarasikan di sini agar kompiler mengenali fungsi sebelum dipanggil di loop()
int readUltrasonicDistance(); 
void feedFish();
void startKuras();
void controlKurasAir(int distance);
void displayModeStatus();
void displayTimeAndDate(DateTime now);
void displayNormalStatus(int distance);

// --- VARIABEL STATUS & KONSTANTA ---
const int JARAK_AIR_HABIS_CM = 12; // Jarak (dari sensor ke permukaan air) saat air dianggap habis
const int JARAK_AIR_PENUH_CM = 3;  // Jarak saat air dianggap penuh (mendekati sensor)

bool isKurasActive = false; // TRUE jika sedang dalam siklus kuras/isi
bool isDemoMode = false;    // FALSE = Mode Asli (RTC), TRUE = Mode Demo (Cepat)
bool pakanDoneToday = false; // Flag untuk mencegah pakan berulang di hari/mode yang sama
String inputString = "";    // Buffer untuk input Serial

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  inputString.reserve(10); 

  // Setup Pin Output
  pinMode(PUMP1_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, LOW);
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP2_PIN, LOW);

  // Setup Ultrasonik
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Setup I2C & RTC
  Wire.begin(); 
  if (!rtc.begin()) {
    Serial.println("Tidak dapat menemukan sensor RTC");
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // UNTUK SETTING WAKTU PERTAMA KALI
  
  // Setup Servo & LCD
  myservo.attach(SERVO_PIN);
  myservo.write(0); // Posisi awal tertutup
  lcd.begin(16, 2); 
  lcd.print("Setup Complete!");
  
  delay(1500);
  lcd.clear();
  Serial.println("System Ready. Ketik 'ganti' untuk beralih mode.");
  Serial.print("Mode Saat Ini: ");
  Serial.println(isDemoMode ? "DEMO" : "ASLI (RTC)");
}

// --- LOOP UTAMA ---
void loop() {
    // Membaca data utama
    DateTime now = rtc.now();
    int distance = readUltrasonicDistance(); 
    
    // Membaca input serial (untuk ganti mode)
    serialEvent();
    
    // --- 1. KONTROL MODE KURAS/ISI ---
    if (isKurasActive) {
        controlKurasAir(distance);
        pakanDoneToday = true; 
        
    } else {
        // --- 2. KONTROL PAKAN ---
        
        // Kriteria Mode Asli: Jam 14:00 (Harian)
        bool isFeedTimeRTC = (now.hour() == 14 && now.minute() == 0 && now.second() == 0);
        // Kriteria Mode Demo: Setiap Menit ke-15 (Cepat)
        bool isFeedTimeDemo = (now.second() == 5); 
        
        if ((isDemoMode && isFeedTimeDemo) || (!isDemoMode && isFeedTimeRTC)) {
            if (!pakanDoneToday) {
                feedFish();
                pakanDoneToday = true; 
            }
        }
        
        // Reset flag pakan setelah waktu pakan berlalu
        if (now.hour() != 14) { 
            pakanDoneToday = false; 
        }

        // --- 3. KONTROL KURAS ---

        // Kriteria Mode Asli: Hari Minggu (0) jam 10:00 (Mingguan)
        bool isKurasTimeRTC = (now.dayOfTheWeek() == 0 && now.hour() == 10 && now.minute() == 0 && now.second() == 0);
        // Kriteria Mode Demo: Setiap Detik ke-5 (Sangat Cepat)
        bool isKurasTimeDemo = (now.second() == 15); 

        if ((isDemoMode && isKurasTimeDemo) || (!isDemoMode && isKurasTimeRTC)) {
            startKuras();
        }

        // --- 4. TAMPILAN NORMAL ---
        displayModeStatus();
        displayTimeAndDate(now);
        displayNormalStatus(distance);
    }
    
    delay(500); 
}

// ===========================================
// FUNGSI SERIAL UNTUK PERUBAHAN MODE
// ===========================================
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


// ===========================================
// FUNGSI KONTROL AKSI
// ===========================================

// Fungsi untuk membaca jarak (cm) dari Sensor Ultrasonik
int readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

// Fungsi untuk menggerakkan servo (memberi pakan)
void feedFish() {
    Serial.println("Pakan Diberikan!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEMBERI PAKAN!");
    
    myservo.write(90); // Buka
    delay(1000);       // Tahan 3 detik
    myservo.write(0);  // Tutup
    
    lcd.clear(); 
}

// Fungsi untuk memulai siklus kuras air
void startKuras() {
    if (!isKurasActive) {
        Serial.println("Kuras Dimulai! Pompa 1 ON.");
        isKurasActive = true;
        digitalWrite(PUMP1_PIN, HIGH); // Pompa 1 (Kuras) ON
        digitalWrite(PUMP2_PIN, LOW);
        lcd.clear();
    }
}

// Fungsi kontrol pompa berdasarkan pembacaan Ultrasonik selama siklus kuras
void controlKurasAir(int distance) {
    // --- KONDISI 1: TRANSISI DARI MENGURAS (P1) KE MENGISI (P2) ---
    // P1 sedang ON, dan Jarak >= 12cm (Air Habis, siap diisi)
    if (digitalRead(PUMP1_PIN) == HIGH && distance >= JARAK_AIR_HABIS_CM) {
        
        digitalWrite(PUMP1_PIN, LOW);   // Pompa 1 OFF (Stop Kuras)
        digitalWrite(PUMP2_PIN, HIGH);  // Pompa 2 ON (Mulai Isi)
        Serial.println("Air Habis (>=12cm). TRANSISI -> Isi Air (Pompa 2 ON).");
    }
    
    // --- KONDISI 2: MENGISI (P2) SELESAI ---
    // P2 sedang ON, dan Jarak <= 3cm (Air Penuh, siap dihentikan)
    else if (digitalRead(PUMP2_PIN) == HIGH && distance <= JARAK_AIR_PENUH_CM) {
        
        digitalWrite(PUMP2_PIN, LOW); // Pompa 2 OFF (Stop Isi)
        isKurasActive = false;        // Siklus selesai
        Serial.println("Air Penuh (<=3cm). Siklus Selesai.");

        // Tampilan Notifikasi Selesai
        lcd.clear();
        lcd.setCursor(0, 0); 
        lcd.print("KURAS SELESAI!");
        delay(3000); 
    }
    
    // --- TAMPILAN SAAT KURAS/ISI BERLANGSUNG ---
    // Baris 0: Status
    lcd.setCursor(0, 0); 
    if (digitalRead(PUMP1_PIN) == HIGH) {
        lcd.print("MENGURAS AIR...");
    } else if (digitalRead(PUMP2_PIN) == HIGH) {
        lcd.print("ISI AIR...");
    } else {
        // Kondisi jika siklus baru saja selesai (sebelum delay 3000 habis)
        lcd.print("PROSES SELESAI"); 
    }
    
    // Baris 1: Detail Jarak
    lcd.setCursor(0, 1); 
    lcd.print("Jarak: "); lcd.print(distance); lcd.print(" cm");
    lcd.print("            "); // Membersihkan sisa tulisan
}

// ===========================================
// FUNGSI TAMPILAN LCD
// ===========================================

// Tampilkan mode RTC/DEMO di Baris 0 (kanan)
void displayModeStatus() {
  lcd.setCursor(12, 0);
  lcd.print(isDemoMode ? "DEMO" : "RTC ");
}

// Tampilkan Jam dan Tanggal di Baris 0 (kiri)
void displayTimeAndDate(DateTime now) {
  if (isKurasActive) return;
  
  lcd.setCursor(0, 0); // Baris 1
  
  // Tampilkan Jam:Menit:Detik
  lcd.print(now.hour() < 10 ? "0" : "");
  lcd.print(now.hour());
  lcd.print(":");
  lcd.print(now.minute() < 10 ? "0" : "");
  lcd.print(now.minute());
  lcd.print(":");
  lcd.print(now.second() < 10 ? "0" : "");
  lcd.print(now.second());
  lcd.print(" ");
  
  // Tampilkan Tanggal
  lcd.print(now.day() < 10 ? "0" : "");
  lcd.print(now.day());
  lcd.print("/");
  lcd.print(now.month() < 10 ? "0" : "");
  lcd.print(now.month());
  
}

// Tampilkan Status Normal di Baris 1
void displayNormalStatus(int distance) {
  if (isKurasActive) return;
  
  lcd.setCursor(0, 1); // Baris 2
  
  lcd.print("Jarak Air: ");
  
  // Tampilkan Jarak
  if (distance <= 0 || distance > 300) { 
    lcd.print("...");
  } else {
    lcd.print(distance);
    lcd.print("cm");
  }
  
  lcd.print(" (P1/P2 OFF)"); 
  // Kosongkan sisa baris
  lcd.print("   ");
}
