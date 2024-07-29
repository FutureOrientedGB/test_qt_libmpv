#pragma once

// c++
#include <map>
#include <string>

// project
#include "spsc.hpp"

// libmpv
struct mpv_handle;

// qt
struct QWidget;



class MpvWrapper {
public:
	MpvWrapper(uint32_t buffer_size);
	~MpvWrapper();

	bool start(int index, int64_t container_wid, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path);
	void stop();

	void stopping();

	bool create_handle();

	bool initialize_handle();

	bool add_io_read_callbacks();

	bool set_log_level(std::string min_level);

	bool call_command(std::vector<std::string> args);

	bool set_option(std::string key, bool value);
	bool set_option(std::string key, int64_t value);
	bool set_option(std::string key, double value);
	bool set_option(std::string key, std::string value);

	bool is_buffer_null();

	bool write(const uint8_t *buf, uint32_t length);
	int64_t read(char *buf, uint64_t nbytes);


private:
	bool m_stopping;
	mpv_handle *m_mpv_context;
	lock_free_spsc<uint8_t> m_spsc;
};

