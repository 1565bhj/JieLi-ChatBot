#include "../../../lib/system/lvgl_v8/lvgl.h"
#include "system/includes.h"
#include <stdio.h>

void lv_port_disp_init(void);
void lv_port_indev_init(void);
void lv_port_fs_init(void);
void lv_png_init(void);

static lv_obj_t *g_status_label;
static lv_obj_t *g_hint_label;
static bool g_toggled;

static void demo_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED) {
        return;
    }

    g_toggled = !g_toggled;

    if (g_status_label) {
        lv_label_set_text(g_status_label, g_toggled ? "Button: ON" : "Button: OFF");
    }

    if (g_hint_label) {
        lv_label_set_text(g_hint_label, g_toggled ? "Touch path works" : "Touch to verify input");
    }

    lv_obj_set_style_bg_color(btn,
                              g_toggled ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_BLUE),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

int lvgl_handwritten_demo_init(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL 160x128 Demo");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FB), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    g_status_label = lv_label_create(scr);
    lv_label_set_text(g_status_label, "Button: OFF");
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x8DD7BF), 0);
    lv_obj_align(g_status_label, LV_ALIGN_TOP_MID, 0, 34);

    g_hint_label = lv_label_create(scr);
    lv_label_set_text(g_hint_label, "Touch to verify input");
    lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0xC7D0D9), 0);
    lv_obj_align(g_hint_label, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 88, 36);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, demo_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Press");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);

    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "Handwritten UI");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x6E7B88), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_scr_load(scr);
    printf("[LVGL] handwritten demo loaded\n");
    return 0;
}

static void lvgl_handwritten_demo_task(void *priv)
{
    LV_UNUSED(priv);
    printf("[LVGL] handwritten sys init...\n");

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_port_fs_init();
    lv_png_init();

    lvgl_handwritten_demo_init();

    while (1) {
        u32 time_till_next = lv_timer_handler();
        if (LV_DISP_DEF_REFR_PERIOD > 1 && time_till_next >= 1000 / OS_TICKS_PER_SEC) {
            msleep(time_till_next);
        } else {
            msleep(5);
        }
    }
}

int lvgl_handwritten_demo_task_init(void)
{
    return thread_fork("lvgl_hw_demo", 1, 16 * 1024, 0, 0, lvgl_handwritten_demo_task, NULL);
}
