// self
#include "mpv_wrapper.hpp"

// c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <locale.h>

// c++
#include <chrono>
#include <fstream>

// libmpv
#include <mpv/client.h>
#include <mpv/stream_cb.h>

// spdlog
#include <spdlog/spdlog.h>

// windows
#ifdef _WIN32
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif // _WIN32

// linux
#ifdef __linux__
#include <X11/Xlib.h>
#endif



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
	//call mpv_terminate_destroy/mpv_destroy from close_fn will block forever
	//auto thiz = (MpvWrapper *)cookie;
	//thiz->stop();
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
	, m_container_wid(0)
	, m_buffer_size(buffer_size)
{
}


MpvWrapper::~MpvWrapper()
{
	stop();
}


bool MpvWrapper::start(int index, int64_t container_wid, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context, std::string log_level, std::string log_path)
{
	do {
		setlocale(LC_NUMERIC, "C");

		if (!create_handle()) {
			break;
		}

		if (!set_option("wid", container_wid)) {
			break;
		}

		if (!profile.empty()) {
			if (!set_option("profile", profile)) {
				break;
			}
		}

		if (!vo.empty()) {
			if (!set_option("vo", vo)) {
				break;
			}
		}

		if (!hwdec.empty()) {
			if (!set_option("hwdec", hwdec)) {
				break;
			}
		}

		if (!gpu_api.empty()) {
			if (!set_option("gpu-api", gpu_api)) {
				break;
			}
		}

		if (!gpu_context.empty()) {
			if (!set_option("gpu-context", gpu_context)) {
				break;
			}
		}

		if (!set_option("keepaspect", std::string("no"))) {
			break;
		}

		if (!log_level.empty()) {
			if (!set_log_level(log_level)) {
				break;
			}
		}

		if (!log_path.empty()) {
			if (!set_option("log-file", log_path.replace(log_path.find(".log"), 3, "") + std::to_string(index) + ".log")) {
				break;
			}
		}

		if (!initialize_handle()) {
			break;
		}

		m_spsc.reset(m_buffer_size);
		if (m_spsc.is_buffer_null()) {
			break;
		}

		std::ifstream file(video_url);
		if (!video_url.empty() && !file.good()) {
			// read from network
			if (!call_command({ "loadfile", video_url })) {
				break;
			}
		}
		else {
			// read from file
			if (!register_stream_callbacks()) {
				break;
			}

			if (!call_command({ "loadfile", "myprotocol://fake" })) {
				break;
			}
		}

		m_stopping = false;

		m_container_wid = container_wid;
#ifdef _WIN32
		ShowWindow((HWND)m_container_wid, SW_SHOW);
#endif // _WIN32
#ifdef __linux__
		Display *display = QX11Info::display();
		XMapWindow(display, container_wid);
		XFlush(display);
#endif

		return true;
	} while (false);

	stop();

	return false;
}


void MpvWrapper::stop()
{
	m_stopping = true;

	m_spsc.stopping();

#ifdef _WIN32
	ShowWindow((HWND)m_container_wid, SW_HIDE);
#endif // _WIN32
#ifdef __linux__
	Display *display = QX11Info::display();
	XUnmapWindow(display, m_container_wid);
	XFlush(display);
#endif
	m_container_wid = 0;

	if (m_mpv_context != nullptr) {
		mpv_terminate_destroy(m_mpv_context);
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


bool MpvWrapper::register_stream_callbacks()
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
		SPDLOG_ERROR("mpv_set_option_flag({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, int64_t value)
{
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_int64({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, double value)
{
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_double({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, std::string value)
{
	int code = mpv_set_option_string(m_mpv_context, key.c_str(), value.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_string({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::get_property(std::string key, bool &value)
{
	int v;
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_FLAG, &v);
	if (code < 0) {
		SPDLOG_ERROR("get_property_flag({}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	value = 1 == v ? true : false;
	return true;
}


bool MpvWrapper::get_property(std::string key, int64_t &value)
{
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("get_property_int64({}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::get_property(std::string key, double &value)
{
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("get_property_double({}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::get_property(std::string key, std::string &value)
{
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_STRING, &value);
	if (code < 0) {
		SPDLOG_ERROR("get_property_string({}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, bool value)
{
	int v = value ? 1 : 0;
	int code = mpv_set_property(m_mpv_context, key.c_str(), MPV_FORMAT_FLAG, &v);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_flag({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, int64_t value)
{
	int code = mpv_set_property(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_int64({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, double value)
{
	int code = mpv_set_property(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_double({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, std::string value)
{
	int code = mpv_set_property_string(m_mpv_context, key.c_str(), value.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_string({}, {}, {}) error, code: {}, msg: {}", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
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
	while (!m_stopping && offset < length) {
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


bool MpvWrapper::play()
{
	return call_command({ "play" });
}


bool MpvWrapper::pause()
{
	return call_command({ "pause" });
}


bool MpvWrapper::step()
{
	return call_command({ "frame-step" });
}


bool MpvWrapper::get_mute_state()
{
	bool r;
	get_property("mute", r);
	return r;
}


void MpvWrapper::set_mute_state(const bool state)
{
	set_property("mute", state);
}


int MpvWrapper::get_volume()
{
	double v = 0.0;
	get_property("volume", v);
	return (int)v;
}


void MpvWrapper::set_volume(const int v)
{
	set_property("volume", (double)v);
}


bool MpvWrapper::get_resolution(int64_t &width, int64_t &height)
{
	bool r1 = get_property("width", width);
	bool r2 = get_property("height", height);
	return r1 && r2;
}


double MpvWrapper::get_speed()
{
	double r;
	return get_property("speed", r);
	return r;
}


bool MpvWrapper::set_speed(double v)
{
	return set_property("speed", v);
}


bool MpvWrapper::screenshot(std::string &path)
{
#ifdef _WIN32
	char *temp_dir = getenv("TEMP");
#else
	char *temp_dir = getenv("TMPDIR");
#endif // _WIN32

	auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	path = fmt::format("{}/{}.jpeg", temp_dir, timestamp_ms);

	if (call_command({ "screenshot-to-file", path })) {
		int timeout_ms = 3000;
		int sleep_ms = 100;
		while (timeout_ms > 0) {
			// open file
			std::ifstream file(path, std::ios::binary);
			if (!file.is_open()) {
				timeout_ms -= sleep_ms;
				std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
				continue;
			}

			// get file size
			file.seekg(0, std::ios::end);
			std::streampos file_size = file.tellg();
			file.seekg(0, std::ios::beg);
			if (file_size < 1024) {
				file.close();
				timeout_ms -= sleep_ms;
				std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
				continue;
			}

			// close file
			file.close();

			return true;
		}
	}

	return false;
}


