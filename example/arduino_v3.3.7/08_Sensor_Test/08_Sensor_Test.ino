#include <Arduino.h>
#include <Wire.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Adafruit_SHTC3.h>
#include <Adafruit_Sensor.h>
#include <SensorQMI8658.hpp>

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48

#define SERIAL_BAUD_RATE 115200
#define DISPLAY_BRIGHTNESS 90
#define SENSOR_REFRESH_MS 200
#define SERIAL_REFRESH_MS 1000

MatrixPanel_I2S_DMA *dma_display = nullptr;
Adafruit_SHTC3 shtc3;
SensorQMI8658 qmi;

struct SensorState {
  bool display_ok;
  bool shtc3_ok;
  bool qmi_ok;
  bool qmi_found;
  float temp_c;
  float hum_rh;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  uint8_t qmi_addr;
  uint8_t qmi_whoami;
  uint8_t qmi_revision;
};

static SensorState g_state = {
  false,
  false,
  false,
  false,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0.0f,
  0x00,
  0x00,
  0x00
};

static uint32_t g_last_sensor_ms = 0;
static uint32_t g_last_serial_ms = 0;
static uint8_t g_qmi_fail_count = 0;

static uint16_t color_black = 0;
static uint16_t color_white = 0;
static uint16_t color_red = 0;
static uint16_t color_green = 0;
static uint16_t color_blue = 0;
static uint16_t color_cyan = 0;
static uint16_t color_yellow = 0;

static void initDisplay()
{
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN
  );

  mxconfig.gpio.e = 9;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (dma_display == nullptr) {
    return;
  }

  if (!dma_display->begin()) {
    return;
  }

  dma_display->setBrightness8(DISPLAY_BRIGHTNESS);
  dma_display->clearScreen();

  color_black = dma_display->color565(0, 0, 0);
  color_white = dma_display->color565(255, 255, 255);
  color_red = dma_display->color565(255, 0, 0);
  color_green = dma_display->color565(0, 255, 0);
  color_blue = dma_display->color565(0, 0, 255);
  color_cyan = dma_display->color565(0, 255, 255);
  color_yellow = dma_display->color565(255, 255, 0);

  g_state.display_ok = true;
}

static void initI2cBus()
{
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
}

static bool probeI2cAddress(uint8_t addr)
{
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool readI2cRegister8(uint8_t addr, uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t read_len = Wire.requestFrom(static_cast<int>(addr), 1);
  if (read_len != 1 || !Wire.available()) {
    return false;
  }

  value = Wire.read();
  return true;
}

static uint8_t detectQmiAddress()
{
  const uint8_t candidate_list[] = {
    QMI8658_H_SLAVE_ADDRESS,
    QMI8658_L_SLAVE_ADDRESS
  };

  uint8_t index = 0;
  while (index < sizeof(candidate_list)) {
    const uint8_t addr = candidate_list[index];
    uint8_t whoami = 0;

    if (probeI2cAddress(addr) && readI2cRegister8(addr, 0x00, whoami) && whoami == 0x05) {
      g_state.qmi_addr = addr;
      g_state.qmi_whoami = whoami;
      g_state.qmi_found = true;
      return addr;
    }

    ++index;
  }

  return 0;
}

static bool initShtc3()
{
  if (!shtc3.begin(&Wire)) {
    return false;
  }

  shtc3.sleep(false);
  return true;
}

static bool initQmi8658()
{
  const uint8_t qmi_addr = detectQmiAddress();
  if (qmi_addr == 0) {
    return false;
  }

  delay(20);

  if (!qmi.begin(Wire, qmi_addr)) {
    return false;
  }

  g_state.qmi_addr = qmi_addr;
  g_state.qmi_whoami = static_cast<uint8_t>(qmi.whoAmI());
  g_state.qmi_revision = qmi.getChipID();

  if (!qmi.configAccelerometer(
        SensorQMI8658::ACC_RANGE_4G,
        SensorQMI8658::ACC_ODR_125Hz,
        SensorQMI8658::LPF_MODE_0)) {
    return false;
  }

  if (!qmi.configGyroscope(
        SensorQMI8658::GYR_RANGE_512DPS,
        SensorQMI8658::GYR_ODR_112_1Hz,
        SensorQMI8658::LPF_MODE_3)) {
    return false;
  }

  const bool accel_enabled = qmi.enableAccelerometer();
  const bool gyro_enabled = qmi.enableGyroscope();
  if (!accel_enabled || !gyro_enabled) {
    return false;
  }

  g_qmi_fail_count = 0;
  return true;
}

static void drawLineText(
  int16_t x,
  int16_t y,
  uint16_t color,
  const String &text)
{
  if (!g_state.display_ok || dma_display == nullptr) {
    return;
  }

  dma_display->setCursor(x, y);
  dma_display->setTextColor(color);
  dma_display->print(text);
}

static void drawStatusScreen()
{
  if (!g_state.display_ok || dma_display == nullptr) {
    return;
  }

  dma_display->fillScreen(color_black);
  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);

  drawLineText(0, 0, color_yellow, "Sensor Test");
  drawLineText(0, 8, g_state.shtc3_ok ? color_green : color_red, String("SHTC3: ") + (g_state.shtc3_ok ? "OK" : "FAIL"));
  drawLineText(0, 16, g_state.qmi_ok ? color_green : color_red, String("QMI8658: ") + (g_state.qmi_ok ? "OK" : "FAIL"));
  if (g_state.qmi_found) {
    drawLineText(0, 24, color_white, String("ADDR:0x") + String(g_state.qmi_addr, HEX));
  }

  if (!g_state.shtc3_ok) {
    drawLineText(0, 40, color_red, "Check SHTC3");
  }

  if (!g_state.qmi_ok) {
    drawLineText(0, 48, color_red, g_state.qmi_found ? "QMI Init Fail" : "QMI Not Found");
  }
}

static void drawSensorScreen()
{
  if (!g_state.display_ok || dma_display == nullptr) {
    return;
  }

  dma_display->fillScreen(color_black);
  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);

  drawLineText(0, 0, color_yellow, "T/H");
  drawLineText(0, 8, color_white, String("T:") + String(g_state.temp_c, 1) + "C");
  drawLineText(0, 16, color_white, String("H:") + String(g_state.hum_rh, 1) + "%");

  drawLineText(0, 26, color_cyan, "ACC(g)");
  drawLineText(0, 34, color_green, String("X:") + String(g_state.ax, 1));
  drawLineText(0, 42, color_green, String("Y:") + String(g_state.ay, 1));
  drawLineText(0, 50, color_green, String("Z:") + String(g_state.az, 1));

  drawLineText(36, 26, color_cyan, "GYR");
  drawLineText(36, 34, color_blue, String("X:") + String(g_state.gx, 0));
  drawLineText(36, 42, color_blue, String("Y:") + String(g_state.gy, 0));
  drawLineText(36, 50, color_blue, String("Z:") + String(g_state.gz, 0));
}

static void printStatusToSerial()
{
  Serial.println("==== Sensor Status ====");
  Serial.print("I2C SDA/SCL: ");
  Serial.print(I2C_SDA_PIN);
  Serial.print("/");
  Serial.println(I2C_SCL_PIN);

  Serial.print("SHTC3: ");
  Serial.println(g_state.shtc3_ok ? "OK" : "FAIL");

  Serial.print("QMI8658: ");
  Serial.println(g_state.qmi_ok ? "OK" : "FAIL");
  Serial.print("QMI found: ");
  Serial.println(g_state.qmi_found ? "YES" : "NO");

  if (g_state.qmi_found) {
    Serial.print("QMI ADDR: 0x");
    Serial.println(g_state.qmi_addr, HEX);
  }

  if (g_state.qmi_ok || g_state.qmi_found) {
    Serial.print("QMI WHOAMI: 0x");
    Serial.println(g_state.qmi_whoami, HEX);
    Serial.print("QMI REV: 0x");
    Serial.println(g_state.qmi_revision, HEX);
  }
}

static void readShtc3()
{
  if (!g_state.shtc3_ok) {
    return;
  }

  sensors_event_t humidity;
  sensors_event_t temp;

  if (!shtc3.getEvent(&humidity, &temp)) {
    g_state.shtc3_ok = false;
    return;
  }

  g_state.temp_c = temp.temperature;
  g_state.hum_rh = humidity.relative_humidity;
}

static void readQmi8658()
{
  if (!g_state.qmi_ok) {
    return;
  }

  bool acc_ok = qmi.getAccelerometer(g_state.ax, g_state.ay, g_state.az);
  bool gyr_ok = qmi.getGyroscope(g_state.gx, g_state.gy, g_state.gz);

  if (acc_ok && gyr_ok) {
    g_qmi_fail_count = 0;
    return;
  }

  ++g_qmi_fail_count;
  if (g_qmi_fail_count >= 10) {
    g_state.qmi_ok = false;
  }
}

static void refreshSensors()
{
  readShtc3();
  readQmi8658();
}

static void printDataToSerial()
{
  Serial.print("Temp(C): ");
  Serial.print(g_state.temp_c, 2);
  Serial.print("  Hum(%RH): ");
  Serial.print(g_state.hum_rh, 2);
  Serial.print("  Acc(g): ");
  Serial.print(g_state.ax, 3);
  Serial.print(",");
  Serial.print(g_state.ay, 3);
  Serial.print(",");
  Serial.print(g_state.az, 3);
  Serial.print("  Gyr(dps): ");
  Serial.print(g_state.gx, 3);
  Serial.print(",");
  Serial.print(g_state.gy, 3);
  Serial.print(",");
  Serial.println(g_state.gz, 3);
}

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  delay(200);
  Serial.println();
  Serial.println("ESP32-S3 RGB Matrix Sensor Test");

  initDisplay();
  initI2cBus();

  g_state.shtc3_ok = initShtc3();
  g_state.qmi_ok = initQmi8658();

  printStatusToSerial();
  drawStatusScreen();

  g_last_sensor_ms = millis();
  g_last_serial_ms = millis();
}

void loop()
{
  const uint32_t now = millis();

  if (now - g_last_sensor_ms >= SENSOR_REFRESH_MS) {
    g_last_sensor_ms = now;
    refreshSensors();

    if (g_state.shtc3_ok && g_state.qmi_ok) {
      drawSensorScreen();
    } else {
      drawStatusScreen();
    }
  }

  if (now - g_last_serial_ms >= SERIAL_REFRESH_MS) {
    g_last_serial_ms = now;

    if (g_state.shtc3_ok || g_state.qmi_ok) {
      printDataToSerial();
    } else {
      printStatusToSerial();
    }
  }
}
