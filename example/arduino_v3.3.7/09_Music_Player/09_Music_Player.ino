#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <Audio.h>
#include <vector>
#include <algorithm>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "es8311.h"

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define BOARD_I2C_SDA 47
#define BOARD_I2C_SCL 48

#define BOARD_I2S_BCLK 43
#define BOARD_I2S_LRC 38
#define BOARD_I2S_DOUT 21
#define BOARD_I2S_MCLK 12

#define BOARD_PA_ENABLE 11
#define BOARD_BUTTON_PIN 0

#define BOARD_SD_D0 17
#define BOARD_SD_CMD 44
#define BOARD_SD_CLK 1

#define SERIAL_BAUD_RATE 115200
#define DISPLAY_BRIGHTNESS 72
#define DEFAULT_VOLUME 14
#define PLAYER_VOLUME_MAX 21
#define CODEC_VOLUME_MAX 100
#define CODEC_VOLUME_MIN 0
#define DISPLAY_REFRESH_MS 150
#define BUTTON_DEBOUNCE_MS 30
#define DOUBLE_CLICK_MS 280
#define LONG_PRESS_MS 500
#define VOLUME_REPEAT_MS 180

MatrixPanel_I2S_DMA *dma_display = nullptr;
Audio audio;
ES8311 codec;

struct PlayerState {
  bool display_ok;
  bool sd_ok;
  bool codec_ok;
  bool audio_ready;
  bool paused;
  bool volume_up_mode;
  bool next_track_requested;
  bool button_pressed;
  bool long_press_active;
  uint8_t volume;
  uint8_t click_count;
  int current_track_index;
  uint32_t last_display_ms;
  uint32_t last_button_ms;
  uint32_t button_press_ms;
  uint32_t last_click_ms;
  uint32_t last_volume_repeat_ms;
  bool last_button_level;
  String status_text;
  String current_track_name;
  String current_track_path;
};

static PlayerState g_player = {
  false,
  false,
  false,
  false,
  false,
  true,
  false,
  false,
  false,
  DEFAULT_VOLUME,
  0,
  -1,
  0,
  0,
  0,
  0,
  0,
  true,
  "Boot",
  "",
  ""
};

static std::vector<String> g_track_list;

static uint16_t color_black = 0;
static uint16_t color_white = 0;
static uint16_t color_red = 0;
static uint16_t color_green = 0;
static uint16_t color_blue = 0;
static uint16_t color_yellow = 0;
static uint16_t color_cyan = 0;

static void audioInfoCallback(Audio::msg_t m)
{
  if (m.e == Audio::evt_eof) {
    g_player.status_text = "Track End";
    g_player.next_track_requested = true;
    return;
  }

  if (m.e == Audio::evt_info) {
    g_player.status_text = String(m.msg ? m.msg : "");
    return;
  }

  if (m.e == Audio::evt_log) {
    g_player.status_text = String(m.msg ? m.msg : "");
  }
}

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
  color_yellow = dma_display->color565(255, 255, 0);
  color_cyan = dma_display->color565(0, 255, 255);

  g_player.display_ok = true;
}

static void initButton()
{
  pinMode(BOARD_BUTTON_PIN, INPUT_PULLUP);
  g_player.last_button_level = digitalRead(BOARD_BUTTON_PIN);
  g_player.last_button_ms = millis();
  g_player.button_pressed = false;
  g_player.long_press_active = false;
  g_player.click_count = 0;
}

static void drawTextLine(int16_t x, int16_t y, uint16_t color, const String &text)
{
  if (!g_player.display_ok || dma_display == nullptr) {
    return;
  }

  dma_display->setCursor(x, y);
  dma_display->setTextColor(color);
  dma_display->print(text);
}

static String formatTime(uint32_t total_sec)
{
  const uint32_t min = total_sec / 60;
  const uint32_t sec = total_sec % 60;
  char buffer[8] = {0};
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu", static_cast<unsigned long>(min), static_cast<unsigned long>(sec));
  return String(buffer);
}

static String fitText(const String &text, size_t max_len)
{
  if (text.length() <= max_len) {
    return text;
  }

  if (max_len < 4) {
    return text.substring(0, max_len);
  }

  return text.substring(0, max_len - 3) + "...";
}

static bool isAudioFile(const String &path)
{
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mp3") ||
         lower.endsWith(".wav") ||
         lower.endsWith(".aac") ||
         lower.endsWith(".m4a") ||
         lower.endsWith(".flac");
}

static String joinPath(const String &base_path, const String &name)
{
  if (name.startsWith("/")) {
    return name;
  }

  if (base_path == "/") {
    return "/" + name;
  }

  return base_path + "/" + name;
}

static void scanAudioFiles(const String &dir_path)
{
  File dir = SD_MMC.open(dir_path);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    String entry_name = String(entry.name());
    String full_path = joinPath(dir_path, entry_name);

    if (entry.isDirectory()) {
      scanAudioFiles(full_path);
      entry.close();
      continue;
    }

    if (isAudioFile(full_path)) {
      g_track_list.push_back(full_path);
    }

    entry.close();
  }
}

static String baseName(const String &path)
{
  int slash_pos = path.lastIndexOf('/');
  if (slash_pos < 0) {
    return path;
  }

  return path.substring(slash_pos + 1);
}

static bool mountSdCard()
{
  pinMode(BOARD_SD_D0, INPUT_PULLUP);
  if (!SD_MMC.setPins(BOARD_SD_CLK, BOARD_SD_CMD, BOARD_SD_D0)) {
    g_player.status_text = "SD setPins fail";
    return false;
  }

  if (!SD_MMC.begin("/sdcard", true, false, 20000)) {
    g_player.status_text = "SD mount fail";
    return false;
  }

  g_player.sd_ok = true;
  g_player.status_text = "SD Ready";
  return true;
}

static bool initCodec()
{
  pinMode(BOARD_PA_ENABLE, OUTPUT);
  digitalWrite(BOARD_PA_ENABLE, HIGH);

  if (!codec.begin(BOARD_I2C_SDA, BOARD_I2C_SCL, 400000)) {
    g_player.status_text = "ES8311 fail";
    return false;
  }

  codec.setVolume(CODEC_VOLUME_MAX);
  codec.setBitsPerSample(16);
  g_player.codec_ok = true;
  g_player.status_text = "Codec Ready";
  return true;
}

static bool initAudioOutput()
{
  Audio::audio_info_callback = audioInfoCallback;

  if (!audio.setPinout(BOARD_I2S_BCLK, BOARD_I2S_LRC, BOARD_I2S_DOUT, BOARD_I2S_MCLK)) {
    g_player.status_text = "I2S pinout fail";
    return false;
  }

  audio.setVolume(g_player.volume);
  g_player.audio_ready = true;
  g_player.status_text = "Audio Ready";
  return true;
}

static void buildTrackList()
{
  g_track_list.clear();
  scanAudioFiles("/music");

  if (g_track_list.empty()) {
    scanAudioFiles("/");
  }

  std::sort(
    g_track_list.begin(),
    g_track_list.end(),
    [](const String &lhs, const String &rhs) {
      return lhs.compareTo(rhs) < 0;
    });
}

static bool playTrackByIndex(int index)
{
  if (index < 0) {
    return false;
  }

  if (index >= static_cast<int>(g_track_list.size())) {
    return false;
  }

  audio.stopSong();

  const String path = g_track_list[index];
  const bool ok = audio.connecttoFS(SD_MMC, path.c_str());
  if (!ok) {
    g_player.status_text = "Open track fail";
    return false;
  }

  g_player.current_track_index = index;
  g_player.current_track_path = path;
  g_player.current_track_name = baseName(path);
  g_player.paused = false;
  g_player.status_text = "Playing";
  return true;
}

static bool playNextTrack()
{
  if (g_track_list.empty()) {
    return false;
  }

  int next_index = g_player.current_track_index + 1;
  if (next_index >= static_cast<int>(g_track_list.size())) {
    next_index = 0;
  }

  return playTrackByIndex(next_index);
}

static void applyVolume()
{
  const uint32_t codec_volume =
    CODEC_VOLUME_MIN +
    ((static_cast<uint32_t>(g_player.volume) * (CODEC_VOLUME_MAX - CODEC_VOLUME_MIN)) / PLAYER_VOLUME_MAX);

  audio.setVolume(g_player.volume);
  codec.setVolume(static_cast<uint8_t>(codec_volume));
}

static void changeVolumeStep()
{
  uint8_t new_volume = g_player.volume;

  if (g_player.volume_up_mode) {
    if (new_volume < PLAYER_VOLUME_MAX) {
      ++new_volume;
    }
  } else {
    if (new_volume > 0) {
      --new_volume;
    }
  }

  if (new_volume == g_player.volume) {
    g_player.status_text = g_player.volume_up_mode ? "VOL MAX" : "VOL MIN";
    return;
  }

  g_player.volume = new_volume;
  applyVolume();
  g_player.status_text = String("VOL ") + String(g_player.volume) + (g_player.volume_up_mode ? "+" : "-");
}

static void toggleVolumeMode()
{
  g_player.volume_up_mode = !g_player.volume_up_mode;
  g_player.status_text = g_player.volume_up_mode ? "VOL MODE +" : "VOL MODE -";
}

static void updateDisplay()
{
  if (!g_player.display_ok || dma_display == nullptr) {
    return;
  }

  dma_display->fillScreen(color_black);
  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);

  drawTextLine(0, 0, color_yellow, "MUSIC");

  if (g_track_list.empty()) {
    drawTextLine(0, 18, color_red, "No File");
    drawTextLine(0, 30, color_white, "/music");
    drawTextLine(0, 52, color_cyan, fitText(g_player.status_text, 10));
    return;
  }

  const uint32_t cur_sec = audio.getAudioCurrentTime();
  const uint32_t dur_sec = audio.getAudioFileDuration();
  const String track_name = fitText(g_player.current_track_name, 10);
  const String play_state = g_player.paused ? "PAUSED" : "PLAY";
  const String time_text = formatTime(cur_sec) + "/" + formatTime(dur_sec);
  const String mode_text = g_player.volume_up_mode ? "V+" : "V-";

  drawTextLine(0, 16, color_white, track_name);
  drawTextLine(0, 30, color_green, play_state + " " + mode_text);
  drawTextLine(0, 40, color_cyan, fitText(g_player.status_text, 10));
  drawTextLine(0, 54, color_blue, time_text);
}

static void printStartupInfo()
{
  Serial.println();
  Serial.println("ESP32-S3 RGB Matrix Music Player");
  Serial.printf("I2C SDA/SCL: %d/%d\n", BOARD_I2C_SDA, BOARD_I2C_SCL);
  Serial.printf("I2S BCLK/LRC/DOUT/MCLK: %d/%d/%d/%d\n", BOARD_I2S_BCLK, BOARD_I2S_LRC, BOARD_I2S_DOUT, BOARD_I2S_MCLK);
  Serial.printf("SD CLK/CMD/D0: %d/%d/%d\n", BOARD_SD_CLK, BOARD_SD_CMD, BOARD_SD_D0);
}

static void printTrackList()
{
  Serial.printf("Track count: %u\n", static_cast<unsigned>(g_track_list.size()));

  size_t index = 0;
  while (index < g_track_list.size()) {
    Serial.printf("[%u] %s\n", static_cast<unsigned>(index), g_track_list[index].c_str());
    ++index;
  }
}

static void handleButton()
{
  const uint32_t now = millis();
  const bool button_level = digitalRead(BOARD_BUTTON_PIN);
  const bool button_changed = button_level != g_player.last_button_level;

  if (button_changed) {
    if (now - g_player.last_button_ms < BUTTON_DEBOUNCE_MS) {
      return;
    }

    g_player.last_button_ms = now;
    g_player.last_button_level = button_level;

    if (!button_level) {
      g_player.button_pressed = true;
      g_player.button_press_ms = now;
      g_player.long_press_active = false;
      g_player.last_volume_repeat_ms = now;
      return;
    }

    g_player.button_pressed = false;
    if (g_player.long_press_active) {
      g_player.long_press_active = false;
      g_player.click_count = 0;
      return;
    }

    ++g_player.click_count;
    g_player.last_click_ms = now;
    return;
  }

  if (!g_player.button_pressed) {
    return;
  }

  if (!g_player.long_press_active) {
    if (now - g_player.button_press_ms < LONG_PRESS_MS) {
      return;
    }

    g_player.long_press_active = true;
    g_player.click_count = 0;
    changeVolumeStep();
    g_player.last_volume_repeat_ms = now;
    return;
  }

  if (now - g_player.last_volume_repeat_ms < VOLUME_REPEAT_MS) {
    return;
  }

  g_player.last_volume_repeat_ms = now;
  changeVolumeStep();
}

static void processPendingActions()
{
  const uint32_t now = millis();

  if (g_player.click_count != 0) {
    if (now - g_player.last_click_ms >= DOUBLE_CLICK_MS) {
      const uint8_t click_count = g_player.click_count;
      g_player.click_count = 0;

      if (click_count >= 2) {
        toggleVolumeMode();
        return;
      }

      g_player.next_track_requested = true;
    }
  }

  if (!g_player.next_track_requested) {
    return;
  }

  g_player.next_track_requested = false;
  if (playNextTrack()) {
    return;
  }

  g_player.status_text = "Next fail";
}

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  delay(200);

  printStartupInfo();
  initDisplay();
  initButton();

  const bool sd_ok = mountSdCard();
  const bool codec_ok = initCodec();
  const bool audio_ok = initAudioOutput();

  if (sd_ok) {
    buildTrackList();
    printTrackList();
  }

  if (sd_ok && codec_ok && audio_ok && !g_track_list.empty()) {
    playTrackByIndex(0);
  }

  g_player.last_display_ms = millis();
  updateDisplay();
}

void loop()
{
  audio.loop();
  handleButton();
  processPendingActions();

  const uint32_t now = millis();
  if (now - g_player.last_display_ms < DISPLAY_REFRESH_MS) {
    delay(1);
    return;
  }

  g_player.last_display_ms = now;
  updateDisplay();
  delay(1);
}
