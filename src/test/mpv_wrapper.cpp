// self
#include "mpv_wrapper.hpp"

// c
#include <stdio.h>
#include <stdint.h>
#include <locale.h>

// c++
#include <chrono>

// libmpv
#include <mpv/client.h>
#include <mpv/stream_cb.h>

// qt
#include <QtCore/QFile>
#include <QtWidgets/QWidget>

// spdlog
#include <spdlog/spdlog.h>


#define READ_INTERVAL_MS 39
#define READ_BUFFER_SIZE 32768
#define STEADY_CLOCK_NOW() std::chrono::steady_clock::now()
#define STEADY_CLOCK_DURATION(begin) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count()



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
	, m_stopping(false)
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
		code = mpv_stream_cb_add_ro(m_mpv_context, "myprotocol", (void *)this, open_fn);
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

	m_stopping = false;

	return true;
}


void MpvWrapper::stop()
{
	m_stopping = true;

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
	while (!m_stopping && offset < length && m_mpv_context != nullptr) {
		offset += m_spsc.put(buf + offset, length - offset);
	}
}


int64_t MpvWrapper::read(char *buf, uint64_t nbytes)
{
	return (int64_t)m_spsc.get((uint8_t *)buf, (uint32_t)nbytes);
}


