  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  #include <RTClib.h>
  #include <math.h>
  LiquidCrystal_I2C lcd(0x27, 16, 2);
  RTC_DS3231 rtc;
  const float pi_val = 3.14159265;
  const float D2R = pi_val / 180.0;
  const float R2D = 180.0 / pi_val;
  // put your setup code here, to run once:

  struct City{
    char name[10];
    float lat;
    float lon;
    int timezone;

  };

  City cities[] = {{//Location Name, //Longitude, //Latitude}
};

float declination, Eqtime;
double fajr_time, zohr_time, sunrise_time, asr_time, maghrib_time, isha_time;

void setup(){
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  if (!rtc.begin()){
    lcd.print("RTC not found!");
    while(1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  DateTime now = rtc.now();
  lcd.setCursor(0,0);
  lcd.print(now.year());
  lcd.print("/");
  lcd.print(now.month());
  lcd.print("/");
  lcd.print(now.day());
  lcd.setCursor(0, 1);
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  delay(2000);
}

void loop() {
  // put your main code here, to run repeatedly:
  DateTime now = rtc.now();
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  delay(5000);

  long jDate = getJulianDate(now.year(), now.month(), now.day());
  calculateSunPosition(jDate);

  for(int i = 0; i < 1; i++){
    solvePrayers(cities[i]);
    displaySchedule(cities[i], now);
  }

}

void solvePrayers(City c){
  zohr_time = 12.0 + c.timezone - (c.lon / 15.0) - (Eqtime / 60.0); // Zohr Calculation
  double h_fajr = getHourAngle(-18.0, c.lat);
  fajr_time = zohr_time - h_fajr; // Fajr Calculation is Zohr's Time minus the 18.0 threshold
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
  if (num/denom > 1.0) return 0;
  if (num/denom < -1.0) return 0;
  return (acos(num/denom)*R2D) / 15;
}

long getJulianDate(int Y, int M, int D){
  if (M<=2) {Y -= 1; M += 12;}
  int A = Y / 100;
  int B = 2 - A + A/4;
  return floor(365.25 * (Y + 4716)) + floor(30.6001 * (M + 1)) + D + B - 1524.5;

}



// --- ADD THIS HELPER FUNCTION ABOVE calculateSunPosition ---
float normalize(float angle) {
  angle = fmod(angle, 360.0); // Modulo 360
  if (angle < 0) angle += 360.0; // Handle negatives
  return angle;
}

void calculateSunPosition(long JD){
  float D = JD - 2451545.0;
  
  // 1. Calculate and IMMEDIATELY normalize
  float g = 357.529 + 0.98560028 * D;
  g = normalize(g); 
  
  float q = 280.459 + 0.98564736 * D;
  q = normalize(q);
  
  float L = q + 1.915 * sin(g * D2R) + 0.020 * sin(2 * g * D2R);
  L = normalize(L);
  
  float e = 23.439 - 0.00000036 * D;
  float RA = atan2(cos(e * D2R) * sin(L * D2R), cos(L * D2R)) * R2D;
  
  // RA is already -180 to 180 due to atan2, but we need hours (0-24)
  RA = RA / 15.0;
  if (RA < 0) RA += 24; 
  if (RA >= 24) RA -= 24;
  
  // Update Globals
  declination = asin(sin(e * D2R) * sin(L * D2R)) * R2D;
  
  // NOW this subtraction is safe because q/15 and RA are both 0-24 range
  Eqtime = (q/15.0 - RA) * 60.0;
}

void displaySchedule(City c, DateTime dt){
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(c.name);
  lcd.setCursor(9, 0);
  lcd.print(dt.day()); lcd.print("/"); lcd.print(dt.month());
  lcd.setCursor(0, 1); lcd.print("Calc Prayers...");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Fajr: "); printHm(fajr_time);
  lcd.setCursor(0, 1); lcd.print("Sunrise: "); printHm(sunrise_time);
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Zohr: "); printHm(zohr_time);
  lcd.setCursor(0, 1); lcd.print("Asr: "); printHm(asr_time);
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Maghrib: "); printHm(maghrib_time);
  lcd.setCursor(0, 1); lcd.print("Isha: "); printHm(isha_time);
  delay(5000);
  

}

void printHm(double t){
  // Handle NaN (Error)
  if (isnan(t)) { lcd.print("--:--"); return; }

  int h = floor(t);
  int m = floor((t - h) * 60);

  // Robust Wrap-around
  while (h >= 24) h -= 24;
  while (h < 0) h += 24;

  if (h < 10) lcd.print("0");
  lcd.print(h);
  lcd.print(":");

  if (m < 10) lcd.print("0");
  lcd.print(m);
}




