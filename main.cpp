#include <opencv2/opencv.hpp>
#include <iostream>
#include <Windows.h>
#include <cmath>
#include <thread>

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

const cv::Scalar yellowLow = cv::Scalar(18, 145, 237);
const cv::Scalar yellowHigh = cv::Scalar(23, 181, 255);

const cv::Scalar pinkLow = cv::Scalar(160, 63, 242);
const cv::Scalar pinkHigh = cv::Scalar(168, 89, 255);

const cv::Scalar blueLow = cv::Scalar(87, 53, 215);
const cv::Scalar blueHigh = cv::Scalar(95, 59, 220);

const int FPS = 60;
const int THREADS_NUM = 12;
int leftGap = 321;
int topGap = 113;
const int RUNTIME = 47;
const long DX_C = 65535 / GetSystemMetrics(SM_CXSCREEN);
const long DY_C = 65535 / GetSystemMetrics(SM_CYSCREEN);

void checkForExit(bool &shouldExit)
{
    time_t start, end;
    double time_elapsed = 0;
    time(&start);
    while (time_elapsed <= RUNTIME)
    {
        if (GetAsyncKeyState('Q') & 0x8000) // Check if 'q' key is pressed
        {
            shouldExit = true;
            break; // Exit the loop
        }
        time(&end);
        time_elapsed = double(end - start);
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Wait for a short period
    }
    shouldExit = true;
}

void ClickMouse(int x, int y, HWND targetWindow, INPUT input)
{
    POINT point = {x, y};
    ClientToScreen(targetWindow, &point);
    input.mi.dx = point.x * DX_C;
    input.mi.dy = point.y * DY_C;

    SendInput(1, &input, sizeof(INPUT));
}

bool isCollideRect(cv::Rect rect, std::vector<cv::Rect> others)
{
    for (size_t i = 0; i < others.size(); i++)
    {
        if (((rect & others[i]).area() > 0))
        {
            return true;
        }
    }
    return false;
}

void handleWindowPart(HWND targetWindow, int partNumber, const bool &shouldExit)
{
    RECT windowRect;
    cv::Mat target;
    cv::Mat yellowMask;
    cv::Mat pinkMask;
    cv::Mat blueMask;
    cv::Mat resultMask;
    int centerX, centerY;
    time_t start, end;
    double time_elapsed = 0;

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

    time(&start);

    while (time_elapsed <= RUNTIME)
    {
        target = screenshotTaker.takeScreenshotPart();

        // Convert to HSV
        cv::cvtColor(target, target, cv::COLOR_BGR2HSV);

        // Create the mask for the yellow color
        cv::inRange(target, yellowLow, yellowHigh, yellowMask);

        cv::inRange(target, pinkLow, pinkHigh, pinkMask);

        cv::bitwise_or(yellowMask, pinkMask, resultMask);

        // EXP
        cv::inRange(target, blueLow, blueHigh, blueMask);

        // Create a vector of all the blue contours bounding rects
        std::vector<cv::Rect> blueRects;
        std::vector<std::vector<cv::Point>> blueContours;
        cv::findContours(blueMask, blueContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (size_t i = 0; i < blueContours.size(); i++)
        {
            blueRects.push_back(cv::boundingRect(blueContours[i]));
        }

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(resultMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        for (size_t i = 0; i < contours.size(); i++)
        {
            cv::Rect boundRect = cv::boundingRect(contours[i]);
            // Calculate the center of the rectangle
            centerX = boundRect.x + boundRect.width / 2;
            centerY = boundRect.y + boundRect.height / 2;

            if (!isCollideRect(boundRect, blueRects))
            {
                // Click on the center of the rectangle
                ClickMouse(centerX + startX, centerY + startY, targetWindow, input);
            }
        }

        time(&end);
        time_elapsed = double(end - start);
    }
}

void handleWindowPartDBG(HWND targetWindow, int partNumber, bool &shouldExit, std::mutex &videoLock)
{
    RECT windowRect;
    time_t start, end, last;
    cv::Mat target;
    cv::Mat yellowMask;
    cv::Mat pinkMask;
    cv::Mat blueMask;
    cv::Mat resultMask;
    cv::Mat previewFrame;
    int centerX, centerY;

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

    cv::Mat videoFrames[FPS * RUNTIME];
    int curFrame = 0;

    int frames = 0;
    time(&start);
    time(&last);

    while (!shouldExit)
    {
        target = screenshotTaker.takeScreenshotPart();

        // Draw bounding rects on the captured frame
        previewFrame = target.clone();

        // Convert to HSV
        cv::cvtColor(target, target, cv::COLOR_BGR2HSV);

        // Create the mask for the yellow color
        cv::inRange(target, yellowLow, yellowHigh, yellowMask);

        cv::inRange(target, pinkLow, pinkHigh, pinkMask);

        cv::bitwise_or(yellowMask, pinkMask, resultMask);

        cv::inRange(target, blueLow, blueHigh, blueMask);

        // Create a vector of all the blue contours bounding rects
        std::vector<cv::Rect> blueRects;
        std::vector<std::vector<cv::Point>> blueContours;
        cv::findContours(blueMask, blueContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (size_t i = 0; i < blueContours.size(); i++)
        {
            blueRects.push_back(cv::boundingRect(blueContours[i]));
            cv::rectangle(previewFrame, blueRects[i], cv::Scalar(255, 0, 0), 2); // Blue rectangle
        }

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(resultMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        for (size_t i = 0; i < contours.size(); i++)
        {
            cv::Rect boundRect = cv::boundingRect(contours[i]);
            cv::rectangle(previewFrame, boundRect, cv::Scalar(0, 255, 0), 2); // Green rectangle
            // Calculate the center of the rectangle
            centerX = boundRect.x + boundRect.width / 2;
            centerY = boundRect.y + boundRect.height / 2;

            if (!isCollideRect(boundRect, blueRects))
            {
                // Click on the center of the rectangle
                ClickMouse(centerX + startX, centerY + startY, targetWindow, input);
            }
        }

        time(&end);

        if (double(end - last) >= (1 / FPS) && curFrame < FPS * RUNTIME)
        {
            videoFrames[curFrame] = previewFrame;
            curFrame++;
            time(&last);
        }
        frames++;
    }

    videoLock.lock();
    std::string videoFileName = "thread_" + std::to_string(partNumber) + ".mp4";
    cv::VideoWriter videoWriter(videoFileName, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), FPS, cv::Size(divisionWidth, divisionHeight), true);

    for (int i = 0; i < RUNTIME * FPS; i++)
    {
        videoWriter.write(videoFrames[i]);
    }

    delete[] videoFrames;

    videoWriter.release();
    videoLock.unlock();

    std::cout << "$ AVG FPS:" << double(frames / RUNTIME) << std::endl;
}

int main()
{
    std::vector<std::thread> threads;
    system("cls");
    const char *welcome = R"(
             _____ ____   _____                _____ ____  _                       
            / ____/ __ \ / ____|   /\         / ____/ __ \| |        /\            
           | |   | |  | | |       /  \ ______| |   | |  | | |       /  \           
           | |   | |  | | |      / /\ \______| |   | |  | | |      / /\ \          
           | |___| |__| | |____ / ____ \     | |___| |__| | |____ / ____ \         
          _ \_____\____/ \_____/_/    \_\___ _\_____\____/|______/_/____\_\ _____  
     /\  | |  | |__   __/ __ \        / ____| |    |_   _/ ____| |/ /  ____|  __ \ 
    /  \ | |  | |  | | | |  | |______| |    | |      | || |    | ' /| |__  | |__) |
   / /\ \| |  | |  | | | |  | |______| |    | |      | || |    |  < |  __| |  _  / 
  / ____ \ |__| |  | | | |__| |      | |____| |____ _| || |____| . \| |____| | \ \ 
 /_/    \_\____/   |_|  \____/        \_____|______|_____\_____|_|\_\______|_|  \_\
                                                                                   
                                                                                   

Made by Doron.
    )";

    std::cout << welcome;

    char inp;
    std::cout << "\nRun in debug mode (Y/N)? ";
    std::cin >> inp;

    bool shouldExit = false; // Atomic bool to indicate whether to exit
    // Start the keyboard input checking thread
    // std::thread exitThread(checkForExit, std::ref(shouldExit));

    if (tolower(inp) == 'y')
    {
        std::mutex videoLock;
        leftGap = 0;
        topGap = 0;
        // Get the target window handle
        HWND targetWindow = FindWindowA(nullptr, "Moving Circles");
        // Check if the pointer is not null
        if (targetWindow == nullptr)
        {
            std::cout << "Window not found." << std::endl;
            return 1;
        }

        // Start all the threads
        for (size_t i = 1; i <= THREADS_NUM; i++)
        {
            threads.emplace_back(handleWindowPartDBG, targetWindow, i, std::ref(shouldExit), std::ref(videoLock));
        }

        // Join all the threads
        for (std::thread &thread : threads)
        {
            thread.join();
        }

        // Combine video
        std::string pythonCommand = "python video_combine.py";
        for (int i = 1; i <= THREADS_NUM; i++)
        {
            pythonCommand += " thread_" + std::to_string(i) + ".mp4";
        }

        int result = system(pythonCommand.c_str());

        if (result == 0)
        {
            std::cout << "Output video is ready" << std::endl;
            for (int i = 1; i <= THREADS_NUM; i++)
            {
                std::string filename = "thread_" + std::to_string(i) + ".mp4";
                std::remove(filename.c_str());
            }
        }
        else
        {
            std::cerr << "Error creating video" << std::endl;
        }
    }

    else
    {
        // Get the target window handle
        HWND targetWindow = FindWindowA(nullptr, "BlueStacks App Player");
        // Check if the pointer is not null
        if (targetWindow == nullptr)
        {
            std::cout << "Window not found." << std::endl;
            return 1;
        }

        // std::thread scoreThread(checkScore, targetWindow);

        std::vector<std::thread> threads;
        // Start all the threads
        for (size_t i = 1; i <= THREADS_NUM; i++)
        {
            threads.emplace_back(handleWindowPart, targetWindow, i, std::ref(shouldExit));
        }

        // Join all the threads
        for (std::thread &thread : threads)
        {
            thread.join();
        }

        // exitThread.join();
    }

    return 0;
}
