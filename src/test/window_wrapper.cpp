// self
#include "window_wrapper.hpp"



WindowWrapper::WindowWrapper()
	: m_layout_ways(PlayerWays::Zero)
	, m_central_widget(nullptr)
	, m_grid_layout(nullptr)
{
	setContextMenuPolicy(Qt::NoContextMenu);

	m_central_widget = new QWidget(this);
	setCentralWidget(m_central_widget);

	m_grid_layout = new QGridLayout(m_central_widget);
	m_grid_layout->setHorizontalSpacing(0);
	m_grid_layout->setVerticalSpacing(0);
	m_grid_layout->setContentsMargins(0, 0, 0, 0);
}


WindowWrapper::~WindowWrapper()
{
	destroy_players();
}


bool WindowWrapper::create_players(int ways, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context)
{
	// validate parameter
	PlayerWays player_ways = (PlayerWays)ways;
	if (
		PlayerWays::One != player_ways &&
		PlayerWays::Four != player_ways &&
		PlayerWays::Six != player_ways &&
		PlayerWays::Eight != player_ways &&
		PlayerWays::Nine != player_ways &&
		PlayerWays::Sixteen != player_ways
		) {
		return false;
	}

	// no need to re-create
	if (player_ways == m_layout_ways) {
		return false;
	}

	m_layout_ways = player_ways;

	// remove exists
	for (auto iter = m_index_to_widget.begin(); iter != m_index_to_widget.end(); iter++) {
		iter->second->hide();
		m_grid_layout->removeWidget(iter->second);
	}

	// re-create
	const double epsinon = 0.00000001;
	double sqrt_root = sqrt((int)ways);
	int rows = 0;
	int columns = 0;
	int first_row_span = 1;
	int first_column_span = 1;
	if (sqrt_root - (double)(int)sqrt_root <= epsinon) {
		// 1, 4, 8, 16
		rows = (int)sqrt_root;
		columns = rows;
	}
	else {
		// 6, 8
		if (ways == 6) {
			rows = 3;
			columns = 3;
			first_row_span = 2;
			first_column_span = 2;
		}
		else if (ways == 8) {
			rows = 4;
			columns = 4;
			first_row_span = 3;
			first_column_span = 3;
		}
	}

	int index = 0;
	for (int row_index = 0; row_index < rows; row_index++) {
		for (int column_index = 0; column_index < columns; column_index++) {
			int row_span = -1;
			int column_span = -1;
			if (row_index == 0 && column_index == 0) {
				row_span = first_row_span;
				column_span = first_column_span;
			}
			else if (row_index >= first_row_span || column_index >= first_column_span) {
				row_span = 1;
				column_span = 1;
			}

			if (row_span > 0 && column_span > 0) {
				QWidget *w = nullptr;
				w = new QWidget();
				m_index_to_widget.insert(std::make_pair(index, w));

				m_grid_layout->addWidget(w, row_index, column_index, row_span, column_span);

				w->show();

				index++;
			}
		}
	}


	return m_mpv_wrapper.create(m_index_to_widget, video_url, profile, vo, hwdec, gpu_api, gpu_context);
}


void WindowWrapper::destroy_players()
{
	m_index_to_widget.clear();
	m_layout_ways = PlayerWays::Zero;

	m_mpv_wrapper.destroy();
}
