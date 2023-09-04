#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#define DEBUG false
#define LIMIT_FPS false

class ScreenshotTaker {
public:
    ScreenshotTaker(Window targetWindow, int captureWidth, int captureHeight, int startX, int startY, Display *targetDisplay)
        : display(targetDisplay),
          target(targetWindow),
          captureWidth(captureWidth),
          captureHeight(captureHeight),
          startX(startX),
          startY(startY) {

        rootWindow = DefaultRootWindow(display);

        // Create an XImage to store the captured data
        image = XGetImage(display, rootWindow, startX, startY, captureWidth, captureHeight, AllPlanes, ZPixmap);

        // Initialize an OpenCV Mat with the same dimensions
        mat = cv::Mat(captureHeight, captureWidth, CV_8UC4, image->data);
    }

    ~ScreenshotTaker() {
        if (image) {
            XDestroyImage(image);
        }

        if (display) {
            XCloseDisplay(display);
        }
    }

    cv::Mat takeScreenshotPart() {
        // Capture the screen region into the XImage
        XGetSubImage(display, rootWindow, startX, startY, captureWidth, captureHeight, AllPlanes, ZPixmap, image, 0, 0);

        // Return the captured data as an OpenCV Mat
        return mat;
    }

private:
    Display *display;
    Window target;
    Window rootWindow;
    int captureWidth;
    int captureHeight;
    int startX;
    int startY;
    XImage *image;
    cv::Mat mat;
};

const cv::Scalar yellowLow = cv::Scalar(18, 100, 235);
const cv::Scalar yellowHigh = cv::Scalar(22, 115, 245);

const cv::Scalar treasureLow = cv::Scalar(0, 0, 240);
const cv::Scalar treasureHigh = cv::Scalar(179, 25, 255);

const cv::Scalar badLow = cv::Scalar(4, 170, 228);
const cv::Scalar badHigh = cv::Scalar(8, 180, 240);

const char *WINDOW_NAME = "Moving Circles"; //"BlueStacks App Player";
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
struct ClickedRect
{
    cv::Point center;
    std::chrono::time_point<std::chrono::high_resolution_clock> clickedTime;
};

struct POINT
{
    int x;
    int y;
};

POINT zeroWindow = {0, 0};

std::mutex coutMutex;

Display* display;

Window getWindowByName(const char* windowName) {
    if (!display) {
        // Handle error: Unable to open X display
        return 0;
    }

    Window rootWindow = DefaultRootWindow(display);

    Window targetWindow = 0;
    Window parent, *children;
    unsigned int nChildren;

    XQueryTree(display, rootWindow, &rootWindow, &parent, &children, &nChildren);

    for (unsigned int i = 0; i < nChildren; ++i) {
        XTextProperty windowNameProperty;
        if (XGetWMName(display, children[i], &windowNameProperty) != 0) {
            char** list;
            int count;
            if (XmbTextPropertyToTextList(display, &windowNameProperty, &list, &count) == Success) {
                for (int j = 0; j < count; ++j) {
                    if (std::strcmp(windowName, list[j]) == 0) {
                        targetWindow = children[i];
                        break;
                    }
                }
                XFreeStringList(list);
            }
        }
    }

    XFree(children);
    XCloseDisplay(display);

    return targetWindow;
}

void clientToScreen(Window window, POINT& clientPoint) {
    Window rootWindow;
    int rootX, rootY;
    unsigned int mask;
    if (XQueryPointer(display, window, &rootWindow, &rootWindow, &rootX, &rootY, &clientPoint.x, &clientPoint.y, &mask)) {
        clientPoint.x = rootX;
        clientPoint.y = rootY;
    } else {
        // Handle error: Unable to convert client coordinates to screen coordinates
    }
}


void clickMouse(int x, int y) {
    // Move the mouse pointer to the specified coordinates
    XTestFakeMotionEvent(display, DefaultScreen(display), x, y, 0);
    
    // Simulate a button press and release (left mouse button)
    XTestFakeButtonEvent(display, Button1, True, 0);
    XTestFakeButtonEvent(display, Button1, False, 0);

    // Flush the event queue to ensure the events are processed
    XFlush(display);
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

void handleWindowPart(Window targetWindow, int partNumber)
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;

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

    int width, height;

    XWindowAttributes windowAttributes;
    if (XGetWindowAttributes(display, targetWindow, &windowAttributes)) {
        width = windowAttributes.width;
        height = windowAttributes.height;
    } else {
        // Handle error: Unable to get window attributes
    }

    // Calculate dimensions for each division
    int divisionWidth = (width - leftGap) / 2;
    int divisionHeight = (height - topGap) / (THREADS_NUM / 2);

    int row = (partNumber - 1) / 2;
    int col = 1 - (partNumber % 2);

    int startX = (col * divisionWidth) + leftGap;
    int startY = (row * divisionHeight) + topGap;

    ScreenshotTaker screenshotTaker(targetWindow, divisionWidth, divisionHeight, startX, startY, display);

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
                    clickMouse(centerX + startX, centerY + startY);

                    ClickedRect clicked = {{centerX, centerY}, Clock::now()};
                    clickedRects.emplace_back(clicked);
                    // counter++;
                }
                // ClickMouse(centerX + startX, centerY + startY, input);
                // ClickedRect clicked = {{centerX, centerY}, Clock::now()};
                // clickedRects.emplace_back(clicked);
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
    // clicksNumber = counter;
    auto elapsedTime = std::chrono::duration_cast<Duration>(currentTime - startTime);
    std::cout << "[+] AVG FPS: " << int(frames / (elapsedTime.count() / 1000)) << std::endl;
    // std::cout << "[-] Contours max size: " << max << ", bad size: " << max2 << std::endl;
}

int main()
{
    std::vector<std::thread> threads;

    char inp;
    std::cout << "\nInput any key to start ";
    std::cin >> inp;

    display = XOpenDisplay(nullptr);
    if (!display) {
        std::cout << "Error opening display." << std::endl;
        return 1;
    }

    // Get the target window 
    Window targetWindow = getWindowByName(WINDOW_NAME);
    // Check if the pointer is not null
    if (targetWindow == 0)
    {
        std::cout << "Window not found." << std::endl;
        return 1;
    }

    clientToScreen(targetWindow, zeroWindow);

    for (size_t i = 1; i <= THREADS_NUM; i++)
    {
        threads.emplace_back(handleWindowPart, targetWindow, i);
    }

    for (std::thread &thread : threads)
    {
        thread.join();
    }

    XCloseDisplay(display);
    
    return 0;
}
