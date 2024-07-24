#pragma once

// c++
#include <map>
#include <string>

// project
#include "mpv_wrapper.hpp"

// qt
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMainWindow>



enum class PlayerWays : uint8_t {
	Zero = 0,
	One = 1,
	Four = 4,
	Six = 6,
	Eight = 8,
	Nine = 9,
	Sixteen = 16,
};

static PlayerWays SUPPORT_PLAYER_WAYS[] = {
	PlayerWays::One,
	PlayerWays::Four,
	PlayerWays::Six,
	PlayerWays::Eight,
	PlayerWays::Nine,
	PlayerWays::Sixteen
};


class WindowWrapper : public QMainWindow {
	Q_OBJECT


public:
	WindowWrapper();
	~WindowWrapper();

	bool create_players(int ways, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path);
	void destroy_players();


private:
	MpvWrapper m_mpv_wrapper;

	PlayerWays m_layout_ways;
	QWidget* m_central_widget;
	QGridLayout* m_grid_layout;
	std::map<int, QWidget *> m_index_to_widget;
};

