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
#include <sstream>

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
	// Note that your custom callbacks must not invoke libmpv APIs as that would cause a deadlock
	return MPV_ERROR_UNSUPPORTED;
}


int64_t seek_fn(void *cookie, int64_t offset)
{
	// Note that your custom callbacks must not invoke libmpv APIs as that would cause a deadlock
	return MPV_ERROR_UNSUPPORTED;
}


int64_t read_fn(void *cookie, char *buf, uint64_t nbytes)
{
	// Note that your custom callbacks must not invoke libmpv APIs as that would cause a deadlock
	auto thiz = (MpvWrapper *)cookie;
	return thiz->read(buf, nbytes);
}



void close_fn(void *cookie)
{
	// Note that your custom callbacks must not invoke libmpv APIs as that would cause a deadlock
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


void event_fn(void *user_data)
{
	auto thiz = (MpvWrapper *)user_data;
	thiz->pool_events();
}


MpvWrapper::MpvWrapper(uint32_t buffer_size)
	: m_stopping(false)
	, m_is_restarting(false)
	, m_mpv_context(nullptr)
	, m_container_wid(0)
	, m_buffer_size(buffer_size)
	, m_mix_cpu_gpu_use(false)
	, m_input_size_2s(0)
	, m_estimated_bitrate(0)
	, m_min_bitrate(0)
	, m_width(0)
	, m_height(0)
{
}


MpvWrapper::~MpvWrapper()
{
	stop();
}


bool MpvWrapper::start(
	int64_t container_wid, bool mix_cpu_gpu_use, std::string video_url,
	std::string profile, std::string vo, std::string hwdec,
	std::string gpu_api, std::string gpu_context, std::string log_level
)
{
	// record options
	m_mix_cpu_gpu_use = mix_cpu_gpu_use;
	m_video_url = video_url;
	m_profile = profile;
	m_vo = vo;
	m_hwdec = hwdec;
	m_gpu_api = gpu_api;
	m_gpu_context = gpu_context;
	m_log_level = log_level;

	// auto-incrementing index
	static std::atomic<uint16_t> index = 0;
	uint16_t i = index.load(std::memory_order_acquire);
	if (0 == i % 2) {
		if (mix_cpu_gpu_use) {
			hwdec = "auto";  // even index: use gpu
		}
	}
	else {
		if (mix_cpu_gpu_use) {
			hwdec = "";  // odd index: use cpu
		}
	}
	index.store(i + 1, std::memory_order_relaxed);

	m_width = 0;
	m_height = 0;
	m_estimated_speed = 1.0;

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

		if (!gpu_api.empty() && gpu_api != "auto") {
			if (!set_option("gpu-api", gpu_api)) {
				break;
			}
		}

		if (!gpu_context.empty() && gpu_context != "auto") {
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

		if (!initialize_handle()) {
			break;
		}

		m_spsc.reset(m_buffer_size);
		if (m_spsc.is_buffer_null()) {
			break;
		}

		register_event_callback();

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
		m_is_restarting.store(false);

		m_last_bitrate_update_time = std::chrono::steady_clock::now();

		m_container_wid = container_wid;
		set_container_window_visiable(true);

		return true;
	} while (false);

	stop();

	return false;
}


void MpvWrapper::stop()
{
	m_stopping = true;

	m_spsc.stopping();

	if (m_mpv_context != nullptr) {
		mpv_terminate_destroy(m_mpv_context);
	}
	m_mpv_context = nullptr;

	m_width = 0;
	m_height = 0;

	if (m_is_restarting) {
		return;
	}

	set_container_window_visiable(false);
	m_container_wid = 0;

	m_mix_cpu_gpu_use = false;
	m_video_url.clear();
	m_profile.clear();
	m_vo.clear();
	m_hwdec.clear();
	m_gpu_api.clear();
	m_gpu_context.clear();
	m_log_level.clear();
}


void MpvWrapper::stopping()
{
	m_stopping = true;

	m_spsc.stopping();
}


bool MpvWrapper::is_buffer_null()
{
	return m_spsc.is_buffer_null();
}


bool MpvWrapper::write(const uint8_t *buf, uint32_t length)
{
	if (m_stopping && !m_is_restarting) {
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

	// estimate bitrate
	estimate_bitrate(length);

	// ajust speed
	reduce_latency();

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
	if (m_width > 0 && m_height > 0) {
		width = m_width;
		height = height;
		return true;
	}

	bool r1 = get_property("width", width);
	bool r2 = get_property("height", height);
	return r1 && r2;
}


double MpvWrapper::get_speed()
{
	double r;
	get_property("speed", r);
	return r;
}


bool MpvWrapper::set_speed(double v)
{
	return set_property("speed", v);
}


int MpvWrapper::get_bitrate()
{
	if (m_estimated_bitrate > 0) {
		return m_estimated_bitrate;
	}

	int64_t v = 0;
	bool r = get_property("video-bitrate", v);
	return (int)v;
}


int MpvWrapper::get_fps()
{
	int64_t v = 0;
	bool r = get_property("estimated-vf-fps", v);
	return v >= 0 ? (int)v : 25;
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


bool MpvWrapper::create_handle()
{
	m_mpv_context = mpv_create();
	if (nullptr == m_mpv_context) {
		SPDLOG_ERROR("mpv_create() error\n");
		return false;
	}
	return true;
}


bool MpvWrapper::initialize_handle()
{
	int code = mpv_initialize(m_mpv_context);
	if (code < 0) {
		SPDLOG_ERROR("mpv_initialize({}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::register_stream_callbacks()
{
	int code = mpv_stream_cb_add_ro(m_mpv_context, "myprotocol", (void *)this, open_fn);
	if (code < 0) {
		SPDLOG_ERROR("mpv_stream_cb_add_ro({}, myprotocol, {}, open_fn) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), code, mpv_error_string(code));
		return false;
	}
	return true;
}


void MpvWrapper::register_event_callback()
{
	mpv_set_wakeup_callback(m_mpv_context, event_fn, this);
}


void MpvWrapper::pool_events()
{
	std::thread thread(
		[this]() {
			while (!m_stopping) {
				mpv_event *event = mpv_wait_event(m_mpv_context, 0);

				if (event->event_id == MPV_EVENT_NONE) {
					break;
				}
				else if (event->event_id == MPV_EVENT_LOG_MESSAGE && event->data != nullptr) {
					// log message
					struct mpv_event_log_message *msg = (struct mpv_event_log_message *)event->data;
					SPDLOG_INFO("[*MPV*] [{}] [{}] {}", msg->prefix, msg->level, msg->text);

					// restart when the codec was changed
					if (restart_codec_changed(msg)) {
					}
					else {
						// get video width and height
						if (get_decoded_resolution(msg)) {
						}
					}
				}
			}

			std::ostringstream oss;
			oss << std::this_thread::get_id() << std::endl;
			SPDLOG_INFO("[*MPV*] [mpv_wrapper] pool_events end, thread: {}", oss.str());
		}
	);

	std::ostringstream oss;
	oss << thread.get_id() << std::endl;
	SPDLOG_INFO("[*MPV*] [mpv_wrapper] pool_events begin, thread: {}", oss.str());

	thread.detach();
}


bool MpvWrapper::set_log_level(std::string min_level)
{
	int code = mpv_request_log_messages(m_mpv_context, min_level.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_request_log_messages({}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), min_level, code, mpv_error_string(code));
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
		SPDLOG_ERROR("mpv_command({}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), fmt::join(args, ", "), code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, bool value)
{
	int v = value ? 1 : 0;
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_FLAG, &v);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_flag({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, int64_t value)
{
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_int64({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, double value)
{
	int code = mpv_set_option(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_double({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_option(std::string key, std::string value)
{
	int code = mpv_set_option_string(m_mpv_context, key.c_str(), value.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_option_string({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::get_property(std::string key, bool &value)
{
	int v;
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_FLAG, &v);
	if (code < 0) {
		SPDLOG_ERROR("get_property_flag({}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	value = 1 == v ? true : false;
	return true;
}


bool MpvWrapper::get_property(std::string key, int64_t &value)
{
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("get_property_int64({}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::get_property(std::string key, double &value)
{
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("get_property_double({}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::get_property(std::string key, std::string &value)
{
	int code = mpv_get_property(m_mpv_context, key.c_str(), MPV_FORMAT_STRING, &value);
	if (code < 0) {
		SPDLOG_ERROR("get_property_string({}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, bool value)
{
	int v = value ? 1 : 0;
	int code = mpv_set_property(m_mpv_context, key.c_str(), MPV_FORMAT_FLAG, &v);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_flag({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, int64_t value)
{
	int code = mpv_set_property(m_mpv_context, key.c_str(), MPV_FORMAT_INT64, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_int64({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, double value)
{
	int code = mpv_set_property(m_mpv_context, key.c_str(), MPV_FORMAT_DOUBLE, &value);
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_double({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


bool MpvWrapper::set_property(std::string key, std::string value)
{
	int code = mpv_set_property_string(m_mpv_context, key.c_str(), value.c_str());
	if (code < 0) {
		SPDLOG_ERROR("mpv_set_property_string({}, {}, {}) error, code: {}, msg: {}\n", fmt::ptr(m_mpv_context), key, value, code, mpv_error_string(code));
		return false;
	}
	return true;
}


void MpvWrapper::set_container_window_visiable(bool state)
{
#ifdef _WIN32
	ShowWindow((HWND)m_container_wid, state ? SW_SHOW : SW_HIDE);
#endif // _WIN32

#ifdef __linux__
	Display *display = X11Info::display();
	if (state) {
		XMapWindow(display, m_container_wid);
	}
	else {
		XUnmapWindow(display, m_container_wid);
	}
	XFlush(display);
#endif
}


bool MpvWrapper::restart_codec_changed(struct mpv_event_log_message *msg)
{
	if (
		!m_is_restarting
		&& msg->text != nullptr
		&& msg->log_level >= MPV_LOG_LEVEL_WARN
		&& strstr(msg->prefix, "ffmpeg/video") != nullptr
		&& strstr(msg->text, "data partitioning is not implemented") != nullptr
		) {
		m_is_restarting.store(true);
		stop();
		start(m_container_wid, m_mix_cpu_gpu_use, m_video_url, m_profile, m_vo, m_hwdec, m_gpu_api, m_gpu_context, m_log_level);
		m_is_restarting.store(false);

		return true;
	}

	return false;
}


bool MpvWrapper::get_decoded_resolution(struct mpv_event_log_message *msg)
{
	// msg->text = Decoder format: 1920x1080 [0:1] d3d11[nv12] auto/auto/auto/auto/auto CL=mpeg2/4/h264 crop=1920x1080+0+0
	if (0 == m_width && 0 == m_height && msg->text != nullptr && strstr(msg->text, "Decoder format:") != 0) {
		char *ptr = (char *)strchr(msg->text, 'x');
		*ptr = '\0';
		m_width = atoi(strrchr(msg->text, ':') + 2);
		m_height = atoi(ptr + 1);

		if (m_width * m_height >= 3840 * 2160) {
			m_min_bitrate = 1600 * 1024 / 4;
		}
		else if (m_width * m_height >= 2560 * 1440) {
			m_min_bitrate = 800 * 1024 / 4;
		}
		else if (m_width * m_height >= 1920 * 1080) {
			m_min_bitrate = 400 * 1024 / 4;
		}
		else if (m_width * m_height >= 1280 * 720) {
			m_min_bitrate = 200 * 1024 / 4;
		}
		else {
			m_min_bitrate = 100 * 1024 / 4;
		}

		return true;
	}

	return false;
}


void MpvWrapper::estimate_bitrate(uint32_t length)
{
	m_input_size_2s += length;
	auto now = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_bitrate_update_time).count();
	if (ms > 2000) {
		m_estimated_speed = std::ceil(get_fps() / 25.0);
		if (m_estimated_speed < 1.0) {
			m_estimated_speed = 1.0;
		}
		m_estimated_bitrate = (uint64_t)std::round(m_input_size_2s * 1000.0 / ms / m_estimated_speed);
		m_input_size_2s = 0;
		m_last_bitrate_update_time = now;
	}
}


void MpvWrapper::reduce_latency()
{
	// refer: https://www.infoq.cn/article/s2zh7b2p0v1xtzvxyavv

	int bitrate = get_bitrate();
	if (bitrate <= 0) {
		return;
	}

	double lag_seconds = (double)m_spsc.buffer_size() / bitrate;
	if (lag_seconds < 6) {
		return;
	}

	double speed = 1.0;
	bool speeding_up = false;
	if (lag_seconds >= 12 && bitrate >= m_min_bitrate) {
		speed = 2.0;
		speeding_up = true;
	}
	else if (lag_seconds >= 10 && bitrate >= m_min_bitrate) {
		speed = 1.8;
		speeding_up = true;
	}
	else if (lag_seconds >= 8 && bitrate >= m_min_bitrate) {
		speed = 1.6;
		speeding_up = true;
	}
	else if (lag_seconds >= 6 && bitrate >= m_min_bitrate) {
		speed = 1.4;
		speeding_up = true;
	}

	if (speeding_up) {
		if (m_estimated_speed < speed && speed != get_speed()) {
			set_speed(speed);
		}
	}
	else {
		if (m_estimated_speed != get_speed()) {
			set_speed(m_estimated_speed);
		}
	}
}

