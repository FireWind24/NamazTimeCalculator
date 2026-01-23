import math
from datetime import datetime

class PrayerTimeCalculator:
    def __init__(self, lat, lon, timezone, method="Karachi", asr_method="Hanafi"):
        self.lat = lat
        self.lon = lon
        self.timezone = timezone
        
        # Method Settings
        # Karachi Method: Fajr 18.0, Isha 18.0
        self.fajr_angle = 18.0
        self.isha_angle = 18.0
        
        # Juristic Settings
        # Hanafi = 2, Shafi/Maliki/Hanbali = 1
        self.shadow_factor = 2 if asr_method == "Hanafi" else 1

    def _degrees_to_radians(self, deg):
        return deg * (math.pi / 180.0)

    def _radians_to_degrees(self, rad):
        return rad * (180.0 / math.pi)

    def _julian_day(self, year, month, day):
        if month <= 2:
            year -= 1
            month += 12
        A = year // 100
        B = 2 - A + A // 4
        return math.floor(365.25 * (year + 4716)) + math.floor(30.6001 * (month + 1)) + day + B - 1524.5

    def _sun_position(self, jd):
        D = jd - 2451545.0
        g = 357.529 + 0.98560028 * D
        g = g % 360 # Normalize anomaly
        
        q = 280.459 + 0.98564736 * D
        q = q % 360 # <--- THIS WAS THE MISSING LINE (Normalize to 0-360)
        
        L = q + 1.915 * math.sin(self._degrees_to_radians(g)) + 0.020 * math.sin(self._degrees_to_radians(2 * g))
        L = L % 360 # Normalize longitude

        # Obliquity of the Ecliptic
        e = 23.439 - 0.00000036 * D

        # Right Ascension
        RA = math.atan2(
            math.cos(self._degrees_to_radians(e)) * math.sin(self._degrees_to_radians(L)),
            math.cos(self._degrees_to_radians(L))
        )
        RA = self._radians_to_degrees(RA)
        
        # Quadrant Fix for RA (Standard Astronomy fix)
        # RA must be in the same quadrant as L
        L_quadrant = (math.floor(L/90.0)) * 90.0
        RA_quadrant = (math.floor(RA/90.0)) * 90.0
        RA = RA + (L_quadrant - RA_quadrant)
        
        RA = RA / 15.0 # Convert to Hours

        # Declination
        sin_decl = math.sin(self._degrees_to_radians(e)) * math.sin(self._degrees_to_radians(L))
        decl = self._radians_to_degrees(math.asin(sin_decl))

        # Equation of Time
        # (q/15) is mean time, RA is actual time
        EqT = q / 15.0 - RA
        
        return decl, EqT

    def _compute_time(self, angle, lat, decl):
        # The main spherical trigonometry formula
        # cos(H) = (sin(Angle) - sin(Lat)*sin(Decl)) / (cos(Lat)*cos(Decl))
        
        numerator = math.sin(self._degrees_to_radians(angle)) - math.sin(self._degrees_to_radians(lat)) * math.sin(self._degrees_to_radians(decl))
        denominator = math.cos(self._degrees_to_radians(lat)) * math.cos(self._degrees_to_radians(decl))
        
        try:
            cosH = numerator / denominator
            # Clamp for safety
            if cosH > 1: cosH = 1
            if cosH < -1: cosH = -1
            
            H = self._radians_to_degrees(math.acos(cosH))
            return H / 15.0
        except ValueError:
            return 0  # Should not happen in normal latitudes

    def _float_to_time(self, val):
        # Convert decimal hours (12.5) to string (12:30)
        if val < 0: val += 24
        if val >= 24: val -= 24
        
        hours = int(val)
        minutes = int((val - hours) * 60)
        return f"{hours:02d}:{minutes:02d}"

    def get_times(self, year, month, day):
        jd = self._julian_day(year, month, day)
        decl, EqT = self._sun_position(jd)

        # 1. Dhuhr (Noon)
        dhuhr = 12 + self.timezone - self.lon / 15.0 - EqT

        # 2. Fajr & Isha (Fixed Angle 18)
        h_fajr = self._compute_time(-self.fajr_angle, self.lat, decl)
        h_isha = self._compute_time(-self.isha_angle, self.lat, decl)

        # 3. Sunrise & Maghrib (Fixed Angle -0.833)
        h_sunrise = self._compute_time(-0.833, self.lat, decl)

        # 4. Asr (Dynamic Angle based on shadow)
        # Angle = arccot(factor + tan(Lat - Decl))
        lat_minus_decl = abs(self.lat - decl)
        asr_angle_rad = math.atan(1.0 / (self.shadow_factor + math.tan(self._degrees_to_radians(lat_minus_decl))))
        asr_angle = self._radians_to_degrees(asr_angle_rad) # Negative because below horizon
        
        h_asr = self._compute_time(asr_angle, self.lat, decl)

        return {
            "Fajr": self._float_to_time(dhuhr - h_fajr),
            "Sunrise": self._float_to_time(dhuhr - h_sunrise),
            "Dhuhr": self._float_to_time(dhuhr),
            "Asr": self._float_to_time(dhuhr + h_asr),
            "Maghrib": self._float_to_time(dhuhr + h_sunrise),
            "Isha": self._float_to_time(dhuhr + h_isha)
        }

# --- TEST EXECUTION ---

# Define Cities
cities = [
    {"name": "Lahore", "lat": 31.5204, "lon": 74.3587},
    {"name": "Karachi", "lat": 24.8607, "lon": 67.0011},
    {"name": "Islamabad", "lat": 33.6844, "lon": 73.0479}
]

# Set Date (Today)
now = datetime.now()
print(f"--- Prayer Times for {now.date()} ---\n")

for city in cities:
    calc = PrayerTimeCalculator(city["lat"], city["lon"], 5) # 5 is PKT Timezone
    times = calc.get_times(now.year, now.month, now.day)
    
    print(f"📍 {city['name']}")
    for prayer, time in times.items():
        print(f"  {prayer}: {time}")
    print("-" * 20)