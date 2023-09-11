#include <opencv2/opencv.hpp>
#include <iostream>
#include <Windows.h>
#include <thread>
#include <chrono>

#define DEBUG false
#define LIMIT_FPS false

class ScreenshotTaker
{
public:
    ScreenshotTaker(HWND targetWindow, int captureWidth, int captureHeight, int startX, int startY)
        : hdc(GetDC(targetWindow)),
          hbitmap(CreateCompatibleBitmap(hdc, captureWidth, captureHeight)),
          memdc(CreateCompatibleDC(hdc)),
          oldbmp(SelectObject(memdc, hbitmap)),
          mat(captureHeight, captureWidth, CV_8UC4),
          bi{sizeof(bi), captureWidth, -captureHeight, 1, 32, BI_RGB},
          startX(startX),
          startY(startY),
          target(targetWindow)
    {
    }

    ~ScreenshotTaker()
    {
        SelectObject(memdc, oldbmp);
        DeleteDC(memdc);
        DeleteObject(hbitmap);
        ReleaseDC(target, hdc);
    }

    cv::Mat takeScreenshotPart()
    {
        BitBlt(memdc, 0, 0, bi.biWidth, -bi.biHeight, hdc, startX, startY, SRCCOPY);
        GetDIBits(hdc, hbitmap, 0, -bi.biHeight, mat.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
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

const cv::Scalar yellowLow = cv::Scalar(18, 100, 235);
const cv::Scalar yellowHigh = cv::Scalar(22, 115, 245);

const cv::Scalar treasureLow = cv::Scalar(0, 0, 240);
const cv::Scalar treasureHigh = cv::Scalar(179, 25, 255);

const cv::Scalar badLow = cv::Scalar(4, 170, 228);
const cv::Scalar badHigh = cv::Scalar(8, 180, 240);

const LPCSTR WINDOW_NAME = "Moving Circles"; //"BlueStacks App Player";
const int THREADS_NUM = 12;
const int leftGap = 0; // 320;
const int topGap = 0;  // 115; // 143;
const int RUNTIME = 20;
const int MAX_DISTANCE = 180;
const int MIN_CLICK_DISTANCE = 5;
const long MAX_CLICKED_TIME = 500; // MS
const int TARGET_FPS = 120;
const double MIN_RECT_AREA = 30 * 30;

const std::chrono::milliseconds FRAME_DURATION(1000 / TARGET_FPS);
const long DX_C = 65535 / GetSystemMetrics(SM_CXSCREEN);
const long DY_C = 65535 / GetSystemMetrics(SM_CYSCREEN);
struct ClickedRect
{
    cv::Point center;
    std::chrono::time_point<std::chrono::high_resolution_clock> clickedTime;
};
POINT zeroWindow = {0, 0};
std::mutex coutMutex;

void ClickMouse(int x, int y, INPUT &input)
{
    input.mi.dx = (x + zeroWindow.x) * DX_C;
    input.mi.dy = (y + zeroWindow.y) * DY_C;

    SendInput(1, &input, sizeof(INPUT));
}

bool isCollideRect(cv::Point rectCenter, const std::vector<cv::Rect> &others)
{
    for (const auto &other : others)
    {
        cv::Point otherCenter(other.x + other.width / 2, other.y + other.height / 2);
        double distance = cv::norm(rectCenter - otherCenter);

        if (distance < MAX_DISTANCE && other.area() > MIN_RECT_AREA)
        {
            return true;
        }
    }
    return false;
}

void handleWindowPart(HWND targetWindow, int partNumber)
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;

    RECT windowRect;
    cv::Mat yellowMask, treasureMask, badMask, resultMask;
    cv::Mat target;
    int centerX, centerY;

    std::vector<cv::Rect> badRects;
    std::vector<std::vector<cv::Point>> badContours;
    std::vector<std::vector<cv::Point>> contours;

    cv::Rect boundRect;

    std::vector<ClickedRect> clickedRects;

    TimePoint lastFrameTime = Clock::now();
    TimePoint currentTime, endTime;

    int frames = 0;
    double time_elapsed = 0;

    bool isClicked;
    double clickDistance;

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;

    GetClientRect(targetWindow, &windowRect);

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    // Calculate dimensions for each division
    int divisionWidth = (width - leftGap) / 2;
    int divisionHeight = (height - topGap) / (THREADS_NUM / 2);

    int row = (partNumber - 1) / 2;
    int col = 1 - (partNumber % 2);

    int startX = (col * divisionWidth) + leftGap;
    int startY = (row * divisionHeight) + topGap;

    ScreenshotTaker screenshotTaker(targetWindow, divisionWidth, divisionHeight, startX, startY);

    TimePoint startTime = Clock::now();

    while (time_elapsed <= RUNTIME)
    {
        target = screenshotTaker.takeScreenshotPart();

        cv::cvtColor(target, target, cv::COLOR_BGR2HSV);

        cv::inRange(target, yellowLow, yellowHigh, yellowMask);

        cv::inRange(target, treasureLow, treasureHigh, treasureMask);
        cv::inRange(target, badLow, badHigh, badMask);

        cv::bitwise_or(yellowMask, treasureMask, resultMask);

        // Create a vector of all the bad contours bounding rects
        cv::findContours(badMask, badContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
        for (const auto &contour : badContours)
        {
            badRects.emplace_back(cv::boundingRect(contour));
        }

        cv::findContours(resultMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

        for (const auto &contour : contours)
        {
            boundRect = cv::boundingRect(contour);
            // Calculate the center of the rectangle
            centerX = boundRect.x + boundRect.width / 2;
            centerY = boundRect.y + boundRect.height / 2;

            if (boundRect.area() > MIN_RECT_AREA && !isCollideRect({centerX, centerY}, badRects))
            {
                isClicked = false;
                for (const ClickedRect &clickedRect : clickedRects)
                {
                    clickDistance = cv::norm(cv::Point(centerX, centerY) - clickedRect.center);

                    if (clickDistance < MIN_CLICK_DISTANCE)
                    {
                        isClicked = true;
                        break;
                    }
                }

                if (!isClicked)
                {
                    // Click on the center of the rectangle
                    ClickMouse(centerX + startX, centerY + startY, input);

                    ClickedRect clicked = {{centerX, centerY}, Clock::now()};
                    clickedRects.emplace_back(clicked);
                }
            }

            currentTime = Clock::now();

            for (auto it = clickedRects.begin(); it != clickedRects.end();)
            {
                if (std::chrono::duration_cast<Duration>(currentTime - it->clickedTime).count() > MAX_CLICKED_TIME)
                {
                    it = clickedRects.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // Clear the vectors after processing the current frame
        badContours.clear();
        badRects.clear();
        contours.clear();

#ifdef LIMIT_FPS
        currentTime = Clock::now();
        Duration elapsedTime = std::chrono::duration_cast<Duration>(currentTime - lastFrameTime);

        Duration sleepDuration = FRAME_DURATION - elapsedTime;
        if (sleepDuration > Duration::zero())
        {
            std::this_thread::sleep_for(sleepDuration);
        }
        lastFrameTime = currentTime;
#endif

        endTime = Clock::now();
        time_elapsed = std::chrono::duration_cast<Duration>(endTime - startTime).count() / 1000;
        frames++;
    }
    currentTime = Clock::now();
    std::unique_lock<std::mutex> lock(coutMutex);
    auto elapsedTime = std::chrono::duration_cast<Duration>(currentTime - startTime);
    std::cout << "[+] AVG FPS: " << int(frames / (elapsedTime.count() / 1000)) << std::endl;
}

int main()
{
    std::vector<std::thread> threads;
    system("cls");

    char inp;
    std::cout << "\nInput any key to start ";
    std::cin >> inp;

    // Get the target window handle
    HWND targetWindow = FindWindowA(nullptr, WINDOW_NAME);
    // Check if the pointer is not null
    if (targetWindow == nullptr)
    {
        std::cout << "Window not found." << std::endl;
        return 1;
    }

    ClientToScreen(targetWindow, &zeroWindow);

    for (size_t i = 1; i <= THREADS_NUM; i++)
    {
        threads.emplace_back(handleWindowPart, targetWindow, i);
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }

    return 0;
}
