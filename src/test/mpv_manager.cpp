// self
#include "mpv_manager.hpp"

// spdlog
#include <spdlog/spdlog.h>

// qt
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

// project
#include "mpv_wrapper.hpp"


#define READ_INTERVAL_MS 40
#define READ_BUFFER_SIZE 32768
#define STEADY_CLOCK_NOW() std::chrono::steady_clock::now()
#define STEADY_CLOCK_DURATION(begin) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count()



MpvManager::MpvManager(uint32_t buffer_size)
	: m_stopping(false)
	, m_buffer_size(buffer_size)
	, m_read_file_thread(nullptr)
{
}


MpvManager::~MpvManager()
{
	stop_players();
}



bool create_mpv_player(uint32_t buffer_size, int index, int64_t wid, std::map<int, MpvWrapper *> &index_to_mpv, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	MpvWrapper *mpv = new MpvWrapper(buffer_size);
	if (nullptr == mpv || mpv->is_buffer_null()) {
		return false;
	}

	index_to_mpv.insert(std::make_pair(index, mpv));

	return mpv->start(index, wid, video_url, profile, vo, hwdec, gpu_api, gpu_context, log_level, log_path);
}


bool MpvManager::start_players(std::map<int, QWidget *> &containers, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	if (containers.empty()) {
		return false;
	}

	for (auto iter = containers.begin(); iter != containers.end(); iter++) {
		int index = iter->first;
		if (!create_mpv_player(m_buffer_size, index, (int64_t)iter->second->winId(), m_index_to_mpv_wrapper, video_url, profile, vo, hwdec, gpu_api, gpu_context, log_level, log_path)) {
			stop_players();
			return false;
		}
	}

	m_stopping = false;

	QString path = QString::fromStdString(video_url);
	if (QFile(path).exists()) {
		m_read_file_thread = new std::thread(
			[path, this]() {
				QFile stream(path);
				if (stream.open(QIODevice::ReadOnly)) {
					std::chrono::steady_clock::time_point time_point_begin;
					bool finished = false;
					while (!m_stopping) {
						time_point_begin = STEADY_CLOCK_NOW();

						QByteArray buf = stream.read(READ_BUFFER_SIZE);
						if (buf.isEmpty()) {
							break;
						}

						for (auto iter = m_index_to_mpv_wrapper.begin(); !m_stopping && iter != m_index_to_mpv_wrapper.end(); iter++) {
							if (!iter->second->write((const uint8_t *)buf.constData(), (uint32_t)buf.size())) {
								finished = true;
								break;
							}
							if (m_stopping) {
								finished = true;
								break;
							}
						}
						if (finished) {
							break;
						}

						auto duration = STEADY_CLOCK_DURATION(time_point_begin);
						if (READ_INTERVAL_MS > duration) {
							std::this_thread::sleep_for(std::chrono::milliseconds(READ_INTERVAL_MS - duration));
						}
					}
				}

				stopping();
			}
		);
	}

	return true;
}


void MpvManager::stop_players()
{
	m_stopping = true;

	for (auto iter = m_index_to_mpv_wrapper.begin(); iter != m_index_to_mpv_wrapper.end(); iter++) {
		if (iter->second != nullptr) {
			iter->second->stop();
			delete iter->second;
			iter->second = nullptr;
		}
	}
	m_index_to_mpv_wrapper.clear();

	if (m_read_file_thread != nullptr) {
		if (m_read_file_thread->joinable()) {
			m_read_file_thread->join();
		}
		delete m_read_file_thread;
	}
	m_read_file_thread = nullptr;
}


void MpvManager::stopping()
{
	for (auto iter = m_index_to_mpv_wrapper.begin(); !m_stopping && iter != m_index_to_mpv_wrapper.end(); iter++) {
		iter->second->stopping();
	}
}
