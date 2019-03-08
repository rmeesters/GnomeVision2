#include "vision.hpp"
using namespace std;


//Set up constants
cv::RNG rng(12345);
cv::Scalar MY_RED (0, 0, 255);
cv::Scalar MY_BLUE (255, 0, 0);
cv::Scalar MY_GREEN (0, 255, 0);
cv::Scalar MY_PURPLE (255, 0, 255);
cv::Scalar GUIDE_DOT(255,255,0);
cv::Point TEST_POINT(120,120);
double pi=acos(-1);

//utility functions
void copyPointData (const cv::Point &pSource, cv::Point &pTarget) {
    pTarget.x = pSource.x;
    pTarget.y = pSource.y;
}

inline int getHue (cv::Mat &img, int r, int c) {
    return img.at<cv::Vec3b>(r, c)[0];
}

inline int getSat (cv::Mat &img, int r, int c) {
    return img.at<cv::Vec3b>(r, c)[1];
}

inline int getVal (cv::Mat &img, int r, int c) {
    return img.at<cv::Vec3b>(r, c)[2];
}

void drawPoint (cv::Mat &img, cv::Point &p, cv::Scalar &color) {
    cv::circle(img, p, 4, color, 4);
}

//checks for contour validity
bool is_valid (contour_type &contour) {
    bool valid = true; //start out assuming its valid, disprove this later

    //find bounding rect & convex hull - modified to using RotatedRect functions (RM)
    // cv::Rect rect = cv::boundingRect(contour);
    cv::Point2f vtx[4];
    cv::RotatedRect rect = cv::minAreaRect(contour);
    rect.points(vtx);
    contour_type hull;
    cv::convexHull(contour, hull);

    double totalArea = (RES_X * RES_Y);

    //calculate relevant ratios & values
    double area = cv::contourArea(contour) / totalArea;
    //double perim = cv::arcLength(hull, true);

    double convex_area = cv::contourArea(hull) / totalArea;

    
    double area_rat = area / convex_area;
    float angle = rect.angle;
    float width = rect.size.width;
    float height = rect.size.height; 
    float rect_rat = width / height; // original height/width - modified due to the way rotatedRect works
   	if (width > height) {
		angle = angle + 90; // find angle relative to vertical plane
	}


  //check ratios & values for validity
    if (area < MIN_AREA || area > MAX_AREA) valid = false;
    if (area_rat < MIN_AREA_RAT || area_rat > MAX_AREA_RAT) valid = false;
    if (angle < 0) valid = false; //find only left contour
    if (rect_rat < MIN_RECT_RAT || rect_rat > MAX_RECT_RAT) valid = false;
    if (width < MIN_WIDTH || width > MAX_WIDTH) valid = false;
    if (height < MIN_HEIGHT || height > MAX_HEIGHT) valid = false;

    return valid;
}

VisionResultsPackage calculate(const cv::Mat &bgr, cv::Mat &processedImage){
    ui64 time_began = millis_since_epoch();
    //blur the image
    cv::blur(bgr, bgr, cv::Size(5,5));
    cv::Mat hsvMat;
    //convert to hsv
    cv::cvtColor(bgr, hsvMat, cv::COLOR_BGR2HSV);
	
	// Real World Vectors - Left Target - new RM
	vector<cv::Point3d> model_points;
	model_points.push_back(cv::Point3d(0.0f, 0.0f, 0.0f)); //Lower Right (Origin)
	model_points.push_back(cv::Point3d(-1.932f, 0.518f, 0.0f)); //Lower Left
	model_points.push_back(cv::Point3d(-0.528f, 5.831f, 0.0f)); //Upper Left
	model_points.push_back(cv::Point3d(1.424f, 5.313f, 0.0f)); //Upper Right
	
	//Camera Internals - new RM
	double focal_length = bgr.cols;
	cv::Point2d  cam_center = cv::Point2d(bgr.cols/2, bgr.rows/2);
	cv::Mat camera_matrix = (cv::Mat_<double>(3,3) << focal_length, 0, cam_center.x, 0, focal_length, cam_center.y, 0, 0, 1);
	cv::Mat dist_coeffs = cv::Mat::zeros(4,1,cv::DataType<double>::type); // Assuming no lens distortion
	
	//cout << "Camera Matrix " << endl << camera_matrix << endl ;
        //cout << "Disortion Coefficients" << endl << dist_coeffs << endl ;
    
	// Output rotation and translation - new RM
    cv::Mat rotation_vector; // Rotation in axis-angle form
    cv::Mat translation_vector;
    cv::Mat rotation_matrix;
	cv::Mat invrot_matrix;
	cv::Mat world_matrix;

    //store HSV values at a given test point to send back
    int hue = getHue(hsvMat, TEST_POINT.x, TEST_POINT.y);
    int sat = getSat(hsvMat, TEST_POINT.x, TEST_POINT.y);
    int val = getVal(hsvMat, TEST_POINT.x, TEST_POINT.y);

    //threshold on green (light ring color)
    cv::Mat greenThreshed;
    cv::inRange(hsvMat,
                cv::Scalar(MIN_HUE, MIN_SAT, MIN_VAL),
                cv::Scalar(MAX_HUE, MAX_SAT, MAX_VAL),
                greenThreshed);

    processedImage = greenThreshed.clone();
    cv::threshold (processedImage, processedImage, 0, 255, cv::THRESH_BINARY);
    cv::cvtColor(processedImage, processedImage, CV_GRAY2BGR); 
    //processedImage = bgr.clone();  

    // drawPoint (processedImage, TEST_POINT, GUIDE_DOT);

    //contour detection
    vector<contour_type> contours;
    vector<cv::Vec4i> hierarchy; //throwaway, needed for function
    try {
        cv::findContours (greenThreshed, contours, hierarchy, 
            cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    }
    catch (...) { //TODO: change this to the error that occurs when there are no contours
        return processingFailurePackage(time_began);
    }

    if (contours.size() < 1) { //definitely did not find 
        return processingFailurePackage(time_began);
    }
    
    //store the convex hulls of any valid contours
    vector<contour_type> valid_contour_hulls;
    for (int i = 0; i < (int)contours.size(); i++) {
        contour_type contour = contours[i];
        if (is_valid (contour)) {
            contour_type hull;
            cv::convexHull(contour, hull);
            valid_contour_hulls.push_back(hull);
        }
    }

    int numContours = valid_contour_hulls.size();
    printf ("Num contours: %d\n", numContours);
    
    if (numContours < 1) { //definitely did not find 
        return processingFailurePackage(time_began);
    }

    // find the largest contour in the image
    contour_type largest;
    double largestArea = 0;
    for (int i = 0; i < numContours; i++){
        double curArea = cv::contourArea(valid_contour_hulls[i], true);
        if (curArea > largestArea){
            largestArea = curArea;
            largest = valid_contour_hulls[i];
        }
    }

    //get the points of corners
    vector<cv::Point> all_points;
    all_points.insert (all_points.end(), largest.begin(), largest.end());

    //find which corner is which
    cv::Point ul (1000, 1000), ur (0, 1000), ll (1000, 0), lr (0, 0);
    for (int i = 0; i < (int)all_points.size(); i++) {
        int sum = all_points[i].x + all_points[i].y;
        int dif = all_points[i].x - all_points[i].y;

        if (sum < ul.x + ul.y) {
            ul = all_points[i];
        }

        if (sum > lr.x + lr.y) {
            lr = all_points[i];
        }

        if (dif < ll.x - ll.y) {
            ll = all_points[i];
        }

        if (dif > ur.x - ur.y) {
            ur = all_points[i];
        }
    } 
    
	vector<cv::Point2d> target_points;
	target_points.push_back(cv::Point2d(lr.x, lr.y)); //Lower Right (Origin)
	target_points.push_back(cv::Point2d(ll.x, ll.y)); //Lower Left
	target_points.push_back(cv::Point2d(ul.x, ul.y)); //Upper Left
	target_points.push_back(cv::Point2d(ur.x, ur.y)); //Upper Right

	
    //cout << "Model Points" << endl << model_points << endl; 
    //cout << "Target Points" << endl << target_points << endl;

    //find the center of mass of the largest contour
    cv::Moments centerMass = cv::moments(largest, true);
    double centerX = (centerMass.m10) / (centerMass.m00);
    double centerY = (centerMass.m01) / (centerMass.m00);
    cv::Point center (centerX, centerY);

    vector<contour_type> largestArr;
    largestArr.push_back(largest);
    cv::drawContours(processedImage, largestArr , 0, MY_GREEN, 2);
    // double largestarea = cv::contourArea(largest);  // -- add angle here as well (todo)
    cv::RotatedRect rctangle = cv::minAreaRect(largest);
    double returnangle = rctangle.angle + 90;
    printf ("Angle %f\n", returnangle);
    
	//SolvePNP - Solve for target - new RM
    cv::solvePnP(model_points, target_points, camera_matrix, dist_coeffs, rotation_vector, translation_vector);
	cv::Rodrigues(rotation_vector, rotation_matrix);
	
	//Test cout for vectors
    //cout << "Rotation Vector " << endl << rotation_vector << endl;
    //cout << "Rotation Matrix" << endl << rotation_matrix << endl;
    cout << "Translation Vector" << endl << translation_vector << endl;

	//Get Distance and Angle 1
	double x_offset = translation_vector.at<double>(0);
	double z_offset = translation_vector.at<double>(2);
    double distance_to_target = sqrt(pow(x_offset,2) + pow (z_offset,2));
    printf("Distance to target %f\n", distance_to_target);
    double robot_angle = (atan2(x_offset, z_offset))*(180/pi);
    printf("Robot Angle %f\n", robot_angle);

	// Get Angle 2 (transpose matrix)
	transpose(rotation_matrix, invrot_matrix);
	world_matrix = (-invrot_matrix * translation_vector);
	double target_angle = (atan2(world_matrix.at<double>(0), world_matrix.at<double>(2)))*(180/pi);
	printf("Target Angle %f\n", target_angle);

    //create the results package
    VisionResultsPackage res;
    res.timestamp = time_began;
    res.valid = true;
    res.xOffset = x_offset;
	res.zOffset = z_offset;
	res.tgtangle = target_angle;
	res.distToTarget = distance_to_target;
    
    copyPointData (ul, res.ul);
    copyPointData (ur, res.ur);
    copyPointData (ll, res.ll);
    copyPointData (lr, res.lr);
    copyPointData (center, res.midPoint);

    res.sampleHue = hue;
    res.sampleSat = sat;
    res.sampleVal = val;

    drawOnImage (processedImage, res);
    return res;
}

void drawOnImage (cv::Mat &img, VisionResultsPackage info) {
    //draw the 4 corners on the image
    drawPoint (img, info.ul, MY_BLUE);
    drawPoint (img, info.ur, MY_RED);
    drawPoint (img, info.ll, MY_BLUE);
    drawPoint (img, info.lr, MY_RED);
    drawPoint (img, info.midPoint, MY_PURPLE);
}

VisionResultsPackage processingFailurePackage(ui64 time) {
    VisionResultsPackage failureResult;
    failureResult.timestamp = time;
    failureResult.valid = false;
    failureResult.xOffset = -1;
	failureResult.zOffset = -1;
    failureResult.tgtangle = -1;
	failureResult.distToTarget = -1;
    
    copyPointData (cv::Point (-1, -1), failureResult.ul);
    copyPointData (cv::Point (-1, -1), failureResult.ur);
    copyPointData (cv::Point (-1, -1), failureResult.ll);
    copyPointData (cv::Point (-1, -1), failureResult.lr);
    copyPointData (cv::Point (-1, -1), failureResult.midPoint);

    failureResult.sampleHue = -1;
    failureResult.sampleSat = -1;
    failureResult.sampleVal = -1;

    return failureResult;
}
