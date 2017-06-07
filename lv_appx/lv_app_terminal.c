/**
 * @file lv_app_terminal.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include <lv_conf.h>
#include <misc/gfx/area.h>
#include <lvgl/lv_obj/lv_dispi.h>
#include <lvgl/lv_obj/lv_obj.h>
#include <lvgl/lv_objx/lv_btn.h>
#include <lvgl/lv_objx/lv_cont.h>
#include <lvgl/lv_objx/lv_ddlist.h>
#include <lvgl/lv_objx/lv_label.h>
#include <lvgl/lv_objx/lv_page.h>
#include <lvgl/lv_objx/lv_ta.h>
#include <misc/gfx/color.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "lv_app_terminal.h"

#if LV_APP_ENABLE != 0 && USE_LV_APP_TERMINAL != 0

#include "lvgl/lv_app/lv_app_util/lv_app_kb.h"
#include <stdio.h>

/*********************
 *      DEFINES
 *********************/
#define OBJ_PAD (LV_DPI / 12)
#define AUTO_CONNECT_TCP_DELAY  5000    /*Wait before TCS server connect when the WiFi connect is ready*/ 

/**********************
 *      TYPEDEFS
 **********************/


/*Application specific data for an instance of this application*/
typedef struct
{
    char txt[LV_APP_TERMINAL_LENGTH + 1];
    lv_app_com_type_t com_type;
    lv_app_terminal_format_t format;
    lv_app_inst_t * last_sender;
}my_app_data_t;

/*Application specific data a window of this application*/
typedef struct
{
    lv_obj_t * label;
    lv_obj_t * ta;
    lv_obj_t * clear_btn;
}my_win_data_t;

/*Application specific data for a shortcut of this application*/
typedef struct
{
    lv_obj_t * label;
}my_sc_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void my_app_run(lv_app_inst_t * app, void * conf);
static void my_app_close(lv_app_inst_t * app);
static void my_com_rec(lv_app_inst_t * app_send, lv_app_inst_t * app_rec, lv_app_com_type_t type , const void * data, uint32_t size);
static void my_sc_open(lv_app_inst_t * app, lv_obj_t * sc);
static void my_sc_close(lv_app_inst_t * app);
static void my_win_open(lv_app_inst_t * app, lv_obj_t * win);
static void my_win_close(lv_app_inst_t * app);
static void my_conf_open(lv_app_inst_t * app, lv_obj_t * conf_win);

static void add_data(lv_app_inst_t * app, const void * data, uint16_t data_len);
static lv_action_res_t win_ta_rel_action(lv_obj_t * ta, lv_dispi_t * dispi);
static lv_action_res_t win_comtype_action(lv_obj_t * ddlist, lv_dispi_t * dispi);
static lv_action_res_t win_format_action(lv_obj_t * ddlist, lv_dispi_t * dispi);
static lv_action_res_t win_clear_rel_action(lv_obj_t * btn, lv_dispi_t * dispi);
static void win_ta_kb_ok_action(lv_obj_t * ta);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_app_dsc_t my_app_dsc =
{
	.name = "Terminal",
	.mode = LV_APP_MODE_NONE,
	.app_run = my_app_run,
	.app_close = my_app_close,
	.com_rec = my_com_rec,
	.win_open = my_win_open,
	.win_close = my_win_close,
	.sc_open = my_sc_open,
	.sc_close = my_sc_close,
    .conf_open = my_conf_open,
	.app_data_size = sizeof(my_app_data_t),
	.sc_data_size = sizeof(my_sc_data_t),
	.win_data_size = sizeof(my_win_data_t),
};

static const char * com_type_list_txt[] = {"Character", "Integer", "Log", "None", ""};
static lv_app_com_type_t com_type_list[] = {LV_APP_COM_TYPE_CHAR, LV_APP_COM_TYPE_INT, LV_APP_COM_TYPE_LOG, LV_APP_COM_TYPE_INV};
static const char * txt_format_list_txt[] = {"ASCII", "Hexadecimal", ""};
static lv_style_t style_sc_term;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the application
 * @return pointer to the application descriptor of this application
 */
const lv_app_dsc_t * lv_app_terminal_init(void)
{
    lv_style_get(LV_STYLE_PLAIN, &style_sc_term);
    style_sc_term.line_space = 0;
    style_sc_term.letter_space = 0;
    style_sc_term.txt_align = 0;
    style_sc_term.ccolor = COLOR_WHITE;
    style_sc_term.mcolor = COLOR_MAKE(0x20, 0x20, 0x20);
    style_sc_term.gcolor = COLOR_MAKE(0x20, 0x20, 0x20);

	return &my_app_dsc;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Run an application according to 'app_dsc'
 * @param app_dsc pointer to an application descriptor
 * @param conf pointer to a lv_app_example_conf_t structure with configuration data or NULL if unused
 * @return pointer to the opened application or NULL if any error occurred
 */
static void my_app_run(lv_app_inst_t * app, void * conf)
{
    /*Initialize the application*/
    my_app_data_t * app_data = app->app_data;

    if(conf != NULL) {
        app_data->com_type = ((lv_app_terminal_conf_t * ) conf)->com_type;
        app_data->format = ((lv_app_terminal_conf_t * ) conf)->format;
    } else {
        app_data->com_type = LV_APP_COM_TYPE_CHAR;
        app_data->format = LV_APP_TERMINAL_FORMAT_ASCII;
    }

    app_data->last_sender = NULL;

    memset(app_data->txt, 0, sizeof(app_data->txt));
}

/**
 * Close a running application.
 * Close the Window and the Shortcut too if opened.
 * Free all the allocated memory by this application.
 * @param app pointer to an application
 */
static void my_app_close(lv_app_inst_t * app)
{
    /*No dynamically allocated data in 'my_app_data'*/
}

/**
 * Read the data have been sent to this applicationstring
 * @param app_send pointer to an application which sent the message
 * @param app_rec pointer to an application which is receiving the message
 * @param type type of data from 'lv_app_com_type_t' enum
 * @param data pointer to the sent data
 * @param size length of 'data' in bytes
 */
static void my_com_rec(lv_app_inst_t * app_send, lv_app_inst_t * app_rec,
                       lv_app_com_type_t type , const void * data, uint32_t size)
{
    my_app_data_t * app_data = app_rec->app_data;

    /*Add the received data if the type is matches*/
	if(type == app_data->com_type) {

        /*Insert the name of the sender application if it is not the last*/
        if(app_data->last_sender != app_send) {
            add_data(app_rec, "\n@", 2);
            add_data(app_rec, app_send->name, strlen(app_send->name));
            add_data(app_rec, "\n", 1);
        }

        if(app_data->format == LV_APP_TERMINAL_FORMAT_HEX) {
            char hex_buf[8];
            const char * data_buf = data;
            uint32_t i;
            for(i = 0; i < size; i++) {
                sprintf(hex_buf, "%02x", data_buf[i]);
                add_data(app_rec, hex_buf, 2);
            }
        } else {
            add_data(app_rec, data, size);
        }
	}

	app_data->last_sender = app_send;
}

/**
 * Open a shortcut for an application
 * @param app pointer to an application
 * @param sc pointer to an object where the application
 *           can create content of the shortcut
 */
static void my_sc_open(lv_app_inst_t * app, lv_obj_t * sc)
{
    my_sc_data_t * sc_data = app->sc_data;
    my_app_data_t * app_data = app->app_data;

    /*Create a dark background*/
    lv_obj_t * txt_bg = lv_obj_create(sc, NULL);
    lv_obj_set_size(txt_bg, 7 * LV_APP_SC_WIDTH / 8 , app->sc->cords.y2 - app->sc_title->cords.y2 - 10 * LV_DOWNSCALE);
    lv_obj_set_style(txt_bg, &style_sc_term);
    lv_obj_align(txt_bg, app->sc_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 3 * LV_DOWNSCALE);
    lv_obj_set_click(txt_bg, false);

    /*Add a text with the text of the terminal*/
    sc_data->label = lv_label_create(txt_bg, NULL);
    lv_label_set_long_mode(sc_data->label, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(sc_data->label, lv_obj_get_width(txt_bg) - LV_APP_SC_WIDTH / 8);
    lv_label_set_text_static(sc_data->label, app_data->txt);
    lv_obj_align(sc_data->label, txt_bg, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
}

/**
 * Close the shortcut of an application
 * @param app pointer to an application
 */
static void my_sc_close(lv_app_inst_t * app)
{
    /*No dynamically allocated data in 'my_sc_data'*/
}

/**
 * Open the application in a window
 * @param app pointer to an application
 * @param win pointer to a window object where
 *            the application can create content
 */
static void my_win_open(lv_app_inst_t * app, lv_obj_t * win)
{
    my_win_data_t * win_data = app->win_data;
    my_app_data_t * app_data = app->app_data;

    /*Make the window content responsive*/
    lv_cont_set_layout(lv_page_get_scrl(lv_win_get_page(win)), LV_CONT_LAYOUT_PRETTY);

    /*Create a label for the text of the terminal*/
    win_data->label = lv_label_create(win, NULL);
    lv_label_set_long_mode(win_data->label, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(win_data->label, lv_win_get_width(win));
    lv_label_set_text_static(win_data->label, app_data->txt); /*Use the app. data text directly*/

    /*Create a text area. Text can be added to the terminal from here by app. keyboard.*/
    win_data->ta = lv_ta_create(win, NULL);
    lv_obj_set_size(win_data->ta, 3 * LV_HOR_RES / 5, LV_VER_RES / 4);
    lv_obj_set_free_p(win_data->ta, app);
    lv_page_set_rel_action(win_data->ta, win_ta_rel_action);
    lv_page_glue_obj(win_data->ta, true);
    lv_ta_set_text(win_data->ta, "");

    /*Create a clear button*/
    win_data->clear_btn = lv_btn_create(win, NULL);
    lv_cont_set_fit(win_data->clear_btn, true, true);
    lv_obj_set_free_p(win_data->clear_btn, app);
    lv_page_glue_obj(win_data->ta, true);
    lv_btn_set_rel_action(win_data->clear_btn, win_clear_rel_action);
    lv_obj_t * btn_label = lv_label_create(win_data->clear_btn, NULL);
    lv_label_set_text(btn_label, "Clear");

    /*Align the window to see the text area on the bottom*/
    lv_obj_t * page = lv_win_get_page(app->win);
    lv_obj_align(lv_page_get_scrl(page), NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, - LV_VER_RES);
}

/**
 * Close the window of an application
 * @param app pointer to an application
 */
static void my_win_close(lv_app_inst_t * app)
{

}

/**
 * Create objects to configure the applications
 * @param app pointer to an application which settings should be created
 * @param conf_win pointer to a window where the objects can be created
 *                (the window has the proper layout)
 */
static void my_conf_open(lv_app_inst_t * app, lv_obj_t * conf_win)
{
    my_app_data_t * app_data = app->app_data;

    lv_obj_t * label;
    label = lv_label_create(conf_win, NULL);
    lv_label_set_text(label, "Communication type");

    lv_obj_t * ddl;
    ddl = lv_ddlist_create(conf_win, NULL);
    lv_obj_set_free_p(ddl, app);
    lv_ddlist_set_options(ddl, com_type_list_txt);
    lv_ddlist_set_action(ddl, win_comtype_action);
    lv_ddlist_set_selected(ddl, app_data->com_type);

    label = lv_label_create(conf_win, label);
    lv_label_set_text(label, "\nText format");  /*First '\n' keeps space from the list above*/
    ddl = lv_ddlist_create(conf_win, ddl);
    lv_ddlist_set_options(ddl, txt_format_list_txt);
    lv_ddlist_set_selected(ddl, app_data->format);
    lv_ddlist_set_action(ddl, win_format_action);
}

/*--------------------
 * OTHER FUNCTIONS
 ---------------------*/

/**
 * Called when the Text area is released to open the app. keybard
 * @param ta pointer to the text area
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_OK because the text area is not deleted
 */
static lv_action_res_t win_ta_rel_action(lv_obj_t * ta, lv_dispi_t * dispi)
{
    lv_app_kb_open(ta, LV_APP_KB_MODE_TXT | LV_APP_KB_MODE_WIN_RESIZE, NULL, win_ta_kb_ok_action);

    return LV_ACTION_RES_OK;
}

/**
 * Called when an option is chosen in the communication type drop down list on the configuration window
 * @param ddl pointer to the drop down list
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_OK because the list is not deleted
 */
static lv_action_res_t win_comtype_action(lv_obj_t * ddlist, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(ddlist);
    my_app_data_t * app_data = app->app_data;

    app_data->com_type = com_type_list[lv_ddlist_get_selected(ddlist)];

    return LV_ACTION_RES_OK;
}

/**
 * Called when an option is chosen in the format drop down list on the configuration window
 * @param ddl pointer to the drop down list
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_OK because the list is not deleted
 */
static lv_action_res_t win_format_action(lv_obj_t * ddlist, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(ddlist);
    my_app_data_t * app_data = app->app_data;
    uint16_t opt = lv_ddlist_get_selected(ddlist);
    if(strcmp(txt_format_list_txt[opt], "Hexadecimal") == 0) {
        app_data->format = LV_APP_TERMINAL_FORMAT_HEX;
    } else if (strcmp(txt_format_list_txt[opt], "ASCII") == 0) {
        app_data->format = LV_APP_TERMINAL_FORMAT_ASCII;
    }

    return LV_ACTION_RES_OK;
}


/**
 * Called when the Clear button is released to clear the text of the terminal
 * @param btn pointer to the clear button
 * @param dispi pointer to the caller display input
 * @return LV_ACTION_RES_OK because the button is not deleted
 */
static lv_action_res_t win_clear_rel_action(lv_obj_t * btn, lv_dispi_t * dispi)
{
    lv_app_inst_t * app = lv_obj_get_free_p(btn);
    my_app_data_t * app_data = app->app_data;
    my_win_data_t * win_data = app->win_data;
    my_sc_data_t * sc_data = app->sc_data;

    app_data->txt[0] = '\0';

    if(sc_data != NULL) {
        lv_label_set_text_static(sc_data->label, app_data->txt);
        lv_obj_align(sc_data->label, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    }

    if(win_data != NULL) {
        lv_label_set_text_static(win_data->label, app_data->txt);
        lv_obj_t * page = lv_win_get_page(app->win);
        lv_obj_align(lv_page_get_scrl(page), NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, - LV_VER_RES);
    }

    return LV_ACTION_RES_OK;
}

/**
 * Called when the 'Ok' button of the keyboard in the window
 * is pressed to write to the Terminal
 * @param ta pointer to the Text area in the window
 */
static void win_ta_kb_ok_action(lv_obj_t * ta)
{
    lv_app_inst_t * app = lv_obj_get_free_p(ta);
    my_app_data_t * app_data = app->app_data;
    const char * ta_txt = lv_ta_get_txt(ta);
    uint32_t ta_txt_len = strlen(ta_txt);
    if(app_data->txt[0] != '\0') add_data(app, "\n", 1);
    add_data(app, ">", 1);
    add_data(app, ta_txt, ta_txt_len);

    lv_app_com_send(app, app_data->com_type, ta_txt, ta_txt_len);

    lv_ta_set_text(ta, "");
    app_data->last_sender = NULL;   /*Now the least data in the terminal is from this app*/
    
}

/**
 * Add data to the terminal
 * @param app pointer to a Terminal application
 * @param data pointer to the data
 * @param data_len length of 'data' in bytes
 */
static void add_data(lv_app_inst_t * app, const void * data, uint16_t data_len)
{
    my_app_data_t * app_data = app->app_data;
    uint16_t old_len = strlen(app_data->txt);
    const char * txt = data;

    /*IF the data is longer then the terminal ax size show the last part of data*/
    if(data_len > LV_APP_TERMINAL_LENGTH) {
        txt += (data_len - LV_APP_TERMINAL_LENGTH);
        data_len = LV_APP_TERMINAL_LENGTH;
        old_len = 0;
    }
    /*If the text become too long 'forget' the oldest lines*/
    else if(old_len + data_len > LV_APP_TERMINAL_LENGTH) {
        uint16_t new_start;
        for(new_start = 0; new_start < old_len; new_start++) {
            if(app_data->txt[new_start] == '\n') {
                /*If there is enough space break*/
                if(new_start >= data_len) {
                    /*Ignore line breaks*/
                    while(app_data->txt[new_start] == '\n' || app_data->txt[new_start] == '\r') new_start++;
                    break;
                }
            }
        }

        /* If it wasn't able to make enough space on line breaks
         * simply forget the oldest characters*/
        if(new_start == old_len) {
            new_start = old_len - (LV_APP_TERMINAL_LENGTH - data_len);
        }
        /*Move the remaining text to the beginning*/
        uint16_t j;
        for(j = new_start; j < old_len; j++) {
            app_data->txt[j - new_start] = app_data->txt[j];
        }
        old_len = old_len - new_start;
        app_data->txt[old_len] = '\0';

    }

    memcpy(&app_data->txt[old_len], txt, data_len);
    app_data->txt[old_len + data_len] = '\0';

    my_win_data_t * win_data = app->win_data;
    my_sc_data_t * sc_data = app->sc_data;

    if(win_data != NULL) {
        lv_label_set_text_static(win_data->label, app_data->txt);
        lv_obj_t * page = lv_win_get_page(app->win);
        lv_obj_align(lv_page_get_scrl(page), NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, - LV_VER_RES);
    }

    /*Set the last line on the shortcut*/
    if(sc_data != NULL) {
        lv_label_set_text_static(sc_data->label, app_data->txt);
        lv_obj_align(sc_data->label, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    }
}

#endif /*LV_APP_ENABLE != 0 && USE_LV_APP_TERMINAL != 0*/
