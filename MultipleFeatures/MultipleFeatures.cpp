/**
 *	多特征融合——色调 + 纹理 + 边缘
 *		边缘计算有两种方法：
 *			<1> 使用Canny算子计算边缘图
 *			<2> 建立分数阶模板计算边缘图
 *
 *	Created on:  2018年4月23
 *	Author: Regan_Chai
 *	E-Mail: regan_chai@163.com
 *
 **/

#include "stdafx.h"
#include <opencv2/opencv.hpp>

#include "lbp.h"
#include "edgeDec.h"

using namespace cv;
using namespace std;

Mat image;

bool backprojMode = false;
bool selectObject = false;
int trackObject = 0;
bool showHist = true;
Point origin;
Rect selection;
int vmin = 10, vmax = 256, smin = 30;

// User draws box around object to track. This triggers CAMShift to start tracking
static void onMouse(int event, int x, int y, int, void*)
{
	if (selectObject)
	{
		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);

		selection &= Rect(0, 0, image.cols, image.rows);
	}

	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
	case EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;   // Set up CAMShift properties in main() loop
		break;
	}
}

const char* keys =
{
	"{help h | | show help message}{@camera_number| 0 | camera number}"
};

int main(int argc, const char** argv)
{
	VideoCapture cap;
	cap.open("E:/视频库/标准库/David.avi");

	if (!cap.isOpened())
	{
		cout << "***Could not initialize capturing...***\n";
		cout << "Current parameter's value: \n";
		return -1;
	}

	Rect trackWindow;
	int hsize = 255;
	float hranges[] = { 0, 180 };
	const float* phranges = hranges;
	CommandLineParser parser(argc, argv, keys);

	namedWindow("CamShift Demo", 0);
	setMouseCallback("CamShift Demo", onMouse, 0);
	//createTrackbar("Vmin", "CamShift Demo", &vmin, 256, 0);
	//createTrackbar("Vmax", "CamShift Demo", &vmax, 256, 0);
	//createTrackbar("Smin", "CamShift Demo", &smin, 256, 0);

	Mat frame, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3);
	Mat hist_hue, hist_by, hist_wl, backproj_hue, backproj_by, backproj_wl;

	bool paused = false;

	Mat gray_image;

	// 多特征判断
	bool mulitFeature = true;
	//bool muliFue = false;

	for (;;)
	{
		if (!paused){
			cap >> frame;
			if (frame.empty())
				break;
		}

		frame.copyTo(image);

		if (!paused){

			cvtColor(image, hsv, COLOR_BGR2HSV);
			cvtColor(image, gray_image, COLOR_BGR2GRAY);

			if (trackObject){
				int _vmin = vmin, _vmax = vmax;

				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)), Scalar(180, 256, MAX(_vmin, _vmax)), mask);
				int ch[] = { 0, 0 };
				hue.create(hsv.size(), hsv.depth());
				mixChannels(&hsv, 1, &hue, 1, ch, 1);

				Mat backproj = Mat::zeros(hue.rows, hue.cols, hue.type());

				// <一> Canny算子做边缘检测
				//Mat bianyuan, edge;
				//blur(gray_image, edge, Size(3, 3));
				//Canny(edge, bianyuan, 3, 9, 3);

				// <二> 分数阶做边缘检测
				Mat bianyuan = EdgeDetection(gray_image);

				Mat wenli = LBP(gray_image);

				///////////////////////////////////////
				if (trackObject < 0){
					// Object has been selected by user, set up CAMShift search properties once
					Mat roi(hue, selection), maskroi(mask, selection);
					calcHist(&roi, 1, 0, maskroi, hist_hue, 1, &hsize, &phranges);
					normalize(hist_hue, hist_hue, 0, 255, NORM_MINMAX);

					trackWindow = selection;
					trackObject = 1; // Don't set up again, unless user selects new ROI


					// 计算边缘图的直方图
					Mat roi_by(bianyuan, selection);
					calcHist(&roi_by, 1, 0, maskroi, hist_by, 1, &hsize, &phranges);
					//normalize(hist_by, hist_by, 0, 255, NORM_MINMAX);

					//double minValue = 0;
					//double maxValue = 0;
					//minMaxLoc(hist_by, &minValue, &maxValue, 0, 0);


					// 计算纹理图的直方图
					Mat roi_wl(wenli, selection);
					calcHist(&roi_wl, 1, 0, maskroi, hist_wl, 1, &hsize, &phranges);
					normalize(hist_wl, hist_wl, 0, 255, NORM_MINMAX);

					// 绘制直方图
					histimg = Scalar::all(0);
					int binW = histimg.cols / hsize;
					Mat buf(1, hsize, CV_8UC3);
					for (int i = 0; i < hsize; i++)
						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);
					cvtColor(buf, buf, COLOR_HSV2BGR);

					for (int i = 0; i < hsize; i++)
					{
						int val = saturate_cast<int>(hist_by.at<float>(i)*histimg.rows / 255);
						rectangle(histimg, Point(i*binW, histimg.rows),
							Point((i + 1)*binW, histimg.rows - val),
							Scalar(buf.at<Vec3b>(i)), -1, 8);
					}
					imshow("边缘直方图", histimg);

				}

				// 计算反向投影图
				calcBackProject(&hue, 1, 0, hist_hue, backproj_hue, &phranges);
				calcBackProject(&bianyuan, 1, 0, hist_by, backproj_by, &phranges);
				calcBackProject(&wenli, 1, 0, hist_wl, backproj_wl, &phranges);
				imshow("色调反向投影图", backproj_hue);
				//imshow("边缘反向投影图", backproj_by);
				//imshow("纹理反向投影图", backproj_wl);

				if (mulitFeature) {
					// 多特征融合
					imshow("边缘图", bianyuan);
					imshow("纹理图", wenli);

					float a = 0.7, b = 0.2, c = 0.1;
					for (int i = 0; i < hue.rows; i++) {
						uchar* data = backproj.ptr<uchar>(i);
						uchar* data_hue = backproj_hue.ptr<uchar>(i);
						uchar* data_by = backproj_by.ptr<uchar>(i);
						uchar* data_wl = backproj_wl.ptr<uchar>(i);
						for (int j = 0; j < hue.cols; j++) {
							data[j] = a * data_hue[j] + b * data_by[j] + c * data_wl[j];
						}
					}
					// 归一化
					normalize(backproj, backproj, 0, 255, NORM_MINMAX);
					imshow("融合反向投影图", backproj);

				}
				else {
					// 基本Camshift算法(基于颜色特征)
					backproj = backproj_hue;
				}

				backproj &= mask;
				RotatedRect trackBox = CamShift(backproj, trackWindow, TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));

				if (trackWindow.area() <= 1){
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r, trackWindow.x + r, trackWindow.y + r) & Rect(0, 0, cols, rows);
				}

				if (backprojMode)
					cvtColor(backproj, image, COLOR_GRAY2BGR);
				ellipse(image, trackBox, Scalar(0, 0, 255), 3, LINE_AA);
			}
		}
		else if (trackObject < 0)
			paused = false;

		if (selectObject && selection.width > 0 && selection.height > 0){
			Mat roi(image, selection);
			bitwise_not(roi, roi);
		}

		imshow("CamShift Demo", image);

		char c = (char)waitKey(30);
		if (c == 27)
			break;
		switch (c)
		{
		case 'b':
			backprojMode = !backprojMode;
			break;
		case 'c':
			trackObject = 0;
			histimg = Scalar::all(0);
			break;
		case 'h':
			showHist = !showHist;
			if (!showHist)
				destroyWindow("Histogram");
			else
				namedWindow("Histogram", 1);
			break;
		case 'p':
			paused = !paused;
			break;
		default:
			;
		}
	}

	return 0;
}
