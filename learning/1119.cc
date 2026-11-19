#include <iostream>
#include <string>
#include "opencv2/opencv.hpp"
#include "opencv2/video.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"

cv::Mat image;

int main(){
    cv::VideoCapture cap("/dev/video0", cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    if (!cap.isOpened()) {
        std::cerr << "No device to use";
	return 0;
    }
    while (1) {
        cap.read(image);
	cv::imshow("676767", image);
    
        int d = cv::waitKey(1);
	if (d != -1) break;
    }
    cap.release();
}

