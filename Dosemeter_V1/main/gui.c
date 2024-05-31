/*
 * gui.c
 *
 *  Created on: 29 трав. 2024 р.
 *      Author:
 */
#include "lvgl.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "main.h"

extern void table_event_cb(lv_event_t* e);
extern void table_widget_event_cb(lv_event_t* e);
extern void draw_event_cb(lv_event_t * e);

extern lv_group_t * g_menu;
extern lv_group_t* g_empty;
extern lv_group_t *g_widgets;
extern lv_obj_t * scr1;
lv_obj_t * obj2;
static lv_style_t style2;
extern char str_adc[10];
extern lv_obj_t * label_adc;
extern lv_obj_t * label_alpha;
extern lv_obj_t * label_beta;
extern lv_obj_t * label_gamma;
extern lv_obj_t * label_neutron;
extern lv_obj_t* chart;
extern lv_chart_series_t * ser;
extern lv_obj_t* table;
extern lv_obj_t* table_widgets;
extern lv_obj_t* screenMenu;
extern lv_obj_t* screenWidgets;
//-----------------------------
extern uint8_t alpha;
extern uint8_t beta;
extern uint8_t gamma;
extern uint8_t neutron;
//extern widget_options options;

static void draw_event_cb_2(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    /*If the cells are drawn...*/
    if(dsc->part == LV_PART_ITEMS) {
        bool chk = lv_table_has_cell_ctrl(obj, dsc->id, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = chk ? lv_theme_get_color_primary(obj) : lv_palette_lighten(LV_PALETTE_GREY, 2);
        rect_dsc.radius = LV_RADIUS_CIRCLE;

        lv_area_t sw_area;
        sw_area.x1 = dsc->draw_area->x2 - 50;
        sw_area.x2 = sw_area.x1 + 40;
        sw_area.y1 = dsc->draw_area->y1 + lv_area_get_height(dsc->draw_area) / 2 - 10;
        sw_area.y2 = sw_area.y1 + 20;
        lv_draw_rect(dsc->draw_ctx, &rect_dsc, &sw_area);

        rect_dsc.bg_color = lv_color_white();
        if(chk) {
            sw_area.x2 -= 2;
            sw_area.x1 = sw_area.x2 - 16;
        }
        else {
            sw_area.x1 += 2;
            sw_area.x2 = sw_area.x1 + 16;
        }
        sw_area.y1 += 2;
        sw_area.y2 -= 2;
        lv_draw_rect(dsc->draw_ctx, &rect_dsc, &sw_area);
    }
}

static void change_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint16_t col;
    uint16_t row;
    lv_table_get_selected_cell(obj, &row, &col);
    bool chk = lv_table_has_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
    if(chk) lv_table_clear_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
    else lv_table_add_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
}

void my_demo2(lv_disp_t *disp)
{
    lv_obj_t * obj1;

    lv_obj_t* scr_intro = lv_scr_act();
    scr1 = lv_obj_create(NULL);
    screenWidgets = lv_obj_create(NULL);

    static lv_style_t style;
    lv_style_init(&style);
    //lv_style_set_text_font(&style, &lv_font_montserrat_);  /*Set a larger font*/
    lv_style_set_text_color(&style, lv_color_white());


    obj1 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj1, lv_pct(103), lv_pct(103));
    lv_obj_set_scrollbar_mode(obj1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(lv_scr_act(), LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(obj1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(obj1, lv_color_black(), LV_PART_MAIN);

    lv_obj_t* logo_img;
    LV_IMG_DECLARE(atom_scan_logo);
    logo_img = lv_img_create(lv_scr_act());
    lv_img_set_src(logo_img, &atom_scan_logo);
    lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, -10);
    //lv_obj_set_size(logo_img, 50, 50);
    lv_obj_set_width(logo_img, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(logo_img, LV_SIZE_CONTENT);    /// 1

    lv_obj_t * label_site = lv_label_create(lv_scr_act());
	lv_obj_align(label_site, LV_ALIGN_CENTER, 10, 100);
	lv_obj_set_width(label_site, 150);
	lv_label_set_text(label_site, "ATOM-SCAN.com");
	lv_obj_add_style(label_site, &style, LV_PART_MAIN);
	LV_FONT_DECLARE(montserrat_170);
	LV_FONT_DECLARE(montserrat_60);

    lv_style_init(&style2);
    lv_style_set_text_font(&style2, &montserrat_60);  /*Set a larger font*/
    lv_style_set_text_color(&style2, lv_color_white());

    //Home screen
    obj2 = lv_obj_create(scr1);
    lv_obj_set_size(obj2, lv_pct(103), lv_pct(103));
    lv_obj_set_scrollbar_mode(obj2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(scr1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(obj2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(obj2, lv_color_black(), LV_PART_MAIN);

    label_adc = lv_label_create(obj2);
    label_alpha = lv_label_create(obj2);
    label_beta = lv_label_create(obj2);
    label_gamma = lv_label_create(obj2);
    label_neutron = lv_label_create(obj2);
    lv_obj_add_flag(label_alpha, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_beta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_gamma, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_neutron, LV_OBJ_FLAG_HIDDEN);
	lv_obj_align(label_adc, LV_ALIGN_CENTER, 40, -50);
	lv_obj_set_width(label_adc, 400);
	lv_label_set_text(label_adc, "ATOM-SCAN.com");
	lv_obj_add_style(label_adc, &style2, LV_PART_MAIN);

	//Create a chart
    chart = lv_chart_create(obj2);
    lv_obj_set_size(chart, 300, 110);
    lv_obj_set_style_bg_color(chart, lv_color_black(), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4000);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);


     ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
    		 LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_point_count(chart, 60);
    lv_chart_set_all_value(chart, ser, 30);

    //table
    lv_group_set_default(g_menu);
    screenMenu = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screenMenu, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screenMenu, lv_color_black(), LV_PART_MAIN);
	table = lv_table_create(screenMenu);
	lv_obj_clear_flag(table, LV_OBJ_FLAG_SCROLLABLE);
	//lv_obj_add_state(table, LV_STATE_EDITED);
	lv_table_set_cell_value(table, 0, 0, "Start");
    lv_table_set_cell_value(table, 1, 0, "Set temperature");
    lv_table_set_cell_value(table, 2, 0, "PID");
    lv_table_set_cell_value(table, 0, 1, "Finish");
    //lv_table_set_cell_value(table, 1, 1, "Finish");
    lv_table_set_cell_value(table, 2, 1, "Finish");
    lv_table_set_cell_value(table, 3, 0, "Merge");
    lv_table_add_cell_ctrl(table, 3, 0, LV_TABLE_CELL_CTRL_MERGE_RIGHT);
    lv_table_add_cell_ctrl(table, 3, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
    lv_table_set_cell_value(table, 4, 0, "Widgets");
    lv_table_add_cell_ctrl(table, 4, 0, LV_TABLE_CELL_CTRL_MERGE_RIGHT);

    lv_obj_center(table);
    //lv_obj_set_height(table, 220);
    //lv_obj_set_width(table,  300);
    lv_obj_set_size(table, lv_pct(103), lv_pct(103));
    lv_table_set_col_width(table, 0, 160);
    lv_table_set_col_width(table, 1, 160);
    lv_obj_set_style_bg_color(table, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(table, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    //temp_label_act = lv_label_create(lv_scr_act());
    //lv_label_set_text(temp_label_act, "The last button event:\nNone");
    lv_obj_add_event_cb(table, draw_event_cb, LV_EVENT_DRAW_PART_END, NULL);
    lv_obj_add_event_cb(table, table_event_cb, LV_EVENT_ALL, NULL);

    vTaskDelay(30);
    // table for widgets
    lv_group_set_default(g_widgets);

    lv_obj_set_scrollbar_mode(screenWidgets, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screenWidgets, lv_color_black(), LV_PART_MAIN);
	table_widgets = lv_table_create(screenWidgets);
	lv_obj_clear_flag(table_widgets, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(table_widgets);
    lv_obj_set_size(table_widgets, lv_pct(103), lv_pct(103));
    lv_table_set_col_width(table_widgets, 0, 320);
    lv_obj_set_style_bg_color(table_widgets, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(table_widgets, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(table_widgets, 0, LV_PART_ITEMS);

    lv_table_set_cell_value(table_widgets, 0, 0, "Alpha");
    lv_table_set_cell_value(table_widgets, 1, 0, "Beta");
    lv_table_set_cell_value(table_widgets, 2, 0, "Gamma");
    lv_table_set_cell_value(table_widgets, 3, 0, "Neutron");

    lv_obj_add_state(table_widgets, LV_STATE_PRESSED);
    lv_event_send(table_widgets, LV_EVENT_PRESSED, NULL);
    lv_group_focus_obj(table_widgets);
    //lv_group_set_editing(g_widgets, true);
    vTaskDelay(15);
    lv_obj_add_event_cb(table_widgets, draw_event_cb_2, LV_EVENT_DRAW_PART_END, NULL);
    lv_obj_add_event_cb(table_widgets, table_widget_event_cb, LV_EVENT_ALL, NULL);
    //lv_obj_add_event_cb(table_widgets, change_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    //vTaskDelay(3000 / portTICK_PERIOD_MS);
	lv_scr_load_anim(scr1, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 1000, true);
	//lv_obj_set_style_bg_color(obj1, lv_color_white(), LV_PART_MAIN);

}//my_demo2

void redraw_widgets(widget_options* opts){

	uint8_t counter = 0;

	counter += opts->alpha;
	counter += opts->beta;
	counter += opts->gamma;
	counter += opts->neutron;

	//clear screen scr1
	lv_obj_clean(obj2);
	if(opts->alpha){
		//if chart place small digits and chart
		if(counter == 1){
			//label style big
			if(opts->chart == 0 && opts->speedo == 0){
				label_alpha = lv_label_create(obj2);
				lv_obj_align(label_alpha, LV_ALIGN_LEFT_MID, 10, 0);
				lv_obj_set_width(label_alpha, 400);
				lv_label_set_text(label_alpha, "ATOM-SCAN.com");
				lv_obj_add_style(label_alpha, &style2, LV_PART_MAIN);

			//if chart place chart
			}/*else if(opts->chart == 1 && opts->speedo == 0){
			    label_adc = lv_label_create(obj2);
				lv_obj_align(label_adc, LV_ALIGN_CENTER, 40, -50);
				lv_obj_set_width(label_adc, 400);
				lv_label_set_text(label_adc, "ATOM-SCAN.com");
				lv_obj_add_style(label_adc, &style2, LV_PART_MAIN);

				//Create a chart
			    chart = lv_chart_create(obj2);
			    lv_obj_set_size(chart, 300, 110);
			    lv_obj_set_style_bg_color(chart, lv_color_black(), LV_PART_MAIN);
			    lv_chart_set_div_line_count(chart, 0, 0);
			    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, 0);
			    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4000);
			    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
			    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);


			     ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
			    		 LV_CHART_AXIS_PRIMARY_Y);

			    lv_chart_set_point_count(chart, 60);
			    lv_chart_set_all_value(chart, ser, 30);
			//else if speedo place speed
			}else if(opts->chart == 0 && opts->speedo == 1){

			//else if speedo n chart
			}else if(opts->chart == 1 && opts->speedo == 1){

			}*/
		}else if(counter == 2){

		}else if(counter == 3){

		}else if(counter == 4){

		}
	}
	if(opts->beta){
		//if chart place small digits and chart
		if(counter == 1){
				label_beta = lv_label_create(obj2);
				lv_obj_align(label_beta, LV_ALIGN_LEFT_MID, 10, 0);
				lv_obj_set_width(label_beta, 400);
				lv_label_set_text(label_beta, "ATOM-SCAN.com");
				lv_obj_add_style(label_beta, &style2, LV_PART_MAIN);
		}else if(counter == 2){

		}else if(counter == 3){

		}else if(counter == 4){

		}
	}
	if(opts->gamma){
		//if chart place small digits and chart
		if(counter == 1){
			label_gamma = lv_label_create(obj2);
			lv_obj_align(label_gamma, LV_ALIGN_LEFT_MID, 10, 0);
			lv_obj_set_width(label_gamma, 400);
			lv_label_set_text(label_gamma, "ATOM-SCAN.com");
			lv_obj_add_style(label_gamma, &style2, LV_PART_MAIN);
		}else if(counter == 2){

		}else if(counter == 3){

		}else if(counter == 4){

		}
	}
	if(opts->neutron){
		//if chart place small digits and chart
		if(counter == 1){
			label_neutron = lv_label_create(obj2);
			lv_obj_align(label_neutron, LV_ALIGN_LEFT_MID, 10, 0);
			lv_obj_set_width(label_neutron, 400);
			lv_label_set_text(label_neutron, "ATOM-SCAN.com");
			lv_obj_add_style(label_neutron, &style2, LV_PART_MAIN);
		}else if(counter == 2){

		}else if(counter == 3){

		}else if(counter == 4){

		}
	}
}
