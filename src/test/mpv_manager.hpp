#pragma once

// c
#include <stdint.h>

// c++
#include <string>
#include <map>
#include <thread>

// qt
class QWidget;

// project
class MpvWrapper;


#ifndef DEFUALT_BUFFER_SIZE
#define DEFUALT_BUFFER_SIZE 2048 * 1024
#endif // !DEFUALT_BUFFER_SIZE



class MpvManager {
public:
	MpvManager(uint32_t buffer_size = DEFUALT_BUFFER_SIZE);
	~MpvManager();

	bool start_players(
		std::map<int, QWidget *> &containers, int gpu_ways, std::string video_url,
		std::string profile, std::string vo, std::string hwdec,
		std::string gpu_api, std::string gpu_context, std::string log_level
	);
	void stop_players();


private:
	bool m_stopping;
	uint32_t m_buffer_size;
	std::thread *m_read_file_thread;
	std::map<int, MpvWrapper *> m_index_to_mpv_wrapper;
};
