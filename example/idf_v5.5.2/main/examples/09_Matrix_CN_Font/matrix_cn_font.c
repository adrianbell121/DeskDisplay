#include "matrix_cn_font.h"
#include "Bitmap_Font_18px.h"

static const char *TAG = "CN_Font";
typedef struct {
  example_ui_t *base;
} CN_Font_UI;

static CN_Font_UI ui;

static void cn_font_ui_init(void) {
  /* =======================
   * 1. Init Common UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  example_ui_t *b = ui.base;

  /* =======================
   * 2. Enable Labels & Recolor
   * ======================= */
  lv_obj_clear_flag(b->line1_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);

  lv_label_set_recolor(b->line1_label, true);
  lv_label_set_recolor(b->line2_label, true);
  lv_label_set_recolor(b->line3_label, true);
  lv_label_set_recolor(b->line4_label, true);

  /* =======================
   * 3. Set CN Font & Spacing
   * ======================= */
  lv_obj_set_style_text_font(b->line1_label, &Bitmap_Font_18px, 0);
  lv_obj_set_style_text_font(b->line2_label, &Bitmap_Font_18px, 0);
  lv_obj_set_style_text_font(b->line3_label, &Bitmap_Font_18px, 0);
  lv_obj_set_style_text_font(b->line4_label, &Bitmap_Font_18px, 0);

  lv_obj_set_style_text_letter_space(b->line1_label, 0, 0);
  lv_obj_set_style_text_letter_space(b->line2_label, 0, 0);
  lv_obj_set_style_text_letter_space(b->line3_label, 0, 0);
  lv_obj_set_style_text_letter_space(b->line4_label, 0, 0);
  lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(b->line2_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(b->line3_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(b->line4_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(b->line1_label, lv_pct(100));
  lv_obj_set_width(b->line2_label, lv_pct(100));
  lv_obj_set_width(b->line3_label, lv_pct(100));
  lv_obj_set_width(b->line4_label, lv_pct(100));

  /* =======================
   * 4. Layout: 4 Lines
   * ======================= */
  lv_obj_align(b->line1_label, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_align_to(b->line2_label, b->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
  lv_obj_align_to(b->line3_label, b->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
  lv_obj_align_to(b->line4_label, b->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
}

static void cn_font_ui_apply(void) {
  example_ui_t *b = ui.base;
  /* =======================
   * 1. Chinese Font Demo
   * ======================= */
  set_label_gradient_text_safe(b->line1_label, "欢迎光临", 0xFF4040, 0xFFFF40);
  set_label_gradient_text_safe(b->line2_label, "微雪电子", 0x40FF40, 0x40FFFF);
  set_label_gradient_text_safe(b->line3_label, "中文字体", 0x4080FF, 0xFF40FF);
  set_label_gradient_text_safe(b->line4_label, "你好世界", 0xFF00FF, 0x00FFFF);
}

void cn_font_start(void) {
  /* =======================
   * 1. Start Display
   * ======================= */
  ESP_LOGI(TAG, "Matrix CN font start");
  /* =======================
   * 2. Init UI (LVGL Locked)
   * ======================= */
  bool locked = lvgl_port_lock(0);
  if (locked) {
    cn_font_ui_init();
    cn_font_ui_apply();
    lvgl_port_unlock();
  }

  /* =======================
   * 3. Idle Loop
   * ======================= */
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
