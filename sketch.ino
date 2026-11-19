#include <Arduino.h>
#include <ArduinoJson.h>

// ===================== OMAR: PIN DEFINITIONS =====================
const int PIN_LEFT_FWD = 14;
const int PIN_LEFT_BWD = 27;
const int PIN_RIGHT_FWD = 26;
const int PIN_RIGHT_BWD = 25;
const int PIN_CONVEYOR = 4;

// ===================== ZIAD: PIN DEFINITIONS =====================
const int PIN_BATTERY_POT = 34;

// ===================== ZIAD: CALIBRATION & MOCK VALUES =====================
const int ADC_MAX = 4095;
const int BATTERY_LEVEL_MIN = 0;
const int BATTERY_LEVEL_MAX = 100;

const float MOCK_TRASH_WEIGHT_KG = 1.25f;
const float WEIGHT_MIN_KG = 0.0f;
const float WEIGHT_MAX_KG = 20.0f;
const double SIM_GPS_LAT = 30.118900;
const double SIM_GPS_LNG = 31.608500;

// ===================== OMAR: BOAT STATUS =====================
enum BoatStatus { IDLE, MOVING_FORWARD, MOVING_BACKWARD, TURNING_LEFT, TURNING_RIGHT, STOPPED };
BoatStatus currentStatus = IDLE;
bool conveyorOn = false;

BoatStatus getBoatStatus() {
    return currentStatus;
}

const char* boatStatusToString(BoatStatus s) {
    switch (s) {
        case IDLE:            return "IDLE";
        case MOVING_FORWARD:  return "MOVING_FORWARD";
        case MOVING_BACKWARD: return "MOVING_BACKWARD";
        case TURNING_LEFT:    return "TURNING_LEFT";
        case TURNING_RIGHT:   return "TURNING_RIGHT";
        case STOPPED:         return "STOPPED";
        default:              return "UNKNOWN";
    }
}

// ===================== OMAR: HELPER FUNCTIONS =====================
void allLedsOff() {
    digitalWrite(PIN_LEFT_FWD, LOW);
    digitalWrite(PIN_LEFT_BWD, LOW);
    digitalWrite(PIN_RIGHT_FWD, LOW);
    digitalWrite(PIN_RIGHT_BWD, LOW);
}

void moveForward() {
    allLedsOff();
    digitalWrite(PIN_LEFT_FWD, HIGH);
    digitalWrite(PIN_RIGHT_FWD, HIGH);
    currentStatus = MOVING_FORWARD;
}

void moveBackward() {
    allLedsOff();
    digitalWrite(PIN_LEFT_BWD, HIGH);
    digitalWrite(PIN_RIGHT_BWD, HIGH);
    currentStatus = MOVING_BACKWARD;
}

void turnLeft() {
    allLedsOff();
    digitalWrite(PIN_LEFT_BWD, HIGH);
    digitalWrite(PIN_RIGHT_FWD, HIGH);
    currentStatus = TURNING_LEFT;
}

void turnRight() {
    allLedsOff();
    digitalWrite(PIN_LEFT_FWD, HIGH);
    digitalWrite(PIN_RIGHT_BWD, HIGH);
    currentStatus = TURNING_RIGHT;
}

void stopBoat() {
    allLedsOff();
    conveyorOn = false;
    digitalWrite(PIN_CONVEYOR, LOW);
    currentStatus = STOPPED;
}

void setConveyor(bool on) {
    conveyorOn = on;
    digitalWrite(PIN_CONVEYOR, on ? HIGH : LOW);
}

// ===================== OMAR: COMMAND HANDLER =====================
void handleCommand(char cmd) {
    switch (cmd) {
        case 'F': case 'f':
            moveForward();
            Serial.println("ACK F");
            break;
        case 'B': case 'b':
            moveBackward();
            Serial.println("ACK B");
            break;
        case 'L': case 'l':
            turnLeft();
            Serial.println("ACK L");
            break;
        case 'R': case 'r':
            turnRight();
            Serial.println("ACK R");
            break;
        case 'S': case 's':
            stopBoat();
            Serial.println("ACK S");
            break;
        case 'C': case 'c':
            setConveyor(!conveyorOn);
            Serial.println(conveyorOn ? "ACK C ON" : "ACK C OFF");
            break;
        default:
            Serial.print("ERROR: UNKNOWN COMMAND: ");
            Serial.println(cmd);
            break;
    }
}

// ===================== ZIAD: SENSOR READ FUNCTIONS =====================
int readBatteryPercentage() {
    int raw = analogRead(PIN_BATTERY_POT);
    int pct = map(raw, 0, ADC_MAX, BATTERY_LEVEL_MIN, BATTERY_LEVEL_MAX);
    return constrain(pct, 0, 100);
}

float readTrashWeightKg() {
    float w = MOCK_TRASH_WEIGHT_KG;
    if (w < WEIGHT_MIN_KG) w = WEIGHT_MIN_KG;
    if (w > WEIGHT_MAX_KG) w = WEIGHT_MAX_KG;
    return w;
}

bool validateReadings(int battery, float weight) {
    if (battery < 0 || battery > 100) return false;
    if (weight < WEIGHT_MIN_KG || weight > WEIGHT_MAX_KG) return false;
    return true;
}

// ===================== ZIAD: TELEMETRY (DYNAMIC STATUS) =====================
void publishValidTelemetry(int battery, float weight) {
    StaticJsonDocument<256> doc;
    doc["battery_level"] = battery;
    doc["trash_weight_kg"] = weight;
    JsonObject gps = doc.createNestedObject("gps");
    gps["lat"] = SIM_GPS_LAT;
    gps["lng"] = SIM_GPS_LNG;
    
    // يربط الحالة بالحالة الفعلية للقارب لديك بشكل ديناميكي
    doc["boat_status"] = boatStatusToString(getBoatStatus());
    
    serializeJson(doc, Serial);
    Serial.println();
}

void publishInvalidTelemetry() {
    StaticJsonDocument<128> doc;
    doc["error"] = "INVALID_SENSOR_READING";
    serializeJson(doc, Serial);
    Serial.println();
}

// ===================== setup =====================
void setup() {
    Serial.begin(115200);
    
    // Omar: Pins setup
    pinMode(PIN_LEFT_FWD, OUTPUT);
    pinMode(PIN_LEFT_BWD, OUTPUT);
    pinMode(PIN_RIGHT_FWD, OUTPUT);
    pinMode(PIN_RIGHT_BWD, OUTPUT);
    pinMode(PIN_CONVEYOR, OUTPUT);
    allLedsOff();
    digitalWrite(PIN_CONVEYOR, LOW);

    // Ziad: Sensors setup
    pinMode(PIN_BATTERY_POT, INPUT);
    analogSetAttenuation(ADC_11db);
    
    Serial.println("BOAT & SENSORS READY. Commands: FBLRSC");
}

// ===================== loop =====================
unsigned long lastPublish = 0;

void loop() {
    // 1. استقبال أوامر التحكم عبر السيريال (عمر)
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd != '\n' && cmd != '\r' && cmd != ' ') {
            handleCommand(cmd);
        }
    }

    // 2. إرسال بيانات الـ Telemetry بانتظام كل 1 ثانية (زياد)
    if (millis() - lastPublish >= 1000) {
        lastPublish = millis();
        int battery = readBatteryPercentage();
        float weight = readTrashWeightKg();
        if (validateReadings(battery, weight)) {
            publishValidTelemetry(battery, weight);
        } else {
            publishInvalidTelemetry();
        }
    }
}