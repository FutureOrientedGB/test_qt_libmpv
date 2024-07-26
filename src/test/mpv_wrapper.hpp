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
	mpv_handle *m_mpv_context;
	lock_free_spsc<uint8_t> m_spsc;
};



class MpvManager {
public:
	MpvManager(uint32_t buffer_size = 2048 * 1024);
	~MpvManager();

	bool start_players(std::map<int, QWidget *> &containers, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path);
	void stop_players();


private:
	uint32_t m_buffer_size;
	std::map<int, MpvWrapper *> m_index_to_mpv_wrapper;
};
