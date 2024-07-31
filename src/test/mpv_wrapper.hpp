#pragma once

// c++
#include <map>
#include <string>

// project
#include "spsc.hpp"

// libmpv
struct mpv_handle;



class MpvWrapper {
public:
	MpvWrapper(uint32_t buffer_size = 1024 * 1024);
	~MpvWrapper();

	// start player
	bool start(int index, int64_t container_wid, std::string video_url = "", std::string profile = "low-latency", std::string vo = "", std::string hwdec = "", std::string gpu_api = "", std::string gpu_context = "", std::string log_level = "v", std::string log_path = "");
	// stop player
	void stop();

	// break infinite loop
	void stopping();

	// wrap mpv_create to create mpv handle ctx
	bool create_handle();

	// wrap mpv_initialize to init mpv handle ctx
	bool initialize_handle();

	// wrap mpv_stream_cb_add_ro to register custom stream protocol
	bool register_stream_callbacks();

	// wrap mpv_request_log_messages to set log level
	bool set_log_level(std::string min_level);

	// wrap mpv_command to call mpv command
	bool call_command(std::vector<std::string> args);

	// wrap mpv_set_option to set mpv option
	bool set_option(std::string key, bool value);
	bool set_option(std::string key, int64_t value);
	bool set_option(std::string key, double value);
	bool set_option(std::string key, std::string value);

	// wrap mpv_get_option to get mpv option
	bool get_property(std::string key, bool &value);
	bool get_property(std::string key, int64_t &value);
	bool get_property(std::string key, double &value);
	bool get_property(std::string key, std::string &value);

	// wrap mpv_set_property to set mpv properity
	bool set_property(std::string key, bool value);
	bool set_property(std::string key, int64_t value);
	bool set_property(std::string key, double value);
	bool set_property(std::string key, std::string value);

	// validate spsc
	bool is_buffer_null();

	// write av stream to spsc
	bool write(const uint8_t *buf, uint32_t length);
	// read av stream from spsc
	int64_t read(char *buf, uint64_t nbytes);

	// play
	bool play();
	// pause
	bool pause();
	// play one frame
	bool step();

	// is mute
	bool get_mute_state();
	// set mute or unmute
	void set_mute_state(const bool state);

	// get volume
	int get_volume();
	// set volume
	void set_volume(const int v);

	// get video resolution
	bool get_resolution(int64_t &width, int64_t &height);

	// take screenshot from video
	bool screenshot(std::vector<uint8_t> &pic, int64_t &width, int64_t &height);


private:
	// flag to break infinite loop
	bool m_stopping;
	// spsc size
	uint32_t m_buffer_size;
	// mpv handle ctx
	mpv_handle *m_mpv_context;
	// player's parent window id
	int64_t m_container_wid;
	// spsc
	lock_free_spsc<uint8_t> m_spsc;
};

