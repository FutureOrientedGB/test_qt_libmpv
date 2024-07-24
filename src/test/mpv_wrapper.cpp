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
	QFile *fp = (QFile *)cookie;
	if (nullptr == fp) {
		return MPV_ERROR_UNSUPPORTED;
	}

	return fp->size();
}


int64_t read_fn(void *cookie, char *buf, uint64_t nbytes)
{
	QFile *fp = (QFile *)cookie;
	if (nullptr == fp) {
		return 0;
	}

	return fp->read(buf, nbytes);
}


int64_t seek_fn(void *cookie, int64_t offset)
{
	QFile *fp = (QFile *)cookie;
	if (nullptr == fp) {
		return MPV_ERROR_UNSUPPORTED;
	}

	if (0 == offset || fp->seek(offset)) {
		return offset;
	}

	return MPV_ERROR_GENERIC;
}


void close_fn(void *cookie)
{
	QFile *fp = (QFile *)cookie;
	if (nullptr == fp) {
		return;
	}

	fp->close();
	delete fp;
	fp = nullptr;
}


int open_fn(void *user_data, char *uri, mpv_stream_cb_info *info)
{
	auto url = (std::string *)user_data;
	QFile *fp = new QFile(QString(url->c_str()));
	delete url;

	if (fp != nullptr && fp->open(QIODevice::ReadOnly)) {
		info->cookie = fp;
		info->size_fn = size_fn;
		info->read_fn = read_fn;
		info->seek_fn = seek_fn;
		info->close_fn = close_fn;

		return 0;
	}

	if (fp != nullptr) {
		delete fp;
	}

	return MPV_ERROR_LOADING_FAILED;
}



MpvWrapper::MpvWrapper()
{
}


MpvWrapper::~MpvWrapper()
{
}


bool MpvWrapper::create(std::map<int, QWidget *> &containers, std::string video_url, std::string profile, std::string vo, std::string hwdec, std::string gpu_api, std::string gpu_context)
{
	setlocale(LC_NUMERIC, "C");

	if (containers.empty()) {
		return false;
	}

	bool has_error = false;
	for (auto iter = containers.begin(); iter != containers.end() && !has_error; iter++) {
		int index = iter->first;
		QWidget *w = iter->second;

		mpv_handle *mpv_context = mpv_create();
		if (!mpv_context) {
			has_error = true;
			SPDLOG_ERROR("mpv_create() error");
			break;
		}

		m_index_to_mpv.insert(std::make_pair(index, mpv_context));

		int code = 0;

		int64_t wid = (int64_t)w->winId();
		code = mpv_set_option(mpv_context, "wid", MPV_FORMAT_INT64, &wid);
		if (code < 0) {
			has_error = true;
			SPDLOG_ERROR("mpv_set_option({}, wid, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), wid, code, mpv_error_string(code));
			break;
		}

		if (!profile.empty()) {
			code = mpv_set_option_string(mpv_context, "profile", profile.c_str());
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_set_option({}, profile, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), profile, code, mpv_error_string(code));
				break;
			}
		}

		if (!vo.empty()) {
			code = mpv_set_option_string(mpv_context, "vo", vo.c_str());
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_set_option({}, vo, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), vo, code, mpv_error_string(code));
				break;
			}
		}

		if (!hwdec.empty()) {
			code = mpv_set_option_string(mpv_context, "hwdec", hwdec.c_str());
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_set_option({}, hwdec, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), hwdec, code, mpv_error_string(code));
				break;
			}
		}

		if (!gpu_api.empty()) {
			code = mpv_set_option_string(mpv_context, "gpu-api", gpu_api.c_str());
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_set_option({}, gpu-api, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), gpu_api, code, mpv_error_string(code));
				break;
			}
		}

		if (!gpu_context.empty()) {
			code = mpv_set_option_string(mpv_context, "gpu-context", gpu_context.c_str());
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_set_option({}, gpu-context, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), gpu_context, code, mpv_error_string(code));
				break;
			}
		}

		code = mpv_initialize(mpv_context);
		if (code < 0) {
			has_error = true;
			SPDLOG_ERROR("mpv_initialize({}) error, code: {}, msg: {}", fmt::ptr(mpv_context), wid, code, mpv_error_string(code));
			break;
		}

		if (!QFile(QString::fromStdString(video_url)).exists()) {
			// read from network
			code = mpv_set_option_string(mpv_context, "stream-open-filename", video_url.c_str());
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_set_option_string({}, stream-open-filename, {}) error, code: {}, msg: {}", fmt::ptr(mpv_context), video_url, code, mpv_error_string(code));
				break;
			}

			code = mpv_command_string(mpv_context, "play");
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_command_string({}, play) error, code: {}, msg: {}", fmt::ptr(mpv_context), code, mpv_error_string(code));
				break;
			}
		}
		else {
			// read from file
			auto url = new std::string(video_url);
			code = mpv_stream_cb_add_ro(mpv_context, "myprotocol", (void *)url, open_fn);
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_stream_cb_add_ro({}, myprotocol, {}, open_fn) error, code: {}, msg: {}", fmt::ptr(mpv_context), video_url, code, mpv_error_string(code));
				break;
			}

			const char *cmd[] = { "loadfile", "myprotocol://fake", NULL };
			code = mpv_command(mpv_context, cmd);
			if (code < 0) {
				has_error = true;
				SPDLOG_ERROR("mpv_command({}, loadfile, myprotocol://fake) error, code: {}, msg: {}", fmt::ptr(mpv_context), code, mpv_error_string(code));
				break;
			}
		}
	}

	if (has_error) {
		destroy();
	}

	return has_error;
}


void MpvWrapper::destroy()
{
	for (auto iter = m_index_to_mpv.begin(); iter != m_index_to_mpv.end(); iter++) {
		mpv_handle *ctx = iter->second;
		if (ctx != nullptr) {
			mpv_terminate_destroy(ctx);
		}
	}

	m_index_to_mpv.clear();
}
