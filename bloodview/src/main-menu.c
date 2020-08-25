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
	enum widget_type type;
	const char *title;
	union {
		struct main_menu_widget_desc_menu {
			const struct main_menu_widget_desc *entries;
			unsigned count;
		} menu;
		struct main_menu_widget_desc_input {
			const sdl_tk_widget_input_cb cb;
			const char *initial;
		} input;
		struct main_menu_widget_desc_action {
			const sdl_tk_widget_action_cb cb;
			void *pw;
		} action;
		struct main_menu_widget_desc_toggle {
			bool initial;
		} toggle;
	};
};

/** Current configuration for LEDs. */
struct main_menu_config_leds {
	uint8_t leds[BL_LED_COUNT];
};

/** Current configuration for a channel. */
struct main_menu_config_channel {
	unsigned gain;
	unsigned offset;
	unsigned shift;
	bool     sample32;
	bool     inverted;

	struct sdl_tk_widget *widget_shift;
	struct sdl_tk_widget *widget_offset;
};

/** Current acquisition configuration. */
struct main_menu_config_acq_params {
	unsigned frequency;
	unsigned oversample;
	uint8_t  sources[BL_ACQ__SRC_COUNT];
};

/** Current filter configuration. */
struct main_menu_config_filter {
	uint8_t normalise_enable;
	double  normalise;

	uint8_t ac_denoise_enable;
	double  ac_denoise;

	struct sdl_tk_widget *widget_normalise;
	struct sdl_tk_widget *widget_ac_denoise;
};

/** Main menu configuration entries. */
struct main_menu_config {
	struct main_menu_config_acq_params acq;
	struct main_menu_config_channel channel[BL_ACQ__SRC_COUNT];
	struct main_menu_config_leds led;
	struct main_menu_config_filter filter;

	struct sdl_tk_widget *widget_cal;
	struct sdl_tk_widget *widget_acq;
	struct sdl_tk_widget *widget_stop;
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
static const struct main_menu_widget_desc bl_menu_config_filter_entries[] = {
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
static const struct main_menu_widget_desc bl_menu_config_led_entries[BL_LED_COUNT] = {
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

/** Channel config menu entries. */
enum {
	CHAN_GAIN,
	CHAN_OFFSET,
	CHAN_SHIFT,
	CHAN_32BIT,
	CHAN_INVERT,
};

/**
 * Channel configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_config_chan_c_entries[] = {
	[CHAN_GAIN] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Gain",
		.input = {
			.initial = "1",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_OFFSET] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Offset",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_SHIFT] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Shift",
		.input = {
			.initial = "0",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[CHAN_32BIT] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "32-bit samples",
	},
	[CHAN_INVERT] = {
		.type   = WIDGET_TYPE_TOGGLE,
		.title  = "Invert data",
	},
};

/**
 * Channel configuration list.
 */
static const struct main_menu_widget_desc bl_menu_config_chan_entries[] = {
	[BL_ACQ_PD1] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 1",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
	[BL_ACQ_PD2] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 2",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
	[BL_ACQ_PD3] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 3",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
	[BL_ACQ_PD4] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Photodiode 4",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
	[BL_ACQ_3V3] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "3.3 Volts",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
	[BL_ACQ_5V0] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "5.0 Volts",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
	[BL_ACQ_TMP] = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Temperature",
		.menu  = {
			.entries = bl_menu_config_chan_c_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_c_entries),
		},
	},
};

/**
 * Channel selection menu.
 */
static const struct main_menu_widget_desc bl_menu_config_acq_sources_entries[] = {
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
	ACQ_OVER,
	ACQ_SRC,
};

/**
 * Acquisition configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_config_acq_entries[] = {
	[ACQ_FREQ] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Frequency (Hz)",
		.input = {
			.initial = "250",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[ACQ_OVER] = {
		.type  = WIDGET_TYPE_INPUT,
		.title = "Oversample",
		.input = {
			.initial = "512",
			.cb = main_menu_numerical_input_cb,
		},
	},
	[ACQ_SRC]  = {
		.type  = WIDGET_TYPE_MENU,
		.title = "Sources",
		.menu  = {
			.entries = bl_menu_config_acq_sources_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_acq_sources_entries),
		},
	},
};

/**
 * Configuration menu.
 */
static const struct main_menu_widget_desc bl_menu_config_entries[] = {
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "Acquisition",
		.menu  = {
			.entries = bl_menu_config_acq_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_acq_entries),
		},
	},
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "Channels",
		.menu  = {
			.entries = bl_menu_config_chan_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_chan_entries),
		},
	},
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "LEDs",
		.menu  = {
			.entries = bl_menu_config_led_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_led_entries),
		},
	},
	{
		.type  = WIDGET_TYPE_MENU,
		.title = "Filtering",
		.menu  = {
			.entries = bl_menu_config_filter_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_filter_entries),
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
			.entries = bl_menu_config_entries,
			.count   = BL_ARRAY_LEN(bl_menu_config_entries),
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

	if (entries == bl_menu_config_acq_entries) {
		switch (entry) {
		case ACQ_FREQ: return &config.acq.frequency;
		case ACQ_OVER: return &config.acq.oversample;
		}

	} else if (entries == bl_menu_config_acq_sources_entries) {
		return &config.acq.sources[entry];

	} else if (entries == bl_menu_config_chan_entries) {
		chan_entry = entry;

	} else if (entries == bl_menu_config_chan_c_entries) {
		switch (entry) {
		case CHAN_GAIN:   return &config.channel[chan_entry].gain;
		case CHAN_OFFSET: return &config.channel[chan_entry].offset;
		case CHAN_SHIFT:  return &config.channel[chan_entry].shift;
		case CHAN_32BIT:  return &config.channel[chan_entry].sample32;
		case CHAN_INVERT: return &config.channel[chan_entry].inverted;
		}

	} else if (entries == bl_menu_config_led_entries) {
		return &config.led.leds[entry];
	} else if (entries == bl_menu_config_filter_entries) {
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

	if (entries == bl_menu_config_chan_entries) {
		chan_entry = entry;

	} else if (entries == bl_menu_config_chan_c_entries) {
		switch (entry) {
		case CHAN_OFFSET:
			return &config.channel[chan_entry].widget_offset;
		case CHAN_SHIFT:
			return &config.channel[chan_entry].widget_shift;
		default:
			break;
		}
	} else if (entries == bl_menu_main_entries) {
		switch (entry) {
		case MAIN_CAL:  return &config.widget_cal;
		case MAIN_ACQ:  return &config.widget_acq;
		case MAIN_STOP: return &config.widget_stop;
		}

	} else if (entries == bl_menu_config_filter_entries) {
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
			bl_menu_config_acq_sources_entries);
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
uint32_t main_menu_conifg_get_oversample(void)
{
	return config.acq.oversample;
}

/* Exported interface, documented in main-menu.h */
uint8_t main_menu_conifg_get_channel_gain(uint8_t channel)
{
	return config.channel[channel].gain;
}

/* Exported interface, documented in main-menu.h */
uint8_t main_menu_conifg_get_channel_shift(uint8_t channel)
{
	return config.channel[channel].shift;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_conifg_get_channel_offset(uint8_t channel)
{
	return config.channel[channel].offset;
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_get_channel_sample32(uint8_t channel)
{
	return config.channel[channel].sample32;
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_get_channel_inverted(uint8_t channel)
{
	return config.channel[channel].inverted;
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

static const char *stringify_unsigned(unsigned value)
{
	static char buffer[32];
	int ret;

	ret = snprintf(buffer, sizeof(buffer), "%u", value);
	if (ret <= 0) {
		return NULL;

	} else if ((unsigned) ret >= sizeof(buffer)) {
		return NULL;
	}

	return buffer;
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_set_channel_shift(uint8_t channel, uint8_t shift)
{
	const char *shift_string = stringify_unsigned(shift);

	if (shift_string == NULL) {
		return false;
	}

	return sdl_tk_widget_input_set_value(
			config.channel[channel].widget_shift,
			shift_string);
}

/* Exported interface, documented in main-menu.h */
bool main_menu_conifg_set_channel_offset(uint8_t channel, uint32_t offset)
{
	const char *offset_string = stringify_unsigned(offset);

	if (offset_string == NULL) {
		return false;
	}

	return sdl_tk_widget_input_set_value(
			config.channel[channel].widget_offset,
			offset_string);
}

/* Exported interface, documented in main-menu.h */
void main_menu_set_acq_available(bool available)
{
	sdl_tk_widget_enable(config.widget_cal, available);
	sdl_tk_widget_enable(config.widget_acq, available);
}
