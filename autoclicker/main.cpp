#include <opencv2/opencv.hpp>
#include <iostream>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

class ScreenshotTaker {
public:
    ScreenshotTaker(HWND targetWindow, int captureWidth, int captureHeight, int startX, int startY)
        : hdc(GetDC(targetWindow)),
        hbitmap(CreateCompatibleBitmap(hdc, captureWidth, captureHeight)),
        memdc(CreateCompatibleDC(hdc)),
        oldbmp(SelectObject(memdc, hbitmap)),
        mat(captureHeight, captureWidth, CV_8UC4),
        bi{ sizeof(bi), captureWidth, -captureHeight, 1, 32, BI_RGB },
        startX(startX),
        startY(startY),
        target(targetWindow)
    {}

    ~ScreenshotTaker() {
        SelectObject(memdc, oldbmp);
        DeleteDC(memdc);
        DeleteObject(hbitmap);
        ReleaseDC(target, hdc);
    }

    cv::Mat takeScreenshotPart() {
        BitBlt(memdc, 0, 0, bi.biWidth, -bi.biHeight, hdc, startX, startY, SRCCOPY);
        GetDIBits(hdc, hbitmap, 0, -bi.biHeight, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        return mat;
    }

private:
    HDC hdc;
    HBITMAP hbitmap;
    HDC memdc;
    HGDIOBJ oldbmp;
    cv::Mat mat;
    BITMAPINFOHEADER bi;
    int startX;
    int startY;
    HWND target;
};

struct Config {
    std::vector<std::pair<cv::Scalar, cv::Scalar>> goodColors; // List of good color ranges
    cv::Scalar badLow;
    cv::Scalar badHigh;
    std::string windowName;
    int threadsNum;
    int leftGap;
    int topGapBase;
    double topBarScreenRatio;
    int runtime;
    int maxDistance;
    int minClickDistance;
    long maxClickedTime;
    int targetFps;
    double minRectArea;
};

Config parseArgs(int argc, char* argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print usage")
        ("window", po::value<std::string>()->default_value("Moving Circles"), "Target window name")
        ("threads", po::value<int>()->default_value(10), "Number of threads")
        ("runtime", po::value<int>()->default_value(10), "Runtime in seconds")
        ("max-distance", po::value<int>()->default_value(180), "Maximum distance for collision detection")
        ("min-click-distance", po::value<int>()->default_value(8), "Minimum distance between clicks")
        ("max-clicked-time", po::value<long>()->default_value(200), "Maximum time to consider a click valid (ms)")
        ("target-fps", po::value<int>()->default_value(120), "Target FPS")
        ("min-rect-area", po::value<double>()->default_value(900), "Minimum rectangle area to consider")
        ("good-colors", po::value<std::vector<std::string>>()->multitoken(), "List of good color ranges in format 'low_h,low_s,low_v,high_h,high_s,high_v'")
        ("bad-color", po::value<std::string>(), "Bad color range in format 'low_h,low_s,low_v,high_h,high_s,high_v'");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cout << desc << std::endl;
        exit(1);
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(0);
    }

    Config config;
    config.windowName = vm["window"].as<std::string>();
    config.threadsNum = vm["threads"].as<int>();
    config.runtime = vm["runtime"].as<int>();
    config.maxDistance = vm["max-distance"].as<int>();
    config.minClickDistance = vm["min-click-distance"].as<int>();
    config.maxClickedTime = vm["max-clicked-time"].as<long>();
    config.targetFps = vm["target-fps"].as<int>();
    config.minRectArea = vm["min-rect-area"].as<double>();

    // Parse good colors
    if (vm.count("good-colors")) {
        auto colorStrings = vm["good-colors"].as<std::vector<std::string>>();
        for (const auto& colorString : colorStrings) {
            std::vector<int> values;
            std::stringstream ss(colorString);
            std::string value;
            while (std::getline(ss, value, ',')) {
                values.push_back(std::stoi(value));
            }
            if (values.size() == 6) {
                config.goodColors.emplace_back(
                    cv::Scalar(values[0], values[1], values[2]),
                    cv::Scalar(values[3], values[4], values[5])
                );
            }
            else {
                std::cerr << "Invalid good color format: " << colorString << std::endl;
                exit(1);
            }
        }
    }
    else {
        std::cerr << "No good colors specified" << std::endl;
        exit(1);
    }

    // Parse bad color
    if (vm.count("bad-color")) {
        std::string badColorString = vm["bad-color"].as<std::string>();
        std::vector<int> values;
        std::stringstream ss(badColorString);
        std::string value;
        while (std::getline(ss, value, ',')) {
            values.push_back(std::stoi(value));
        }
        if (values.size() == 6) {
            config.badLow = cv::Scalar(values[0], values[1], values[2]);
            config.badHigh = cv::Scalar(values[3], values[4], values[5]);
        }
        else {
            std::cerr << "Invalid bad color format: " << badColorString << std::endl;
            exit(1);
        }
    }
    else {
        std::cerr << "No bad color specified" << std::endl;
        exit(1);
    }

    // Set default values for other parameters
    config.leftGap = 0;
    config.topGapBase = 32;
    config.topBarScreenRatio = 0.08;

    return config;
}

void ClickMouse(int x, int y, INPUT& input, const POINT& zeroWindow) {
    input.mi.dx = (x + zeroWindow.x) * 65535 / GetSystemMetrics(SM_CXSCREEN);
    input.mi.dy = (y + zeroWindow.y) * 65535 / GetSystemMetrics(SM_CYSCREEN);
    SendInput(1, &input, sizeof(INPUT));
}

bool isCollideRect(const cv::Point& rectCenter, const std::vector<cv::Rect>& others, int maxDistance, double minRectArea) {
    for (const auto& other : others) {
        cv::Point otherCenter(other.x + other.width / 2, other.y + other.height / 2);
        double distance = cv::norm(rectCenter - otherCenter);
        if (distance < maxDistance && other.area() > minRectArea) {
            return true;
        }
    }
    return false;
}

void handleWindowPart(HWND targetWindow, int partNumber, const Config& config, std::atomic<int>& totalClicks) {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;

    RECT windowRect;
    GetClientRect(targetWindow, &windowRect);

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    const int topGap = config.topGapBase + (height * config.topBarScreenRatio);
    int divisionWidth = config.threadsNum == 1 ? width - config.leftGap : (width - config.leftGap) / 2;
    int divisionHeight = config.threadsNum == 1 ? height - topGap : (height - topGap) / (config.threadsNum / 2);

    int row = (partNumber - 1) / 2;
    int col = 1 - (partNumber % 2);

    int startX = (col * divisionWidth) + config.leftGap;
    int startY = (row * divisionHeight) + topGap;

    ScreenshotTaker screenshotTaker(targetWindow, divisionWidth, divisionHeight, startX, startY);

    cv::Mat target, goodMask, badMask, resultMask;
    std::vector<cv::Rect> badRects;
    std::vector<std::vector<cv::Point>> badContours, contours;

    struct ClickedRect {
        cv::Point center;
        TimePoint clickedTime;
    };
    std::vector<ClickedRect> clickedRects;

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;

    POINT zeroWindow = { 0, 0 };
    ClientToScreen(targetWindow, &zeroWindow);

    TimePoint startTime = Clock::now();
    TimePoint lastFrameTime = startTime;
    const Duration frameDuration(1000 / config.targetFps);

    int localClicks = 0;

    while (std::chrono::duration_cast<Duration>(Clock::now() - startTime).count() / 1000.0 <= config.runtime) {
        target = screenshotTaker.takeScreenshotPart();
        cv::cvtColor(target, target, cv::COLOR_BGR2HSV);

        cv::inRange(target, config.badLow, config.badHigh, badMask);

        resultMask = cv::Mat::zeros(target.size(), CV_8UC1);
        for (const auto& goodColor : config.goodColors) {
            cv::inRange(target, goodColor.first, goodColor.second, goodMask);
            cv::bitwise_or(resultMask, goodMask, resultMask);
        }

        cv::findContours(badMask, badContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
        for (const auto& contour : badContours) {
            badRects.emplace_back(cv::boundingRect(contour));
        }

        cv::findContours(resultMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

        for (const auto& contour : contours) {
            cv::Rect boundRect = cv::boundingRect(contour);
            cv::Point rectCenter(boundRect.x + boundRect.width / 2, boundRect.y + boundRect.height / 2);

            if (boundRect.area() > config.minRectArea && !isCollideRect(rectCenter, badRects, config.maxDistance, config.minRectArea)) {
                bool isClicked = false;
                for (const ClickedRect& clickedRect : clickedRects) {
                    double clickDistance = cv::norm(rectCenter - clickedRect.center);
                    if (clickDistance < config.minClickDistance && clickDistance > 3) {
                        isClicked = true;
                        break;
                    }
                }

                if (!isClicked) {
                    ClickMouse(rectCenter.x + startX, rectCenter.y + startY, input, zeroWindow);
                    clickedRects.emplace_back(ClickedRect{ rectCenter, Clock::now() });
                    localClicks++;
                }
            }
        }

        TimePoint currentTime = Clock::now();
        clickedRects.erase(
            std::remove_if(clickedRects.begin(), clickedRects.end(),
                [&](const ClickedRect& rect) {
                    return std::chrono::duration_cast<Duration>(currentTime - rect.clickedTime).count() > config.maxClickedTime;
                }
            ),
            clickedRects.end()
        );

        badContours.clear();
        badRects.clear();
        contours.clear();

        Duration elapsedTime = std::chrono::duration_cast<Duration>(currentTime - lastFrameTime);
        Duration sleepDuration = frameDuration - elapsedTime;
        if (sleepDuration > Duration::zero()) {
            std::this_thread::sleep_for(sleepDuration);
        }
        lastFrameTime = currentTime;
    }

    totalClicks += localClicks;
}

int main(int argc, char* argv[]) {
    Config config = parseArgs(argc, argv);

    HWND targetWindow = FindWindowA(nullptr, config.windowName.c_str());
    if (targetWindow == nullptr) {
        std::cerr << "Window not found." << std::endl;
        return 1;
    }

    std::vector<std::thread> threads;
    std::atomic<int> totalClicks(0);

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 1; i <= config.threadsNum; i++) {
        threads.emplace_back(handleWindowPart, targetWindow, i, std::ref(config), std::ref(totalClicks));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Total runtime: " << duration.count() / 1000.0 << " seconds" << std::endl;
    std::cout << "Total clicks: " << totalClicks << std::endl;
    std::cout << "Average clicks per second: " << totalClicks / (duration.count() / 1000.0) << std::endl;

    return 0;
}