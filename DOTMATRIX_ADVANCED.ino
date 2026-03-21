#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <math.h>
#include <avr/pgmspace.h>

// --- MAX7219 CONFIG ---
#define CS_PIN      10
#define NUM_DEVICES  4

// --- RTC ---
RTC_DS3231 rtc;

// --- MATH ---
const float pi_val = 3.14159265;
const float D2R = pi_val / 180.0;
const float R2D = 180.0 / pi_val;
struct City { char name[10]; float lat; float lon; int timezone; };
struct HijriDate { int day; int month; int year; };
City cities[] = {{"Lahore", 31.5972, 74.2306, 5}};
float declination, Eqtime;
double fajr_time, zohr_time, sunrise_time, asr_time, maghrib_time, isha_time;

// --- USER CALIBRATION ---
const int HIJRI_ADJUSTMENT = -1;

// --- SCROLL STATE ---
int  scrollPage = 0;
char scrollBuf[80];

// --- HIJRI MONTH NAMES ---
const char* h_months[] = {"","Muh","Saf","Rab","Ra2","Jum","Ju2","Raj","Sha","Ram","Shw","Zqi","Zhj"};
const char* g_months[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
const char* g_days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

// =============================================
// MAX7219 DIRECT DRIVER
// =============================================
void mx_send(byte reg, byte data, int device) {
  digitalWrite(CS_PIN, LOW);
  for (int i = NUM_DEVICES-1; i >= 0; i--) {
    SPI.transfer(i == device ? reg  : 0x00);
    SPI.transfer(i == device ? data : 0x00);
  }
  digitalWrite(CS_PIN, HIGH);
}

void mx_all(byte reg, byte data) {
  digitalWrite(CS_PIN, LOW);
  for (int i = 0; i < NUM_DEVICES; i++) {
    SPI.transfer(reg);
    SPI.transfer(data);
  }
  digitalWrite(CS_PIN, HIGH);
}

void mx_init() {
  pinMode(CS_PIN, OUTPUT);
  SPI.begin();
  mx_all(0x09, 0x00);
  mx_all(0x0A, 0x03);
  mx_all(0x0B, 0x07);
  mx_all(0x0C, 0x01);
  mx_all(0x0F, 0x00);
  mx_clear();
}

void mx_clear() {
  for (int row = 1; row <= 8; row++) mx_all(row, 0x00);
}

const byte font[][5] PROGMEM = {
  {0x00,0x00,0x00,0x00,0x00}, // space
  {0x00,0x00,0x5F,0x00,0x00}, // !
  {0x00,0x07,0x00,0x07,0x00}, // "
  {0x14,0x7F,0x14,0x7F,0x14}, // #
  {0x24,0x2A,0x7F,0x2A,0x12}, // $
  {0x23,0x13,0x08,0x64,0x62}, // %
  {0x36,0x49,0x55,0x22,0x50}, // &
  {0x00,0x05,0x03,0x00,0x00}, // '
  {0x00,0x1C,0x22,0x41,0x00}, // (
  {0x00,0x41,0x22,0x1C,0x00}, // )
  {0x08,0x2A,0x1C,0x2A,0x08}, // *
  {0x08,0x08,0x3E,0x08,0x08}, // +
  {0x00,0x50,0x30,0x00,0x00}, // ,
  {0x08,0x08,0x08,0x08,0x08}, // -
  {0x00,0x60,0x60,0x00,0x00}, // .
  {0x20,0x10,0x08,0x04,0x02}, // /
  {0x3E,0x51,0x49,0x45,0x3E}, // 0
  {0x00,0x42,0x7F,0x40,0x00}, // 1
  {0x42,0x61,0x51,0x49,0x46}, // 2
  {0x21,0x41,0x45,0x4B,0x31}, // 3
  {0x18,0x14,0x12,0x7F,0x10}, // 4
  {0x27,0x45,0x45,0x45,0x39}, // 5
  {0x3C,0x4A,0x49,0x49,0x30}, // 6
  {0x01,0x71,0x09,0x05,0x03}, // 7
  {0x36,0x49,0x49,0x49,0x36}, // 8
  {0x06,0x49,0x49,0x29,0x1E}, // 9
  {0x00,0x36,0x36,0x00,0x00}, // :
  {0x00,0x56,0x36,0x00,0x00}, // ;
  {0x08,0x14,0x22,0x41,0x00}, // 
  {0x14,0x14,0x14,0x14,0x14}, // =
  {0x00,0x41,0x22,0x14,0x08}, // >
  {0x02,0x01,0x51,0x09,0x06}, // ?
  {0x32,0x49,0x79,0x41,0x3E}, // @
  {0x7E,0x11,0x11,0x11,0x7E}, // A
  {0x7F,0x49,0x49,0x49,0x36}, // B
  {0x3E,0x41,0x41,0x41,0x22}, // C
  {0x7F,0x41,0x41,0x22,0x1C}, // D
  {0x7F,0x49,0x49,0x49,0x41}, // E
  {0x7F,0x09,0x09,0x09,0x01}, // F
  {0x3E,0x41,0x49,0x49,0x7A}, // G
  {0x7F,0x08,0x08,0x08,0x7F}, // H
  {0x00,0x41,0x7F,0x41,0x00}, // I
  {0x20,0x40,0x41,0x3F,0x01}, // J
  {0x7F,0x08,0x14,0x22,0x41}, // K
  {0x7F,0x40,0x40,0x40,0x40}, // L
  {0x7F,0x02,0x0C,0x02,0x7F}, // M
  {0x7F,0x04,0x08,0x10,0x7F}, // N
  {0x3E,0x41,0x41,0x41,0x3E}, // O
  {0x7F,0x09,0x09,0x09,0x06}, // P
  {0x3E,0x41,0x51,0x21,0x5E}, // Q
  {0x7F,0x09,0x19,0x29,0x46}, // R
  {0x46,0x49,0x49,0x49,0x31}, // S
  {0x01,0x01,0x7F,0x01,0x01}, // T
  {0x3F,0x40,0x40,0x40,0x3F}, // U
  {0x1F,0x20,0x40,0x20,0x1F}, // V
  {0x3F,0x40,0x38,0x40,0x3F}, // W
  {0x63,0x14,0x08,0x14,0x63}, // X
  {0x07,0x08,0x70,0x08,0x07}, // Y
  {0x61,0x51,0x49,0x45,0x43}, // Z
  {0x00,0x7F,0x41,0x41,0x00}, // [
  {0x02,0x04,0x08,0x10,0x20}, // backslash
  {0x00,0x41,0x41,0x7F,0x00}, // ]
  {0x04,0x02,0x01,0x02,0x04}, // ^
  {0x40,0x40,0x40,0x40,0x40}, // _
  {0x00,0x01,0x02,0x04,0x00}, // `
  {0x20,0x54,0x54,0x54,0x78}, // a
  {0x7F,0x48,0x44,0x44,0x38}, // b
  {0x38,0x44,0x44,0x44,0x20}, // c
  {0x38,0x44,0x44,0x48,0x7F}, // d
  {0x38,0x54,0x54,0x54,0x18}, // e
  {0x08,0x7E,0x09,0x01,0x02}, // f
  {0x0C,0x52,0x52,0x52,0x3E}, // g
  {0x7F,0x08,0x04,0x04,0x78}, // h
  {0x00,0x44,0x7D,0x40,0x00}, // i
  {0x20,0x40,0x44,0x3D,0x00}, // j
  {0x7F,0x10,0x28,0x44,0x00}, // k
  {0x00,0x41,0x7F,0x40,0x00}, // l
  {0x7C,0x04,0x18,0x04,0x78}, // m
  {0x7C,0x08,0x04,0x04,0x78}, // n
  {0x38,0x44,0x44,0x44,0x38}, // o
  {0x7C,0x14,0x14,0x14,0x08}, // p
  {0x08,0x14,0x14,0x18,0x7C}, // q
  {0x7C,0x08,0x04,0x04,0x08}, // r
  {0x48,0x54,0x54,0x54,0x20}, // s
  {0x04,0x3F,0x44,0x40,0x20}, // t
  {0x3C,0x40,0x40,0x40,0x7C}, // u
  {0x1C,0x20,0x40,0x20,0x1C}, // v
  {0x3C,0x40,0x30,0x40,0x3C}, // w
  {0x44,0x28,0x10,0x28,0x44}, // x
  {0x0C,0x50,0x50,0x50,0x3C}, // y
  {0x44,0x64,0x54,0x4C,0x44}, // z
};

byte dispBuf[8][32];
void buf_clear() { memset(dispBuf, 0, sizeof(dispBuf)); }

void buf_write_char(char c, int x) {
  if (c < 32 || c > 'z') c = ' ';
  int idx = c - 32;
  for (int col = 0; col < 5; col++) {
    if (x+col < 0 || x+col >= 32) continue;
    byte colData = pgm_read_byte(&font[idx][col]);
    for (int row = 0; row < 8; row++)
      dispBuf[row][x+col] = (colData >> row) & 1;
  }
}

void buf_write_str(const char* str, int x) {
  while (*str) { buf_write_char(*str++, x); x += 6; }
}

void buf_flush() {
  for (int dev = 0; dev < NUM_DEVICES; dev++) {
    int physDev = (NUM_DEVICES-1) - dev;
    for (int row = 0; row < 8; row++) {
      byte rowData = 0;
      for (int col = 0; col < 8; col++)
        rowData |= (dispBuf[row][dev*8+col] << (7-col));
      mx_send(row+1, rowData, physDev);
    }
  }
}

void mx_scroll(const char* text, int speed_ms) {
  int textLen = strlen(text) * 6;
  for (int offset = 32; offset > -textLen; offset--) {
    buf_clear();
    buf_write_str(text, offset);
    buf_flush();
    delay(speed_ms);
  }
}

void mx_print(const char* text) {
  int textLen = strlen(text) * 6;
  int startX = (32 - textLen) / 2;
  buf_clear();
  buf_write_str(text, startX);
  buf_flush();
}

// =============================================
// HIJRI
// =============================================
HijriDate getHijriDate(long JD) {
  JD += HIJRI_ADJUSTMENT;
  long days = JD - 1948440;
  long cycles = days / 10631;
  days = days % 10631;
  long year = cycles * 30, i = 0;
  while(true) {
    long diy = 354; int yc = i+1;
    if(yc==2||yc==5||yc==7||yc==10||yc==13||yc==16||yc==18||yc==21||yc==24||yc==26||yc==29) diy=355;
    if(days < diy) break;
    days -= diy; i++;
  }
  year += i+1;
  int month = 0;
  while(true) {
    int dim = (month%2==0) ? 30 : 29;
    if(i==29 && month==11) dim=30;
    if(days < dim) break;
    days -= dim; month++;
  }
  return {(int)(days+1), month+1, (int)year};
}

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(300);
  mx_init();
  mx_print("INIT");
  delay(1500);

  if (!rtc.begin()) {
    mx_print("RTC ERR");
    while(1);
  }

  mx_print("SYNC?");
  delay(4000);
  while (Serial.available()) Serial.read();

  unsigned long waitStart = millis();
  while (millis() - waitStart < 15000) {
    if (Serial.available() >= 12) {
      char buf[13];
      for (int i = 0; i < 12; i++) buf[i] = Serial.read();
      buf[12] = '\0';
      bool valid = true;
      for (int i = 0; i < 12; i++) {
        if (buf[i] < '0' || buf[i] > '9') { valid = false; break; }
      }
      if (valid) {
        int hh = (buf[0]-'0')*10  + (buf[1]-'0');
        int mm = (buf[2]-'0')*10  + (buf[3]-'0');
        int ss = (buf[4]-'0')*10  + (buf[5]-'0');
        int dd = (buf[6]-'0')*10  + (buf[7]-'0');
        int mo = (buf[8]-'0')*10  + (buf[9]-'0');
        int yy = (buf[10]-'0')*10 + (buf[11]-'0');
        rtc.adjust(DateTime(2000+yy, mo, dd, hh, mm, ss));
        mx_scroll("TIME SYNCED!", 40);
        break;
      } else {
        while (Serial.available()) Serial.read();
      }
    }
  }
  mx_clear();
}

// =============================================
// LOOP
// =============================================
void loop() {
  DateTime now = rtc.now();
  long jDate = getJulianDate(now.year(), now.month(), now.day());
  calculateSunPosition(jDate);
  solvePrayers(cities[0]);
  HijriDate h = getHijriDate(jDate);

  // Adhan check
  const char* activePrayer = "";
  if (isTimeNow(fajr_time,    now)) activePrayer = "FAJR";
  if (isTimeNow(zohr_time,    now)) activePrayer = "ZOHR";
  if (isTimeNow(asr_time,     now)) activePrayer = "ASR";
  if (isTimeNow(maghrib_time, now)) activePrayer = "MAGHRIB";
  if (isTimeNow(isha_time,    now)) activePrayer = "ISHA";
  if (isTimeNow(sunrise_time, now)) activePrayer = "SUNRISE";

  if (strlen(activePrayer) > 0) {
    char adhan[40];
    sprintf(adhan, ">> %s << ALLAHU AKBAR!", activePrayer);
    mx_scroll(adhan, 55);
    return;
  }

  buildMessage(now, h);
  mx_scroll(scrollBuf, 40);
  scrollPage = (scrollPage + 1) % 8;
}

// =============================================
// MESSAGE BUILDER
// =============================================
void buildMessage(DateTime now, HijriDate h) {
  char t1[9],t2[9],t3[9],t4[9],t5[9],t6[9];
  formatPrayerTime(t1, fajr_time);
  formatPrayerTime(t2, sunrise_time);
  formatPrayerTime(t3, zohr_time);
  formatPrayerTime(t4, asr_time);
  formatPrayerTime(t5, maghrib_time);
  formatPrayerTime(t6, isha_time);
  int h12 = (now.hour()==0||now.hour()==12) ? 12 : (now.hour()%12);

  switch (scrollPage) {

    case 0: // Time + day
      sprintf(scrollBuf, "%s  %02d:%02d:%02d",
              g_days[now.dayOfTheWeek()], h12, now.minute(), now.second());
      break;

    case 1: // Gregorian date
      sprintf(scrollBuf, "%02d %s %04d",
              now.day(), g_months[now.month()], now.year());
      break;

    case 2: // Hijri date
      sprintf(scrollBuf, "%02d %s %04d  (Hijri)",
              h.day, h_months[h.month], h.year);
      break;

    case 3: // Fajr + Sunrise
      sprintf(scrollBuf, "Fajr:%s  Sunrise:%s", t1, t2);
      break;

    case 4: // Zohr + Asr
      sprintf(scrollBuf, "Zohr:%s  Asr:%s", t3, t4);
      break;

    case 5: // Maghrib + Isha
      sprintf(scrollBuf, "Maghrib:%s  Isha:%s", t5, t6);
      break;

    case 6: { // Next prayer countdown
      double nowDec = now.hour()+(now.minute()/60.0)+(now.second()/3600.0);
      const char* nextName = "";
      double nextTimeVal = 0;
      if      (nowDec < fajr_time)    { nextName="Fajr";    nextTimeVal=fajr_time; }
      else if (nowDec < sunrise_time) { nextName="Sunrise"; nextTimeVal=sunrise_time; }
      else if (nowDec < zohr_time)    { nextName="Zohr";    nextTimeVal=zohr_time; }
      else if (nowDec < asr_time)     { nextName="Asr";     nextTimeVal=asr_time; }
      else if (nowDec < maghrib_time) { nextName="Maghrib"; nextTimeVal=maghrib_time; }
      else if (nowDec < isha_time)    { nextName="Isha";    nextTimeVal=isha_time; }
      else                            { nextName="Fajr";    nextTimeVal=fajr_time+24.0; }
      int remH, remM;
      getStrictCountdown(nextTimeVal, now.hour(), now.minute(), remH, remM);
      sprintf(scrollBuf, "Next:%s in %02dh%02dm", nextName, remH, remM);
      break;
    }

    case 7: { // Islamic greeting or day of week
      const char* greeting = "";
      if      (h.month==10 && h.day==1)  greeting = "EID MUBARAK! Eid al-Fitr";
      else if (h.month==12 && h.day==10) greeting = "EID MUBARAK! Eid al-Adha";
      else if (h.month==1  && h.day==1)  greeting = "HIJRI NEW YEAR MUBARAK!";
      else if (h.month==1  && h.day==10) greeting = "YOM-E-ASHURA MUBARAK!";
      else if (h.month==3  && h.day==12) greeting = "EID MILAD UN NABI MUBARAK!";
      else if (h.month==7  && h.day==27) greeting = "SHAB-E-MERAJ MUBARAK!";
      else if (h.month==8  && h.day==15) greeting = "SHAB-E-BARAT MUBARAK!";
      else if (h.month==9  && h.day==17) greeting = "YOM-E-BADR MUBARAK!";
      else if (h.month==9  && h.day==27) greeting = "SHAB-E-QADR MUBARAK!";
      else if (h.month==9)               greeting = "RAMADAN KAREEM!";
      else if (now.dayOfTheWeek()==5)    greeting = "JUMMA MUBARAK!";
      if (strlen(greeting) > 0) {
        strcpy(scrollBuf, greeting);
      } else {
        sprintf(scrollBuf, "%02d %s / %02d %s",
                now.day(), g_months[now.month()],
                h.day, h_months[h.month]);
      }
      break;
    }
  }
}

// =============================================
// HELPERS
// =============================================
void formatPrayerTime(char* buf, double t) {
  t += (0.5/60.0);
  int h=floor(t), m=floor((t-h)*60);
  if(h>=24) h-=24; if(h<0) h+=24;
  int h12=(h==0||h==12)?12:(h%12);
  sprintf(buf, "%02d:%02d%s", h12, m, (h>=12)?"PM":"AM");
}

bool isTimeNow(double target, DateTime dt) {
  target += (0.5/60.0);
  int h=floor(target), m=floor((target-h)*60);
  if(h>=24) h-=24;
  return (dt.hour()==h && dt.minute()==m && dt.second()<30);
}

void getStrictCountdown(double t, int ch, int cm, int &oh, int &om) {
  t += (0.5/60.0);
  int tH=floor(t), tM=floor((t-tH)*60);
  if(tH>=24) tH-=24;
  int diff=(tH*60+tM)-(ch*60+cm);
  if(diff<0) diff+=1440;
  oh=diff/60; om=diff%60;
}

long getJulianDate(int year, int month, int day) {
  long y=year, m=month, d=day;
  if(m<=2){y--;m+=12;}
  long a=y/100, b=2-a+(a/4);
  return (long)(365.25*(y+4716))+(long)(30.6001*(m+1))+d+b-1524;
}

void solvePrayers(City c) {
  zohr_time=12.0+c.timezone-(c.lon/15.0)-(Eqtime/60.0);
  double hf=getHourAngle(-18.0,c.lat);
  fajr_time=zohr_time-hf; isha_time=zohr_time+hf;
  double hs=getHourAngle(-0.8333,c.lat);
  sunrise_time=zohr_time-hs; maghrib_time=zohr_time+hs;
  float ld=abs(c.lat-declination);
  float sl=2+tan(ld*D2R);
  float aa=(pi_val/2.0-atan(sl))*R2D;
  asr_time=zohr_time+getHourAngle(aa,c.lat);
}

double getHourAngle(float targetAlt, float lat) {
  float num=sin(targetAlt*D2R)-(sin(lat*D2R)*sin(declination*D2R));
  float den=cos(lat*D2R)*cos(declination*D2R);
  if(num/den>1.0||num/den<-1.0) return 0;
  return (acos(num/den)*R2D)/15;
}

float normalize(float a) {
  a=fmod(a,360.0); if(a<0) a+=360.0; return a;
}

void calculateSunPosition(long JD) {
  float D=JD-2451545.0;
  float g=normalize(357.529+0.98560028*D);
  float q=normalize(280.459+0.98564736*D);
  float L=normalize(q+1.915*sin(g*D2R)+0.020*sin(2*g*D2R));
  float e=23.439-0.00000036*D;
  float RA=atan2(cos(e*D2R)*sin(L*D2R),cos(L*D2R))*R2D/15.0;
  if(RA<0) RA+=24; if(RA>=24) RA-=24;
  declination=asin(sin(e*D2R)*sin(L*D2R))*R2D;
  
  // Normalize the Equation of Time to prevent equinox wrap-around
  float E = q/15.0 - RA;
  if (E > 12.0) E -= 24.0;
  else if (E < -12.0) E += 24.0;
  Eqtime = E * 60.0;
}
