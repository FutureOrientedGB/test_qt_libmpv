#pragma once

// c++
#include <map>
#include <string>

// libmpv
struct mpv_handle;

// qt
struct QWidget;



class MpvWrapper {
public:
	MpvWrapper();
	~MpvWrapper();

	bool create(std::map<int, QWidget *> &containers, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context);
	void destroy();


private:
	std::map<int, mpv_handle *> m_index_to_mpv;
};
