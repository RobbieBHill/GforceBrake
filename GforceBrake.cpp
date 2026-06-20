#include <Arduino_LSM6DS3.h>
#include <Adafruit_NeoPixel.h>

// ---- PINS ----
int signalPin = 4;
int redPin   = 5;
int greenPin = 6;
int bluePin  = 7;

// ---- ONBOARD RGB ----
#define LED_PIN PIN_NEOPIXEL
Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---- STATES ----
enum State {
  STARTUP,
  WAITING_UPRIGHT,
  CALIBRATING,
  IDLE,
  BRAKING,
  FAULT
};

State state = STARTUP;

// ---- TIMING ----
unsigned long now;
unsigned long lastBlinkTime = 0;
unsigned long startupTime = 0;

// ---- CALIBRATION ----
float zeroOffsetX = 0;
unsigned long stableStartTime = 0;

// ---- BRAKE DETECTION ----
float filteredAccelX = 0;
const float alpha = 0.2;

bool brakeActive = false;
unsigned long brakeStartTime = 0;

// ---- THRESHOLDS ----
const float uprightTiltDeg = 10.0;
const float stableAccelBand = 0.03;

const float brakeThreshold = -0.18;
const float brakeReleaseThreshold = -0.08;

const unsigned long brakeHoldTime = 120;

// ---- RGB (MIRRORED OUTPUT) ----
void setColor(bool r, bool g, bool b) {
  // External LED
  digitalWrite(redPin, r);
  digitalWrite(greenPin, g);
  digitalWrite(bluePin, b);

  // Onboard LED mirrors exactly
  uint8_t rVal = r ? 255 : 0;
  uint8_t gVal = g ? 255 : 0;
  uint8_t bVal = b ? 255 : 0;

  onboardLED.setPixelColor(0, onboardLED.Color(rVal, gVal, bVal));
  onboardLED.show();
}

// ---- SETUP ----
void setup() {
  Serial.begin(115200);

  pinMode(signalPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  onboardLED.begin();
  onboardLED.show();

  Serial.println("[BOOT] LED white test");
  setColor(1,1,1);
  delay(500);
  setColor(0,0,0);

  if (!IMU.begin()) {
    Serial.println("[ERROR] IMU failed");
    state = FAULT;
  }

  startupTime = millis();
  Serial.println("[BOOT] System ready");
}

// ---- LOOP ----
void loop() {
  now = millis();

  float ax, ay, az;
  if (!IMU.accelerationAvailable()) return;
  IMU.readAcceleration(ax, ay, az);

  float mag = sqrt(ax*ax + ay*ay + az*az);

  if (mag < 0.5 || mag > 2.0) {
    // skip bad readings
    return;
  }

  // Normalize gravity vector
  float gx = ax / mag;
  float gy = ay / mag;
  float gz = az / mag;

  // Tilt detection
  float tiltDeg = acos(gz) * 180.0 / PI;

  // Proper gravity removal
  float linearX = ax - (gx * mag);

  // Filter
  filteredAccelX = alpha * linearX + (1 - alpha) * filteredAccelX;

  switch (state) {

    case STARTUP:
      if (now - lastBlinkTime > 300) {
        lastBlinkTime = now;
        setColor(0,0,1); // blue blink
      }

      if (now - startupTime > 1500) {
        state = WAITING_UPRIGHT;
        Serial.println("[STATE] WAITING UPRIGHT");
      }
      break;

    case WAITING_UPRIGHT:
      setColor(0,0,1); // solid blue

      Serial.print("[WAIT] Tilt: ");
      Serial.print(tiltDeg);
      Serial.print(" LinX: ");
      Serial.println(filteredAccelX);

      if (tiltDeg < uprightTiltDeg) {
        stableStartTime = now;
        state = CALIBRATING;
        Serial.println("[STATE] CALIBRATING");
      }
      break;

    case CALIBRATING:
      setColor(0,1,0); // green

      if (tiltDeg > uprightTiltDeg) {
        Serial.println("[CAL] Lost upright");
        state = WAITING_UPRIGHT;
        break;
      }

      if (abs(filteredAccelX) > stableAccelBand) {
        stableStartTime = now; // reset if moving
      }

      if (now - stableStartTime > 1000) {
        zeroOffsetX = filteredAccelX;

        Serial.print("[CAL] Zero set: ");
        Serial.println(zeroOffsetX);

        state = IDLE;
      }
      break;

    case IDLE:
      digitalWrite(signalPin, LOW);
      setColor(0,0,0); // off

      if (filteredAccelX - zeroOffsetX < brakeThreshold) {
        if (!brakeActive) {
          brakeStartTime = now;
          brakeActive = true;
        }

        if (now - brakeStartTime > brakeHoldTime) {
          Serial.println("[EVENT] BRAKE DETECTED");
          state = BRAKING;
        }
      } else {
        brakeActive = false;
      }
      break;

    case BRAKING:
      digitalWrite(signalPin, HIGH);
      setColor(1,0,0); // red

      if (filteredAccelX - zeroOffsetX > brakeReleaseThreshold) {
        state = IDLE;
      }
      break;

    case FAULT:
      setColor(0,0,1); // blue fault
      digitalWrite(signalPin, LOW);
      break;
  }

  // ---- DEBUG ----
  Serial.print("[DATA] Tilt: ");
  Serial.print(tiltDeg);
  Serial.print(" LinX: ");
  Serial.println(filteredAccelX);
}
