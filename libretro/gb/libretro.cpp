#include "libretro.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>


#define MIA_LIBRARY
#include <mia/mia.hpp>
#include <gb/gb.hpp>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr double kFrameRate = 59.72750056960583;
constexpr unsigned kDefaultWidth = 160;
constexpr unsigned kDefaultHeight = 144;

retro_environment_t environmentCallback = nullptr;
retro_video_refresh_t videoCallback = nullptr;
retro_audio_sample_t audioCallback = nullptr;
retro_audio_sample_batch_t audioBatchCallback = nullptr;
retro_input_poll_t inputPollCallback = nullptr;
retro_input_state_t inputStateCallback = nullptr;
retro_log_callback logCallback = {};
bool hasLogInterface = false;

auto logMessage(enum retro_log_level level, const char* message) -> void {
  if(hasLogInterface && logCallback.log) {
    logCallback.log(level, "%s", message);
  }
}

struct GameBoyCore : ares::Platform {
  GameBoyCore();

  auto load(const retro_game_info* info) -> bool;
  auto unload() -> void;
  auto reset() -> void;
  auto run() -> void;
  auto serialize_size() const -> size_t;
  auto serialize(void* data, size_t size) -> bool;
  auto unserialize(const void* data, size_t size) -> bool;
  auto av_info() const -> retro_system_av_info { return avInfo; }

  auto attach(ares::Node::Object node) -> void override;
  auto detach(ares::Node::Object node) -> void override;
  auto pak(ares::Node::Object node) -> std::shared_ptr<vfs::directory> override;
  auto log(ares::Node::Debugger::Tracer::Tracer, string_view message) -> void override;
  auto status(string_view message) -> void override;
  auto video(ares::Node::Video::Screen node, const u32* data, u32 pitch, u32 width, u32 height) -> void override;
  auto audio(ares::Node::Audio::Stream node) -> void override;
  auto input(ares::Node::Input::Input node) -> void override;

private:
  auto configure_filesystem(const char* systemDirectory, const char* saveDirectory) -> void;
  auto save_nonvolatile() -> void;
  auto update_geometry(u32 width, u32 height) -> void;

  std::shared_ptr<mia::Pak> systemPak;
  std::shared_ptr<mia::Pak> gamePak;
  ares::Node::System root;
  std::vector<ares::Node::Video::Screen> screens;
  std::vector<ares::Node::Audio::Stream> streams;
  retro_system_av_info avInfo{};
  string homePath;
  string savePath;
  std::vector<int16_t> audioBuffer;
  bool loaded = false;
};

GameBoyCore::GameBoyCore() {
  ares::platform = this;
  avInfo.geometry.base_width = kDefaultWidth;
  avInfo.geometry.base_height = kDefaultHeight;
  avInfo.geometry.max_width = kDefaultWidth;
  avInfo.geometry.max_height = kDefaultHeight;
  avInfo.geometry.aspect_ratio = static_cast<float>(kDefaultWidth) / static_cast<float>(kDefaultHeight);
  avInfo.timing.fps = kFrameRate;
  avInfo.timing.sample_rate = kSampleRate;
}

auto GameBoyCore::configure_filesystem(const char* systemDirectory, const char* saveDirectory) -> void {
  auto sanitize = [](const char* path) -> string {
    if(path == nullptr || *path == '\0') return {};
    string normalized = path;
    if(normalized.endsWith("/") == false) normalized.append("/");
    return normalized;
  };

  homePath = sanitize(systemDirectory);
  savePath = sanitize(saveDirectory);

  if(homePath) {
    auto pathCopy = homePath;
    mia::setHomeLocation([pathCopy]() -> string { return pathCopy; });
  }

  if(savePath) {
    auto pathCopy = savePath;
    mia::setSaveLocation([pathCopy]() -> string { return pathCopy; });
  } else {
    mia::setSaveLocation([]() -> string { return string{}; });
  }
}

auto GameBoyCore::load(const retro_game_info* info) -> bool {
  unload();

  if(!info || !info->path || !*info->path) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: retro_game_info.path is required");
    return false;
  }

  mia::construct();

  if(environmentCallback) {
    const char* systemDirectory = nullptr;
    const char* saveDirectory = nullptr;
    environmentCallback(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &systemDirectory);
    environmentCallback(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &saveDirectory);
    configure_filesystem(systemDirectory, saveDirectory);
  }

  gamePak = mia::Medium::create("Game Boy");
  if(!gamePak) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: failed to create Game Boy medium");
    return false;
  }
  auto gameResult = gamePak->load(info->path);
  if(gameResult != successful) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: failed to load Game Boy cartridge");
    gamePak = {};
    return false;
  }

  systemPak = mia::System::create("Game Boy");
  if(!systemPak) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: failed to create Game Boy system pak");
    gamePak = {};
    return false;
  }
  auto systemResult = systemPak->load();
  if(systemResult != successful) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: failed to load Game Boy system pak");
    systemPak = {};
    gamePak = {};
    return false;
  }

  if(!ares::GameBoy::load(root, "[Nintendo] Game Boy")) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: ares::GameBoy::load failed");
    systemPak = {};
    gamePak = {};
    return false;
  }

  if(auto port = root->find<ares::Node::Port>("Cartridge Slot")) {
    port->allocate();
    port->connect();
  }

  screens = root->find<ares::Node::Video::Screen>();
  streams = root->find<ares::Node::Audio::Stream>();
  for(auto& stream : streams) stream->setResamplerFrequency(kSampleRate);

  update_geometry(kDefaultWidth, kDefaultHeight);

  root->power();
  loaded = true;
  return true;
}

auto GameBoyCore::unload() -> void {
  if(!loaded) return;

  save_nonvolatile();

  if(root) root->unload();
  root = {};
  screens.clear();
  streams.clear();
  systemPak = {};
  gamePak = {};
  audioBuffer.clear();
  loaded = false;
}

auto GameBoyCore::reset() -> void {
  if(!loaded) return;
  root->power(true);
}

auto GameBoyCore::run() -> void {
  if(!loaded) return;
  if(inputPollCallback) inputPollCallback();
  root->run();
}

auto GameBoyCore::serialize_size() const -> size_t {
  if(!loaded) return 0;
  if(auto state = root->serialize(false)) return state.size();
  return 0;
}

auto GameBoyCore::serialize(void* data, size_t size) -> bool {
  if(!loaded) return false;
  auto state = root->serialize(false);
  if(!state || state.size() > size) return false;
  memcpy(data, state.data(), state.size());
  return true;
}

auto GameBoyCore::unserialize(const void* data, size_t size) -> bool {
  if(!loaded) return false;
  serializer state{(const u8*)data, (u32)size};
  return root->unserialize(state);
}

auto GameBoyCore::attach(ares::Node::Object node) -> void {
  if(node->cast<ares::Node::Video::Screen>()) {
    screens = root->find<ares::Node::Video::Screen>();
  }

  if(auto stream = node->cast<ares::Node::Audio::Stream>()) {
    streams = root->find<ares::Node::Audio::Stream>();
    stream->setResamplerFrequency(kSampleRate);
  }
}

auto GameBoyCore::detach(ares::Node::Object node) -> void {
  if(node->cast<ares::Node::Video::Screen>()) {
    screens = root->find<ares::Node::Video::Screen>();
  }

  if(node->cast<ares::Node::Audio::Stream>()) {
    streams = root->find<ares::Node::Audio::Stream>();
  }
}

auto GameBoyCore::pak(ares::Node::Object node) -> std::shared_ptr<vfs::directory> {
  if(!node) return {};
  if(node->name() == "Game Boy" && systemPak) return systemPak->pak;
  if(node->name() == "Game Boy Cartridge" && gamePak) return gamePak->pak;
  return {};
}

auto GameBoyCore::log(ares::Node::Debugger::Tracer::Tracer, string_view message) -> void {
  if(!hasLogInterface || !logCallback.log) return;
  string text{message};
  logCallback.log(RETRO_LOG_INFO, "%s", text.data());
}

auto GameBoyCore::status(string_view message) -> void {
  if(!hasLogInterface || !logCallback.log) return;
  string text{message};
  logCallback.log(RETRO_LOG_INFO, "%s", text.data());
}

auto GameBoyCore::video(ares::Node::Video::Screen node, const u32* data, u32 pitch, u32 width, u32 height) -> void {
  if(!videoCallback) return;
  (void)node;
  if(width != avInfo.geometry.base_width || height != avInfo.geometry.base_height) {
    update_geometry(width, height);
  }
  videoCallback(data, width, height, pitch);
}

auto GameBoyCore::audio(ares::Node::Audio::Stream) -> void {
  if(streams.empty()) return;
  if(!audioBatchCallback && !audioCallback) return;

  audioBuffer.clear();
  while(true) {
    for(auto& stream : streams) {
      if(!stream->pending()) goto flush;
    }

    f64 mix[2] = {0.0, 0.0};
    for(auto& stream : streams) {
      f64 temp[2] = {0.0, 0.0};
      auto channels = stream->read(temp);
      if(channels == 1) {
        mix[0] += temp[0];
        mix[1] += temp[0];
      } else {
        mix[0] += temp[0];
        mix[1] += temp[1];
      }
    }

    for(auto& sample : mix) {
      sample = std::clamp(sample, -1.0, 1.0);
      audioBuffer.push_back((int16_t)std::lround(sample * 32767.0));
    }
  }

flush:
  if(audioBuffer.empty()) return;

  if(audioBatchCallback) {
    audioBatchCallback(audioBuffer.data(), audioBuffer.size() / 2);
  } else if(audioCallback) {
    for(size_t index = 0; index + 1 < audioBuffer.size(); index += 2) {
      audioCallback(audioBuffer[index + 0], audioBuffer[index + 1]);
    }
  }

  audioBuffer.clear();
}

auto GameBoyCore::input(ares::Node::Input::Input node) -> void {
  if(!inputStateCallback) return;

  if(auto button = node->cast<ares::Node::Input::Button>()) {
    int16_t pressed = 0;
    if(node->name() == "Up") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
    else if(node->name() == "Down") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
    else if(node->name() == "Left") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
    else if(node->name() == "Right") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    else if(node->name() == "B") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
    else if(node->name() == "A") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
    else if(node->name() == "Select") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
    else if(node->name() == "Start") pressed = inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
    button->setValue(pressed ? 1 : 0);
  }
}

auto GameBoyCore::save_nonvolatile() -> void {
  if(!root) return;
  root->save();
  if(systemPak) systemPak->save(systemPak->location);
  if(gamePak) gamePak->save(gamePak->location);
}

auto GameBoyCore::update_geometry(u32 width, u32 height) -> void {
  avInfo.geometry.base_width = width;
  avInfo.geometry.base_height = height;
  avInfo.geometry.max_width = width;
  avInfo.geometry.max_height = height;
  avInfo.geometry.aspect_ratio = height ? (float)width / (float)height : 1.0f;
  if(environmentCallback) {
    environmentCallback(RETRO_ENVIRONMENT_SET_GEOMETRY, &avInfo.geometry);
  }
}

GameBoyCore core;

}  // namespace

extern "C" {

auto retro_api_version() -> unsigned { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t callback) {
  environmentCallback = callback;
  hasLogInterface = false;
  if(environmentCallback) {
    retro_log_callback logger{};
    if(environmentCallback(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger)) {
      logCallback = logger;
      hasLogInterface = logger.log != nullptr;
    }
  }
}

void retro_get_system_info(struct retro_system_info* info) {
  static const char* extensions = "gb|zip";
  static const char* libraryName = "ares (Game Boy)";
  info->library_name = libraryName;
  info->library_version = ares::Version;
  info->valid_extensions = extensions;
  info->need_fullpath = true;
  info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
  *info = core.av_info();
}

void retro_set_video_refresh(retro_video_refresh_t callback) { videoCallback = callback; }
void retro_set_audio_sample(retro_audio_sample_t callback) { audioCallback = callback; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback) { audioBatchCallback = callback; }
void retro_set_input_poll(retro_input_poll_t callback) { inputPollCallback = callback; }
void retro_set_input_state(retro_input_state_t callback) { inputStateCallback = callback; }

void retro_init() {}
void retro_deinit() { core.unload(); }

void retro_reset() { core.reset(); }

void retro_run() { core.run(); }

bool retro_load_game(const struct retro_game_info* info) {
  if(!environmentCallback) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: environment callback is not set");
    return false;
  }

  retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
  if(!environmentCallback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) {
    logMessage(RETRO_LOG_ERROR, "ares-libretro: RETRO_PIXEL_FORMAT_XRGB8888 not supported");
    return false;
  }

  retro_input_descriptor descriptors[] = {
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
    {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
    {0, RETRO_DEVICE_NONE, 0, 0, nullptr},
  };
  environmentCallback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, descriptors);

  static const retro_controller_description controllers[] = {
    {"RetroPad", RETRO_DEVICE_JOYPAD},
  };
  static const retro_controller_info controllerInfo = {controllers, 1};
  environmentCallback(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)&controllerInfo);

  return core.load(info);
}

void retro_unload_game() { core.unload(); }

size_t retro_serialize_size() { return core.serialize_size(); }

bool retro_serialize(void* data, size_t size) { return core.serialize(data, size); }

bool retro_unserialize(const void* data, size_t size) { return core.unserialize(data, size); }

void* retro_get_memory_data(unsigned) { return nullptr; }

size_t retro_get_memory_size(unsigned) { return 0; }

unsigned retro_get_region() { return RETRO_REGION_NTSC; }

bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t) { return false; }

void retro_cheat_reset() {}

void retro_cheat_set(unsigned, bool, const char*) {}

}  // extern "C"
