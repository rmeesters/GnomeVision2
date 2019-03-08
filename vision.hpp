#ifndef VISION_HPP
#define VISION_HPP

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <vector>
#include <iostream>
#include <utility>
#include "helper.hpp"

using namespace std;

struct VisionResultsPackage {
    i64 timestamp;
    bool valid;
    double xOffset; 
    double zOffset; 
	double tgtangle;
	double distToTarget;
	cv::Point midPoint;
    cv::Point ul, ur, ll, lr;
	double upperWidth, lowerWidth;
	double leftHeight, rightHeight;
	int sampleHue, sampleSat, sampleVal;
 //   double ury = ur.y;
 //   double lry = lr.y;
    static string createCSVHeader () {
        return 
            "Timestamp,"
            "Valid,"
            "XOffset,"
            "ZOffset,"
			"tgtangle,"
            "DistToTgt,"
			"MidPoint_x, MidPoint_y"
            "UL_x,UL_y,"
            "UR_x,UR_y,"
            "LL_x,LL_y,"
            "LR_x,LR_y";
    }

    string createCSVLine () {
        stringstream ss;
        ss << timestamp << ",";
        ss << valid << ","; //either 0 or 1
	    ss << xOffset << ",";
        ss << zOffset << ",";
		ss << tgtangle << ",";
		ss << distToTarget << ",";
        ss << midPoint.x << "," << midPoint.y << ",";
        ss << ul.x << "," << ul.y << ",";
        ss << ur.x << "," << ur.y << ",";
        ss << ll.x << "," << ll.y << ",";
        ss << lr.x << "," << lr.y;
        return ss.str();
    }
};

typedef std::vector<cv::Point> contour_type;

const int RES_X = 640, RES_Y = 480;
const int MIN_HUE = 50, MAX_HUE = 90; // Original Value 55-65
const int MIN_SAT = 0, MAX_SAT = 255; // Original Value 0-255 - 100,255
const int MIN_VAL = 100, MAX_VAL = 255; //Original Value 50-255 - 100,255

const double
MIN_AREA = 0.001, MAX_AREA = 1000000,
MIN_WIDTH = 0, MAX_WIDTH = 100000, //rectangle width
MIN_HEIGHT = 0, MAX_HEIGHT = 100000, //rectangle height
MIN_RECT_RAT = 1.5, MAX_RECT_RAT = 5, //rect height / rect width - original 1.5,8 - good, 1.75:5
MIN_AREA_RAT = 0.85, MAX_AREA_RAT = 100; //cvxhull area / contour area

/**
 * Processes the raw image provided in order to determine interesting values
 * from the image. The OpenCV image pipeline (thresholding, etc) is performed on
 * processedImage so this may be sent back to the driver station laptop for 
 * debugging purposes. Results package is returned in a struct.
 * @param bgr raw image to do processing on
 * @param processedImage results of OpenCV image pipeline
 * @return results of vision processing (e.g location of target, timestamp)
 */ 
VisionResultsPackage calculate(const cv::Mat &bgr, cv::Mat &processedImage);
void drawOnImage (cv::Mat &img, VisionResultsPackage info);
VisionResultsPackage processingFailurePackage(ui64 time);

#endif
