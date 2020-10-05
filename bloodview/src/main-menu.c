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

#include <cyaml/cyaml.h>

#include "../../src/acq.h"
#include "../../src/util.h"

#include "sdl-tk/widget.h"

#include "sdl-tk/widget/menu.h"
#include "sdl-tk/widget/input.h"
#include "sdl-tk/widget/action.h"
#include "sdl-tk/widget/select.h"
#include "sdl-tk/widget/toggle.h"

#include "util.h"
#include "locked.h"
#include "bloodview.h"
#include "main-menu.h"

/** Directory to load/save config files to/from. */
static const char *bv_config_dir_path;

/** Bloodview actions. */
enum bv_action {
	BV_ACTION_CAL,
	BV_ACTION_ACQ,
	BV_ACTION_STOP,
	BV_ACTION_QUIT,
};

/** Toggle widget data. */
struct desc_widget_toggle {
	char *title; /**< Widget title. */
	bool value;  /**< Widget value. */
};

/** Action widget data. */
struct desc_widget_action {
	char *title;       /**< Widget title. */
	enum bv_action cb; /**< Widget action callback. */
};

/** Input widget value data. */
struct desc_widget_value {
	enum {
		INPUT_TYPE_DOUBLE,
		INPUT_TYPE_UNSIGNED,
		INPUT_TYPE_ACQ_MODE,
	} type; /**< The input value type. */
	union {
		double           type_double;   /**< Data for double values */
		unsigned         type_unsigned; /**< Data for unsigned values */
		enum bl_acq_mode type_acq_mode; /**< Data for acquisition modes */
	};
};

/** Input widget data. */
struct desc_widget_input {
	char *title;                    /**< Widget title. */
	struct desc_widget_value value; /**< Widget value. */
};

/** Select widget data. */
struct desc_widget_select {
	char *title;                    /**< Widget title. */
	struct desc_widget_value value; /**< Widget value. */
};

struct desc_widget;

/** Menu widget data. */
struct desc_widget_menu {
	char *title;                /**< Widget title. */
	struct desc_widget **entry; /**< Menu entries array. */
	unsigned entry_count;       /**< Number of menu entries. */
};

/** Main menu widget data. */
struct desc_widget {
	enum {
		WIDGET_TYPE_MENU,
		WIDGET_TYPE_INPUT,
		WIDGET_TYPE_ACTION,
		WIDGET_TYPE_SELECT,
		WIDGET_TYPE_TOGGLE,
	} type; /**< Widget type. */
	union {
		struct desc_widget_menu menu;     /**< Menu widget data. */
		struct desc_widget_input input;   /**< Input widget data. */
		struct desc_widget_action action; /**< Action widget data. */
		struct desc_widget_select select; /**< Select widget data. */
		struct desc_widget_toggle toggle; /**< Toggle widget data. */
	};
	struct sdl_tk_widget *widget; /**< SDL-TK widget. */
};

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
static struct {
	struct update list[64]; /**< Array of pending main menu updates.*/
	locked_uint_t count;    /**< Number of pending main menu updates. */
} update_ctx;

/**
 * Get the title from a widget descriptor.
 *
 * \param[in]  desc  A widget descriptor.
 * \return the widget title, or NULL.
 */
const char *main_menu__desc_get_title(
		struct desc_widget *desc)
{
	const char *title = NULL;

	switch (desc->type) {
	case WIDGET_TYPE_MENU:
		title = desc->menu.title;
		break;
	case WIDGET_TYPE_INPUT:
		title = desc->input.title;
		break;
	case WIDGET_TYPE_ACTION:
		title = desc->action.title;
		break;
	case WIDGET_TYPE_SELECT:
		title = desc->select.title;
		break;
	case WIDGET_TYPE_TOGGLE:
		title = desc->toggle.title;
		break;
	}

	return title;
}

/**
 * Get a child widget descriptor for first section of a given widget path.
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  path      Path to look for first section in.
 * \param[out] path_out  Updated to point into path, after returned descriptor.
 * \return The appropriate child widget descriptor for the first path section,
 *         or NULL if first section not found in base.
 */
static struct desc_widget *main_menu__get_desc_child(
		const struct desc_widget *base,
		const char *path,
		const char **path_out)
{
	const char *pos;

	assert(base->type == WIDGET_TYPE_MENU);

	while (*path == '/' || *path == '[') {
		path++;
	}

	pos = path;
	while (*pos != '/' && *pos != ']' && *pos != '\0') {
		pos++;
	}

	if (*pos == '/' || *pos == '\0') {
		size_t len = pos - path;
		for (unsigned i = 0; i < base->menu.entry_count; i++) {
			const char *title = main_menu__desc_get_title(
					base->menu.entry[i]);

			if (strlen(title) == len &&
			    strncmp(title, path, len) == 0) {
				*path_out = pos;
				return base->menu.entry[i];
			}
		}
		fprintf(stderr, "Config path component not found: %*s\n",
				(int)(pos - path), pos);
		return NULL;

	} else if (*pos == ']') {
		unsigned i;

		if (!util_read_unsigned(path, &i)) {
			fprintf(stderr, "Failed to read config index: %*s\n",
					(int)(pos - path), pos);
			return NULL;
		}

		if (i > base->menu.entry_count) {
			fprintf(stderr, "Config index too high: %u\n", i);
			return NULL;
		}

		*path_out = pos + 1;
		return base->menu.entry[i];
	}

	return NULL;
}

/**
 * Get widget descriptor corresponding to given path.
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  path      Path to get widget descriptor for.
 * \return The widget descriptor for the path, or NULL if not found.
 */
static struct desc_widget *main_menu__get_desc(
		struct desc_widget *base,
		const char *path)
{
	struct desc_widget *desc = base;

	while (desc != NULL && *path != '\0') {
		desc = main_menu__get_desc_child(desc, path, &path);
	}

	return desc;
}

/**
 * Get widget descriptor corresponding to path given by format string (va_list).
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  fmt       Format string for path to get widget descriptor for.
 * \param[in]  ap        Variadic arguments appropriate for `fmt`.
 * \return The widget descriptor for the path, or NULL if not found.
 */
static struct desc_widget *main_menu__get_desc_va_list(
		struct desc_widget *base,
		const char *fmt,
		va_list ap)
{
	static char path[128];
	int n = vsnprintf(path, sizeof(path), fmt, ap);

	if (n < 0 || (unsigned) n >= sizeof(path)) {
		return NULL;
	}

	return main_menu__get_desc(base, path);
}

/**
 * Get widget descriptor corresponding to path given by format string.
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  fmt       Format string for path to get widget descriptor for.
 * \param[in]  ...       Variadic arguments appropriate for `fmt`.
 * \return The widget descriptor for the path, or NULL if not found.
 */
static struct desc_widget *main_menu__get_desc_fmt(
		struct desc_widget *base,
		const char *fmt,
		...)
{
	struct desc_widget *desc;
	va_list ap;

	va_start(ap, fmt);
	desc = main_menu__get_desc_va_list(base, fmt, ap);
	va_end(ap);

	return desc;
}

/**
 * Get double value for input widget corresponding to path.
 *
 * Must be called with an appropriate path for the widget type.
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  fmt       Format string for path to get widget descriptor for.
 * \param[in]  ...       Variadic arguments appropriate for `fmt`.
 * \return The double value for input widget at path.
 */
static double main_menu__get_desc_input_double(
		struct desc_widget *base,
		const char *fmt,
		...)
{
	struct desc_widget *desc;
	va_list ap;

	va_start(ap, fmt);
	desc = main_menu__get_desc_va_list(base, fmt, ap);
	va_end(ap);

	assert(desc != NULL);
	assert(desc->type == WIDGET_TYPE_INPUT);
	assert(desc->input.value.type == INPUT_TYPE_DOUBLE);

	return desc->input.value.type_double;
}

/**
 * Get unsigned value for input widget corresponding to path.
 *
 * Must be called with an appropriate path for the widget type.
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  fmt       Format string for path to get widget descriptor for.
 * \param[in]  ...       Variadic arguments appropriate for `fmt`.
 * \return The unsigned value for input widget at path.
 */
static unsigned main_menu__get_desc_input_unsigned(
		struct desc_widget *base,
		const char *fmt,
		...)
{
	struct desc_widget *desc;
	va_list ap;

	va_start(ap, fmt);
	desc = main_menu__get_desc_va_list(base, fmt, ap);
	va_end(ap);

	assert(desc != NULL);
	assert(desc->type == WIDGET_TYPE_INPUT);
	assert(desc->input.value.type == INPUT_TYPE_UNSIGNED);

	return desc->input.value.type_unsigned;
}

/**
 * Get current state of toggle widget corresponding to path.
 *
 * Must be called with an appropriate path for the widget type.
 *
 * \param[in]  base      Root widget descriptor for path.
 * \param[in]  fmt       Format string for path to get widget descriptor for.
 * \param[in]  ...       Variadic arguments appropriate for `fmt`.
 * \return The current toggle widget state.
 */
static bool main_menu__get_desc_toggle_value(
		struct desc_widget *base,
		const char *fmt,
		...)
{
	struct desc_widget *desc;
	va_list ap;

	va_start(ap, fmt);
	desc = main_menu__get_desc_va_list(base, fmt, ap);
	va_end(ap);

	assert(desc != NULL);
	assert(desc->type == WIDGET_TYPE_TOGGLE);

	return desc->toggle.value;
}

/** Bloodlight main menu descriptor root. */
static struct desc_widget *bl_main_menu;

/** CYAML schema: Valid input widget type mapping. */
static const cyaml_strval_t widget_input_type[] = {
	{ .val = INPUT_TYPE_DOUBLE,   .str = "double" },
	{ .val = INPUT_TYPE_UNSIGNED, .str = "unsigned" },
	{ .val = INPUT_TYPE_ACQ_MODE, .str = "acq-mode" },
};

/** CYAML schema: Valid widget type mapping. */
static const cyaml_strval_t widget_type[] = {
	{ .val = WIDGET_TYPE_MENU,   .str = "menu" },
	{ .val = WIDGET_TYPE_INPUT,  .str = "input" },
	{ .val = WIDGET_TYPE_ACTION, .str = "action" },
	{ .val = WIDGET_TYPE_SELECT, .str = "select" },
	{ .val = WIDGET_TYPE_TOGGLE, .str = "toggle" },
};

/** CYAML schema: Valid action type mapping. */
static const cyaml_strval_t bv_action_type[] = {
	{ .val = BV_ACTION_CAL,  .str = "bv_action_cal" },
	{ .val = BV_ACTION_ACQ,  .str = "bv_action_acq" },
	{ .val = BV_ACTION_STOP, .str = "bv_action_stop" },
	{ .val = BV_ACTION_QUIT, .str = "bv_action_quit" },
};

/** CYAML schema: Valid acquisition mode mapping. */
static const cyaml_strval_t bv_acq_mode[] = {
	{ .val = BL_ACQ_MODE_CONTINUOUS, .str = "Continuous" },
	{ .val = BL_ACQ_MODE_FLASH,      .str = "Flash" },
};

static const cyaml_schema_value_t schema_main_menu;

/** CYAML schema: Menu widget mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_menu_mapping[] = {
	CYAML_FIELD_STRING_PTR("title", CYAML_FLAG_POINTER,
			struct desc_widget_menu, title,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_SEQUENCE("entries", CYAML_FLAG_POINTER,
			struct desc_widget_menu, entry,
			&schema_main_menu,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/** CYAML schema: Input widget value mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_input_value_mapping[] = {
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_OPTIONAL,
			struct desc_widget_value, type,
			widget_input_type, CYAML_ARRAY_LEN(widget_input_type)),
	CYAML_FIELD_FLOAT("double", CYAML_FLAG_DEFAULT,
			struct desc_widget_value, type_double),
	CYAML_FIELD_UINT("unsigned", CYAML_FLAG_DEFAULT,
			struct desc_widget_value, type_unsigned),
	CYAML_FIELD_END
};

/** CYAML schema: Input widget mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_input_mapping[] = {
	CYAML_FIELD_STRING_PTR("title", CYAML_FLAG_POINTER,
			struct desc_widget_action, title,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_UNION("value", CYAML_FLAG_DEFAULT,
			struct desc_widget_input, value,
			schema_main_menu_widget_input_value_mapping,
			"kind"),
	CYAML_FIELD_END
};

/** CYAML schema: Action widget mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_action_mapping[] = {
	CYAML_FIELD_STRING_PTR("title", CYAML_FLAG_POINTER,
			struct desc_widget_action, title,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_ENUM("cb", CYAML_FLAG_DEFAULT,
			struct desc_widget_action, cb,
			bv_action_type, CYAML_ARRAY_LEN(bv_action_type)),
	CYAML_FIELD_END
};

/** CYAML schema: Input widget value mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_select_value_mapping[] = {
	CYAML_FIELD_ENUM("kind", CYAML_FLAG_OPTIONAL,
			struct desc_widget_value, type,
			widget_input_type, CYAML_ARRAY_LEN(widget_input_type)),
	CYAML_FIELD_ENUM("acq-mode", CYAML_FLAG_DEFAULT,
			struct desc_widget_value, type_acq_mode,
			bv_acq_mode, CYAML_ARRAY_LEN(bv_acq_mode)),
	CYAML_FIELD_END
};

/** CYAML schema: Select widget mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_select_mapping[] = {
	CYAML_FIELD_STRING_PTR("title", CYAML_FLAG_POINTER,
			struct desc_widget_action, title,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_UNION("value", CYAML_FLAG_DEFAULT,
			struct desc_widget_input, value,
			schema_main_menu_widget_select_value_mapping,
			"kind"),
	CYAML_FIELD_END
};

/** CYAML schema: Toggle widget mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_toggle_mapping[] = {
	CYAML_FIELD_STRING_PTR("title", CYAML_FLAG_POINTER,
			struct desc_widget_toggle, title,
			0, CYAML_UNLIMITED),
	CYAML_FIELD_BOOL("value", CYAML_FLAG_OPTIONAL,
			struct desc_widget_toggle, value),
	CYAML_FIELD_END
};

/** CYAML schema: Widget mapping fields. */
static const cyaml_schema_field_t schema_main_menu_widget_mapping[] = {
	CYAML_FIELD_IGNORE("sub-menus", CYAML_FLAG_OPTIONAL),
	CYAML_FIELD_ENUM("type", CYAML_FLAG_OPTIONAL,
			struct desc_widget, type,
			widget_type, CYAML_ARRAY_LEN(widget_type)),
	CYAML_FIELD_MAPPING("menu", CYAML_FLAG_DEFAULT,
			struct desc_widget, menu,
			schema_main_menu_widget_menu_mapping),
	CYAML_FIELD_MAPPING("input", CYAML_FLAG_FLOW,
			struct desc_widget, input,
			schema_main_menu_widget_input_mapping),
	CYAML_FIELD_MAPPING("action", CYAML_FLAG_DEFAULT,
			struct desc_widget, action,
			schema_main_menu_widget_action_mapping),
	CYAML_FIELD_MAPPING("select", CYAML_FLAG_DEFAULT,
			struct desc_widget, select,
			schema_main_menu_widget_select_mapping),
	CYAML_FIELD_MAPPING("toggle", CYAML_FLAG_FLOW,
			struct desc_widget, toggle,
			schema_main_menu_widget_toggle_mapping),
	CYAML_FIELD_END
};

/** CYAML schema: Top level. */
static const cyaml_schema_value_t schema_main_menu = {
	CYAML_VALUE_UNION(CYAML_FLAG_POINTER, struct desc_widget,
			schema_main_menu_widget_mapping, "type"),
};

/** CYAML config data. */
static const cyaml_config_t config = {
	.log_level = CYAML_LOG_WARNING, /* Logging errors and warnings only. */
	.log_fn    = cyaml_log,         /* Use the default logging function. */
	.mem_fn    = cyaml_mem,         /* Use the default memory allocator. */
};

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
	struct desc_widget *desc_normalisation;
	struct desc_widget *desc_ac_denoise;
	struct desc_widget *value = pw;

	assert(value->type == WIDGET_TYPE_TOGGLE);

	/* TODO: We could store the path to anything that gets enabled and
	 *       and disabled in the YAML menu definition for the toggle,
	 *       and avoid encoding this behavior here. */
	desc_normalisation = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Filtering/Normalisation");

	desc_ac_denoise = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Filtering/AC denoise");

	if (value == desc_normalisation) {
		struct desc_widget *desc_val =
			main_menu__get_desc_fmt(bl_main_menu,
			"Config/Filtering/Normalisation frequency (Hz)");

		assert(desc_val != NULL);

		sdl_tk_widget_enable(desc_val->widget, new_value);

	} else if (value == desc_ac_denoise) {
		struct desc_widget *desc_val =
			main_menu__get_desc_fmt(bl_main_menu,
			"Config/Filtering/AC denoise frequency (Hz)");

		assert(desc_val != NULL);

		sdl_tk_widget_enable(desc_val->widget, new_value);
	}

	value->toggle.value = new_value;
}

/**
 * Callback for the select widgets.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New select widget value.
 */
static void main_menu_select_cb(
		void     *pw,
		unsigned  new_value)
{
	struct desc_widget *value = pw;

	assert(value->type == WIDGET_TYPE_SELECT);

	switch (value->select.value.type) {
	case INPUT_TYPE_ACQ_MODE:
		value->select.value.type_acq_mode = new_value;
		break;
	default:
		break;
	}
}

/**
 * Callback for input widget change notification and validation.
 *
 * \param[in]  widget_value  Value to update.
 * \param[in]  new_value     New input widget value.
 * \return true to accept the new value. false to reject it.
 */
bool main_menu_unsigned_input_cb(
		unsigned   *widget_value,
		const char *new_value)
{
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
 * \param[in]  widget_value  Value to update.
 * \param[in]  new_value     New input widget value.
 * \return true to accept the new value. false to reject it.
 */
bool main_menu_double_input_cb(
		double     *widget_value,
		const char *new_value)
{
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

/**
 * Callback for input widget change notification and validation.
 *
 * \param[in]  pw         Client private context data.
 * \param[in]  new_value  New input widget value.
 * \return true to accept the new value. false to reject it.
 */
static bool main_menu__input_cb(
		void       *pw,
		const char *new_value)
{
	bool ret = false;
	struct desc_widget_value *val = pw;

	switch (val->type) {
	case INPUT_TYPE_DOUBLE:
		ret = main_menu_double_input_cb(
				&val->type_double, new_value);
		break;

	case INPUT_TYPE_UNSIGNED:
		ret = main_menu_unsigned_input_cb(
				&val->type_unsigned, new_value);
		break;

	default:
		break;
	}

	return ret;
}

/** Action widget callback functions. */
static const sdl_tk_widget_action_cb action_cb[] = {
	[BV_ACTION_CAL]  = bloodview_start_cal_cb,
	[BV_ACTION_ACQ]  = bloodview_start_acq_cb,
	[BV_ACTION_STOP] = bloodview_stop_cb,
	[BV_ACTION_QUIT] = bloodview_quit_cb,
};

/**
 * Get an input value as a string.
 *
 * \param[in]  val  Input value to convert to string.
 * \return Borrow of converted string, that is invalidated by the next call,
 *         or NULL on error.
 */
static const char *main_menu__get_input_value_string(
		const struct desc_widget_value *val)
{
	static char scratch[64];
	int ret = 0;

	assert(val != NULL);

	switch (val->type) {
	case INPUT_TYPE_DOUBLE:
		ret = snprintf(scratch, sizeof(scratch), "%f",
				val->type_double);
		break;
	case INPUT_TYPE_UNSIGNED:
		ret = snprintf(scratch, sizeof(scratch), "%u",
				val->type_unsigned);
		break;

	default:
		break;
	}

	if (ret < 0) {
		return NULL;
	} else if ((unsigned) ret >= sizeof(scratch)) {
		return NULL;
	}

	return scratch;
}

/**
 * Get a select widget's value.
 *
 * \param[in]  val  Value to get.
 * \return select widget value.
 */
static unsigned main_menu__select_value(
		const struct desc_widget_value *val)
{
	unsigned value = 0;

	switch (val->type) {
	case INPUT_TYPE_ACQ_MODE:
		value = val->type_acq_mode;
		break;

	default:
		break;
	}

	return value;
}

/**
 * Free a string vector.
 *
 * \param[in]  strings  String array to free.
 * \param[in]  count    Number of entries in `strings`.
 */
static void main_menu__free_string_vector(
		char **strings,
		unsigned count)
{
	if (strings != NULL) {
		for (unsigned i = 0; i < count; i++) {
			free(strings[i]);
		}
		free(strings);
	}
}

/**
 * Get select options.
 *
 * \param[in]  val            Value to get select options for.
 * \param[out] options        Returns select widget options on success.
 * \param[out] options_count  Returns number of options entries on success.
 * \return true on success, false on error.
 */
static bool main_menu__get_select_options(
		struct desc_widget_value *val,
		char ***options,
		unsigned *options_count)
{
	unsigned count = 0;
	char **strings = NULL;
	const cyaml_strval_t *strval = NULL;

	switch (val->type) {
	case INPUT_TYPE_ACQ_MODE:
		strval = bv_acq_mode;
		count = CYAML_ARRAY_LEN(bv_acq_mode);
		break;

	default:
		break;
	}

	if (strval == NULL) {
		goto error;
	}

	strings = calloc(count, sizeof(*strings));
	if (strings == NULL) {
		goto error;
	}

	for (unsigned i = 0; i < count; i++) {
		strings[i] = strdup(strval[i].str);
		if (strings[i] == NULL) {
			goto error;
		}
	}

	*options = strings;
	*options_count = count;
	return true;

error:
	main_menu__free_string_vector(strings, count);
	return false;
}

/**
 * Main menu entry creator.
 *
 * \param[in]  desc    The menu entry descriptor to create.
 * \param[in]  parent  The widget currently being created.
 * \return the created sdl-tk widget for the menu entry or NULL on error.
 */
static struct sdl_tk_widget *main_menu__create_from_desc(
		struct desc_widget *desc,
		struct sdl_tk_widget *parent)
{
	struct sdl_tk_widget *widget = NULL;

	switch (desc->type) {
	case WIDGET_TYPE_MENU:
		widget = sdl_tk_widget_menu_create(parent,
				desc->menu.title);
		if (widget == NULL) {
			return NULL;
		}
		desc->widget = widget;

		for (unsigned i = 0; i < desc->menu.entry_count; i++) {
			struct sdl_tk_widget *entry;

			entry = main_menu__create_from_desc(
					desc->menu.entry[i],
					widget);
			if (entry == NULL) {
				return NULL;
			}

			if (!sdl_tk_widget_menu_add_entry(widget,
					entry, SDL_TK_WIDGET_POS_END)) {
				return NULL;
			}
		}
		break;

	case WIDGET_TYPE_INPUT: {
		const char *initial = main_menu__get_input_value_string(
				&desc->input.value);
		if (initial == NULL) {
			return NULL;
		}
		widget = sdl_tk_widget_input_create(
				parent,
				desc->input.title,
				initial,
				main_menu__input_cb,
				&desc->input.value);
		if (widget == NULL) {
			return NULL;
		}
		desc->widget = widget;
		break;
	}

	case WIDGET_TYPE_ACTION:
		widget = sdl_tk_widget_action_create(
				parent,
				desc->action.title,
				action_cb[desc->action.cb],
				NULL);
		if (widget == NULL) {
			return NULL;
		}
		desc->widget = widget;
		break;

	case WIDGET_TYPE_SELECT: {
		char **options;
		unsigned options_count;
		if (!main_menu__get_select_options(&desc->select.value,
				&options, &options_count)) {
			fprintf(stderr, "Failed to get select options\n");
			return NULL;
		}
		widget = sdl_tk_widget_select_create(
				parent,
				desc->select.title,
				(const char * const *) options,
				options_count,
				main_menu__select_value(&desc->select.value),
				main_menu_select_cb,
				desc);
		main_menu__free_string_vector(options, options_count);
		if (widget == NULL) {
			fprintf(stderr, "Failed to create select widget\n");
			return NULL;
		}
		desc->widget = widget;
		break;
	}

	case WIDGET_TYPE_TOGGLE:
		widget = sdl_tk_widget_toggle_create(
				parent,
				desc->toggle.title,
				desc->toggle.value,
				main_menu_toggle_cb,
				desc);
		if (widget == NULL) {
			return NULL;
		}
		desc->widget = widget;
		break;

	default:
		fprintf(stderr, "Error: Unknown entry type (%i) "
				"in main menu construction.\n",
				(int) desc->type);
		break;
	}

	return widget;
}

/**
 * Turn filename and directory path in into full path.
 *
 * \param[in] dir_path  The directory to add filename to, or NULL.
 * \param[in] filename  The filename to add to dir_path.
 * \return Combined path, or NULL on error.
 */
static char *main_menu__create_path(
		const char *dir_path,
		const char *filename)
{
	int n = 0;
	size_t size = 0;
	char *str = NULL;

	assert(filename != NULL);

	if (dir_path == NULL) {
		return strdup(filename);
	}

	/* Determine required size */
	n = snprintf(NULL, 0, "%s/%s", dir_path, filename);
	if (n < 0) {
		return NULL;
	}

	size = (size_t) n + 1;

	str = malloc(size);
	if (str == NULL) {
		return NULL;
	}

	n = snprintf(str, size, "%s/%s", dir_path, filename);
	if (n < 0) {
		free(str);
		return NULL;
	}

	return str;
}

/**
 * Save the current config to given filename.
 *
 * \param[in] filename  The filename to save the current config as.
 * \return true on success, or false on failure.
 */
static bool main_menu_save_config(
		const char *filename)
{
	cyaml_err_t err;
	char *path;
	bool ret = false;
	const struct desc_widget *desc;

	path = main_menu__create_path(bv_config_dir_path, filename);
	if (path == NULL) {
		goto error;
	}

	desc = main_menu__get_desc_fmt(bl_main_menu, "Config");
	if (desc == NULL) {
		goto error;
	}

	err = cyaml_save_file(path, &config, &schema_main_menu, desc, 0);
	if (err != CYAML_OK) {
		fprintf(stderr, "ERROR: %s\n", cyaml_strerror(err));
		goto error;
	}

	fprintf(stderr, "Saved config to '%s'.\n", path);
	ret = true;

error:
	free(path);
	return ret;
}

/**
 * Recursively copy loaded config data into current state.
 *
 * \param[in]  current  Current state to update.
 * \param[in]  load     Loaded content to set as current state.
 * \return true on success, or false on error.
 */
static bool main_menu__load_config_helper(
		struct desc_widget *current,
		struct desc_widget *load)
{
	const char *value;

	if (current == NULL || load == NULL) {
		return false;
	}

	if (strcmp(main_menu__desc_get_title(load),
	           main_menu__desc_get_title(current)) != 0) {
		fprintf(stderr, "Error: Config load: Unexpected entry: %s, "
				"expecting: %s\n",
				main_menu__desc_get_title(load),
				main_menu__desc_get_title(current));
		return false;
	}

	if (current->type != load->type) {
		fprintf(stderr, "Error: Config load: Type mismatch for %s\n",
				main_menu__desc_get_title(current));
		return false;
	}

	switch (load->type) {
	case WIDGET_TYPE_MENU:
		if (load->menu.entry_count > current->menu.entry_count) {
			fprintf(stderr, "Error: Config load: "
					"Too many entries for %s\n",
					main_menu__desc_get_title(load));
			return false;
		}
		for (unsigned i = 0; i < load->menu.entry_count; i++) {
			if (!main_menu__load_config_helper(
					current->menu.entry[i],
					load->menu.entry[i])) {
				return false;
			}
		}
		break;

	case WIDGET_TYPE_INPUT:
		value = main_menu__get_input_value_string(&load->input.value);
		if (value == NULL) {
			fprintf(stderr, "Error: No input value for %s\n",
					main_menu__desc_get_title(load));
			return false;
		}
		if (!sdl_tk_widget_input_set_value(current->widget, value)) {
			fprintf(stderr, "Error: "
					"Failed to set input value %s for %s\n",
					value,
					main_menu__desc_get_title(current));
			return false;
		}
		break;

	case WIDGET_TYPE_SELECT:
		if (!sdl_tk_widget_select_set_value(current->widget,
				main_menu__select_value(&load->select.value))) {
			fprintf(stderr, "Error: "
					"Failed to set select value for %s\n",
					main_menu__desc_get_title(current));
			return false;
		}
		break;

	case WIDGET_TYPE_TOGGLE:
		if (!sdl_tk_widget_toggle_set_value(current->widget,
				load->toggle.value)) {
			fprintf(stderr, "Error: "
					"Failed to set toggle value for %s\n",
					main_menu__desc_get_title(current));
			return false;
		}
		break;

	default:
		fprintf(stderr, "Error: Config load: "
				"Unexpected widget type for '%s'\n",
				main_menu__desc_get_title(load));
		return false;
	}

	return true;
}

/**
 * Load a config file.
 *
 * \param[in]  config_file  The config file to load.
 * \return true on success, or false on error.
 */
static bool main_menu__load_config(
		const char *config_file)
{
	struct desc_widget *current = NULL;
	struct desc_widget *load = NULL;
	bool ret = false;
	cyaml_err_t err;
	char *path;

	path = main_menu__create_path(bv_config_dir_path, config_file);
	if (path == NULL) {
		goto cleanup;
	}

	err = cyaml_load_file(path, &config, &schema_main_menu,
			(void **) &load, 0);
	if (err != CYAML_OK) {
		fprintf(stderr, "ERROR: %s\n", cyaml_strerror(err));
		goto cleanup;
	}

	current = main_menu__get_desc_fmt(bl_main_menu, "Config");
	if (current == NULL) {
		goto cleanup;
	}

	if (!main_menu__load_config_helper(current, load)) {
		goto cleanup;
	}

	fprintf(stderr, "Loaded config file: %s\n", path);
	ret = true;

cleanup:
	free(path);
	if (load != NULL) {
		cyaml_free(&config, &schema_main_menu, load, 0);
	}
	return ret;
}

/* Exported function, documented in main-menu.h */
struct sdl_tk_widget *main_menu_create(
		const char *resources_dir_path,
		const char *config_dir_path,
		const char *config_file)
{
	cyaml_err_t err;
	char *path;

	bv_config_dir_path = config_dir_path;

	path = main_menu__create_path(resources_dir_path, "main-menu.yaml");
	if (path == NULL) {
		goto error;
	}

	err = cyaml_load_file(path, &config, &schema_main_menu,
			(void **) &bl_main_menu, NULL);
	free(path);
	if (err != CYAML_OK) {
		fprintf(stderr, "Failed to load main-menu.yaml: %s\n",
				cyaml_strerror(err));
		goto error;
	}

	if (bl_main_menu->type != WIDGET_TYPE_MENU) {
		fprintf(stderr, "Error: Main menu has bad type.\n");
		goto error;
	}

	if (!main_menu__create_from_desc(bl_main_menu, NULL)) {
		fprintf(stderr, "Error: Failed to create main menu.\n");
		goto error;
	}

	if (config_file != NULL) {
		if (!main_menu__load_config(config_file)) {
			fprintf(stderr, "Error: Failed to load config '%s'.\n",
					config_file);
			goto error;
		}
	}

	if (!locked_uint_init(&update_ctx.count)) {
		fprintf(stderr, "Error: Failed to initialise mutex.\n");
		goto error;
	}

	return bl_main_menu->widget;

error:
	if (bl_main_menu != NULL) {
		if (bl_main_menu->widget != NULL) {
			sdl_tk_widget_destroy(bl_main_menu->widget);
		}
		cyaml_free(&config, &schema_main_menu, bl_main_menu, 0);
		bl_main_menu = NULL;
	}
	return NULL;
}

/* Exported interface, documented in main-menu.h */
void main_menu_destroy(struct sdl_tk_widget *main_menu)
{
	assert(main_menu == bl_main_menu->widget);
	BV_UNUSED(main_menu);

	/* Always save the current config as "previous" so you can get it
	 * back if you forgot to save before quitting. */
	main_menu_save_config("previous.yaml");

	sdl_tk_widget_destroy(bl_main_menu->widget);
	bl_main_menu->widget = NULL;

	cyaml_free(&config, &schema_main_menu, bl_main_menu, 0);
	bl_main_menu = NULL;

	locked_uint_fini(&update_ctx.count);
}

/**
 * Get the LED mask from an LED menu.
 *
 * \param[in]  led_desc  Menu descriptor for LEDs.
 * \return the LED mask.
 */
static uint16_t main_menu__config_get_led_mask_helper(
		struct desc_widget *led_desc)
{
	static const uint8_t mapping[BL_LED_COUNT] = {
		15, 14, 13, 12, 11, 10,  9,  8,
		 0,  1,  2,  3,  4,  5,  6,  7,
	};
	uint16_t led_mask = 0;

	assert(led_desc != NULL);
	assert(led_desc->type == WIDGET_TYPE_MENU);
	assert(led_desc->menu.entry_count <= BL_LED_COUNT);

	for (unsigned i = 0; i < led_desc->menu.entry_count; i++) {
		struct desc_widget *desc = led_desc->menu.entry[i];
		assert(desc != NULL);
		assert(desc->type == WIDGET_TYPE_TOGGLE);
		if (desc->toggle.value) {
			led_mask |= 1U << mapping[i];
		}
	}

	return led_mask;
}

/* Exported interface, documented in main-menu.h */
enum bl_acq_mode main_menu_config_get_acq_mode(void)
{
	struct desc_widget *desc = main_menu__get_desc_fmt(
			bl_main_menu, "Config/Acquisition/Mode");

	assert(desc != NULL);
	assert(desc->type == WIDGET_TYPE_SELECT);

	return main_menu__select_value(&desc->select.value);
}

/**
 * Get the Acquisition Mode.
 *
 * \return the configured acquisition mode.
 */
static const char *main_menu_config_get_acq_mode_string(void)
{
	enum bl_acq_mode mode = main_menu_config_get_acq_mode();

	for (unsigned i = 0; i < BV_ARRAY_LEN(bv_acq_mode); i++) {
		if (bv_acq_mode[i].val == mode) {
			return bv_acq_mode[i].str;
		}
	}

	return NULL;
}

/* Exported interface, documented in main-menu.h */
uint16_t main_menu_config_get_led_mask(void)
{
	struct desc_widget *desc = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Acquisition/%s/LEDs",
			main_menu_config_get_acq_mode_string());

	assert(desc != NULL);

	return main_menu__config_get_led_mask_helper(desc);
}

/* Exported interface, documented in main-menu.h */
uint16_t main_menu_config_get_source_mask(void)
{
	struct desc_widget *sources = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Acquisition/%s/Sources",
			main_menu_config_get_acq_mode_string());
	uint16_t src_mask = 0;
	uint16_t src_count;

	assert(sources != NULL);

	src_count = sources->menu.entry_count;
	for (unsigned i = 0; i < src_count; i++) {
		struct desc_widget *desc = main_menu__get_desc_fmt(sources,
				"[%u]", i);
		assert(desc != NULL);
		if (desc->toggle.value) {
			src_mask |= 1U << i;
		}
	}

	return src_mask;
}

/* Exported interface, documented in main-menu.h */
uint16_t main_menu_config_get_frequency(void)
{
	return main_menu__get_desc_input_unsigned(bl_main_menu,
			"Config/Acquisition/Frequency (Hz)");
}

/**
 * Get the descriptor for a source setup.
 *
 * \param[in]  source  The source to look up.
 * \return the source descriptor.
 */
struct desc_widget *main_menu__get_source_desc(enum bl_acq_source source)
{
	enum bl_acq_mode mode = main_menu_config_get_acq_mode();
	struct desc_widget *desc = NULL;

	switch (mode) {
	case BL_ACQ_MODE_CONTINUOUS:
		desc = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Acquisition/Continuous/Channels/[%u]/Source",
			source);
		break;

	case BL_ACQ_MODE_FLASH:
		desc = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Acquisition/Flash/Source setup/[%u]", source);
		break;
	}

	assert(desc != NULL);
	assert(desc->type == WIDGET_TYPE_MENU);

	return desc;
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_config_get_source_sw_oversample(enum bl_acq_source source)
{
	struct desc_widget *desc = main_menu__get_source_desc(source);
	return main_menu__get_desc_input_unsigned(desc, "Software Oversample");
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_config_get_source_opamp_gain(enum bl_acq_source source)
{
	struct desc_widget *desc = main_menu__get_source_desc(source);
	return main_menu__get_desc_input_unsigned(desc, "Op-Amp Gain");
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_config_get_source_opamp_offset(enum bl_acq_source source)
{
	struct desc_widget *desc = main_menu__get_source_desc(source);
	return main_menu__get_desc_input_unsigned(desc, "Op-Amp Offset");
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_config_get_source_hw_oversample(enum bl_acq_source source)
{
	struct desc_widget *desc = main_menu__get_source_desc(source);
	return main_menu__get_desc_input_unsigned(desc, "Hardware Oversample");
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_config_get_source_hw_shift(enum bl_acq_source source)
{
	struct desc_widget *desc = main_menu__get_source_desc(source);
	return main_menu__get_desc_input_unsigned(desc, "Hardware Shift");
}

/**
 * Channel conversion types.
 */
enum channel_conv {
	CHANNEL_CONV_NONE,     /**< No channel conversion. */
	CHANNEL_CONV_HW_TO_MM, /**< Hardware order to main menu order. */
	CHANNEL_CONV_MM_TO_HW, /**< Main menu to hardware order. */
};

/**
 * Convert channel index between types.
 *
 * The main menu presents LEDs in wavelength sort order, but the hardware
 * has its own order.
 *
 * \param[in]  channel     The channel to look up.
 * \param[in]  input_type  The input channel index type.
 * \return converted channel index.
 */
static inline uint8_t main_menu__convert_led_index(
		uint8_t channel,
		enum channel_conv input_type)
{
	static const uint8_t mapping_mm_to_hw[BL_LED_COUNT] = {
		15, 14, 13, 12, 11, 10,  9,  8,
		 0,  1,  2,  3,  4,  5,  6,  7,
	};
	static const uint8_t mapping_hw_to_mm[BL_LED_COUNT] = {
		 8,  9, 10, 11, 12, 13, 14, 15,
		 7,  6,  5,  4,  3,  2,  1,  0,
	};
	const uint8_t *mapping = NULL;

	switch (input_type) {
	case CHANNEL_CONV_HW_TO_MM: mapping = mapping_hw_to_mm; break;
	case CHANNEL_CONV_MM_TO_HW: mapping = mapping_mm_to_hw; break;
	default:
		assert(input_type == CHANNEL_CONV_NONE);
		break;
	}

	if (mapping != NULL && channel < BL_LED_COUNT) {
		channel = mapping[channel];
	}

	return channel;
}

/**
 * Get the descriptor for a channel setup.
 *
 * \param[in]  channel     The channel to look up.
 * \param[in]  input_type  The input channel index type.
 * \return the channel descriptor.
 */
static struct desc_widget *main_menu__get_channel_desc(
		uint8_t channel,
		enum channel_conv input_type)
{
	enum bl_acq_mode mode = main_menu_config_get_acq_mode();
	struct desc_widget *desc = NULL;

	switch (mode) {
	case BL_ACQ_MODE_CONTINUOUS:
		desc = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Acquisition/Continuous/Channels/[%u]/Channel",
			channel);
		break;

	case BL_ACQ_MODE_FLASH:
		channel = main_menu__convert_led_index(channel, input_type);
		desc = main_menu__get_desc_fmt(bl_main_menu,
			"Config/Acquisition/Flash/Channels/[%u]", channel);
		break;
	}

	assert(desc != NULL);
	assert(desc->type == WIDGET_TYPE_MENU);

	return desc;
}

/* Exported interface, documented in main-menu.h */
uint8_t main_menu_config_get_channel_shift(uint8_t channel)
{
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	return main_menu__get_desc_input_unsigned(desc, "Software Shift");
}

/* Exported interface, documented in main-menu.h */
uint32_t main_menu_config_get_channel_offset(uint8_t channel)
{
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	return main_menu__get_desc_input_unsigned(desc, "Software Offset");
}

/* Exported interface, documented in main-menu.h */
bool main_menu_config_get_channel_sample32(uint8_t channel)
{
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	return main_menu__get_desc_toggle_value(desc, "32-bit samples");
}

/* Exported interface, documented in main-menu.h */
bool main_menu_config_get_channel_inverted(uint8_t channel)
{
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	return main_menu__get_desc_toggle_value(desc, "Invert data");
}

/* Exported interface, documented in main-menu.h */
SDL_Color main_menu_config_get_channel_colour(uint8_t channel)
{
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	struct desc_widget *col = main_menu__get_desc_fmt(desc, "Colour");

	assert(col != NULL);
	assert(col->type == WIDGET_TYPE_MENU);

	return sdl_tk_colour_get_hsv(
			main_menu__get_desc_input_unsigned(col, "Hue"),
			main_menu__get_desc_input_unsigned(col, "Saturation"),
			main_menu__get_desc_input_unsigned(col, "Value"));
}

/* Exported interface, documented in main-menu.h */
bool main_menu_config_get_filter_normalise_enabled(void)
{
	return main_menu__get_desc_toggle_value(bl_main_menu,
			"Config/Filtering/Normalisation");
}

/* Exported interface, documented in main-menu.h */
bool main_menu_config_get_filter_ac_denoise_enabled(void)
{
	return main_menu__get_desc_toggle_value(bl_main_menu,
			"Config/Filtering/AC denoise");
}

/* Exported interface, documented in main-menu.h */
double main_menu_config_get_filter_normalise_frequency(void)
{
	return main_menu__get_desc_input_double(bl_main_menu,
			"Config/Filtering/Normalisation frequency (Hz)");
}

/* Exported interface, documented in main-menu.h */
double main_menu_config_get_filter_ac_denoise_frequency(void)
{
	return main_menu__get_desc_input_double(bl_main_menu,
			"Config/Filtering/AC denoise frequency (Hz)");
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
bool main_menu_config_set_channel_shift(uint8_t channel, uint8_t shift)
{
	union update_data data;
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	struct desc_widget *shift_desc = main_menu__get_desc_fmt(desc,
			"Software Shift");

	assert(shift_desc != NULL);
	assert(shift_desc->type == WIDGET_TYPE_INPUT);
	assert(shift_desc->input.value.type == INPUT_TYPE_UNSIGNED);

	data.set_value.value = stringify_unsigned(shift);
	if (data.set_value.value == NULL) {
		return false;
	}

	return main_menu__push_update(UPDATE_TYPE_SET_VALUE,
			shift_desc->widget, &data);
}

/* Exported interface, documented in main-menu.h */
bool main_menu_config_set_channel_offset(uint8_t channel, uint32_t offset)
{
	union update_data data;
	struct desc_widget *desc = main_menu__get_channel_desc(
			channel, CHANNEL_CONV_HW_TO_MM);
	struct desc_widget *offset_desc = main_menu__get_desc_fmt(desc,
			"Software Offset");

	assert(offset_desc != NULL);
	assert(offset_desc->type == WIDGET_TYPE_INPUT);
	assert(offset_desc->input.value.type == INPUT_TYPE_UNSIGNED);

	data.set_value.value = stringify_unsigned(offset);
	if (data.set_value.value == NULL) {
		return false;
	}

	return main_menu__push_update(UPDATE_TYPE_SET_VALUE,
			offset_desc->widget, &data);
}

/* Exported interface, documented in main-menu.h */
void main_menu_set_acq_available(bool available)
{
	struct desc_widget *acq;
	struct desc_widget *cal;
	union update_data data = {
		.enable = {
			.enable = available,
		},
	};

	acq = main_menu__get_desc_fmt(bl_main_menu, "Acquisition");
	cal = main_menu__get_desc_fmt(bl_main_menu, "Calibrate");

	assert(acq != NULL);
	assert(cal != NULL);

	main_menu__push_update(UPDATE_TYPE_ENABLE, acq->widget, &data);
	main_menu__push_update(UPDATE_TYPE_ENABLE, cal->widget, &data);
}
