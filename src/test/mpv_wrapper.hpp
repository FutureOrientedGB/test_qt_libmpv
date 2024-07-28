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

	bool start(int index, QWidget *container, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path);
	void stop();

	bool is_buffer_null();

	void write(const uint8_t *buf, uint32_t length);
	int64_t read(char *buf, uint64_t nbytes);


private:
	bool m_stopping;
	mpv_handle *m_mpv_context;
	lock_free_spsc<uint8_t> m_spsc;
};

