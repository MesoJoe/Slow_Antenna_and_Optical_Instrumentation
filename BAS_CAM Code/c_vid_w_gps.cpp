
/// c_vid_w_gps.cpp

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/VideoWriter.h>

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

using namespace Pylon;
using namespace GenApi;
using namespace std;
using namespace Basler_UniversalCameraParams;

static const int cFramesPerSecond = 100;
static const uint32_t cQuality = 80;
static const uint32_t c_secondsPerVideo = 60;
static const uint32_t c_countOfImagesPerVideo = cFramesPerSecond * c_secondsPerVideo;

const char* GPS_DEVICE =
"/dev/serial/by-id/usb-Prolific_Technology_Inc._USB-Serial_Controller_CIBPb115819-if00-port0";

std::mutex gpsMutex;
std::string latestGPS = "GPS: STARTING";
std::atomic<bool> runGPS(true);

std::string getCurrentTimestamp()
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);
    auto now_us = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;

    std::ostringstream oss;

    oss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S")
        << "_"
        << std::setw(6)
        << std::setfill('0')
        << now_us.count();

    return oss.str();
}

double nmeaToDecimal(const std::string& val, const std::string& dir)
{
    if (val.empty())
        return 0.0;

    double raw = std::stod(val);

    int deg = static_cast<int>(raw / 100);
    double min = raw - deg * 100;

    double dec = deg + min / 60.0;

    if (dir == "S" || dir == "W")
        dec *= -1.0;

    return dec;
}

std::vector<std::string> splitCSV(const std::string& s)
{
    std::vector<std::string> out;

    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, ','))
        out.push_back(item);

    return out;
}

void parseRMC(const std::string& line)
{
    auto fields = splitCSV(line);

    if (fields.size() < 7)
        return;

    if (fields[0] != "$GPRMC")
        return;

    if (fields[2] != "A")
    {
        std::lock_guard<std::mutex> lock(gpsMutex);
        latestGPS = "GPS: NO FIX";
        return;
    }

    double lat = nmeaToDecimal(fields[3], fields[4]);
    double lon = nmeaToDecimal(fields[5], fields[6]);

    std::ostringstream oss;

    oss << fixed
        << setprecision(6)
        << "LAT "
        << lat
        << " LON "
        << lon
        << " UTC "
        << fields[1];

    std::lock_guard<std::mutex> lock(gpsMutex);
    latestGPS = oss.str();
}

void gpsThreadFunc()
{
    int fd = open(GPS_DEVICE, O_RDONLY | O_NOCTTY);

    if (fd < 0)
    {
        std::lock_guard<std::mutex> lock(gpsMutex);
        latestGPS = "GPS OPEN FAIL";
        return;
    }

    termios tty{};

    tcgetattr(fd, &tty);

    cfsetispeed(&tty, B4800);
    cfsetospeed(&tty, B4800);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_iflag = 0;
    tty.c_lflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    tcsetattr(fd, TCSANOW, &tty);

    std::string line;
    char ch;

    while (runGPS)
    {
        int n = read(fd, &ch, 1);

        if (n > 0)
        {
            if (ch == '\n')
            {
                if (line.find("$GPRMC") == 0)
                    parseRMC(line);

                line.clear();
            }
            else if (ch != '\r')
            {
                if (line.size() < 128)
                    line += ch;
                else
                    line.clear();
            }
        }
    }

    close(fd);
}

const uint8_t* getCharBitmap(char c)
{
    static const uint8_t SPACE[7] = {0,0,0,0,0,0,0};
    static const uint8_t COLON[7] = {0,4,4,0,4,4,0};
    static const uint8_t DOT[7]   = {0,0,0,0,0,6,6};
    static const uint8_t MINUS[7] = {0,0,0,31,0,0,0};

    static const uint8_t NUM[10][7] = {
        {14,17,19,21,25,17,14},
        {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},
        {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},
        {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}
    };

    static const uint8_t A[7] = {14,17,17,31,17,17,17};
    static const uint8_t C[7] = {14,17,16,16,16,17,14};
    static const uint8_t E[7] = {31,16,16,30,16,16,31};
    static const uint8_t F[7] = {31,16,16,30,16,16,16};
    static const uint8_t G[7] = {14,17,16,23,17,17,14};
    static const uint8_t I[7] = {14,4,4,4,4,4,14};
    static const uint8_t L[7] = {16,16,16,16,16,16,31};
    static const uint8_t N[7] = {17,25,21,19,17,17,17};
    static const uint8_t O[7] = {14,17,17,17,17,17,14};
    static const uint8_t P[7] = {30,17,17,30,16,16,16};
    static const uint8_t R[7] = {30,17,17,30,20,18,17};
    static const uint8_t S[7] = {15,16,16,14,1,1,30};
    static const uint8_t T[7] = {31,4,4,4,4,4,4};
    static const uint8_t U[7] = {17,17,17,17,17,17,14};
    static const uint8_t X[7] = {17,17,10,4,10,17,17};
    static const uint8_t B[7] = {30,17,17,30,17,17,30};
    static const uint8_t H[7] = {17,17,17,31,17,17,17};
    static const uint8_t J[7] = {1,1,1,1,17,17,14};
    static const uint8_t K[7] = {17,18,20,24,20,18,17};

    static const uint8_t M[7] = {17,27,21,21,17,17,17};

    static const uint8_t Q[7] = {14,17,17,17,21,18,13};

    static const uint8_t V[7] = {17,17,17,17,17,10,4};

    static const uint8_t W[7] = {17,17,17,21,21,21,10};
    
    static const uint8_t Y[7] = {17,17,10,4,4,4,4};

    static const uint8_t Z[7] = {31,1,2,4,8,16,31};

    if (c >= '0' && c <= '9')
        return NUM[c - '0'];

    switch (c)
    {
        case 'A': return A;
        case 'B': return B;
        case 'C': return C;
        case 'E': return E;
        case 'F': return F;
        case 'G': return G;
        case 'H': return H;
        case 'I': return I;
        case 'J': return J;
        case 'K': return K;
        case 'L': return L;
        case 'M': return M;
        case 'N': return N;
        case 'O': return O;
        case 'P': return P;
        case 'Q': return Q;
        case 'R': return R;
        case 'S': return S;
        case 'T': return T;
        case 'U': return U;
        case 'V': return V;
        case 'W': return W; 
        case 'X': return X;
        case 'Y': return Y;
        case 'Z': return Z;
        case ':': return COLON;
        case '.': return DOT;
        case '-': return MINUS;
        case ' ': return SPACE;
        default: return SPACE;
    }
}
void drawChar(uint8_t* img,
              int width,
              int height,
              int x,
              int y,
              char c)
{
    const uint8_t* bmp = getCharBitmap(c);

    for (int row = 0; row < 7; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            if (bmp[row] & (1 << (4 - col)))
            {
                int px = x + col;
                int py = y + row;

                if (px >= 0 && px < width &&
                    py >= 0 && py < height)
                {
                    img[py * width + px] = 255;
                }
            }
        }
    }
}

void drawText(uint8_t* img,
              int width,
              int height,
              int x,
              int y,
              const std::string& text)
{
    for (size_t i = 0; i < text.size(); i++)
    {
        drawChar(img,
                 width,
                 height,
                 x + static_cast<int>(i) * 6,
                 y,
                 text[i]);
    }
}

void drawOverlay(uint8_t* img,
                 int width,
                 int height,
                 const std::string& timeText,
                 const std::string& gpsText)
{
    for (int y = 5; y < 32 && y < height; y++)
    {
        for (int x = 5; x < 900 && x < width; x++)
        {
            img[y * width + x] = 0;
        }
    }

    drawText(img, width, height, 12, 10, timeText);
    drawText(img, width, height, 12, 22, gpsText);
}

int main()
{
    int exitCode = 0;

    PylonInitialize();

    std::thread gpsThread(gpsThreadFunc);

    try
    {
        CBaslerUniversalInstantCamera camera(
            CTlFactory::GetInstance().CreateFirstDevice()
        );

        camera.Open();

        INodeMap& nodemap = camera.GetNodeMap();

        CBooleanParameter(nodemap,
                          "AcquisitionFrameRateEnable").SetValue(true);

        CFloatParameter(nodemap,
                        "AcquisitionFrameRate").SetValue(100.0);

        CEnumParameter(nodemap,
                       "AcquisitionMode").SetValue("Continuous");

        CEnumParameter(nodemap,
                       "TriggerMode").SetValue("Off");

        CEnumParameter(nodemap,
                       "GainAuto").SetValue("Off");

        CFloatParameter(nodemap,
                        "Gain").SetValue(30.0);

        CEnumParameter(nodemap,
                       "ExposureAuto").SetValue("Off");

        CFloatParameter(nodemap,
                        "ExposureTime").SetValue(2000.0);

        CIntegerParameter width(nodemap, "Width");
        CIntegerParameter height(nodemap, "Height");

        CEnumParameter pixelFormat(nodemap, "PixelFormat");

        width.SetValue(1300, IntegerValueCorrection_Nearest);
        height.SetValue(500, IntegerValueCorrection_Nearest);

        pixelFormat.SetValue("Mono8");

        CPixelTypeMapper pixelTypeMapper(&pixelFormat);

        EPixelType pixelType =
            pixelTypeMapper.GetPylonPixelTypeFromNodeValue(
                pixelFormat.GetIntValue()
            );

        int frameWidth = static_cast<int>(width.GetValue());
        int frameHeight = static_cast<int>(height.GetValue());

        camera.StartGrabbing(GrabStrategy_OneByOne);

        cout << "Camera started." << endl;

        CBaslerUniversalGrabResultPtr ptrGrabResult;

        while (camera.IsGrabbing())
        {
            std::string timestamp = getCurrentTimestamp();

            std::string videoFileName =
                timestamp + "_Video.mp4";

            CVideoWriter videoWriter;

            videoWriter.SetParameter(
                static_cast<uint32_t>(frameWidth),
                static_cast<uint32_t>(frameHeight),
                pixelType,
                cFramesPerSecond,
                cQuality
            );

            videoWriter.Open(videoFileName.c_str());

            cout << "Recording: "
                 << videoFileName
                 << endl;

            uint32_t imageCount = 0;

            auto startTime = chrono::steady_clock::now();

            while (camera.IsGrabbing() &&
                   imageCount < c_countOfImagesPerVideo)
            {
                camera.RetrieveResult(
                    8,
                    ptrGrabResult,
                    TimeoutHandling_ThrowException
                );

                if (ptrGrabResult->GrabSucceeded())
                {
                    uint8_t* buffer =
                        static_cast<uint8_t*>(
                            ptrGrabResult->GetBuffer()
                        );

                    std::string gpsCopy;

                    {
                        std::lock_guard<std::mutex> lock(gpsMutex);
                        gpsCopy = latestGPS;
                    }

                    std::string timeText =
                        "TIME " + getCurrentTimestamp();

                    drawOverlay(
                        buffer,
                        frameWidth,
                        frameHeight,
                        timeText,
                        gpsCopy
                    );

                    videoWriter.Add(ptrGrabResult);

                    imageCount++;
                }

                auto elapsed =
                    chrono::duration_cast<chrono::seconds>(
                        chrono::steady_clock::now() - startTime
                    ).count();

                if (elapsed >= c_secondsPerVideo)
                    break;
            }
        }
    }
    catch (const GenericException& e)
    {
        cerr << "Pylon exception: "
             << e.GetDescription()
             << endl;

        exitCode = 1;
    }

    runGPS = false;

    if (gpsThread.joinable())
        gpsThread.join();

    PylonTerminate();

    return exitCode;
}

