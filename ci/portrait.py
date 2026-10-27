"""Guarded portrait adapter, recovered from original installer semantics; no old UI."""
import re

def transform(original):
    for guard in ['lvgl_glue_start','board_p4_present_rotated','PANEL_NATIVE_W','PANEL_NATIVE_H','static void disp_flush_cb','static void touch_read_cb']:
        if guard not in original: raise ValueError('Unknown BSP glue: missing '+guard)
    if 'board_p4_flush_region' in original: raise ValueError('Use clean BSP glue')
    pattern=r'(?m)^#define\s+LV_HOR\s+PANEL_NATIVE_H[^\n]*\n#define\s+LV_VER\s+PANEL_NATIVE_W[^\n]*$'
    modified,count=re.subn(pattern,'#define LV_HOR              PANEL_NATIVE_W /* 480 portrait */\n#define LV_VER              PANEL_NATIVE_H /* 800 portrait */',original)
    if count!=1: raise ValueError('Unknown geometry macros')
    line='board_p4_present_rotated((const uint16_t *)color_p, w, h);'
    if modified.count(line)!=1: raise ValueError('Unknown flush callback')
    modified=modified.replace(line,'board_p4_flush_region(area->x1, area->y1, w, h, (const uint16_t *)color_p);\n    if (lv_disp_flush_is_last(drv)) board_p4_present();')
    start=modified.find('    if (pressed) {',modified.index('static void touch_read_cb'))
    stop=modified.find('    } else {',start)
    if start<0 or stop<start: raise ValueError('Unknown touch callback')
    modified=modified[:start]+'''    if (pressed) {
        if (px < 0) px = 0; else if (px >= PANEL_NATIVE_W) px = PANEL_NATIVE_W - 1;
        if (py < 0) py = 0; else if (py >= PANEL_NATIVE_H) py = PANEL_NATIVE_H - 1;
        data->point.x = px;
        data->point.y = py;
        data->state = LV_INDEV_STATE_PRESSED;
'''+modified[stop:]
    return modified
