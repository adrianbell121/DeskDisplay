#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h>

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define DISPLAY_BRIGHTNESS 90
#define FONT_SWITCH_MS 1600

MatrixPanel_I2S_DMA *dma_display = nullptr;
U8G2_FOR_ADAFRUIT_GFX u8g2_for_display;

struct ChineseFontEntry {
  const uint8_t *font;
  const char *name;
};

static const ChineseFontEntry kFontList[] = {
  {u8g2_font_wqy12_t_gb2312, "WQY12"},
  {u8g2_font_wqy13_t_gb2312, "WQY13"},
  {u8g2_font_wqy14_t_gb2312, "WQY14"},
  {u8g2_font_wqy15_t_gb2312, "WQY15"},
  {u8g2_font_wqy16_t_gb2312, "WQY16"}
};

static uint32_t g_last_switch_ms = 0;
static uint8_t g_font_index = 0;

static void drawCenteredUtf8Line(const char *text, int16_t baseline_y, uint16_t color)
{
  const int16_t text_width = u8g2_for_display.getUTF8Width(text);
  int16_t start_x = (dma_display->width() - text_width) / 2;
  if (start_x < 0) {
    start_x = 0;
  }

  u8g2_for_display.setForegroundColor(color);
  u8g2_for_display.drawUTF8(start_x, baseline_y, text);
}

static void drawPage(uint8_t font_index)
{
  const ChineseFontEntry &entry = kFontList[font_index];

  dma_display->fillScreen(dma_display->color565(0, 0, 0));
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setTextColor(dma_display->color565(255, 255, 0));
  dma_display->setCursor(0, 0);
  dma_display->print(entry.name);
  dma_display->setCursor(40, 0);
  dma_display->print(font_index + 1);
  dma_display->print("/");
  dma_display->print(sizeof(kFontList) / sizeof(kFontList[0]));

  u8g2_for_display.setFontMode(1);
  u8g2_for_display.setFontDirection(0);
  u8g2_for_display.setFont(entry.font);

  drawCenteredUtf8Line("中文显示", 26, dma_display->color565(255, 255, 255));
  drawCenteredUtf8Line("你好世界", 50, dma_display->color565(80, 220, 255));
}

void setup()
{
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,
    PANEL_RES_Y,
    PANEL_CHAIN
  );

  mxconfig.gpio.e = 9;
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  mxconfig.min_refresh_rate = 120;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(DISPLAY_BRIGHTNESS);
  dma_display->clearScreen();

  u8g2_for_display.begin(*dma_display);
  drawPage(g_font_index);
  g_last_switch_ms = millis();
}

void loop()
{
  const uint32_t now = millis();
  if (now - g_last_switch_ms < FONT_SWITCH_MS) {
    return;
  }

  g_last_switch_ms = now;
  ++g_font_index;
  if (g_font_index >= (sizeof(kFontList) / sizeof(kFontList[0]))) {
    g_font_index = 0;
  }

  drawPage(g_font_index);
}
