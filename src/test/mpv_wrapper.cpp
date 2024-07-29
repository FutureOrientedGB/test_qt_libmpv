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
	: m_stopping(false)
	, m_mpv_context(nullptr)
{
	m_spsc.reset(buffer_size);
}


MpvWrapper::~MpvWrapper()
{
	stop();
}


bool MpvWrapper::start(int index, int64_t container_wid, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	if (!create_handle()) {
		return false;
	}

	if (!set_option("wid", container_wid)) {
		return false;
	}

	if (!profile.empty()) {
		if (!set_option("profile", profile)) {
			return false;
		}
	}

	if (!vo.empty()) {
		if (!set_option("vo", vo)) {
			return false;
		}
	}

	if (!hwdec.empty()) {
		if (!set_option("hwdec", hwdec)) {
			return false;
		}
	}

	if (!gpu_api.empty()) {
		if (!set_option("gpu-api", gpu_api)) {
			return false;
		}
	}

	if (!gpu_context.empty()) {
		if (!set_option("gpu-context", gpu_context)) {
			return false;
		}
	}

	if (!log_level.empty()) {
		if (!set_log_level(log_level)) {
			return false;
		}
	}

	if (!log_path.empty()) {
		if (!set_option("log-file", QString::fromStdString(log_path).replace(".log", QString(".%1.log").arg(index)).toStdString())) {
			return false;
		}
	}

	if (!initialize_handle()) {
		return false;
	}

	if (!QFile(QString::fromStdString(video_url)).exists()) {
		// read from network
		if (!call_command({ "loadfile", video_url })) {
			return false;
		}
	}
	else {
		// read from file
		if (!add_io_read_callbacks()) {
			return false;
		}

		if (!call_command({ "loadfile", "myprotocol://fake" })) {
			return false;
		}
	}

	m_stopping = false;

	return true;
}


void MpvWrapper::stop()
{
	m_stopping = true;

	m_spsc.reset(0);

	if (m_mpv_context != nullptr) {
		mpv_destroy(m_mpv_context);
	}
	m_mpv_context = nullptr;
}


void MpvWrapper::stopping()
{
	m_stopping = true;

	m_spsc.stopping();
}


bool MpvWrapper::create_handle()
{
	m_mpv_context = mpv_create();
	if (nullptr == m_mpv_context) {
		SPDLOG_ERROR("mpv_create() error");
		return false;
	}
	return true;
}


bool MpvWrapper::initialize_handle()
{
	int code = mpv_initialize(m_mpv_context);
	if (code < 0) {
		SPDLOG_ERROR("mpv_initialize({}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::add_io_read_callbacks()
{
	int code = mpv_stream_cb_add_ro(m_mpv_context, "myprotocol", (void *)this, open_fn);
	if (code < 0) {
		SPDLOG_ERROR("mpv_stream_cb_add_ro({}, myprotocol, {}, open_fn) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_log_level(std::string min_level)
{
	int code = mpv_request_log_messages(m_mpv_context, min_level.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_request_log_messages({}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), min_level, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::call_command(std::vector<std::string> args)
{
	// new array of str
	char **cmd = new char *[args.size() + 1];
	for (int i = 0; i < args.size(); i++) {
		char *c = new char[args[i].size() + 1];
		strncpy(c, args[i].c_str(), args[i].size());
		c[args[i].size()] = '\0';

		cmd[i] = c;
	}
	cmd[args.size()] = NULL;

	int code = mpv_command(m_mpv_context, (const char **)cmd);

	// delete array of str
	for (int i = 0; i < args.size(); i++) {
		delete[] cmd[i];
	}
	delete[] cmd;

	if (code < 0) {
		SPDLOG_ERROR("mpv_command({}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), fmt::join(args, ", "), code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, bool value)
{
	int v = value ? 1 : 0;
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_FLAG, &v);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_flag({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, int64_t value)
{
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_int64({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, double value)
{
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_double({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, std::string value)
{
	int code = mpv_set_option_string(m_mpv_context, key.c_str(), value.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_string({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::is_buffer_null()
{
	return m_spsc.is_buffer_null();
}


bool MpvWrapper::write(const uint8_t *buf, uint32_t length)
{
	if (m_stopping) {
		return false;
	}

	uint32_t offset = 0;
	while (offset < length) {
		uint32_t c = m_spsc.put_if_not_full(buf, length);
		offset += c;
		if (0 == c) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	return true;
}


int64_t MpvWrapper::read(char *buf, uint64_t nbytes)
{
	return (int64_t)m_spsc.get_if_not_empty((uint8_t *)buf, (uint32_t)nbytes);;
}


