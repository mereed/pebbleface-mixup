/*
Copyright (C) 2014 Mark Reed

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <pebble.h>

Window *window;
GColor background_color = GColorBlack;

static AppSync sync;
static uint8_t sync_buffer[160];

static bool appStarted = false;

static int swap;
static int invert;
static int bluetoothvibe;
static int hourlyvibe;

enum {
  SWAP_KEY = 0x0,
  INVERT_KEY = 0x1,
  BLUETOOTHVIBE_KEY = 0x2,
  HOURLYVIBE_KEY = 0x3
};


TextLayer *time1_text_layer;
char time1_buffer[] = "      ";

TextLayer *date_text_layer;
char date_buffer[] = "         ";

#define TOTAL_TIME_DIGITS 4
static GBitmap *time_digits_images[TOTAL_TIME_DIGITS];
static BitmapLayer *time_digits_layers[TOTAL_TIME_DIGITS];

const int BIG_DIGIT_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_NUM_0,
  RESOURCE_ID_IMAGE_NUM_1,
  RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3,
  RESOURCE_ID_IMAGE_NUM_4,
  RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6,
  RESOURCE_ID_IMAGE_NUM_7,
  RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

int charge_percent = 0;

static Layer *path_layer;

InverterLayer *inverter_layer = NULL;

static GPath *batt10;
static GPath *batt20;
static GPath *batt30;
static GPath *batt40;
static GPath *batt50;
static GPath *batt60;
static GPath *batt70;
static GPath *batt80;
static GPath *batt90;
static GPath *batt100;

#define NUM_GRAPHIC_PATHS 10

static GPath *graphic_paths[NUM_GRAPHIC_PATHS];

static GPath *current_path = NULL;

static int current_path_index = 0;

static bool outline_mode = false;

// This defines graphics path information to be loaded as a path later
static const GPathInfo BATT10 = {
  4,
  (GPoint []) {
    {-1, -1},
    {72, -1},
    {72, 0},
    {-1, 0}
  }
};

static const GPathInfo BATT20 = {
  4,
  (GPoint []) {
    {-1, -1},
    {145, -1},
    {142, 0},
    {-1, 0}
  }
};

static const GPathInfo BATT30 = {
  6,
  (GPoint []) {
    {-1, -1},
    {145, -1},
	{145, 56},
	{142, 56},
    {142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT40 = {
  6,
  (GPoint []) {
    {-1, -1},
    {145, -1},
	{145, 112},
	{142, 112},
    {142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT50 = {
  6,
  (GPoint []) {
    {-1, -1},
    {145, -1},
	{145, 169},
	{142, 169},
    {142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT60 = {
  8,
  (GPoint []) {
    {-1, -1},
    {145, -1},
	{145, 169},
	{72, 169},
	{72,167},
	{142, 167},
    {142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT70 = {
  8,
  (GPoint []) {
    {-1, 0},
    {145, 0},
	{145, 169},
	{-1, 169},
	{-1, 167},
	{142, 167},
	{142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT80 = {
  10,
  (GPoint []) {
    {-1, 0},
    {145, 0},
	{145, 169},
	{-1, 169},
	{-1, 112},
	{1, 112},
	{1, 166},
	{142, 166},
	{142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT90 = {
  10,
  (GPoint []) {
    {-1, 0},
    {145, 0},
	{145, 169},
	{-1, 169},
	{-1, 56},
	{1, 56},	
	{1, 166},
	{142, 166},
	{142, 1},
    {-1, 1}
  }
};

static const GPathInfo BATT100 = {
  10,
  (GPoint []) {
    {-1, 0},
    {145, 0},
	{145, 169},
	{-1, 169},
	{-1, 1},
	{1,1},
	{1, 166},
	{142, 166},
	{142, 1},
    {-1, 1}
  }
};

static void path_layer_update_callback(Layer *me, GContext *ctx) {
  (void)me;
	if (outline_mode) {
    // draw outline uses the stroke color
    graphics_context_set_stroke_color(ctx, GColorWhite);
    gpath_draw_outline(ctx, current_path);
  } else {
    // draw filled uses the fill color
    graphics_context_set_fill_color(ctx, GColorWhite);
    gpath_draw_filled(ctx, current_path);
  }
}

void set_invert_color(bool invert) {
  if (invert && inverter_layer == NULL) {
    // Add inverter layer
    Layer *window_layer = window_get_root_layer(window);

    inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
  } else if (!invert && inverter_layer != NULL) {
    // Remove Inverter layer
    layer_remove_from_parent(inverter_layer_get_layer(inverter_layer));
    inverter_layer_destroy(inverter_layer);
    inverter_layer = NULL;
  }
  // No action required
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {

	switch (key) {
    case SWAP_KEY:
      swap = new_tuple->value->uint8;
	  persist_write_bool(SWAP_KEY, swap);
      break;
		
    case INVERT_KEY:
      invert = new_tuple->value->uint8 != 0;
      persist_write_bool(INVERT_KEY, invert);
      set_invert_color(invert);
      break;
	  
	case BLUETOOTHVIBE_KEY:
      bluetoothvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(BLUETOOTHVIBE_KEY, bluetoothvibe);
      break;      
	  
    case HOURLYVIBE_KEY:
      hourlyvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(HOURLYVIBE_KEY, hourlyvibe);	  
      break;		
		
  }
}

void update_battery_state(BatteryChargeState charge_state) {
	
	  if (charge_state.is_charging) {
		graphic_paths[9] = batt100;
		current_path = graphic_paths[9];	
		
    } else {
        
		  if (charge_state.charge_percent <= 10) {  
				graphic_paths[0] = batt10;
			  	current_path = graphic_paths[0];
          } else if (charge_state.charge_percent <= 20) {
			  graphic_paths[1] = batt20;
			  current_path = graphic_paths[1];
		  } else if (charge_state.charge_percent <= 30) {
			  graphic_paths[2] = batt30;
			  current_path = graphic_paths[2];
	 	  } else if (charge_state.charge_percent <= 40) {
			  graphic_paths[3] = batt40;
			  current_path = graphic_paths[3];
		  } else if (charge_state.charge_percent <= 50) {
			  graphic_paths[4] = batt50;
			  current_path = graphic_paths[4];
		  } else if (charge_state.charge_percent <= 60) {
			  graphic_paths[5] = batt60;
			  current_path = graphic_paths[5];
		  } else if (charge_state.charge_percent <= 70) {
			  graphic_paths[6] = batt70;
			  current_path = graphic_paths[6];
		  } else if (charge_state.charge_percent <= 80) {
			  graphic_paths[7] = batt80;
			  current_path = graphic_paths[7];
		  } else if (charge_state.charge_percent <= 90) {
			  graphic_paths[8] = batt90;
			  current_path = graphic_paths[8];
		  } else if (charge_state.charge_percent <= 98) {
			  graphic_paths[9] = batt100;
			  current_path = graphic_paths[9];
		  
		  } else {
			  graphic_paths[9] = batt100;
			  current_path = graphic_paths[9];
		  }
		
    } 
    charge_percent = charge_state.charge_percent;
} 

static void toggle_bluetooth(bool connected) {
  if(appStarted && !connected && bluetoothvibe) {
    //vibe!
    vibes_long_pulse();
  }
}

void bluetooth_connection_callback(bool connected) {
  toggle_bluetooth(connected);
}

static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  GBitmap *old_image = *bmp_image;
  *bmp_image = gbitmap_create_with_resource(resource_id);
  GRect frame = (GRect) {
    .origin = origin,
    .size = (*bmp_image)->bounds.size
  };
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);
  gbitmap_destroy(old_image);
}

static void update_hours(struct tm *tick_time) {

  if(appStarted && hourlyvibe) {
    //vibe!
    vibes_short_pulse();
  }
}

static void update_minutes(struct tm *tick_time) {
	
  set_container_image(&time_digits_images[2], time_digits_layers[2], BIG_DIGIT_IMAGE_RESOURCE_IDS[tick_time->tm_min/10], GPoint(6, 6));
  set_container_image(&time_digits_images[3], time_digits_layers[3], BIG_DIGIT_IMAGE_RESOURCE_IDS[tick_time->tm_min%10], GPoint(75, 6));
}
	
void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  
	if (units_changed & MINUTE_UNIT) {
    update_minutes(tick_time);
	}
	if (units_changed & HOUR_UNIT) {
    update_hours(tick_time);
    }
	
	int hour = tick_time->tm_hour;
   
	switch (hour % 12) {
    case 0:
      strcpy(time1_buffer, "twe12e");
	  	  if (swap) {
		        strcpy(time1_buffer, "twelve");
	      }
      break;
    case 1:
      strcpy(time1_buffer, "1ne");
			  	  if (swap) {
		        strcpy(time1_buffer, "one");
	      }
      break;
    case 2:
      strcpy(time1_buffer, "2wo");
			  	  if (swap) {
		        strcpy(time1_buffer, "two");
	      }
      break;
    case 3:
      strcpy(time1_buffer, "3hree");
	    if (swap) {
		        strcpy(time1_buffer, "three");
	      }
      break;
    case 4:
      strcpy(time1_buffer, "4our");
			  	  if (swap) {
		        strcpy(time1_buffer, "four");
	      }
      break;
    case 5:
      strcpy(time1_buffer, "5ive");
			    if (swap) {
		        strcpy(time1_buffer, "five");
	      }
      break;
    case 6:
      strcpy(time1_buffer, "6ix");
	  if (swap) {
		        strcpy(time1_buffer, "six");
	  }
      break;
    case 7:
      strcpy(time1_buffer, "7even");
			  	  if (swap) {
		        strcpy(time1_buffer, "seven");
	      }
      break;
    case 8:
      strcpy(time1_buffer, "8ight");
			  	  if (swap) {
		        strcpy(time1_buffer, "eight");
	      }
      break;
    case 9:
      strcpy(time1_buffer, "9ine");
			  	  if (swap) {
		        strcpy(time1_buffer, "nine");
	      }
      break;
    case 10:
      strcpy(time1_buffer, "10en");
			  	  if (swap) {
		        strcpy(time1_buffer, "ten");
	      }
      break;
    case 11:
      strcpy(time1_buffer, "11even");
			  	  if (swap) {
		        strcpy(time1_buffer, "eleven");
	      }
      break;
  }
  text_layer_set_text(time1_text_layer, time1_buffer);
  
//  if (units_changed & DAY_UNIT) {
    char wday_buffer[4];
    int wday = tick_time->tm_wday;
    switch (wday) {
      case 0:
        strcpy(wday_buffer, "5und4y");
		if (swap) {
		        strcpy(wday_buffer, "sunday");
	      }
        break;
      case 1:
        strcpy(wday_buffer, "m0nd4y");
			  	  if (swap) {
		        strcpy(wday_buffer, "monday");
	      }
        break;
      case 2:
        strcpy(wday_buffer, "tu3sd4y");
			  	  if (swap) {
		        strcpy(wday_buffer, "tuesday");
	      }
        break;
      case 3:
        strcpy(wday_buffer, "w3dn3sd4y");
			  	  if (swap) {
		        strcpy(wday_buffer, "wednesday");
	      }
        break;
      case 4:
        strcpy(wday_buffer, "thur5d4y");
			  	  if (swap) {
		        strcpy(wday_buffer, "thursday");
	      }
        break;
      case 5:
        strcpy(wday_buffer, "fr1d4y");
			  	  if (swap) {
		        strcpy(wday_buffer, "friday");
	      }
        break;
      case 6:
        strcpy(wday_buffer, "54turd4y");
			  	  if (swap) {
		        strcpy(wday_buffer, "saturday");
	      }
        break;
    }
    
//    int mday = tick_time->tm_mday;
	  
    snprintf(date_buffer, sizeof date_buffer, "%s", wday_buffer);
//  }
}

void init() {
  memset(&time_digits_layers, 0, sizeof(time_digits_layers));
  memset(&time_digits_images, 0, sizeof(time_digits_images));
	
  const int inbound_size = 160;
  const int outbound_size = 160;
  app_message_open(inbound_size, outbound_size);  

  window = window_create();
  if (window == NULL) {
      return;
  }
	
    window_stack_push(window, true);
  
	background_color  = GColorBlack;
    window_set_background_color(window, background_color);
		
	Layer *window_layer = window_get_root_layer(window);

    GRect bounds = layer_get_frame(window_layer);
    path_layer = layer_create(bounds);
    layer_set_update_proc(path_layer, path_layer_update_callback);
    layer_add_child(window_layer, path_layer);

    // Pass the corresponding GPathInfo to initialize a GPath
    batt10 = gpath_create(&BATT10);
    batt20 = gpath_create(&BATT20);
    batt30 = gpath_create(&BATT30);
    batt40 = gpath_create(&BATT40);
    batt50 = gpath_create(&BATT50);
    batt60 = gpath_create(&BATT60);
    batt70 = gpath_create(&BATT70);
    batt80 = gpath_create(&BATT80);
    batt90 = gpath_create(&BATT90);
    batt100 = gpath_create(&BATT100);
  
	// Create time and date layers
    GRect dummy_frame = { {0, 0}, {0, 0} };
	
  for (int i = 0; i < TOTAL_TIME_DIGITS; ++i) {
    time_digits_layers[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(window_layer, bitmap_layer_get_layer(time_digits_layers[i]));
  }

  time1_text_layer = text_layer_create(GRect(0, 64, 147, 30));
  text_layer_set_background_color(time1_text_layer, GColorClear);
  text_layer_set_text_color(time1_text_layer, GColorWhite);
  text_layer_set_text_alignment(time1_text_layer, GTextAlignmentCenter);
  text_layer_set_font(time1_text_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_HELVETICA_BOLD_24)));
  text_layer_set_text(time1_text_layer, time1_buffer);
  layer_add_child(window_get_root_layer(window), (Layer*) time1_text_layer);
  

  date_text_layer = text_layer_create(GRect(0, 128, 144, 20));
  text_layer_set_background_color(date_text_layer, GColorClear);
  text_layer_set_text_color(date_text_layer, GColorWhite);
  text_layer_set_text_alignment(date_text_layer, GTextAlignmentCenter);
  text_layer_set_font(date_text_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_HELVETICA_16)));
  text_layer_set_text(date_text_layer, date_buffer);
  layer_add_child(window_get_root_layer(window), (Layer*) date_text_layer);
  
  toggle_bluetooth(bluetooth_connection_service_peek());
  battery_state_service_subscribe(&update_battery_state);

  Tuplet initial_values[] = {
    TupletInteger(SWAP_KEY, persist_read_bool(SWAP_KEY)),
    TupletInteger(INVERT_KEY, persist_read_bool(INVERT_KEY)),
    TupletInteger(BLUETOOTHVIBE_KEY, persist_read_bool(BLUETOOTHVIBE_KEY)),
    TupletInteger(HOURLYVIBE_KEY, persist_read_bool(HOURLYVIBE_KEY)),
  };
  
  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, NULL, NULL);
   
  appStarted = true;
  
  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  tick_handler(localtime(&now), DAY_UNIT + HOUR_UNIT + MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler) tick_handler);
	
  // update the battery on launch
  update_battery_state(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bluetooth_connection_callback);
		
}

void deinit() {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
	
  text_layer_destroy(time1_text_layer);
  text_layer_destroy(date_text_layer);

  gpath_destroy(batt10);
  gpath_destroy(batt20);
  gpath_destroy(batt30);
  gpath_destroy(batt40);
  gpath_destroy(batt50);
  gpath_destroy(batt60);
  gpath_destroy(batt70);
  gpath_destroy(batt80);
  gpath_destroy(batt90);
  gpath_destroy(batt100);

  layer_destroy(path_layer);
  
//  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}