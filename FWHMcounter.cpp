#ifdef _WIN64
#include "stdafx.h"
#include "windows.h"
// anything before a precompiled header is ignored, 
// so no endif here! add #endif to compile on __unix__ !
#endif



/*
* modified from BscanFFTpeak.cpp
* and BscanFFTxml2m.cpp
*
* Open xml file BscanFFT.xml from $PWD, display bscans one by one,
* allow for ROI selection, find FWHM of membrane in ROI,
* output to csv
* 
* optional inputs from ini file - not yet implemented.
*
* R to select Peak Hold Region of Interest (toggle ROI display on and off)
* < for ROI (top-left) position left
* > for ROI (top-left) position right
* . for ROI (top-left) position up
* , for ROI (top-left) position down
* m for ROI width decrease
* M for ROI width increase
* / for ROI height decrease
* ? for ROI height increase
*
*
* ESC, x or X key quits
*
*
*
* Hari Nandakumar
* 05 May 2019  *
*
*
*/

//#define _WIN64
//#define __unix__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
// this is for mkdir

#include <opencv2/opencv.hpp>
// used the above include when imshow was being shown as not declared
// removing
// #include <opencv/cv.h>
// #include <opencv/highgui.h>


using namespace cv;

// have to make these global so that the onMouse can see them
int ROIposx = 0, ROIposy = 0, ROIw = 10, ROIh = 10;
Point P1, P2;
bool clicked;


void onMouse(int event, int x, int y, int f, void*)
{
	// modified from https://stackoverflow.com/questions/22140880/drawing-rectangle-or-line-using-mouse-events-in-open-cv-using-python
	switch (event)
	{

	case  CV_EVENT_LBUTTONDOWN:
		clicked = 1;

		P1.x = x;
		P1.y = y;
		P2.x = x;
		P2.y = y;
		break;

	case  CV_EVENT_LBUTTONUP:
		P2.x = x;
		P2.y = y;
		clicked = 0;

		break;

	case  CV_EVENT_MOUSEMOVE:

		if (clicked == 1) {
			P2.x = x;
			P2.y = y;

		}
		break;

	default:   break;


	}


	if (clicked == 1)
	{
		if (P1.x>P2.x)
		{
			ROIposx = P2.x;
			ROIw = P1.x - P2.x;
		}
		else
		{
			ROIposx = P1.x;
			ROIw = P2.x - P1.x;
		}

		if (P1.y>P2.y)
		{
			ROIposy = P2.y;
			ROIh = P1.y - P2.y;
		}
		else
		{
			ROIposy = P1.y;
			ROIh = P2.y - P1.y;
		}

	} // end if clicked

	
	

}



inline void printStatus( int indexi, Mat& statusimg)
{
	 
	char textbuffer[80];
	//Mat firstrowofstatusimg = statusimg(Rect(0, 0, 600, 50));
	//Mat secrowofstatusimg = statusimg(Rect(0, 50, 600, 50));
	//Mat thirdrowofstatusimg = statusimg(Rect(0, 100, 600, 50));
	Mat fourthrowofstatusimg = statusimg(Rect(0, 150, 600, 50));
	//Mat fifthrowofstatusimg = statusimg(Rect(0, 200, 600, 50));
	//Mat sixthrowofstatusimg = statusimg(Rect(0, 250, 600, 50));
	
	sprintf(textbuffer, "Index =  %d", indexi );
	fourthrowofstatusimg = Mat::zeros(cv::Size(600, 50), CV_64F);
	putText(statusimg, textbuffer, Point(0, 180), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 3, 1);
	 
	imshow("Status", statusimg);

}

inline void printStatusDC(bool bupper, Mat& statusimg)
{
	 
	char textbuffer[80];
	//Mat firstrowofstatusimg = statusimg(Rect(0, 0, 600, 50));
	//Mat secrowofstatusimg = statusimg(Rect(0, 50, 600, 50));
	Mat thirdrowofstatusimg = statusimg(Rect(0, 100, 600, 50));
	//Mat fourthrowofstatusimg = statusimg(Rect(0, 150, 600, 50));
	//Mat fifthrowofstatusimg = statusimg(Rect(0, 200, 600, 50));
	//Mat sixthrowofstatusimg = statusimg(Rect(0, 250, 600, 50));
	
	if(bupper==1)
		sprintf(textbuffer, "Using upper 5 pix as DC" );
	else
		sprintf(textbuffer, "Using lower 5 pix as DC" );
	thirdrowofstatusimg = Mat::zeros(cv::Size(600, 50), CV_64F);
	putText(statusimg, textbuffer, Point(0, 130), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 3, 1);
	 
	imshow("Status", statusimg);

}

inline void makeonlypositive(Mat& src, Mat& dst)
{
	// from https://stackoverflow.com/questions/48313249/opencv-convert-all-negative-values-to-zero
	max(src, 0, dst);

}



int main(int argc, char *argv[])
{
	cv::Mat m, imgoverlay, bscan[1001], temp;
	Mat zvals, zvalslin, ascanlin;
	Point minLoc, maxLoc;
	double minVal, maxVal, DCVal, peak, halfpeak;
	Scalar meantemp;
	bool exists[1001], bupper=0;
	for (int indexi = 0; indexi<1001; indexi++)
	{
		exists[indexi]=0;
	}
	int jj, lhshm, rhshm, FWHMpix;
	cv::FileStorage fs("BscanFFT.xml", cv::FileStorage::READ);
	std::ofstream outfile("FWHMcount.csv");
	char stringvar[80], studyId[40];
	Mat statusimg = Mat::zeros(cv::Size(600, 300), CV_64F);
	
	bool doneflag = 0, skeypressed = 0, nkeypressed = 0, pkeypressed = 0;
	bool jlockin = 0, jkeypressed = 0, ckeypressed = 0;
	int indexi;
	
	int key=0;
	
	std::cout<<"Type OCT study ID like 012R or 123L and press Enter :";
	std::cin>>studyId;
	std::cout<<std::endl<<"Study ID entered as "<<studyId<<" ....reading data..."<<std::endl;

	//fs["camgain"] >> camgain;
	//fs["camtime"] >> camtime;
	//fs["normfactor"] >> normfactor;
	//fs["bscan001"] >> bscan;
	
	namedWindow("Bscan", 0); // 0 = WINDOW_NORMAL
	moveWindow("Bscan", 800, 0);
	
	namedWindow("Status", 0); // 0 = WINDOW_NORMAL
	moveWindow("Status", 0, 500);


	for (indexi = 0; indexi<1001; indexi++)
	{
		sprintf(stringvar, "bscan%03d", indexi);
		
		if (!fs[stringvar].isNone())
			{
				fs[stringvar] >> bscan[indexi];
				exists[indexi]=1;
				
			}
			
	}
	
	indexi=0;
	
	while(1)
	{
		if (exists[indexi]==1)
		{
			normalize(bscan[indexi], m, 0, 1, NORM_MINMAX);
			m.copyTo(imgoverlay);
			// void rectangle(Mat& img, Point pt1, Point pt2, const Scalar& color, int thickness=1, 
					//			int lineType=8, int shift=0)
			rectangle(imgoverlay, Point(ROIposx, ROIposy), Point(ROIposx + ROIw, ROIposy + ROIh), Scalar(255, 255, 255), 1);

			imshow("Bscan", imgoverlay);
			setMouseCallback("Bscan", onMouse);
			
			if (skeypressed==1)
			// do all the computations
			if (ROIw > 5 and ROIh > 5) // meaning ROI has been initialized
			{
				zvals = bscan[indexi](Rect(ROIposx, ROIposy, ROIw, ROIh));
				// debug
				//imshow("debug",zvals);
				
				//convert to linear from dB
				//zvals=10^(zvals/20);
				zvals.copyTo(zvalslin);
				zvalslin=zvals*0.05;
				exp(zvalslin, temp);
				zvalslin=2.303*temp;
				// debug
				//normalize(zvalslin, zvalslin, 0, 1, NORM_MINMAX);
				//imshow("debug",zvalslin);
				
				// now loop through each Ascan
				for(int ii=0; ii<ROIw;ii++)
				{
					ascanlin=zvalslin.col(ii);
					// find the max value
					minMaxLoc(ascanlin, &minVal, &maxVal, &minLoc, &maxLoc);
					
					if(bupper==0)
					{
						// DC is taken to be the average of the lowermost 5 pixels
						meantemp = mean(ascanlin.rowRange(ROIh-5,ROIh-1));
					}
					else
					{
						// DC is taken to be the average of the uppermost 5 pixels
						meantemp = mean(ascanlin.rowRange(0,4));
					}
					DCVal=meantemp[0];	// since we are using only a single channel
					//debug
					//std::cout<<DCVal<<std::endl;
					//std::cout<<maxLoc.x<<","<<maxLoc.y<<std::endl; // maxLoc.y is the relevant one
					
					peak = maxVal - DCVal;
					halfpeak = peak/2;
					halfpeak = halfpeak + DCVal;
					
					//loop to find lhs
					for (jj=0; jj<maxLoc.y; jj++)
					{
						if (ascanlin.at<double>(jj,0)>halfpeak)
							break;
					}
					lhshm=jj;
					
					//loop to find rhs
					for (jj=maxLoc.y; jj<ROIh; jj++)
					{
						if (ascanlin.at<double>(jj,0)<halfpeak)
							break;
					}
					rhshm=jj;
					
					FWHMpix = rhshm - lhshm;
					//debug
					//std::cout<<FWHMpix<<std::endl;
					outfile<<studyId<<", "<<indexi<<", "<<ii<<", "<<FWHMpix<<std::endl;
					
				}	// end of ascan for loop
				skeypressed=0;
			}	// end of if skeypressed and ROI selected computation block
			
			
			
			
			
		}
		else 		// such a bscan[indexi] does not exist
		if (indexi<1000)
			indexi++;
			else
			indexi==0;
			
		printStatus(indexi, statusimg);
	
	
				key = waitKey(30); // wait 30 milliseconds for keypress
								  // max frame rate at 1280x960 is 30 fps => 33 milliseconds

				switch (key)
				{

				case 27: //ESC key
				case 'x':
				case 'X':
					doneflag = 1;
					break;

				
				case 's':
				case 'S':

					skeypressed = 1;
					break;
					
				case 'u':
				case 'U':

					//toggle the upper flag
					if(bupper==0)
						bupper=1;
					else
						bupper=0;
					printStatusDC(bupper, statusimg);
					break;

				case 'n':
				case 'N':

					if (indexi<1000)
						indexi++;
						 
					if (exists[indexi]==0)	// don't allow the increment if no frame exists
						indexi--;
					break;

				case 'p':
				case 'P':

					if (indexi>0)
						indexi--;
						
					break;

				case 'j':
				case 'J':

					jkeypressed = 1;
					break;

				case 'c':
				case 'C':

					ckeypressed = 1;
					break;

				
				
				
				default:
					break;

				}
				
		if (doneflag == 1)
				{
					break;
				}

	}	// end while(1)

				

			

#ifdef __unix__
	/*outfile << "% Parameters were - camgain, camtime, bpp, w , h , camspeed, usbtraffic, binvalue, bscanthreshold" << std::endl;
	outfile << "% " << camgain;
	outfile << ", " << camtime;
	outfile << ", " << bpp;
	outfile << ", " << w;
	outfile << ", " << h;
	outfile << ", " << camspeed;
	outfile << ", " << usbtraffic;
	outfile << ", " << binvalue;
	outfile << ", " << int(bscanthreshold);





#else
	  //imwrite("bscan.png", normfactorforsave*bscan);


	outfile << "camgain" << camgain;
	outfile << "camtime" << camtime;
	outfile << "bscanthreshold" << int(threshold);*/


#endif



	return 0;

failure:
	printf("Fatal error !! \n");
	return 1;
}

