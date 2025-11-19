#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RETRO_API_VERSION 1

typedef bool (*retro_environment_t)(unsigned cmd, void* data);
typedef void (*retro_video_refresh_t)(const void* data, unsigned width, unsigned height, size_t pitch);
typedef void (*retro_audio_sample_t)(int16_t left, int16_t right);
typedef size_t (*retro_audio_sample_batch_t)(const int16_t* data, size_t frames);
typedef void (*retro_input_poll_t)(void);
typedef int16_t (*retro_input_state_t)(unsigned port, unsigned device, unsigned index, unsigned id);

enum retro_log_level {
  RETRO_LOG_DEBUG = 0,
  RETRO_LOG_INFO,
  RETRO_LOG_WARN,
  RETRO_LOG_ERROR,
  RETRO_LOG_DUMMY = INT32_MAX
};

struct retro_log_callback {
  void (*log)(enum retro_log_level level, const char* fmt, ...);
};

struct retro_game_info {
  const char* path;
  const void* data;
  size_t size;
  const char* meta;
};

struct retro_system_info {
  const char* library_name;
  const char* library_version;
  const char* valid_extensions;
  bool need_fullpath;
  bool block_extract;
};

struct retro_game_geometry {
  unsigned base_width;
  unsigned base_height;
  unsigned max_width;
  unsigned max_height;
  float aspect_ratio;
};

struct retro_system_timing {
  double fps;
  double sample_rate;
};

struct retro_system_av_info {
  struct retro_game_geometry geometry;
  struct retro_system_timing timing;
};

struct retro_input_descriptor {
  unsigned port;
  unsigned device;
  unsigned index;
  unsigned id;
  const char* description;
};

struct retro_controller_description {
  const char* desc;
  unsigned id;
};

struct retro_controller_info {
  const struct retro_controller_description* types;
  unsigned num_types;
};

enum retro_region {
  RETRO_REGION_NTSC = 0,
  RETRO_REGION_PAL = 1
};

enum retro_pixel_format {
  RETRO_PIXEL_FORMAT_0RGB1555 = 0,
  RETRO_PIXEL_FORMAT_XRGB8888 = 1,
  RETRO_PIXEL_FORMAT_RGB565 = 2
};

enum {
  RETRO_DEVICE_NONE = 0,
  RETRO_DEVICE_JOYPAD = 1,
  RETRO_DEVICE_ANALOG = 5
};

enum {
  RETRO_DEVICE_ID_JOYPAD_B = 0,
  RETRO_DEVICE_ID_JOYPAD_Y,
  RETRO_DEVICE_ID_JOYPAD_SELECT,
  RETRO_DEVICE_ID_JOYPAD_START,
  RETRO_DEVICE_ID_JOYPAD_UP,
  RETRO_DEVICE_ID_JOYPAD_DOWN,
  RETRO_DEVICE_ID_JOYPAD_LEFT,
  RETRO_DEVICE_ID_JOYPAD_RIGHT,
  RETRO_DEVICE_ID_JOYPAD_A,
  RETRO_DEVICE_ID_JOYPAD_X,
  RETRO_DEVICE_ID_JOYPAD_L,
  RETRO_DEVICE_ID_JOYPAD_R,
  RETRO_DEVICE_ID_JOYPAD_L2,
  RETRO_DEVICE_ID_JOYPAD_R2,
  RETRO_DEVICE_ID_JOYPAD_L3,
  RETRO_DEVICE_ID_JOYPAD_R3
};

enum retro_environment_cmd {
  RETRO_ENVIRONMENT_SET_ROTATION = 1,
  RETRO_ENVIRONMENT_GET_OVERSCAN = 2,
  RETRO_ENVIRONMENT_GET_CAN_DUPE = 3,
  RETRO_ENVIRONMENT_SET_MESSAGE = 6,
  RETRO_ENVIRONMENT_SHUTDOWN = 7,
  RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL = 8,
  RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY = 9,
  RETRO_ENVIRONMENT_SET_PIXEL_FORMAT = 10,
  RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS = 11,
  RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK = 12,
  RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE = 13,
  RETRO_ENVIRONMENT_SET_HW_RENDER = 14,
  RETRO_ENVIRONMENT_GET_VARIABLE = 15,
  RETRO_ENVIRONMENT_SET_VARIABLES = 16,
  RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE = 17,
  RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME = 18,
  RETRO_ENVIRONMENT_GET_LIBRETRO_PATH = 19,
  RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK = 22,
  RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE = 23,
  RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES = 26,
  RETRO_ENVIRONMENT_GET_LOG_INTERFACE = 27,
  RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY = 31,
  RETRO_ENVIRONMENT_SET_CONTROLLER_INFO = 40,
  RETRO_ENVIRONMENT_SET_GEOMETRY = 43
};

#ifdef __cplusplus
}
#endif
