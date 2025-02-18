// project
#include "mpv_wrapper.hpp"
#include "window_wrapper.hpp"

// fmt
#include <fmt/format.h>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>

// cli11
#include <CLI/CLI.hpp>

// qt
#include <QtCore/QString>
#include <QtWidgets/QApplication>




class CommandArguments {
public:
    CommandArguments()
        : log_path("qt-mpv.log")
        , log_level(SPDLOG_LEVEL_INFO)
        , ways(1)
        , gpu_ways(-1)
        , profile("low-latency")
        , vo("")
        , hwdec("auto")
        , gpu_api("")
        , gpu_context("")
        , mpv_log_level("v")
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
        app.add_option("--ways", ways, fmt::format("ways (default {})", ways));
        app.add_option("--gpu_ways", gpu_ways, "ways use gpu decoding, left ways use cpu decoding(default all)");
        app.add_option("--video_url", video_url, "video file path or stream url");
        app.add_option("--profile", profile, fmt::format("mpv profile (default {})", profile));
        app.add_option("--vo", vo, "mpv vo");
        app.add_option("--hwdec", hwdec, fmt::format("mpv hwdec (default {})", hwdec));
        app.add_option("--gpu_api", gpu_api, "mpv gpu-api");
        app.add_option("--gpu_context", gpu_context, "mpv gpu-context");
        app.add_option("--mpv_log_level", mpv_log_level, "mpv log level (default verbose)");
        app.add_option("--window_left_pos", window_left_pos, fmt::format("window left position (default {})", window_left_pos));
        app.add_option("--window_top_pos", window_top_pos, fmt::format("window left position (default {})", window_top_pos));
        app.add_option("--window_width", window_width, fmt::format("window width (default {})", window_width));
        app.add_option("--window_height", window_height, fmt::format("window height (default {})", window_height));
    }

    void print()
    {
        SPDLOG_INFO(
            "\nqt-mpv\n"
            "    --log_path={}\n"
            "    --log_level={}\n"
            "    --ways={}\n"
            "    --gpu_ways={}\n"
            "    --video_url={}\n"
            "    --profile={}\n"
            "    --vo={}\n"
            "    --hwdec={}\n"
            "    --gpu_api={}\n"
            "    --gpu_context={}\n"
            "    --mpv_log_level={}\n"
            "    --window_left_pos={}\n"
            "    --window_top_pos={}\n"
            "    --window_width={}\n"
            "    --window_height={}\n",
            log_path, log_level, ways, gpu_ways, video_url, profile, vo, hwdec, gpu_api,
            gpu_context, mpv_log_level, window_left_pos, window_top_pos, window_width, window_height
        );
    }

    std::string log_path;
    int log_level;
    int ways;
    int gpu_ways;
    std::string video_url;
    std::string profile;
    std::string vo;
    std::string hwdec;
    std::string gpu_api;
    std::string gpu_context;
    std::string mpv_log_level;
    int window_left_pos;
    int window_top_pos;
    int window_width;
    int window_height;
};


int main(int argc, char** argv) {
    // parse cli
    CLI::App app("qt-mpv");
    CommandArguments args;
    args.add_options(app);
    CLI11_PARSE(app, argc, argv);
    if (args.gpu_ways <= 0) {
        args.gpu_ways = args.ways;
    }

    // init log
    auto file_logger = spdlog::rotating_logger_mt("qt-mpv", args.log_path, 10 * 1024 * 1024, 3);
    auto no_eof_formatter = std::make_unique<spdlog::pattern_formatter>("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s L%# P%P T%t] %v", spdlog::pattern_time_type::local, std::string(""));  // disable eol
    file_logger->set_formatter(std::move(no_eof_formatter));
    spdlog::set_default_logger(file_logger);
    spdlog::set_level((spdlog::level::level_enum)args.log_level);
    spdlog::flush_on((spdlog::level::level_enum)args.log_level);

    args.print();
    if (args.video_url.empty()) {
        SPDLOG_ERROR("empty video_url not allowed\n");
        return -1;
    }

    QApplication qt_app(argc, argv);
    qt_app.setApplicationName("qt-mpv");

    WindowWrapper w;
    w.setWindowTitle(QString("qt-mpv %1").arg(QString::fromStdString(args.video_url)));
    w.setGeometry(args.window_left_pos, args.window_top_pos, args.window_width, args.window_height);
    w.show();

    if (!w.create_players(args.ways, args.gpu_ways, args.video_url, args.profile, args.vo, args.hwdec, args.gpu_api, args.gpu_context, args.mpv_log_level)) {
        SPDLOG_ERROR("create_players error\n");
        return -2;
    }

    return qt_app.exec();
}
