/*
 * Copyright 2020 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file
 * \brief Implementation of the main menu.
 *
 * This module implements the main menu.
 */

#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../src/acq.h"
#include "../../src/util.h"

#include "sdl-tk/widget.h"

#include "sdl-tk/widget/menu.h"
#include "sdl-tk/widget/input.h"
#include "sdl-tk/widget/action.h"
#include "sdl-tk/widget/toggle.h"

#include "util.h"
#include "locked.h"
#include "bloodview.h"
#include "main-menu.h"

/** List of widget types used in the main menu. */
enum widget_type {
	WIDGET_TYPE_MENU,
	WIDGET_TYPE_INPUT,
	WIDGET_TYPE_ACTION,
	WIDGET_TYPE_TOGGLE,
};

/** Main menu widget descriptor. */
struct main_menu_widget_desc {
	enum widget_type type; /**< Widget type to create. */
	const char *title;     /**< Widget title. */
	union {
		struct main_menu_widget_desc_menu {
			/** Array of menu entries' details. */
			const struct main_menu_widget_desc *entries;
			/** Count of menu entries. */
			unsigned count;
		} menu; /**< Menu widget type-specific data. */
		struct main_menu_widget_desc_input {
			/** Client callback for input. */
			const sdl_tk_widget_input_cb cb;
			/** Initial value. */
			const char *initial;
		} input; /**< Input widget type-specific data. */
		struct main_menu_widget_desc_action {
			/** Client callback for activation. */
			const sdl_tk_widget_action_cb cb;
			/**< Client private data. */
			void *pw;
		} action; /**< Action widget type-specific data. */
		struct main_menu_widget_desc_toggle {
			bool initial; /**< Initial value. */
		} toggle; /**< Toggle widget type-specific data. */
	};
};

/** An HSV colour. */
struct main_menu_colour {
	unsigned hue;        /**< Hue in HSV colour space. */
	unsigned saturation; /**< Saturation in HSV colour space. */
	unsigned value;      /**< Value in HSV colour space. */
};

/** Current configuration for LEDs. */
struct main_menu_config_leds {
	uint8_t leds[BL_LED_COUNT]; /**< Array of LED enablement. */
};

/** Current configuration for a channel channel. */
struct main_menu_config_channel_chan {
	unsigned offset;   /**< Offset for 16-bit sample mode. */
	unsigned shift;    /**< Shift for 16-bit sample mode. */
	bool     sample32; /**< Whether 16-bit sample mode is enabled. */
	bool     inverted; /**< Whether the data should be inverted. */

	struct main_menu_colour colour; /**< The channel rendering colour. */

	struct sdl_tk_widget *widget_shift;  /**< Widget for channel shift. */
	struct sdl_tk_widget *widget_offset; /**< Widget for channel offset. */
};

/** Current configuration for a channel source. */
struct main_menu_config_channel_src {
	unsigned sw_oversample; /**< Software oversample count. */
	unsigned opamp_gain;    /**< Source sample gain. */
	unsigned opamp_offset;  /**< Source sample offset. */
	unsigned hw_oversample; /**< Source hardware oversample. */
	unsigned hw_shift;      /**< Source hardware shift. */
};

/** Current acquisition configuration. */
struct main_menu_config_acq_params {
	unsigned frequency;                  /**< Sampling frequency in Hz. */
	uint8_t  sources[BL_ACQ__SRC_COUNT]; /**< Source enablement. */
};

/** Current filter configuration. */
struct main_menu_config_filter {
	uint8_t normalise_enable; /**< Normalisation filter enabled. */
	double  normalise;        /**< Frequency parameter for normalisation. */

	uint8_t ac_denoise_enable; /**< Denoising filter enabled. */
	double  ac_denoise;        /**< Frequency parameter for denoising */

	struct sdl_tk_widget *widget_normalise;  /**< Normalisation widget. */
	struct sdl_tk_widget *widget_ac_denoise; /**< Denoising widget. */
};

/** Config for channels == sources acquisitions. */
struct main_menu_config_channel {
	struct main_menu_config_channel_chan channel; /**< Channel config. */
	struct main_menu_config_channel_src  source;  /**< Source config. */
};

/** Main menu configuration entries. */
struct main_menu_config {
	struct main_menu_config_acq_params acq; /**< Acquisition config. */
	/** Channel config. */
	struct main_menu_config_channel channel[BL_ACQ__SRC_COUNT];
	struct main_menu_config_leds led;       /**< LED config. */
	struct main_menu_config_filter filter;  /**< Data filter config. */

	struct sdl_tk_widget *widget_cal;  /**< Widget for calibration. */
	struct sdl_tk_widget *widget_acq;  /**< Widget for acquisition. */
	struct sdl_tk_widget *widget_stop; /**< Widget to stop device. */
};

/** Bloodview main menu configuration context. */
static struct main_menu_config config;

/**
 * Callback for the toggle widgets.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New input widget value.
 */
static void main_menu_toggle_cb(
		void *pw,
		bool  new_value)
{
	uint8_t *value = pw;

	if (value == &config.filter.normalise_enable) {
		sdl_tk_widget_enable(
				config.filter.widget_normalise,
				new_value);

	} else if (value == &config.filter.ac_denoise_enable) {
		sdl_tk_widget_enable(
				config.filter.widget_ac_denoise,
				new_value);
	}

	*value = new_value;
}

/**
 * Callback for input widget change notification and validation.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New input widget value.
 * \return true to accept the new value. false to reject it.
 */
bool main_menu_numerical_input_cb(
		void       *pw,
		const char *new_value)
{
	unsigned *widget_value = pw;
	size_t len = strlen(new_value);

	for (unsigned i = 0; i < len; i++) {
		if (isdigit(new_value[i]) == false) {
			return false;
		}
	}

	return util_read_unsigned(new_value, widget_value);
}

/**
 * Callback for input widget change notification and validation.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New input widget value.
 * \return true to accept the new value. false to reject it.
 */
bool main_menu_double_input_cb(
		void       *pw,
		const char *new_value)
{
	double *widget_value = pw;
	size_t len = strlen(new_value);

	for (unsigned i = 0; i < len; i++) {
		if (isdigit(new_value[i]) == false &&
		    new_value[i] != '.' &&
		    new_value[i] != ',') {
			return false;
		}
	}

	return util_read_double(new_value, widget_value);
}

/* Filter menu entries/ */
enum {
	FILTER_VALUE_NORMALISE,
	FILTER_VALUE_AC_DENOISE,
	FILTER_ENABLE_NORMALISE,
	FILTER_ENABLE_AC_DENOISE,
};

/**
 * Filter setup menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_filter_entries[] = {
	[FILTER_VALUE_NORMALISE] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Normalisation frequency (Hz)",
		.input = {
			.initial = "0.5",
			.cb = main_menu_double_input_cb,
		},
	},
	[FILTER_VALUE_AC_DENOISE] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "AC denoise frequency (Hz)",
		.input = {
			.initial = "50",
			.cb = main_menu_double_input_cb,
		},
	},
	[FILTER_ENABLE_NORMALISE] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Normalisation",
		.toggle = {
			.initial = true,
		},
	},
	[FILTER_ENABLE_AC_DENOISE] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "AC denoise",
		.toggle = {
			.initial = true,
		},
	},
};

/**
 * LED selection menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_led_entries[BL_LED_COUNT] = {
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Blue (470nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Green (528nm)",
		.toggle = {
			.initial = true,
		},
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Yellow (570nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Orange (590nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Orange (612nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Red (638nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Red (660nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Red (740nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (850nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (880nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (940nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (1040nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (1200nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (1450nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (1550nm)",
	},
	{
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Infrared (1650nm)",
	},
};

/** Colour picker menu entry list */
enum {
	COLOUR_HUE,
	COLOUR_SAT,
	COLOUR_VAL,
};

/**
 * Colour picker menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_colour_entries[] = {
	[COLOUR_HUE] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Hue",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[COLOUR_SAT] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Saturation",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[COLOUR_VAL] = {
		.type   = WIDGET_TYPE_INPUT,
		.title  = "Value",
		.input = {
			.initial = "100",
			.cb = main_menu_numerical_input_cb,
		},
	},
};

/** Channel config menu entries. */
enum {
	CHAN_CHAN_SW_OFFSET,
	CHAN_CHAN_SW_SHIFT,
	CHAN_CHAN_32BIT,
	CHAN_CHAN_INVERT,
	CHAN_CHAN_COLOUR,
};

/**
 * Channel configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_chan_c_entries[] = {
	[CHAN_CHAN_SW_OFFSET] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Software Offset",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_CHAN_SW_SHIFT] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Software Shift",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_CHAN_32BIT] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "32-bit samples",
	},
	[CHAN_CHAN_INVERT] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Invert data",
	},
	[CHAN_CHAN_COLOUR] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Colour",
		.menu  = {
			.entries = bl_menu_conf_colour_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_colour_entries),
		},
	},
};

/** Channel config menu entries. */
enum {
	CHAN_SRC_SW_OVERSAMPLE,
	CHAN_SRC_OPAMP_GAIN,
	CHAN_SRC_OPAMP_OFFSET,
	CHAN_SRC_HW_OVERSAMPLE,
	CHAN_SRC_HW_SHIFT,
};

/**
 * Channel source configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_chan_s_entries[] = {
	[CHAN_SRC_SW_OVERSAMPLE] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Software Oversample",
		.input = {
			.initial = "512",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_SRC_OPAMP_GAIN] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Op-Amp Gain",
		.input = {
			.initial = "1",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_SRC_OPAMP_OFFSET] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Op-Amp Offset",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_SRC_HW_OVERSAMPLE] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Hardware Oversample",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_SRC_HW_SHIFT] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Hardware Shift",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
};

/** List of channel settings groupings. */
enum {
	BL_CHAN_SETUP_CHAN,
	BL_CHAN_SETUP_SRC,
};

/**
 * Channel configuration groups.
 */
static const struct main_menu_widget_desc bl_menu_conf_chan_setup_entries[] = {
	[BL_CHAN_SETUP_CHAN] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Channel",
		.menu  = {
			.entries = bl_menu_conf_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_c_entries),
		},
	},
	[BL_CHAN_SETUP_SRC] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Source",
		.menu  = {
			.entries = bl_menu_conf_chan_s_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_s_entries),
		},
	},
};

/**
 * Channel configuration list.
 */
static const struct main_menu_widget_desc bl_menu_conf_chan_entries[] = {
	[BL_ACQ_PD1] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 1",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
	[BL_ACQ_PD2] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 2",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
	[BL_ACQ_PD3] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 3",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
	[BL_ACQ_PD4] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 4",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
	[BL_ACQ_3V3] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "3.3 Volts",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
	[BL_ACQ_5V0] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "5.0 Volts",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
	[BL_ACQ_TMP] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Temperature",
		.menu  = {
			.entries = bl_menu_conf_chan_setup_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_setup_entries),
		},
	},
};

/**
 * Channel selection menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_acq_sources_entries[] = {
	[BL_ACQ_PD1] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Photodiode 1",
		.toggle = {
			.initial = true,
		},
	},
	[BL_ACQ_PD2] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Photodiode 2",
		.toggle = {
			.initial = true,
		},
	},
	[BL_ACQ_PD3] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Photodiode 3",
		.toggle = {
			.initial = true,
		},
	},
	[BL_ACQ_PD4] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Photodiode 4",
		.toggle = {
			.initial = true,
		},
	},
	[BL_ACQ_3V3] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "3.3 Volts",
	},
	[BL_ACQ_5V0] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "5.0 Volts",
	},
	[BL_ACQ_TMP] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Temperature",
	},
};

/** Acquisition config menu entries. */
enum {
	ACQ_FREQ,
	ACQ_SRC,
};

/**
 * Acquisition configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_acq_entries[] = {
	[ACQ_FREQ] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Frequency (Hz)",
		.input = {
			.initial = "250",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[ACQ_SRC]  = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Sources",
		.menu  = {
			.entries = bl_menu_conf_acq_sources_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_acq_sources_entries),
		},
	},
};

/**
 * Configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_conf_entries[] = {
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "Acquisition",
		.menu  = {
			.entries = bl_menu_conf_acq_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_acq_entries),
		},
	},
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "Channels",
		.menu  = {
			.entries = bl_menu_conf_chan_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_chan_entries),
		},
	},
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "LEDs",
		.menu  = {
			.entries = bl_menu_conf_led_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_led_entries),
		},
	},
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "Filtering",
		.menu  = {
			.entries = bl_menu_conf_filter_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_filter_entries),
		},
	},
};

enum {
	MAIN_CAL,
	MAIN_ACQ,
	MAIN_STOP,
	MAIN_CONFIG,
	MAIN_QUIT,
};

/**
 * Main menu listing.
 */
static const struct main_menu_widget_desc bl_menu_main_entries[] = {
	[MAIN_CAL]    = {
		.type   = WIDGET_TYPE_ACTION,
		.title  = "Calibrate",
		.action = {
			.cb = bloodview_start_cal_cb,
		},
	},
	[MAIN_ACQ]    = {
		.type   = WIDGET_TYPE_ACTION,
		.title  = "Acquisition",
		.action = {
			.cb = bloodview_start_acq_cb,
		},
	},
	[MAIN_STOP]   = {
		.type   = WIDGET_TYPE_ACTION,
		.title  = "Stop",
		.action = {
			.cb = bloodview_stop_cb,
		},
	},
	[MAIN_CONFIG] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Config",
		.menu  = {
			.entries = bl_menu_conf_entries,
			.count   = BL_ARRAY_LEN(bl_menu_conf_entries),
		},
	},
	[MAIN_QUIT]   = {
		.type   = WIDGET_TYPE_ACTION,
		.title  = "Quit",
		.action = {
			.cb = bloodview_quit_cb,
		},
	},
};

/**
 * Main menu widget.
 */
static const struct main_menu_widget_desc bl_main_menu = {
	.type  = WIDGET_TYPE_MENU,
	.title = "Bloodlight Viewer",
	.menu  = {
		.entries = bl_menu_main_entries,
		.count   = BL_ARRAY_LEN(bl_menu_main_entries),
	},
};

/**
 * Get the config private word for a main menu entry.
 *
 * \param[in]  desc   Menu descriptor for entry's parent menu.
 * \param[in]  entry  The entry index to create a widget for.
 * \return config private word for the menu entry, or NULL if none.
 */
static void *get_config_pw_for_entry(
		const struct main_menu_widget_desc *desc,
		unsigned entry)
{
	static unsigned chan_entry;
	const struct main_menu_widget_desc *entries = desc->menu.entries;

	if (entries == bl_menu_conf_acq_entries) {
		switch (entry) {
		case ACQ_FREQ: return &config.acq.frequency;
		}

	} else if (entries == bl_menu_conf_acq_sources_entries) {
		return &config.acq.sources[entry];

	} else if (entries == bl_menu_conf_chan_entries) {
		chan_entry = entry;

	} else if (entries == bl_menu_conf_chan_c_entries) {
		switch (entry) {
		case CHAN_CHAN_SW_OFFSET: return &config.channel[chan_entry].channel.offset;
		case CHAN_CHAN_SW_SHIFT:  return &config.channel[chan_entry].channel.shift;
		case CHAN_CHAN_32BIT:     return &config.channel[chan_entry].channel.sample32;
		case CHAN_CHAN_INVERT:    return &config.channel[chan_entry].channel.inverted;
		}

	} else if (entries == bl_menu_conf_colour_entries) {
		switch (entry) {
		case COLOUR_HUE: return &config.channel[chan_entry].channel.colour.hue;
		case COLOUR_SAT: return &config.channel[chan_entry].channel.colour.saturation;
		case COLOUR_VAL: return &config.channel[chan_entry].channel.colour.value;
		}

	} else if (entries == bl_menu_conf_chan_s_entries) {
		switch (entry) {
		case CHAN_SRC_SW_OVERSAMPLE: return &config.channel[chan_entry].source.sw_oversample;
		case CHAN_SRC_OPAMP_GAIN:    return &config.channel[chan_entry].source.opamp_gain;
		case CHAN_SRC_OPAMP_OFFSET:  return &config.channel[chan_entry].source.opamp_offset;
		case CHAN_SRC_HW_OVERSAMPLE: return &config.channel[chan_entry].source.hw_oversample;
		case CHAN_SRC_HW_SHIFT:      return &config.channel[chan_entry].source.hw_shift;
		}

	} else if (entries == bl_menu_conf_led_entries) {
		return &config.led.leds[entry];
	} else if (entries == bl_menu_conf_filter_entries) {
		switch (entry) {
		case FILTER_VALUE_NORMALISE:   return &config.filter.normalise;
		case FILTER_VALUE_AC_DENOISE:  return &config.filter.ac_denoise;
		case FILTER_ENABLE_NORMALISE:  return &config.filter.normalise_enable;
		case FILTER_ENABLE_AC_DENOISE: return &config.filter.ac_denoise_enable;
		}
	}

	return NULL;
}

/**
 * Record any widgets we might need to update the values of.
 *
 * \param[in]  desc   Menu descriptor for entry's parent menu.
 * \param[in]  entry  The menu entry index widget is for.
 * \return config private word for the menu entry, or NULL if none.
 */
static struct sdl_tk_widget **main_menu_record_widget(
		const struct main_menu_widget_desc *desc,
		unsigned entry)
{
	static unsigned chan_entry;
	const struct main_menu_widget_desc *entries = desc->menu.entries;

	if (entries == bl_menu_conf_chan_entries) {
		chan_entry = entry;

	} else if (entries == bl_menu_conf_chan_c_entries) {
		switch (entry) {
		case CHAN_CHAN_SW_OFFSET:
			return &config.channel[chan_entry].channel.widget_offset;
		case CHAN_CHAN_SW_SHIFT:
			return &config.channel[chan_entry].channel.widget_shift;
		default:
			break;
		}
	} else if (entries == bl_menu_main_entries) {
		switch (entry) {
		case MAIN_CAL:  return &config.widget_cal;
		case MAIN_ACQ:  return &config.widget_acq;
		case MAIN_STOP: return &config.widget_stop;
		}

	} else if (entries == bl_menu_conf_filter_entries) {
		switch (entry) {
		case FILTER_VALUE_NORMALISE:  return &config.filter.widget_normalise;
		case FILTER_VALUE_AC_DENOISE: return &config.filter.widget_ac_denoise;
		}
	}

	return NULL;
}

/**
 * Main menu entry creation callback.
 *
 * \param[in]  parent  The widget currently being created.
 * \param[in]  entry   The entry index to create a widget for.
 * \param[in]  pw      Client private context data.
 * \return the created sdl-tk widget for the menu entry or NULL on error.
 */
static struct sdl_tk_widget *desc_menu_create_cb(
		struct sdl_tk_widget *parent,
		unsigned entry,
		void *pw)
{
	const struct main_menu_widget_desc *menu_desc = pw;
	const struct main_menu_widget_desc *desc;
	struct sdl_tk_widget **widget_record;
	struct sdl_tk_widget *widget = NULL;
	void *entry_pw;

	assert(menu_desc->type == WIDGET_TYPE_MENU);

	desc = &menu_desc->menu.entries[entry];

	widget_record = main_menu_record_widget(menu_desc, entry);
	entry_pw = get_config_pw_for_entry(menu_desc, entry);

	switch (desc->type) {
	case WIDGET_TYPE_MENU:
		widget = sdl_tk_widget_menu_create(
				parent,
				desc->title,
				desc->menu.count,
				desc_menu_create_cb,
				(void *) desc);
		break;

	case WIDGET_TYPE_INPUT:
		widget = sdl_tk_widget_input_create(
				parent,
				desc->title,
				desc->input.initial,
				desc->input.cb,
				entry_pw);
		break;

	case WIDGET_TYPE_ACTION:
		widget = sdl_tk_widget_action_create(
				parent,
				desc->title,
				desc->action.cb,
				desc->action.pw);
		break;

	case WIDGET_TYPE_TOGGLE:
		widget = sdl_tk_widget_toggle_create(
				parent,
				desc->title,
				desc->toggle.initial,
				main_menu_toggle_cb,
				entry_pw);
		break;

	default:
		fprintf(stderr, "Error: Unknown entry type (%i) "
				"in main menu construction.\n",
				(int) desc->type);
		break;
	}

	if (widget_record != NULL) {
		*widget_record = widget;
	}

	return widget;
}

/* Exported function, documented in main-menu.h */
struct sdl_tk_widget *main_menu_create(void)
{
	struct sdl_tk_widget *widget;
	assert(bl_main_menu.type == WIDGET_TYPE_MENU);

	widget = sdl_tk_widget_menu_create(
			NULL,
			bl_main_menu.title,
			bl_main_menu.menu.count,
			desc_menu_create_cb,
			(void *) &bl_main_menu);
	if (widget == NULL) {
		fprintf(stderr, "Error: Failed to create main menu.\n");
	}

	return widget;
}

/* Exported interface, documented in main-menu.h */
uint16_t main_menu_conifg_get_led_mask(void)
{
	const uint16_t led_count = BL_LED_COUNT;
	static const uint8_t mapping[BL_LED_COUNT] = {
		15, 14, 13, 12, 11, 10,  9,  8,
		 0,  1,  2,  3,  4,  5,  6,  7,
	};
	uint16_t led_mask = 0;

	for (unsigned i = 0; i < led_count; i++) {
		if (config.led.leds[i] != 0) {
			led_mask |= 1U << mapping[i];
		}
	}

	return led_mask;
}

/* Exported interface, documented in main-menu.h */
uint16_t main_menu_conifg_get_source_mask(void)
{
	const uint16_t src_count = BL_ARRAY_LEN(
			bl_menu_conf_acq_sources_entries);
	uint16_t src_mask = 0;

	for (unsigned i = 0; i < src_count; i++) {
		if (config.acq.sources[i] != 0) {
			src_mask |= 1U << i;
		}
	}

	return src_mask;
}

/* Exported interface, documented in main-menu.h */
uint16_t main_menu_conifg_get_frequency(void)
{
	return config.acq.frequency;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_source_sw_oversample(enum bl_acq_source source)
{
	return config.channel[source].source.sw_oversample;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_source_opamp_gain(enum bl_acq_source source)
{
	return config.channel[source].source.opamp_gain;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_source_opamp_offset(enum bl_acq_source source)
{
	return config.channel[source].source.opamp_offset;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_source_hw_oversample(enum bl_acq_source source)
{
	return config.channel[source].source.hw_oversample;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_source_hw_shift(enum bl_acq_source source)
{
	return config.channel[source].source.hw_shift;
}

/* Exported interface, documented in main-menu.h */
uint8_t main_menu_conifg_get_channel_shift(uint8_t channel)
{
	return config.channel[channel].channel.shift;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_channel_offset(uint8_t channel)
{
	return config.channel[channel].channel.offset;
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_get_channel_sample32(uint8_t channel)
{
	return config.channel[channel].channel.sample32;
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_get_channel_inverted(uint8_t channel)
{
	return config.channel[channel].channel.inverted;
}

/* Exported interface, documented in main-menu.h */
SDL_Color main_menu_conifg_get_channel_colour(uint8_t channel)
{
	return sdl_tk_colour_get_hsv(
			config.channel[channel].channel.colour.hue,
			config.channel[channel].channel.colour.saturation,
			config.channel[channel].channel.colour.value);
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_get_filter_normalise_enabled(void)
{
	return config.filter.normalise_enable;
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_get_filter_ac_denoise_enabled(void)
{
	return config.filter.ac_denoise_enable;
}

/* Exported interface, documented in main-menu.h */
double main_menu_conifg_get_filter_normalise_frequency(void)
{
	return config.filter.normalise;
}

/* Exported interface, documented in main-menu.h */
double main_menu_conifg_get_filter_ac_denoise_frequency(void)
{
	return config.filter.ac_denoise;
}

/**
 * Convert an unsigned value to a string.
 *
 * \param[in]  value  The value to convert to a string.
 * \return A string on success, or NULL on failure.
 */
static char *stringify_unsigned(unsigned value)
{
	static char buffer[32];
	int ret;

	ret = snprintf(buffer, sizeof(buffer), "%u", value);
	if (ret <= 0) {
		return NULL;

	} else if ((unsigned) ret >= sizeof(buffer)) {
		return NULL;
	}

	return strdup(buffer);
}

/** Type pf update to apply to a widget. */
enum update_type {
	UPDATE_TYPE_SET_VALUE, /**< Value setting update. */
	UPDATE_TYPE_ENABLE,    /**< Widget enable/disable update type. */
};

/** Details of an update to apply to a menu. */
struct update {
	enum update_type type;        /**< Type of main menu update. */
	struct sdl_tk_widget *widget; /**< Widget to update. */
	/** Update type-specific data. */
	union update_data {
		struct {
			char *value;  /**< Value to set. */
		} set_value;          /**< \ref UPDATE_TYPE_SET_VALUE data. */
		struct {
			bool enable;  /**< Whether to enable or disable. */
		} enable;             /**< \ref UPDATE_TYPE_ENABLE data. */
	} data;                       /**< Update type-specific data. */
};

/**
 * Update context.
 *
 * Contains a list of updates that need to be applied to the main menu
 * before it is next rendered.
 */
struct {
	struct update list[64]; /**< Array of pending main menu updates.*/
	locked_uint_t count;    /**< Number of pending main menu updates. */
} update_ctx;

/**
 * Put an update onto the update context list.
 *
 * \param[in]  type    The type of update to perform.
 * \param[in]  widget  The widget to update.
 * \param[in]  data    The type-specific data.
 * \return true on succes, or false on error.
 */
static bool main_menu__push_update(
		enum update_type type,
		struct sdl_tk_widget *widget,
		const union update_data *data)
{
	bool ret = true;

	locked_uint_claim(&update_ctx.count);
	if (update_ctx.count.value == BV_ARRAY_LEN(update_ctx.list)) {
		ret = false;
		goto cleanup;
	}

	update_ctx.list[update_ctx.count.value].type = type;
	update_ctx.list[update_ctx.count.value].widget = widget;
	update_ctx.list[update_ctx.count.value].data = *data;
	update_ctx.count.value++;

cleanup:
	locked_uint_release(&update_ctx.count);
	if (ret == false) {
		if (type == UPDATE_TYPE_SET_VALUE) {
			free(data->set_value.value);
		}
	}
	return ret;
}

/* Exported interface, documented in main-menu.h */
void main_menu_update(void)
{
	if (update_ctx.count.value == 0) {
		return;
	}

	locked_uint_claim(&update_ctx.count);
	for (unsigned i = 0; i < update_ctx.count.value; i++) {
		switch (update_ctx.list[i].type) {
		case UPDATE_TYPE_SET_VALUE:
			sdl_tk_widget_input_set_value(
					update_ctx.list[i].widget,
					update_ctx.list[i].data.set_value.value);
			free(update_ctx.list[i].data.set_value.value);
			break;
		case UPDATE_TYPE_ENABLE:
			sdl_tk_widget_enable(
					update_ctx.list[i].widget,
					update_ctx.list[i].data.enable.enable);
			break;
		}
	}

	update_ctx.count.value = 0;
	locked_uint_release(&update_ctx.count);
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_set_channel_shift(uint8_t channel, uint8_t shift)
{
	union update_data data;

	data.set_value.value = stringify_unsigned(shift);
	if (data.set_value.value == NULL) {
		return false;
	}

	return main_menu__push_update(
			UPDATE_TYPE_SET_VALUE,
			config.channel[channel].channel.widget_shift,
			&data);
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_set_channel_offset(uint8_t channel, uint32_t offset)
{
	union update_data data;

	data.set_value.value = stringify_unsigned(offset);
	if (data.set_value.value == NULL) {
		return false;
	}

	return main_menu__push_update(
			UPDATE_TYPE_SET_VALUE,
			config.channel[channel].channel.widget_offset,
			&data);
}

/* Exported interface, documented in main-menu.h */
void main_menu_set_acq_available(bool available)
{
	union update_data data = {
		.enable = {
			.enable = available,
		},
	};

	main_menu__push_update(UPDATE_TYPE_ENABLE, config.widget_cal, &data);
	main_menu__push_update(UPDATE_TYPE_ENABLE, config.widget_acq, &data);
}
