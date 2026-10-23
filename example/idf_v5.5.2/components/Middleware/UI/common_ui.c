#include "common_ui.h"
#include <stdio.h>
#include <string.h>

static common_ui ui;

common_ui *common_ui_get(void) { return &ui; }

static size_t utf8_char_len(uint8_t c)
{
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static size_t utf8_char_count(const char *text)
{
    size_t count = 0;
    const uint8_t *p = (const uint8_t *)text;

    while (*p) {
        p += utf8_char_len(*p);
        count++;
    }

    return count;
}

void middle_fmt_fixed1(char *dest, size_t size, int32_t val_x10) {
    if (!dest || size == 0) return;
    bool neg = (val_x10 < 0);
    uint32_t u = (uint32_t)(neg ? -val_x10 : val_x10);
    snprintf(dest, size, "%s%u.%u", neg ? "-" : "", (unsigned)(u / 10), (unsigned)(u % 10));
}

void set_label_gradient_text(lv_obj_t *label, const char *text, uint32_t color_start, uint32_t color_end)
{
    size_t text_len = strlen(text);
    if (text_len == 0) {
        lv_label_set_text(label, "");
        return;
    }

    static char gradient_buf[256];
    char *buf_ptr = gradient_buf;
    char *buf_end = gradient_buf + sizeof(gradient_buf) - 1;

    uint8_t r_start = (color_start >> 16) & 0xFF;
    uint8_t g_start = (color_start >> 8)  & 0xFF;
    uint8_t b_start =  color_start        & 0xFF;
    uint8_t r_end = (color_end >> 16)     & 0xFF;
    uint8_t g_end = (color_end >> 8)      & 0xFF;
    uint8_t b_end =  color_end            & 0xFF;

    for (size_t idx = 0; idx < text_len && buf_ptr < buf_end; idx++) {
        float ratio = (text_len > 1) ? (float)idx / (float)(text_len - 1) : 0.0f;

        uint8_t r = (uint8_t)(r_start + (r_end - r_start) * ratio);
        uint8_t g = (uint8_t)(g_start + (g_end - g_start) * ratio);
        uint8_t b = (uint8_t)(b_start + (b_end - b_start) * ratio);

        int written = snprintf(buf_ptr, buf_end - buf_ptr, "#%02X%02X%02X %c#", r, g, b, text[idx]);
        if (written <= 0) break;
        buf_ptr += written;
    }

    *buf_ptr = '\0';
    lv_label_set_text(label, gradient_buf);
}

void set_label_gradient_text_safe(lv_obj_t *label, const char *text,
                                  uint32_t start_color, uint32_t end_color)
{
    static char gradient_buf[512];
    char *dst = gradient_buf;
    char *dst_end = gradient_buf + sizeof(gradient_buf) - 1;
    const char *src = text;
    size_t char_count = 0;
    size_t idx = 0;
    uint8_t r_start = (start_color >> 16) & 0xFF;
    uint8_t g_start = (start_color >> 8) & 0xFF;
    uint8_t b_start = start_color & 0xFF;
    uint8_t r_end = (end_color >> 16) & 0xFF;
    uint8_t g_end = (end_color >> 8) & 0xFF;
    uint8_t b_end = end_color & 0xFF;

    if (label == NULL || text == NULL) return;

    char_count = utf8_char_count(text);
    if (char_count == 0) {
        lv_label_set_text(label, "");
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    while (*src && dst < dst_end) {
        size_t char_len = utf8_char_len((uint8_t)*src);
        float ratio = (char_count > 1) ? (float)idx / (float)(char_count - 1) : 0.0f;
        uint8_t r = (uint8_t)(r_start + (r_end - r_start) * ratio);
        uint8_t g = (uint8_t)(g_start + (g_end - g_start) * ratio);
        uint8_t b = (uint8_t)(b_start + (b_end - b_start) * ratio);
        int written = snprintf(dst, dst_end - dst + 1, "#%02X%02X%02X ", r, g, b);

        if (written <= 0 || written >= (dst_end - dst + 1)) break;
        dst += written;

        if ((size_t)(dst_end - dst) < char_len + 1U) break;
        memcpy(dst, src, char_len);
        dst += char_len;
        *dst++ = '#';

        src += char_len;
        idx++;
    }

    *dst = '\0';
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(label, gradient_buf);
}

/**
 * Create and layout common LVGL widgets on the active screen.
 */
void common_ui_init(void) {
    common_ui *ui = common_ui_get();
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    const lv_color_t text_color = lv_color_hex(0xFFFFFF);
    ui->line1_label = lv_label_create(scr);
    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui->line1_label, text_color, 0);
    lv_obj_align(ui->line1_label, LV_ALIGN_TOP_MID, 0, 0);
    ui->line2_label = lv_label_create(scr);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui->line2_label, text_color, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    ui->line3_label = lv_label_create(scr);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui->line3_label, text_color, 0);
    lv_obj_align_to(ui->line3_label, ui->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    ui->line4_label = lv_label_create(scr);
    lv_obj_set_width(ui->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line4_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui->line4_label, text_color, 0);
    lv_obj_align_to(ui->line4_label, ui->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
}

/**
 * Install a periodic LVGL timer.
 */
void ui_create_timer(uint32_t period_ms, lv_timer_cb_t cb) {
    lv_timer_create(cb, period_ms, NULL);
}
