// self
#include "mpv_wrapper.hpp"

// c
#include <stdio.h>
#include <stdint.h>
#include <locale.h>

// libmpv
#include <mpv/client.h>
#include <mpv/stream_cb.h>

// qt
#include <QtCore/QFile>
#include <QtWidgets/QWidget>

// spdlog
#include <spdlog/spdlog.h>



int64_t size_fn(void *cookie)
{
	return MPV_ERROR_UNSUPPORTED;
}


int64_t seek_fn(void *cookie, int64_t offset)
{
	return MPV_ERROR_UNSUPPORTED;
}


int64_t read_fn(void *cookie, char *buf, uint64_t nbytes)
{
	auto thiz = (MpvWrapper *)cookie;
	return thiz->read(buf, nbytes);
}



void close_fn(void *cookie)
{
	auto thiz = (MpvWrapper *)cookie;
	thiz->stop();
}


int open_fn(void *user_data, char *uri, mpv_stream_cb_info *info)
{
	auto thiz = (MpvWrapper *)user_data;
	if (nullptr == thiz || thiz->is_buffer_null()) {
		return MPV_ERROR_LOADING_FAILED;
	}

	info->cookie = thiz;
	info->size_fn = size_fn;
	info->read_fn = read_fn;
	info->seek_fn = seek_fn;
	info->close_fn = close_fn;

	return 0;
}


MpvWrapper::MpvWrapper(uint32_t buffer_size)
	: m_mpv_context(nullptr)
{
	m_spsc.reset(buffer_size);
}


MpvWrapper::~MpvWrapper()
{
	stop();
}


bool MpvWrapper::start(int index, QWidget *container, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	m_mpv_context = mpv_create();
	if (nullptr == m_mpv_context) {
		SPDLOG_ERROR("mpv_create() error");
		return false;
	}

	int code = 0;

	int64_t wid = (int64_t)container->winId();
	code = mpv_set_option(m_mpv_context, "wid", MPV_FORMAT_INT64, &wid);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option({}, wid, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), wid, code, mpv_error_string(code));
		return false;
	}

	if (!profile.empty()) {
		code = mpv_set_option_string(m_mpv_context, "profile", profile.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option({}, profile, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), profile, code, mpv_error_string(code));
			return false;
		}
	}

	if (!vo.empty()) {
		code = mpv_set_option_string(m_mpv_context, "vo", vo.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option({}, vo, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), vo, code, mpv_error_string(code));
			return false;
		}
	}

	if (!hwdec.empty()) {
		code = mpv_set_option_string(m_mpv_context, "hwdec", hwdec.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option({}, hwdec, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), hwdec, code, mpv_error_string(code));
			return false;
		}
	}

	if (!gpu_api.empty()) {
		code = mpv_set_option_string(m_mpv_context, "gpu-api", gpu_api.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option({}, gpu-api, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), gpu_api, code, mpv_error_string(code));
			return false;
		}
	}

	if (!gpu_context.empty()) {
		code = mpv_set_option_string(m_mpv_context, "gpu-context", gpu_context.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option({}, gpu-context, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), gpu_context, code, mpv_error_string(code));
			return false;
		}
	}

	if (!log_level.empty()) {
		code = mpv_request_log_messages(m_mpv_context, log_level.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_request_log_messages({}, log_level) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), log_level, code, mpv_error_string(code));
			return false;
		}
	}

	if (!log_path.empty()) {
		code = mpv_set_option_string(m_mpv_context, "log-file", QString::fromStdString(log_path).replace(".log", QString(".%1.log").arg(index)).toStdString().c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option_string({}, log-file, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), log_path, code, mpv_error_string(code));
			return false;
		}
	}

	code = mpv_initialize(m_mpv_context);
	if (code < 0) {
		SPDLOG_ERROR("mpv_initialize({}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), wid, code, mpv_error_string(code));
		return false;
	}

	if (!QFile(QString::fromStdString(video_url)).exists()) {
		// read from network
		code = mpv_set_option_string(m_mpv_context, "stream-open-filename", video_url.c_str());
		if (code < 0) {
			SPDLOG_ERROR("mpv_set_option_string({}, stream-open-filename, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), video_url, code, mpv_error_string(code));
			return false;
		}

		code = mpv_command_string(m_mpv_context, "play");
		if (code < 0) {
			SPDLOG_ERROR("mpv_command_string({}, play) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), code, mpv_error_string(code));
			return false;
		}
	}
	else {
		// read from file
		auto url = new std::string(video_url);
		code = mpv_stream_cb_add_ro(m_mpv_context, "myprotocol", (void *)url, open_fn);
		if (code < 0) {
			SPDLOG_ERROR("mpv_stream_cb_add_ro({}, myprotocol, {}, open_fn) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), video_url, code, mpv_error_string(code));
			return false;
		}

		const char *cmd[] = { "loadfile", "myprotocol://fake", NULL };
		code = mpv_command(m_mpv_context, cmd);
		if (code < 0) {
			SPDLOG_ERROR("mpv_command({}, loadfile, myprotocol://fake) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), code, mpv_error_string(code));
			return false;
		}
	}

	return true;
}


void MpvWrapper::stop()
{
	if (m_mpv_context != nullptr) {
		mpv_terminate_destroy(m_mpv_context);
	}
	m_mpv_context = nullptr;
}


bool MpvWrapper::is_buffer_null()
{
	return m_spsc.is_buffer_null();
}


void MpvWrapper::write(const uint8_t *buf, uint32_t length)
{
	uint32_t offset = 0;
	while (offset < length && m_mpv_context != nullptr) {
		offset += m_spsc.put(buf + offset, length - offset);
	}
}


int64_t MpvWrapper::read(char *buf, uint64_t nbytes)
{
	return (int64_t)m_spsc.get((uint8_t *)buf, (uint32_t)nbytes);
}


MpvManager::MpvManager(uint32_t buffer_size)
	: m_buffer_size(buffer_size)
{
}


MpvManager::~MpvManager()
{
}



bool create_mpv_player(uint32_t buffer_size, int index, QWidget *window, std::map<int, MpvWrapper *> &index_to_mpv, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	MpvWrapper *mpv = new MpvWrapper(buffer_size);
	if (nullptr == mpv || mpv->is_buffer_null()) {
		return false;
	}

	mpv_handle *m_mpv_context = mpv_create();
	if (!m_mpv_context) {
		SPDLOG_ERROR("mpv_create() error");
		return false;
	}

	index_to_mpv.insert(std::make_pair(index, mpv));

	bool status = mpv->start(index, window, video_url, profile, vo, hwdec, gpu_api, gpu_context, log_level, log_path);
	if (status) {
		QString path = QString::fromStdString(video_url);
		if (QFile(path).exists()) {
			std::thread read_file_thread(
				[path, mpv]() {
					QFile stream(path);
					if (stream.open(QIODevice::ReadOnly)) {
						while (true) {
							QByteArray buf = stream.read(32768);
							if (buf.isEmpty()) {
								return;
							}

							mpv->write((const uint8_t *)buf.constData(), (uint32_t)buf.size());

							std::this_thread::sleep_for(std::chrono::milliseconds(33));
						}
					}
				}
			);
			read_file_thread.detach();
		}
	}

	return status;
}


bool MpvManager::start_players(std::map<int, QWidget *> &containers, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	setlocale(LC_NUMERIC, "C");

	if (containers.empty()) {
		return false;
	}

	for (auto iter = containers.begin(); iter != containers.end(); iter++) {
		int index = iter->first;
		QWidget *w = iter->second;
		if (!create_mpv_player(m_buffer_size, index, w, m_index_to_mpv_wrapper, video_url, profile, vo, hwdec, gpu_api, gpu_context, log_level, log_path)) {
			stop_players();
			return false;
		}
	}

	return true;
}


void MpvManager::stop_players()
{
	for (auto iter = m_index_to_mpv_wrapper.begin(); iter != m_index_to_mpv_wrapper.end(); iter++) {
		if (iter->second != nullptr) {
			delete iter->second;
			iter->second = nullptr;
		}
	}

	m_index_to_mpv_wrapper.clear();
}