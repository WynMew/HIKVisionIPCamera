// multi-thread c98
#ifndef HIKCAPTURE_H_
#define HIKCAPTURE_H_

#include <cstdio>  
#include <cstring>  
#include <iostream>  
#include "Windows.h"  
#include "HCNetSDK.h"  
#include "plaympeg4.h"  
#include <opencv2\opencv.hpp>  
#include <opencv\cv.h>
#include <opencv\highgui.h>  
#include <time.h>  
#include <process.h>  
#include <boost\thread\thread.hpp>
#include <boost\thread\mutex.hpp>
#include <boost\thread\condition.hpp>
#include <boost\thread\locks.hpp>
#include <boost\chrono.hpp>


#define LEN 2
struct FrameRepository {
	cv::Mat item_buffer[LEN]; 
	size_t read_position;
	size_t write_position;
	boost::mutex mtx;
	boost::condition_variable repo_not_full;
	boost::condition_variable repo_not_empty; 
};


class HIKCapture {
	//--------------------------------------------  
	// Global parameters & variables
private:
	static HANDLE hTh1, hTh2;
	static unsigned thID1, thID2;
	static int iMaxFrameCacheNum;
	static int giCameraFrameWidth;
	static int giCameraFrameHeight;
	static CRITICAL_SECTION g_cs_frameList;
	static std::list<char *> g_frameList;
	static int iFrameRow;
	static int iFrameCol;
	static int iFPS;
	static LONG lPort;
	static HWND hWnd;
	//static cv::Mat* mFrameQuo;

	// variables for debug
	//static int iFrameRepositorySize; // Item buffer size.
	//static int iItemsToProduce;   // How many items we plan to produce.

public:
	HIKCapture();
	~HIKCapture();
	void initialHIKCamera();
	void clearHIKCameraSession();
	static FrameRepository gFrameRepository;
	static cv::Mat HIKCapture::GetFrameItem(FrameRepository *ir);
private:
	static IplImage* resizing(IplImage *source);
	static cv::Mat Yv12ToRGB(uchar *pBuffer, int width, int height);
	static void yv12toYUV(char *outYuv, char *inYv12, int width, int height, int widthStep);
	static void CALLBACK DecCBFun(long lPort, char * pBuf, long nSize, FRAME_INFO * pFrameInfo, long nReserved1, long nReserved2);
	static void CALLBACK fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void *pUser);
	static void CALLBACK g_RealDataCallBack_V30(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void* dwUser);
	static void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser);
	static unsigned __stdcall  CameraThread(void *param);
	static unsigned __stdcall  ImgConvertThread(void *param);
	static void HIKCapture::ProduceFrameItem(FrameRepository *ir, cv::Mat item);
	//static void HIKCapture::FrameProducerTask();
	static void HIKCapture::FrameDispTask();
	void HIKCapture::InitFrameRepository(FrameRepository *ir);

};

#endif //HIKCAPTURE_H_
