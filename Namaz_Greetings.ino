#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <math.h>

// --- CONFIG ---
LiquidCrystal_I2C lcd(0x27, 16, 4); 
RTC_DS3231 rtc;

// --- USER CALIBRATION ---
const int HIJRI_ADJUSTMENT = -1; 

// --- MATH DATA ---
const float pi_val = 3.14159265;
const float D2R = pi_val / 180.0;
const float R2D = 180.0 / pi_val;
struct City{ char name[10]; float lat; float lon; int timezone; };
City cities[] = {{"Lahore", 31.5972, 74.2306, 5}}; 
float declination, Eqtime;
double fajr_time, zohr_time, sunrise_time, asr_time, maghrib_time, isha_time;

// --- TIMING CONTROL ---
int lastSecond = -1;

// --- HIJRI DATA ---
const char* h_months[] = {"", "Muh", "Saf", "Rab", "Ra2", "Jum", "Ju2", "Raj", "Sha", "Ram", "Shw", "Zqi", "Zhj"};
struct HijriDate { int day; int month; int year; };

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(300);

  if (!rtc.begin()) {
    lcd.init(); lcd.backlight();
    lcd.print("RTC Error!");
    while(1);
  }

  lcd.init();
  lcd.backlight();

  // serial sync window only, nothing else touching the RTC
  lcd.setCursor(0,0); lcd.print("Send time or    ");
  lcd.setCursor(0,1); lcd.print("wait 10 secs... ");

  unsigned long waitStart = millis();
  while (millis() - waitStart < 10000) {
    if (Serial.available() >= 12) {
      int hh = (Serial.read()-'0')*10 + (Serial.read()-'0');
      int mm = (Serial.read()-'0')*10 + (Serial.read()-'0');
      int ss = (Serial.read()-'0')*10 + (Serial.read()-'0');
      int dd = (Serial.read()-'0')*10 + (Serial.read()-'0');
      int mo = (Serial.read()-'0')*10 + (Serial.read()-'0');
      int yy = (Serial.read()-'0')*10 + (Serial.read()-'0');
      rtc.adjust(DateTime(2000+yy, mo, dd, hh, mm, ss));
      lcd.clear();
      lcd.setCursor(0,0); lcd.print(" Time Synced!   ");
      delay(1500);
      break;
    }
  }

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("System Ready... ");
  delay(1000);
  lcd.clear();
}

void loop() {
  DateTime now = rtc.now();
  
  if (now.second() != lastSecond) {
    lastSecond = now.second();
    
    long jDate = getJulianDate(now.year(), now.month(), now.day());
    calculateSunPosition(jDate);
    solvePrayers(cities[0]);

    displayManager(now, jDate);
  }
}

// --- HELPER: 12-HOUR CONVERTER (For Header) ---
int convert12h(int h) {
  if (h == 0) return 12;
  if (h > 12) return h - 12;
  if (h == 12) return 12;
  return h;
}

// --- DISPLAY ENGINE ---
// --- DISPLAY ENGINE ---
// --- DISPLAY ENGINE ---
void displayManager(DateTime dt, long jDate) {
  char lineTop[33]; char lineBot[33];
  char r0[17], r1[17], r2[17], r3[17];

  // 1. VISUAL ADHAN CHECK
  const char* activePrayer = "";
  if (isTimeNow(fajr_time, dt))    activePrayer = "FAJR";
  if (isTimeNow(zohr_time, dt))    activePrayer = "ZOHR";
  if (isTimeNow(asr_time, dt))     activePrayer = "ASR";
  if (isTimeNow(maghrib_time, dt)) activePrayer = "MAGHRIB";
  if (isTimeNow(isha_time, dt))    activePrayer = "ISHA";
  if (isTimeNow(sunrise_time, dt)) activePrayer = "SUNRISE";

  if (strlen(activePrayer) > 0) {
    // === ALARM MODE ===
    strcpy(r0, " IT IS TIME FOR ");
    
    int nameLen = strlen(activePrayer);
    int leftPad = (16 - (nameLen + 6)) / 2;
    strcpy(r1, "");
    for(int i=0; i<leftPad; i++) strcat(r1, " ");
    strcat(r1, ">> "); strcat(r1, activePrayer); strcat(r1, " <<");
    padTo16(r1);

    strcpy(r2, "  Allahu Akbar  ");
    
    // Split Date Footer
    const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    HijriDate h = getHijriDate(jDate);
    sprintf(r3, "%02d-%s    %02d-%s", 
            dt.day(), months[dt.month()], h.day, h_months[h.month]);
    padTo16(r3);
    
  } else {
    // === NORMAL MODE ===
    unsigned long currentMs = millis();
    unsigned long cycle = currentMs / 8000;
    int page = cycle % 3;      
    bool showHijri = cycle % 2; 

    HijriDate h = getHijriDate(jDate);
    bool isRamadan = (h.month == 9); // Restored for the Fasting Bar!

    // --- HEADER (Toggle between Time and Greetings) ---
    int h12 = convert12h(dt.hour());
    bool showGreeting = (currentMs / 4000) % 2 == 0; // Show greeting every 4 seconds
    
    // Determine the highest priority greeting
    const char* todayGreeting = "";

    if (h.month == 10 && h.day == 1)       todayGreeting = "  EID MUBARAK!  "; // Eid al-Fitr
    else if (h.month == 12 && h.day == 10) todayGreeting = "  EID MUBARAK!  "; // Eid al-Adha
    else if (h.month == 1 && h.day == 1)   todayGreeting = " HIJRI NEW YEAR "; // 1 Muharram
    else if (h.month == 1 && h.day == 10)  todayGreeting = " YOM-E-ASHURA!  "; // 10 Muharram
    else if (h.month == 3 && h.day == 12)  todayGreeting = " EID MILAD NABI "; // 12 Rabi ul-Awwal
    else if (h.month == 7 && h.day == 27)  todayGreeting = " SHAB-E-MERAJ!  "; // 27 Rajab
    else if (h.month == 8 && h.day == 15)  todayGreeting = " SHAB-E-BARAT!  "; // 15 Shaban
    else if (h.month == 9 && h.day == 17)  todayGreeting = "  YOM-E-BADR!   "; // 17 Ramadan
    else if (h.month == 9 && h.day == 27)  todayGreeting = "  SHAB-E-QADR!  "; // 27 Ramadan
    else if (isRamadan)                    todayGreeting = "RAMADAN KAREEM!!"; // General Ramadan
    else if (dt.dayOfTheWeek() == 5)       todayGreeting = " JUMMA MUBARAK! "; // Friday

    // Display the greeting or the time
    if (showGreeting && strlen(todayGreeting) > 0) {
      strcpy(r0, todayGreeting);
    } else {
      // Normal Time Display
      if (showHijri) {
        sprintf(r0, "%02d-%s  %02d:%02d:%02d", 
                h.day, h_months[h.month], h12, dt.minute(), dt.second());
      } else {
        const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        sprintf(r0, "%02d-%s  %02d:%02d:%02d", 
                dt.day(), months[dt.month()], h12, dt.minute(), dt.second());
      }
    }
    padTo16(r0);

    // --- CONTENT (UPDATED FORMAT) ---
    if (page == 0) { // Morning
      formatRowPrayer(r1, "Fajr", fajr_time);
      formatRowPrayer(r2, "Sunrise", sunrise_time);
      formatRowPrayer(r3, "Zohr", zohr_time);
    } 
    else if (page == 1) { // Evening
      formatRowPrayer(r1, "Asr", asr_time);
      formatRowPrayer(r2, "Maghrib", maghrib_time);
      formatRowPrayer(r3, "Isha", isha_time);
    } 
    else { // Dashboard 
      double nowDec = dt.hour() + (dt.minute() / 60.0) + (dt.second() / 3600.0);
      double nextTimeVal = 0;
      const char* nextName = "";
      
      if (nowDec < fajr_time)         { nextName = "Fajr";    nextTimeVal = fajr_time; }
      else if (nowDec < sunrise_time) { nextName = "Sunrs";   nextTimeVal = sunrise_time; }
      else if (nowDec < zohr_time)    { nextName = "Zohr";    nextTimeVal = zohr_time; }
      else if (nowDec < asr_time)     { nextName = "Asr";     nextTimeVal = asr_time; }
      else if (nowDec < maghrib_time) { nextName = "Maghb";   nextTimeVal = maghrib_time; }
      else if (nowDec < isha_time)    { nextName = "Isha";    nextTimeVal = isha_time; }
      else                            { nextName = "Fajr";    nextTimeVal = fajr_time + 24.0; }

      strcpy(r1, "Next:");
      int s1 = 16 - 5 - strlen(nextName);
      for(int i=0; i<s1; i++) strcat(r1, " "); strcat(r1, nextName);

      // Countdown
      int remH, remM;
      getStrictCountdown(nextTimeVal, dt.hour(), dt.minute(), remH, remM);
      char val2[10]; sprintf(val2, "%02dh %02dm", remH, remM);
      strcpy(r2, "In:");
      int s2 = 16 - 3 - strlen(val2);
      for(int i=0; i<s2; i++) strcat(r2, " "); strcat(r2, val2);

      // --- ROW 3 LOGIC (Fasting Bar vs. Temp) ---
      if (isRamadan) {
        formatFastingBar(r3, nowDec, fajr_time, maghrib_time);
      } else {
        float temp = rtc.getTemperature();
        char val3[10]; sprintf(val3, "%d.%02d%cC", (int)temp, (int)((temp-(int)temp)*100), 223);
        strcpy(r3, "Temp:");
        int s3 = 16 - 5 - strlen(val3);
        for(int i=0; i<s3; i++) strcat(r3, " "); strcat(r3, val3);
        padTo16(r3);
      }
      
      padTo16(r1); padTo16(r2);
    }
  }

  // MERGE
  strcpy(lineTop, r0); strcat(lineTop, r2);
  strcpy(lineBot, r1); strcat(lineBot, r3);
  lcd.setCursor(0, 0); lcd.print(lineTop);
  lcd.setCursor(0, 1); lcd.print(lineBot);
}

// --- NEW HELPER: PRAYER ROW FORMATTER ---
// Output: "Name     05:45PM"
void formatRowPrayer(char* buffer, const char* name, double t) {
  if (isnan(t)) t = 0; t += (0.5/60.0); 
  int h = floor(t); int m = floor((t - h) * 60);
  if(h >= 24) h-=24; if(h < 0) h+=24;
  
  // 1. Determine Suffix
  const char* suffix = (h >= 12) ? "PM" : "AM";
  
  // 2. Convert to 12h
  int h12 = (h == 0 || h == 12) ? 12 : (h % 12);

  // 3. Format string (Total 7 chars: "05:45PM")
  char timeStr[8]; 
  sprintf(timeStr, "%02d:%02d%s", h12, m, suffix);
  
  // 4. Align
  strcpy(buffer, name);
  int spaces = 16 - strlen(name) - 7; // Leave space for time+suffix
  for(int i=0; i<spaces; i++) strcat(buffer, " ");
  strcat(buffer, timeStr);
}

// --- INTEGER JULIAN DATE ---
long getJulianDate(int year, int month, int day) {
  long y = (long)year; long m = (long)month; long d = (long)day;
  if (m <= 2) { y -= 1; m += 12; }
  long a = y / 100; long b = 2 - a + (a / 4);
  return (long)(365.25 * (y + 4716)) + (long)(30.6001 * (m + 1)) + d + b - 1524;
}

// --- INTEGER HIJRI ALGORITHM ---
HijriDate getHijriDate(long JD) {
  JD = JD + HIJRI_ADJUSTMENT;
  long days = JD - 1948440;
  long cycles = days / 10631;
  days = days % 10631;
  long year = cycles * 30;
  long i = 0;
  while(true) {
    long daysInYear = 354;
    int yCheck = i + 1;
    if (yCheck==2 || yCheck==5 || yCheck==7 || yCheck==10 || 
        yCheck==13 || yCheck==16 || yCheck==18 || yCheck==21 || 
        yCheck==24 || yCheck==26 || yCheck==29) daysInYear = 355;
    if (days < daysInYear) break;
    days -= daysInYear;
    i++;
  }
  year += i + 1;
  int month = 0;
  while(true) {
    int daysInMonth = 29;
    if ((month % 2) == 0) daysInMonth = 30; 
    if (i == 29 && month == 11) daysInMonth = 30; 
    if (days < daysInMonth) break;
    days -= daysInMonth;
    month++;
  }
  month++; 
  int day = days + 1; 
  return {(int)day, month, (int)year};
}

// --- HELPER: GHOST ERASER ---
void padTo16(char* buffer) {
  int len = strlen(buffer);
  while(len < 16) { buffer[len] = ' '; len++; }
  buffer[16] = '\0'; 
}

// --- HELPER: VISUAL ADHAN TRIGGER ---
bool isTimeNow(double target, DateTime dt) {
  target += (0.5/60.0);
  int h = floor(target);
  int m = floor((target - h) * 60);
  if(h >= 24) h-=24;
  if (dt.hour() == h && dt.minute() == m && dt.second() < 30) return true;
  return false;
}

// --- HELPER: STRICT MINUTE DIFFERENCE ---
void getStrictCountdown(double targetTime, int currentH, int currentM, int &outH, int &outM) {
  targetTime += (0.5/60.0); 
  int tH = floor(targetTime); int tM = floor((targetTime - tH) * 60);
  if(tH >= 24) tH -= 24;
  int targetTotal = (tH * 60) + tM;
  int currentTotal = (currentH * 60) + currentM;
  int diff = targetTotal - currentTotal;
  if (diff < 0) diff += (24 * 60);
  outH = diff / 60; outM = diff % 60;
}

// --- MATH & SOLAR ---
void solvePrayers(City c){
  zohr_time = 12.0 + c.timezone - (c.lon / 15.0) - (Eqtime / 60.0);
  double h_fajr = getHourAngle(-18.0, c.lat);
  fajr_time = zohr_time - h_fajr;
  isha_time = zohr_time + h_fajr;
  double h_sunrise = getHourAngle(-0.8333, c.lat);
  sunrise_time = zohr_time - h_sunrise;
  maghrib_time = zohr_time + h_sunrise;
  float latDelta = abs(c.lat - declination);
  float shadowLength = 2 + tan(latDelta*D2R);
  float asr_alt = (pi_val / 2.0 - atan(shadowLength))*R2D;
  double h_asr = getHourAngle(asr_alt, c.lat);
  asr_time = zohr_time + h_asr;
}

double getHourAngle(float targetAlt, float lat){
  float num = sin(targetAlt * D2R) - (sin(lat * D2R) * sin(declination * D2R));
  float denom = cos(lat * D2R) * cos(declination * D2R);
  if (num/denom > 1.0) return 0; if (num/denom < -1.0) return 0;
  return (acos(num/denom)*R2D) / 15;
}

float normalize(float angle) {
  angle = fmod(angle, 360.0); if (angle < 0) angle += 360.0; return angle;
}

void calculateSunPosition(long JD){
  float D = JD - 2451545.0;
  float g = normalize(357.529 + 0.98560028 * D);
  float q = normalize(280.459 + 0.98564736 * D);
  float L = normalize(q + 1.915 * sin(g * D2R) + 0.020 * sin(2 * g * D2R));
  float e = 23.439 - 0.00000036 * D;
  float RA = atan2(cos(e * D2R) * sin(L * D2R), cos(L * D2R)) * R2D;
  RA = RA / 15.0; if (RA < 0) RA += 24; if (RA >= 24) RA -= 24;
  declination = asin(sin(e * D2R) * sin(L * D2R)) * R2D;
  Eqtime = (q/15.0 - RA) * 60.0;
}

// --- NEW HELPER: FASTING PROGRESS BAR ---
void formatFastingBar(char* buffer, double nowDec, double fajr, double maghrib) {
  // If it's before Fajr or after Maghrib
  if (nowDec < fajr || nowDec >= maghrib) {
    strcpy(buffer, " Fast Complete! ");
    return;
  }
  
  // Calculate percentages
  float totalFast = maghrib - fajr;
  float elapsed = nowDec - fajr;
  int blocks = (int)((elapsed / totalFast) * 14); // 14 blocks to leave room for [ ]

  // Draw the bar
  buffer[0] = '[';
  for (int i = 0; i < 14; i++) {
    if (i < blocks) buffer[i+1] = '=';
    else if (i == blocks) buffer[i+1] = '>';
    else buffer[i+1] = ' ';
  }
  buffer[15] = ']';
  buffer[16] = '\0';
}
