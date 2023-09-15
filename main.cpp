#include <opencv2/opencv.hpp>
#include <iostream>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <cmath>

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

struct RGB_COLOR
{
    int red;
    int green;
    int blue;
};

const RGB_COLOR goodColors[2] = {{239, 64, 56}, {144, 167, 112}};
const RGB_COLOR badColor = {0, 0, 0}; //;{255, 222, 115};

const LPCSTR WINDOW_NAME = "BlueStacks App Player";
const int THREADS_NUM = 8;
const int leftGap = 320;
const int topGap = 115; // 143;
const int RUNTIME = 47;
const int MIN_CLICK_DISTANCE = 10;
const long MAX_CLICKED_TIME = 300; // MS
const int TARGET_FPS = 240;
const int COLOR_ERROR = 5;
const int minProximity = 150;

const std::chrono::milliseconds FRAME_DURATION(1000 / TARGET_FPS);
const long DX_C = 65535 / GetSystemMetrics(SM_CXSCREEN);
const long DY_C = 65535 / GetSystemMetrics(SM_CYSCREEN);
struct ClickedPoint
{
    cv::Point center;
    std::chrono::time_point<std::chrono::high_resolution_clock> clickedTime;
};
POINT zeroWindow = {0, 0};
std::mutex coutMutex;

bool compareColors(const RGB_COLOR &color1, const cv::Vec3b &color2, int errorRange)
{
    int rDiff = std::abs(color1.red - color2[2]);
    int gDiff = std::abs(color1.green - color2[1]);
    int bDiff = std::abs(color1.blue - color2[0]);

    return (rDiff <= errorRange) && (gDiff <= errorRange) && (bDiff <= errorRange);
}

double calculateDistance(const cv::Point &p1, const cv::Point &p2)
{
    int dx = p2.x - p1.x;
    int dy = p2.y - p1.y;

    return std::sqrt(dx * dx + dy * dy);
}

bool isBadPointNearby(const cv::Mat &image, const cv::Point &point)
{
    // Calculate the valid range
    int xStart = std::max<int>(0, point.x - minProximity);
    int xEnd = std::min<int>(image.cols, point.x + minProximity);
    int yStart = std::max<int>(0, point.y - minProximity);
    int yEnd = std::min<int>(image.rows, point.y + minProximity);

    // Iterate through a square region around the specified point
    for (int y = yStart; y <= yEnd; ++y)
    {
        for (int x = xStart; x <= xEnd; ++x)
        {
            // Check the color of the pixel in the proximity
            cv::Vec3b nearbyColor = image.at<cv::Vec3b>(y, x);

            // Compare the color to the predefined red color with a specified error range
            if (compareColors(badColor, nearbyColor, COLOR_ERROR))
            {
                // Found a red point within the proximity
                return true;
            }
        }
    }

    // No red point found within the specified proximity
    return false;
}

void ClickMouse(int x, int y, INPUT &input)
{
    input.mi.dx = (x + zeroWindow.x) * DX_C;
    input.mi.dy = (y + zeroWindow.y) * DY_C;

    SendInput(1, &input, sizeof(INPUT));
    std::cout << " ! ";
}

void handleWindowPart(HWND targetWindow, int partNumber)
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;

    RECT windowRect;
    cv::Mat target;

    std::vector<ClickedPoint> clickedPoints;

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

        for (int i = 0; i < target.rows; i++)
        {
            for (int j = 0; j < target.cols; j++)
            {
                auto color = target.at<cv::Vec3b>(i, j);

                for (const auto &goodColor : goodColors)
                {
                    if (compareColors(goodColor, color, COLOR_ERROR))
                    {
                        isClicked = false;

                        for (const ClickedPoint &clickedPoint : clickedPoints)
                        {
                            clickDistance = calculateDistance(cv::Point(j, i), clickedPoint.center);

                            if (clickDistance < MIN_CLICK_DISTANCE)
                            {
                                isClicked = true;
                                break;
                            }
                        }

                        if (!isClicked && !isBadPointNearby(target, {j, i}))
                        {
                            // Click on the center of the rectangle
                            ClickMouse(j + startX, i + startY, input);
                            ClickedPoint clicked = {{j, i}, Clock::now()};
                            clickedPoints.emplace_back(clicked);
                        }
                    }
                }
            }
        }

        currentTime = Clock::now();

        for (auto it = clickedPoints.begin(); it != clickedPoints.end();)
        {
            if (std::chrono::duration_cast<Duration>(currentTime - it->clickedTime).count() > MAX_CLICKED_TIME)
            {
                it = clickedPoints.erase(it);
            }
            else
            {
                ++it;
            }
        }

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

        // if (partNumber == 1)
        // {
        //     cv::imshow("test", target);
        //     cv::waitKey(1);
        // }

        endTime = Clock::now();
        time_elapsed = std::chrono::duration_cast<Duration>(endTime - startTime).count() / 1000;
        frames++;
    }

    currentTime = Clock::now();
    std::unique_lock<std::mutex> lock(coutMutex);
    std::cout << "w: " << divisionWidth << " h: " << divisionHeight << std::endl;

    std::cout << "sX: " << startX << ", sY: " << startY << std::endl;

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
