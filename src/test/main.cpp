// project
#include "mpv_wrapper.hpp"
#include "window_wrapper.hpp"

// fmt
#include <fmt/format.h>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

// cli11
#include <CLI/CLI.hpp>

// qt
#include <QtCore/QString>
#include <QtWidgets/QApplication>




class CommandArguments {
public:
    CommandArguments()
        : log_path("test_qt_libmpv.log")
        , log_level((int)spdlog::level::info)
        , ways(1)
        , mix_cpu_gpu_use(false)
        , profile("low-latency")
        , vo("")
        , hwdec("auto")
        , gpu_api("")
        , gpu_context("")
        , mpv_log_level("v")
        , mpv_log_path("mpv.log")
        , window_left_pos(0)
        , window_top_pos(0)
        , window_width(800)
        , window_height(480)
    {
    }

    void add_options(CLI::App& app)
    {
        app.add_option("--log_path", log_path, fmt::format("log path (default {})", log_path));
        app.add_option("--log_level", log_level, "log level (default spdlog::level::info)");
        app.add_option("--mix_cpu_gpu_use", mix_cpu_gpu_use, fmt::format("mix_cpu_gpu_use (default {})", mix_cpu_gpu_use));
        app.add_option("--ways", ways, fmt::format("ways (default {})", ways));
        app.add_option("--video_url", video_url, "video file path or stream url");
        app.add_option("--profile", profile, fmt::format("mpv profile (default {})", profile));
        app.add_option("--vo", vo, "mpv vo");
        app.add_option("--hwdec", hwdec, fmt::format("mpv hwdec (default {})", hwdec));
        app.add_option("--gpu_api", gpu_api, "mpv gpu-api");
        app.add_option("--gpu_context", gpu_context, "mpv gpu-context");
        app.add_option("--mpv_log_level", mpv_log_level, "mpv log level (default verbose)");
        app.add_option("--mpv_log_path", mpv_log_path, "mpv log path (default mpv.*.log)");
        app.add_option("--window_left_pos", window_left_pos, fmt::format("window left position (default {})", window_left_pos));
        app.add_option("--window_top_pos", window_top_pos, fmt::format("window left position (default {})", window_top_pos));
        app.add_option("--window_width", window_width, fmt::format("window width (default {})", window_width));
        app.add_option("--window_height", window_height, fmt::format("window height (default {})", window_height));
    }

    std::string log_path;
    int log_level;
    int ways;
    bool mix_cpu_gpu_use;
    std::string video_url;
    std::string profile;
    std::string vo;
    std::string hwdec;
    std::string gpu_api;
    std::string gpu_context;
    std::string mpv_log_level;
    std::string mpv_log_path;
    int window_left_pos;
    int window_top_pos;
    int window_width;
    int window_height;
};


int main(int argc, char** argv) {
    // parse cli
    CLI::App app("test_qt_libmpv");
    CommandArguments args;
    args.add_options(app);
    CLI11_PARSE(app, argc, argv);

    // init log
    auto file_logger = spdlog::basic_logger_mt("test_qt_libmpv", args.log_path);
    spdlog::set_default_logger(file_logger);
    spdlog::set_level((spdlog::level::level_enum)args.log_level);
    spdlog::flush_on((spdlog::level::level_enum)args.log_level);

    if (args.video_url.empty()) {
        SPDLOG_ERROR("empty video_url not allowed");
        return -1;
    }

    QApplication qt_app(argc, argv);
    qt_app.setApplicationName("test_qt_libmpv");

    WindowWrapper w;
    w.setWindowTitle(QString("test qt libmpv (%1)").arg(QString::fromStdString(args.video_url)));
    w.setGeometry(args.window_left_pos, args.window_top_pos, args.window_width, args.window_height);
    w.show();

    if (!w.create_players(args.ways, args.mix_cpu_gpu_use, args.video_url, args.profile, args.vo, args.hwdec, args.gpu_api, args.gpu_context, args.mpv_log_level, args.mpv_log_path)) {
        SPDLOG_ERROR("create_players error");
        return -2;
    }


    return qt_app.exec();
}
