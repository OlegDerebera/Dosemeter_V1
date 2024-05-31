


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/soc_caps.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "lvgl.h"
#include "main.h"


#define LV_CONF_INCLUDE_SIMPLE 1
#define CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341 		1

#if CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341
#include "esp_lcd_ili9341.h"
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
#include "esp_lcd_gc9a01.h"
#endif

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_STMPE610
#include "esp_lcd_touch_stmpe610.h"
#endif

static const char *TAG = "example";

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (30 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_SCLK           18
#define EXAMPLE_PIN_NUM_MOSI           23//19
#define EXAMPLE_PIN_NUM_MISO           21
#define EXAMPLE_PIN_NUM_LCD_DC         27//5
#define EXAMPLE_PIN_NUM_LCD_RST        33//3
#define EXAMPLE_PIN_NUM_LCD_CS         14//4
#define EXAMPLE_PIN_NUM_BK_LIGHT       32//2
#define EXAMPLE_PIN_NUM_TOUCH_CS       15

// The pixel number in horizontal and vertical
#if CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341
#define EXAMPLE_LCD_H_RES              320
#define EXAMPLE_LCD_V_RES              240
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              240
#endif
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2


#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
esp_lcd_touch_handle_t tp = NULL;
#endif

#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_4
#define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_7
#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_2
//================================================================
extern void example_lvgl_demo_ui(lv_disp_t *disp);
extern void my_demo(lv_disp_t *disp);
extern void my_demo2(lv_disp_t *disp);
extern void redraw_widgets(widget_options* opts);
void lcd_init();
void spi_init();
void int_init(gpio_config_t* gpio_intrpt);
void lv_driver_init();
//void led_init(void);
//void draw_table();
static void continuous_adc_init(adc_channel_t *channel,
		uint8_t channel_num, adc_continuous_handle_t *out_handle);
static bool example_adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);
//===========================================================
TaskHandle_t task_gpio_h, task_adc_h, task_lvgl_tick_h, task_gpio_2_h;
QueueHandle_t btn_queue = NULL;
int queue_buffer[5];
nvs_handle_t nvs_h;
esp_err_t error;
/*extern*/ lv_obj_t* slider;
lv_disp_drv_t disp_drv;
lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
lv_disp_t *disp;
lv_indev_t * my_indev;
lv_group_t * g;
esp_lcd_panel_handle_t panel_handle = NULL;
spi_bus_config_t buscfg;
static int adc_raw[2][10];
static int voltage[2][10];
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1;
adc_cali_handle_t adc1_cali_handle = NULL;
bool do_calibration1 ;
adc_oneshot_unit_handle_t adc2_handle;
adc_oneshot_unit_init_cfg_t init_config2;
adc_cali_handle_t adc2_cali_handle = NULL;
bool do_calibration2;
adc_oneshot_chan_cfg_t config;
//-----------LVGL--------------------------------
lv_group_t * g_menu;
lv_group_t* g_empty;
lv_group_t *g_widgets;
/*static*/ lv_obj_t * scr1;
char str_adc[10];
char str_alpha[10];
char str_beta[10];
char str_gamma[10];
char str_neutron[10];
lv_obj_t * label_adc;
lv_obj_t * label_alpha;
lv_obj_t * label_beta;
lv_obj_t * label_gamma;
lv_obj_t * label_neutron;
lv_obj_t* chart;
lv_chart_series_t * ser;
lv_obj_t* table;
lv_obj_t* table_widgets;
/*static*/ lv_obj_t* screenMenu;
/*static*/ lv_obj_t* screenWidgets;
//-----------------------------------------------
uint8_t inMenu = 0;
uint8_t isSelected = 0;
uint8_t is38disabled = 0;
uint8_t isExit = 0;
uint8_t navigation = 0;
uint8_t alpha, beta, gamma, neutron;

volatile widget_options options = {0, 0, 0, 0, 0, 0};

//========================
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void example_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);	//true false
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}


static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void IRAM_ATTR gpio_isr_slider(void* args)
{
	int pin = (int)args;
	xQueueSendFromISR(btn_queue, &pin, NULL);
}

void encoder_with_keys_read(lv_indev_drv_t * drv, lv_indev_data_t* data){

	(void) drv;

	if(gpio_get_level(37) && gpio_get_level(38)
	   && gpio_get_level(39) )
	{
		data->state = LV_INDEV_STATE_RELEASED;
	}else{

		if(gpio_get_level(37) == 0)
		{

			data->key = LV_KEY_DOWN;
		}
		if(gpio_get_level(38) == 0)
		{
			data->key = LV_KEY_ENTER;
		}
		if(gpio_get_level(39) == 0)
		{
			data->key = LV_KEY_UP;
		}
		data->state = LV_INDEV_STATE_PRESSED;
	}
}


//========================================================================
static lv_obj_t *meter;
static lv_obj_t * btn;
static lv_disp_rot_t rotation = LV_DISP_ROT_NONE;

static void set_value(void *indic, int32_t v)
{
    lv_meter_set_indicator_end_value(meter, indic, v);
}


static void btn_cb(lv_event_t * e)
{
    lv_disp_t *disp = lv_event_get_user_data(e);
    rotation++;
    if (rotation > LV_DISP_ROT_270) {
        rotation = LV_DISP_ROT_NONE;
    }
    lv_disp_set_rotation(disp, rotation);
}
//======

void lv_table_set_selected_cell(lv_obj_t * obj, uint16_t * row, uint16_t * col)
{
    lv_table_t * table = (lv_table_t *)obj;
    table->row_act = row;
    table->col_act = col;
}

/*static*/ void table_event_cb(lv_event_t* e)
{
	lv_obj_t* obj = lv_event_get_target(e);
	static uint8_t isEnterPressed = 0;
	static uint8_t isLongPressed = 0;
    uint16_t col;
    uint16_t row;
    lv_table_get_selected_cell(obj, &row, &col);
    navigation = row;
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

    if(code == LV_EVENT_PRESSED){
    	ESP_LOGI(TAG, "enter");
    	isEnterPressed = 1;

    }

    if(code == LV_EVENT_LONG_PRESSED_REPEAT)
    {
    	//lv_obj_del(target);
    	isLongPressed = 1;
    	ESP_LOGI(TAG, "press");
    }
    if(code == LV_EVENT_RELEASED)
    {
		if(isLongPressed == 1 && isEnterPressed == 1)
		{
			ESP_LOGI(TAG, "Long press release");
	    	lv_scr_load(scr1);
	    	inMenu = 0;
	    	//is38disabled = 1;
	    	gpio_intr_enable(38);
			//lv_obj_add_flag(table, LV_OBJ_FLAG_HIDDEN);
	    	lv_indev_set_group(my_indev, g_empty);
	    	isLongPressed = 0;
	    	isEnterPressed = 0;
	    	isExit = 1;
	    	//xTaskNotifyGive(task_gpio_h);

		}else if(isEnterPressed == 1)
		{

			ESP_LOGI(TAG, "Row: %d, Col: %d", row, col);
			if(row == 1)	// set temp
			{
				ESP_LOGI(TAG, "Table trans");

				isSelected = 1;
				lv_table_set_selected_cell(table, (uint16_t*)1, (uint16_t*)1);
				lv_indev_set_group(my_indev, g_empty);
				gpio_intr_enable(39);
				gpio_intr_enable(37);
				//gpio_intr_disable(38);
				gpio_intr_enable(38);

			}else if(row == 2){		//pid
				ESP_LOGI(TAG, "Table trans");

				isSelected = 1;
				lv_table_set_selected_cell(table, (uint16_t*)2, (uint16_t*)1);
				lv_indev_set_group(my_indev, g_empty);
				gpio_intr_enable(39);
				gpio_intr_enable(37);
				//gpio_intr_disable(38);
				gpio_intr_enable(38);

			}else if(row == 0){		//start stop

				ESP_LOGI(TAG, "Table trans");

				isSelected = 1;
				lv_table_set_selected_cell(table, (uint16_t*)0, (uint16_t*)1);
				lv_indev_set_group(my_indev, g_empty);
				gpio_intr_enable(39);
				gpio_intr_enable(37);
				//gpio_intr_disable(38);
				gpio_intr_enable(38);

			}else if(row == 0 && col == 0){
				//lv_table_set_selected_cell(table, (uint16_t*)0, (uint16_t*)0);
			}else if(row == 4){
				lv_scr_load(screenWidgets);
				lv_indev_set_group(my_indev, g_widgets);
				lv_group_set_editing(g_widgets, true);
				//lv_group_focus_obj(table_widgets);
				//lv_event_send(table_widgets, LV_EVENT_PRESSED, NULL);
			}
			ESP_LOGI(TAG, "pussy");
			isEnterPressed = 0;
		}else{
			ESP_LOGI(TAG, "noPussy");
		}
    }
}


void table_widget_event_cb(lv_event_t* e)
{
	lv_obj_t* obj = lv_event_get_target(e);
	static uint8_t isEnterPressed = 0;
	static uint8_t isLongPressed = 0;
    uint16_t col;
    uint16_t row;
    lv_table_get_selected_cell(obj, &row, &col);
    navigation = row;
    lv_event_code_t code = lv_event_get_code(e);
    //lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

    if(code == LV_EVENT_PRESSED){
    	ESP_LOGI(TAG, "enter");
    	isEnterPressed = 1;

    }

    if(code == LV_EVENT_LONG_PRESSED_REPEAT)
    {
    	isLongPressed = 1;
    	ESP_LOGI(TAG, "press");
    }
    if(code == LV_EVENT_RELEASED)
    {
		if(isLongPressed == 1 && isEnterPressed == 1)//exit
		{
			ESP_LOGI(TAG, "Long press release");
	    	lv_scr_load(screenMenu);
	    	inMenu = 1;
	    	//is38disabled = 1;
	    	//gpio_intr_enable(38);
	    	lv_indev_set_group(my_indev, g_menu);
	    	isLongPressed = 0;
	    	isEnterPressed = 0;
	    	isExit = 1;
	    	//redraw_widgets(&options);
	    	//xTaskNotifyGive(task_gpio_h);

		}else if(isEnterPressed == 1)
		{

			ESP_LOGI(TAG, "Row: %d, Col: %d", row, col);
			if(row == 0)	// Alpha
			{
				ESP_LOGI(TAG, "Table trans");

			    bool chk = lv_table_has_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    if(chk){
			    	lv_table_clear_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.alpha = 0;
			    }
			    else{
			    	lv_table_add_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	//options.alpha = 1;
			    }

			}else if(row == 1){		//Beta
				ESP_LOGI(TAG, "Table trans");
			    bool chk = lv_table_has_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    if(chk){
			    	lv_table_clear_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.beta = 0;
			    }
			    else{
			    	lv_table_add_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.beta = 1;
			    }

			}else if(row == 2){		//Gamma

				ESP_LOGI(TAG, "Table trans");
			    bool chk = lv_table_has_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    if(chk){
			    	lv_table_clear_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.gamma = 0;
			    }
			    else{
			    	lv_table_add_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.gamma = 1;
			    }
			}else if(row == 3){		//Neutron
			    bool chk = lv_table_has_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    if(chk){
			    	lv_table_clear_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.neutron = 0;
			    }
			    else{
			    	lv_table_add_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_CUSTOM_1);
			    	options.neutron = 1;
			    }
			}
			ESP_LOGI(TAG, "pussy");
			isEnterPressed = 0;
		}else{
			ESP_LOGI(TAG, "noPussy");
		}
    }
}	// table widget

void my_demo(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);



    meter = lv_meter_create(scr);


    //meter = lv_meter_create(scr);
    lv_obj_center(meter);
    lv_obj_set_size(meter, 200, 200);

    /*Add a scale first*/
    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 15, lv_color_black(), 10);

    lv_meter_indicator_t *indic;

    /*Add a blue arc to the start*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /*Make the tick lines blue at the start of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /*Add a red arc to the end*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /*Make the tick lines red at the end of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /*Add a needle line indicator*/
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    btn = lv_btn_create(scr);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(btn, LV_STATE_FOCUSED);
    lv_group_add_obj(g, btn);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_REFRESH" ROTATE");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    /*Button event*/
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, disp);

    /*Create an animation to set the value*/
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_value);
    lv_anim_set_var(&a, indic);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/*static*/ void draw_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    /*If the cells are drawn...*/
    if(dsc->part == LV_PART_ITEMS && dsc->id == 6) {
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

void task_gpio(void* args) // 0
{
	int pin = 0;
	uint16_t row = 0;
	uint16_t col = 0;

	for(;;)
	{

		if(xQueueReceive(btn_queue, &pin, portMAX_DELAY)){
			if(pin == 38){
				vTaskDelay(100 / portTICK_PERIOD_MS);
				if(inMenu)
				{
					if(isSelected){
						//vTaskDelay(100 / portTICK_PERIOD_MS);
						ESP_LOGI(TAG, "selected");
						//gpio_intr_disable(38);
						lv_table_set_selected_cell(table,
								(uint16_t*)navigation, (uint16_t*)0);
						lv_indev_set_group(my_indev, g_menu);
						isSelected = 0;
						//vTaskDelay(100 / portTICK_PERIOD_MS);
						gpio_intr_disable(38);
						gpio_intr_disable(39);

					}else{
						vTaskDelay(100 / portTICK_PERIOD_MS);
						ESP_LOGI(TAG, "intr pin %d", pin);
						//lv_table_set_selected_cell(table, (uint16_t*)0, (uint16_t*)1);
						//lv_indev_set_group(my_indev, g_empty);
						//isSelected = 1;
					}

				}else{ //no menu
					vTaskDelay(100 / portTICK_PERIOD_MS);
					lv_scr_load(screenMenu);
					inMenu = 1;
					lv_indev_set_group(my_indev, g_menu);
					lv_group_set_editing(g_menu, true);
					gpio_intr_disable(38);
				}
			}else if(pin == 39){

				lv_table_get_selected_cell(table, &row, &col);
				ESP_LOGI(TAG, "Row: %d, Col: %d", row, col);
				lv_table_set_cell_value(table, row, col, "Yupppy!");
				//lv_indev_set_group(my_indev, g_menu);
				is38disabled = 0;
				//ESP_LOGI(TAG, "intr pin %d", pin);
			}else if(pin == 37){

			}
		}
		ESP_LOGI(TAG, "before");
		//ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		ESP_LOGI(TAG, "after");
		vTaskDelay(100 / portTICK_PERIOD_MS);

	}
}

void task_gpio_2(void* args) // 0
{
	int pin = 0;
	uint16_t row = 0;
	uint16_t col = 0;

	for(;;)
	{
		if(isExit)
		{
			ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			ESP_LOGI(TAG, "notifytake");
			vTaskDelay(500 / portTICK_PERIOD_MS);
			isExit = 0;
			continue;
		}

			if(gpio_get_level(38) == 0){
				vTaskDelay(100 / portTICK_PERIOD_MS);
				if(inMenu)
				{
					if(isSelected){
						//vTaskDelay(100 / portTICK_PERIOD_MS);
						ESP_LOGI(TAG, "selected");
						lv_table_set_selected_cell(table, (uint16_t*)0, (uint16_t*)0);
						lv_indev_set_group(my_indev, g_menu);
						isSelected = 0;
						vTaskDelay(100 / portTICK_PERIOD_MS);
					}else{
						vTaskDelay(100 / portTICK_PERIOD_MS);
						ESP_LOGI(TAG, "intr pin %d", pin);

						//lv_table_set_selected_cell(table, (uint16_t*)0, (uint16_t*)1);
						//lv_indev_set_group(my_indev, g_empty);
						//isSelected = 1;
					}

				}else{ //no menu
					vTaskDelay(100 / portTICK_PERIOD_MS);
					lv_scr_load(screenMenu);
					inMenu = 1;
					lv_indev_set_group(my_indev, g_menu);
					//gpio_intr_disable(38);
				}
			}else if(pin == 39){

				lv_table_get_selected_cell(table, &row, &col);
				lv_table_set_cell_value(table, 0, 1, "Yupppy!");
				lv_indev_set_group(my_indev, g_menu);
				is38disabled = 0;
				//ESP_LOGI(TAG, "intr pin %d", pin);
			}else if(pin == 37){

			}
		}
		ESP_LOGI(TAG, "before");
		//ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		ESP_LOGI(TAG, "after");
		//vTaskDelay(1000 / portTICK_PERIOD_MS);
		if(is38disabled){
			//ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			gpio_intr_enable(38);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}

}


void task_lvgl_tick(void* args)//2
{
	while(1)
	{
		// raise the task priority of LVGL and/or reduce the handler period can improve the performance
		vTaskDelay(pdMS_TO_TICKS(10));
		// The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
		lv_timer_handler();
	}
}
void task_adc(void* args)//1
{

	while(1)
	{
		ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0,
				&adc_raw[0][0]));
		//ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw[0][0]);
		if (do_calibration1) {
			ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle,
					adc_raw[0][0], &voltage[0][0]));
			//ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, voltage[0][0]);
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN1,
				&adc_raw[0][1]));
		//ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN1, adc_raw[0][1]);
		if (do_calibration1) {
			ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle,
					adc_raw[0][1], &voltage[0][1]));
			//ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN1, voltage[0][1]);
		}
		vTaskDelay(pdMS_TO_TICKS(100));

		ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle,
				EXAMPLE_ADC2_CHAN0, &adc_raw[1][0]));
		//ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, adc_raw[1][0]);
		if (do_calibration2) {
			ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle,
					adc_raw[1][0], &voltage[1][0]));
			//ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, EXAMPLE_ADC2_CHAN0, voltage[1][0]);
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
		if(options.alpha){
			sprintf(str_alpha, "%d", adc_raw[1][0]);
			lv_label_set_text(label_alpha, str_alpha);
			lv_chart_set_next_value(chart, ser, adc_raw[1][0]);
		}
		if(options.beta){
			sprintf(str_beta, "%d", adc_raw[0][0]);
			lv_label_set_text(label_beta, str_beta);
			lv_chart_set_next_value(chart, ser, adc_raw[0][0]);
		}
		if(options.gamma){
			sprintf(str_gamma, "%d", adc_raw[0][1]);
			lv_label_set_text(label_gamma, str_gamma);
			lv_chart_set_next_value(chart, ser, adc_raw[0][1]);
		}
		if(options.neutron){
			sprintf(str_adc, "%d", adc_raw[1][0]);
			lv_label_set_text(label_adc, str_adc);
			lv_chart_set_next_value(chart, ser, adc_raw[1][0]);
		}
		sprintf(str_adc, "%d", adc_raw[1][0]);
		lv_label_set_text(label_adc, str_adc);
		lv_chart_set_next_value(chart, ser, adc_raw[1][0]);
	}
}//task_adc

void app_main(void)
{
	//off fucking stereo
		gpio_set_direction(25, GPIO_MODE_OUTPUT);
		gpio_set_level(25, 0);

		gpio_config_t gpio = {
				.pin_bit_mask = 1ULL << GPIO_NUM_39,
				.mode = GPIO_MODE_INPUT,
				.pull_up_en = GPIO_PULLUP_ENABLE,
				.pull_down_en = GPIO_PULLDOWN_DISABLE,
		};
		gpio_config(&gpio);
		gpio.pin_bit_mask = 1ULL << GPIO_NUM_37;
		gpio.mode = GPIO_MODE_INPUT,
		gpio.pull_up_en = GPIO_PULLUP_ENABLE,
		gpio.pull_down_en = GPIO_PULLDOWN_DISABLE,
		gpio_config(&gpio);
		gpio.pin_bit_mask = 1ULL << GPIO_NUM_38;
		gpio.mode = GPIO_MODE_INPUT,
		gpio.pull_up_en = GPIO_PULLUP_ENABLE,
		gpio.pull_down_en = GPIO_PULLDOWN_DISABLE,
		gpio_config(&gpio);
		//Hardware Buttons initialization
		gpio_config_t gpio_intrpt;
		int_init(&gpio_intrpt);

	    //-------------ADC1 Init---------------//

	    init_config1.unit_id = ADC_UNIT_1;

	    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

	    //-------------ADC1 Config---------------//

	    config.bitwidth = ADC_BITWIDTH_DEFAULT;
	    config.atten = ADC_ATTEN_DB_11;

	    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));
	    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN1, &config));

	    //-------------ADC1 Calibration Init---------------//
	    adc1_cali_handle = NULL;
	    do_calibration1 = example_adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc1_cali_handle);

	    //-------------ADC2 Init---------------//

	    init_config2.unit_id = ADC_UNIT_2;
	    init_config2.ulp_mode = ADC_ULP_MODE_DISABLE;

	    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

	    //-------------ADC2 Calibration Init---------------//
	    adc2_cali_handle = NULL;
	    do_calibration2 = example_adc_calibration_init(ADC_UNIT_2, ADC_ATTEN_DB_11, &adc2_cali_handle);

	    //-------------ADC2 Config---------------//
	    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_ADC2_CHAN0, &config));

	    //esp_err_t ret;
	    //uint32_t ret_num = 0;
	    //uint8_t result[100] = {0};

	    ESP_LOGI(TAG, "Turn off LCD backlight");
	    gpio_config_t bk_gpio_config = {
	        .mode = GPIO_MODE_OUTPUT,
	        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
	    };
	    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

	    btn_queue = xQueueCreate(5, sizeof(32));

	    ESP_LOGI(TAG, "Initialize SPI bus");
	    spi_init();
	    lcd_init();

	    ESP_LOGI(TAG, "Initialize LVGL library");
	    lv_init();
	    // alloc draw buffers used by LVGL
	    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
	    lv_driver_init();
	    g = lv_group_create();
	    g_menu = lv_group_create();
	    g_empty = lv_group_create();
	    g_widgets = lv_group_create();
	    lv_group_set_default(g);

	    //my_demo(disp);
	    //my_demo2(disp);
	    //lv_group_add_obj(g, slider);
	    //lv_group_add_obj(g, table);
	    lv_indev_set_group(my_indev, g);

	    //adc_continuous_handle_t adc1_h = NULL;

	    //continuous_adc_init(ADC_CHANNEL_0, sizeof(ADC_CHANNEL_0) / sizeof(adc_channel_t), &adc1_h);

	    xTaskCreate((void*)task_lvgl_tick, "task_lvgl", 8192, NULL,
	    		2, &task_lvgl_tick_h);
	    xTaskCreate((void*)task_gpio, "task_gpio", 2048, NULL,
	    		0, &task_gpio_h);
	    //xTaskCreate((void*)task_gpio_2, "task_gpio", 2048, NULL,
	    		//0, &task_gpio_2_h);

	    my_demo2(disp);
	    xTaskCreate((void*)task_adc, "task_adc", 2048, NULL,
	    		1, &task_adc_h);
	    //lv_table_set_selected_cell(table, (uint16_t*)1, (uint16_t*)1);
    while (1) {
        // raise the task priority of LVGL and/or reduce the handler period can improve the performance
        vTaskDelay(pdMS_TO_TICKS(10));
        // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        //lv_timer_handler();

        /*ret = adc_continuous_read(adc1_h, result, 100, &ret_num, 0);
        if (ret == ESP_OK) {
            ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32, ret, ret_num);
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (void*)&result[i];

            ESP_LOGI(TAG, "Unit: %d, Channel: %d, Value: %x", 1, p->type1.channel, p->type1.data);
            }
        }*/
    }
}

void lv_driver_init()
{
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 20);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    disp = lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);      			/*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_ENCODER;			/*See below.*/
    indev_drv.read_cb =	encoder_with_keys_read;		/*See below.*/
    indev_drv.long_press_time = 700;
    indev_drv.long_press_repeat_time = 100;
    /*Register the driver in LVGL and save the created input device object*/
    my_indev = lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

}
void int_init(gpio_config_t* gpio_intrpt)
{
	// GPIO 38
	gpio_intrpt->intr_type = GPIO_INTR_NEGEDGE;
	gpio_intrpt->mode = GPIO_MODE_INPUT;
	gpio_intrpt->pin_bit_mask = 1ULL << GPIO_NUM_38;
	gpio_intrpt->pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_intrpt->pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(gpio_intrpt);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(GPIO_NUM_38, gpio_isr_slider, (void*)38);
	//gpio_intr_disable(38);
	/*vTaskDelay(100 / portTICK_PERIOD_MS);
	gpio_intr_disable(38);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	gpio_intr_enable(38);*/

	//GPIO 39
	gpio_intrpt->intr_type = GPIO_INTR_POSEDGE;
	gpio_intrpt->mode = GPIO_MODE_INPUT;
	gpio_intrpt->pin_bit_mask = 1ULL << GPIO_NUM_39;
	gpio_intrpt->pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_intrpt->pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(gpio_intrpt);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(GPIO_NUM_39, gpio_isr_slider, (void*)39);
	gpio_intr_disable(39);

	//GPIO 37
	gpio_intrpt->intr_type = GPIO_INTR_POSEDGE;
	gpio_intrpt->mode = GPIO_MODE_INPUT;
	gpio_intrpt->pin_bit_mask = 1ULL << GPIO_NUM_37;
	gpio_intrpt->pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_intrpt->pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(gpio_intrpt);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(GPIO_NUM_37, gpio_isr_slider, (void*)37);
	gpio_intr_disable(37);
}
void spi_init()
{

	buscfg.sclk_io_num = EXAMPLE_PIN_NUM_SCLK;
	buscfg.mosi_io_num = EXAMPLE_PIN_NUM_MOSI;
	buscfg.miso_io_num = EXAMPLE_PIN_NUM_MISO;
	buscfg.quadwp_io_num = -1;
	buscfg.quadhd_io_num = -1;
	buscfg.max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t);

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
}
void lcd_init()
{
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    //esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
#if CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#if CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_STMPE610_CONFIG(EXAMPLE_PIN_NUM_TOUCH_CS);
    // Attach the TOUCH to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_STMPE610
    ESP_LOGI(TAG, "Initialize touch controller STMPE610");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_stmpe610(tp_io_handle, &tp_cfg, &tp));
#endif // CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_STMPE610
#endif // CONFIG_EXAMPLE_LCD_TOUCH_ENABLED

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
}
static void continuous_adc_init(adc_channel_t *channel,
		uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
	adc_continuous_handle_t adc1_h = NULL;

	adc_continuous_handle_cfg_t adc_handle_config = {
		.max_store_buf_size = 512,
		.conv_frame_size = 100,
	};
	ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_handle_config, &adc1_h));

	adc_continuous_config_t adc_config = {
		.pattern_num = 1,
		.sample_freq_hz = 20 * 1000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,

	};

	adc_digi_pattern_config_t adc_digi_pattern = {
			.atten = ADC_ATTEN_DB_11,
			.bit_width = ADC_BITWIDTH_12,
			.channel = ADC_CHANNEL_0,
			.unit = ADC_UNIT_1,
	};

	adc_config.adc_pattern = &adc_digi_pattern;

	ESP_ERROR_CHECK(adc_continuous_config(adc1_h, &adc_config));



	*out_handle = adc1_h;
}
static bool example_adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}
static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}



