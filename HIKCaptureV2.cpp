#include "HIKcaptureV2.h"  

using namespace std;
using namespace boost;
using namespace boost::this_thread;
using namespace cv;

HANDLE HIKCapture::hTh1, HIKCapture::hTh2;
unsigned HIKCapture::thID1, HIKCapture::thID2;
int HIKCapture::iMaxFrameCacheNum;
int HIKCapture::giCameraFrameWidth;
int HIKCapture::giCameraFrameHeight;
CRITICAL_SECTION HIKCapture::g_cs_frameList;
std::list<char *> HIKCapture::g_frameList;
int HIKCapture::iFrameRow;
int HIKCapture::iFrameCol;
int HIKCapture::iFPS;
LONG HIKCapture::lPort;
HWND HIKCapture::hWnd;

typedef struct FrameRepository FrameRepository;

FrameRepository HIKCapture::gFrameRepository;

HIKCapture::HIKCapture()
{
	HWND hWnd = NULL;
	iFrameRow = 1440;
	iFrameCol = 2560;
	iFPS = 50;
	iMaxFrameCacheNum = 30;

}

HIKCapture::~HIKCapture()
{
	//delete [] ;
}

IplImage* HIKCapture::resizing(IplImage *source)
{
	IplImage *desI = cvCreateImage(cvSize(iFrameCol, iFrameRow), source->depth, source->nChannels);
	cvResize(source, desI);
	return desI;
}

cv::Mat HIKCapture::Yv12ToRGB(uchar *pBuffer, int width, int height)
{
	cv::Mat result(height, width, CV_8UC3);
	uchar y, cb, cr;

	long ySize = width*height;
	long uSize;
	uSize = ySize >> 2;

	//assert(bufferSize == ySize + uSize * 2);

	uchar *output = result.data;
	uchar *pY = pBuffer;
	uchar *pU = pY + ySize;
	uchar *pV = pU + uSize;

	uchar r, g, b;
	for (int i = 0; i<uSize; ++i)
	{
		for (int j = 0; j<4; ++j)
		{
			y = pY[i * 4 + j];
			cb = pU[i];
			cr = pV[i];

			//ITU-R standard
			b = saturate_cast<uchar>(y + 1.772*(cb - 128));
			g = saturate_cast<uchar>(y - 0.344*(cb - 128) - 0.714*(cr - 128));
			r = saturate_cast<uchar>(y + 1.402*(cr - 128));

			*output++ = b;
			*output++ = g;
			*output++ = r;
		}
	}
	return result;
}

void HIKCapture::yv12toYUV(char *outYuv, char *inYv12, int width, int height, int widthStep)
{
	int col, row;
	unsigned int Y, U, V;
	int tmp;
	int idx;

	for (row = 0; row<height; row++)
	{
		idx = row * widthStep;
		int rowptr = row*width;

		for (col = 0; col<width; col++)
		{

			tmp = (row / 2)*(width / 2) + (col / 2);
			Y = (unsigned int)inYv12[row*width + col];
			U = (unsigned int)inYv12[width*height + width*height / 4 + tmp];
			V = (unsigned int)inYv12[width*height + tmp];
			outYuv[idx + col * 3] = Y;
			outYuv[idx + col * 3 + 1] = U;
			outYuv[idx + col * 3 + 2] = V;
		}
	}
	//cout << "frame color transformed: " << "col = " << col << "; row = " << row << endl;
}

void CALLBACK HIKCapture::DecCBFun(long lPort, char * pBuf, long nSize, FRAME_INFO * pFrameInfo, long nReserved1, long nReserved2)
{
	long lFrameType = pFrameInfo->nType;
	giCameraFrameWidth = pFrameInfo->nWidth;
	giCameraFrameHeight = pFrameInfo->nHeight;
	//cout << "you are here" << endl;
	if (lFrameType == T_YV12)
	{
		EnterCriticalSection(&g_cs_frameList);
		g_frameList.push_back(pBuf);

		if (g_frameList.size() > iMaxFrameCacheNum)
		{
			g_frameList.pop_front();
			//cout << "cache max volume" << endl;
		}
		//cout << "frame pushed" << endl;
		LeaveCriticalSection(&g_cs_frameList);
	}
}

void CALLBACK HIKCapture::fRealDataCallBack(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void *pUser)
{
	//HWND hWnd = GetConsoleWindow();
	//DWORD dRet;
	switch (dwDataType)
	{
	case NET_DVR_SYSHEAD:    //system head
		if (!PlayM4_GetPort(&lPort)) //unavailable port 
		{
			break;
		}
		if (dwBufSize > 0)
		{
			if (!PlayM4_SetStreamOpenMode(lPort, STREAME_REALTIME))  //set strean mode
			{
				break;
			}
			if (!PlayM4_OpenStream(lPort, pBuffer, dwBufSize, iFrameCol * iFrameRow))
			{
				//dRet = PlayM4_GetLastError(lPort);
				break;
			}
			//decoding callback, no show
			if (!PlayM4_SetDecCallBack(lPort, DecCBFun))//DecCBFun
			{
				//dRet = PlayM4_GetLastError(lPort);
				break;
			}
			//open decoding
			if (!PlayM4_Play(lPort, hWnd)) // play the video stream
			{
				//dRet = PlayM4_GetLastError(lPort);
				break;
			}
		}
		break;

	case NET_DVR_STREAMDATA:
		if (dwBufSize > 0 && lPort != -1)
		{
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize))
			{
				break;
			}
		}
		break;
	default:
		if (dwBufSize > 0 && lPort != -1)
		{
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize))
			{
				break;
			}
		}
		break;
	}
}

void CALLBACK HIKCapture::g_RealDataCallBack_V30(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void* dwUser)
{
	HWND hWnd = GetConsoleWindow();

	switch (dwDataType)
	{
	case NET_DVR_SYSHEAD:

		if (!PlayM4_GetPort(&lPort))  //获取播放库未使用的通道号
		{
			break;
		}
		//m_iPort = lPort; //第一次回调的是系统头，将获取的播放库port号赋值给全局port，下次回调数据时即使用此port号播放
		if (dwBufSize > 0)
		{
			if (!PlayM4_SetStreamOpenMode(lPort, STREAME_REALTIME))  //设置实时流播放模式
			{
				break;
			}

			if (!PlayM4_OpenStream(lPort, pBuffer, dwBufSize, 1280 * 720))
			{
				break;
			}

			if (!PlayM4_Play(lPort, hWnd))
			{
				break;
			}
		}
		break;
	case NET_DVR_STREAMDATA:
		if (dwBufSize > 0 && lPort != -1)
		{
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize))
			{
				break;
			}
		}
		break;
	default:
		if (dwBufSize > 0 && lPort != -1)
		{
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize))
			{
				break;
			}
		}
		break;
	}
}

void CALLBACK HIKCapture::g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
	char tempbuf[256] = { 0 };
	switch (dwType)
	{
	case EXCEPTION_RECONNECT:
		printf("----------reconnect--------%d\n", time(NULL));
		break;
	default:
		break;
	}
}

unsigned __stdcall  HIKCapture::CameraThread(void *param)
{
	cout << "Enter camera thread" << endl; // do not change codes here
										   //---------------------------------------  
										   // 初始化  
	NET_DVR_Init();
	//设置连接时间与重连时间  
	NET_DVR_SetConnectTime(2000, 1);
	NET_DVR_SetReconnect(10000, true);

	//---------------------------------------  
	//设置异常消息回调函数  
	NET_DVR_SetExceptionCallBack_V30(0, NULL, g_ExceptionCallBack, NULL);

	// 注册设备  
	LONG lUserID;

	//登录参数，包括设备地址、登录用户、密码等
	NET_DVR_USER_LOGIN_INFO struLoginInfo = { 0 };
	struLoginInfo.bUseAsynLogin = 0; //同步登录方式
	strcpy_s(struLoginInfo.sDeviceAddress, "0.0.0.0"); //设备IP地址
	struLoginInfo.wPort = 8000; //设备服务端口
	strcpy_s(struLoginInfo.sUserName, "admin"); //设备登录用户名
	strcpy_s(struLoginInfo.sPassword, "admin"); //设备登录密码

												   //设备信息, 输出参数
	NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = { 0 };

	cout << "start login" << endl;

	lUserID = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
	if (lUserID < 0)
	{
		printf("Login failed, error code: %d\n", NET_DVR_GetLastError());
		NET_DVR_Cleanup();
		return 0;
	}

	cout << "login success" << endl;

	LONG lRealPlayHandle;
	//HWND hWnd = GetConsoleWindow(); Do not set the handle twice, or you get shacking image.
	NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
	struPlayInfo.hPlayWnd = hWnd;         //需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
	struPlayInfo.lChannel = 1;       //预览通道号
	struPlayInfo.dwStreamType = 0;       //0-主码流，1-子码流，2-码流3，3-码流4，以此类推
	struPlayInfo.dwLinkMode = 0;       //0- TCP方式，1- UDP方式，2- 多播方式，3- RTP方式，4-RTP/RTSP，5-RSTP/HTTP

									   //lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL);
	lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, fRealDataCallBack, NULL);
	if (lRealPlayHandle < 0)
	{
		printf("NET_DVR_RealPlay_V40 error, %d\n", NET_DVR_GetLastError());
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return 0;
	}

	//cvWaitKey(0);  
	Sleep(-1); //-1 : forever;

			   //---------------------------------------  
			   //关闭预览  
	if (!NET_DVR_StopRealPlay(lRealPlayHandle))
	{
		printf("NET_DVR_StopRealPlay error! Error number: %d\n", NET_DVR_GetLastError());
		return 0;
	}
	//log out
	NET_DVR_Logout(lUserID);
	NET_DVR_Cleanup();

	_endthreadex(0);
	return 0;
}

unsigned __stdcall  HIKCapture::ImgConvertThread(void *param)
{
	cout << "Img convert thread started" << endl;  // do not change codes here

	list<char *>::iterator it;
	IplImage* pImgYCrCb;
	IplImage* pImg;
	IplImage* pImResize;
	cv::Mat frameTmp;

	while (1)
	{
		if (g_frameList.size()>0) {
			EnterCriticalSection(&g_cs_frameList);
			it = g_frameList.end();
			it--;
			pImgYCrCb = cvCreateImage(cvSize(giCameraFrameWidth, giCameraFrameHeight), 8, 3);
			yv12toYUV(pImgYCrCb->imageData, (*(it)), giCameraFrameWidth, giCameraFrameHeight, pImgYCrCb->widthStep);
			LeaveCriticalSection(&g_cs_frameList);
			pImg = cvCreateImage(cvSize(giCameraFrameWidth, giCameraFrameHeight), 8, 3);
			cvCvtColor(pImgYCrCb, pImg, CV_YCrCb2RGB);
			pImResize = resizing(pImg);
			frameTmp = cv::cvarrToMat(pImResize); // IplImage to MAT;
			ProduceFrameItem(&gFrameRepository, frameTmp);
			cvReleaseImage(&pImgYCrCb);
			cvReleaseImage(&pImResize);
			cvReleaseImage(&pImg);
			Sleep(1000 / iFPS); //set FPS
		}
	}

	cout << "end frame selection thread" << endl;
	Sleep(-1); //-1 : forever;
	_endthreadex(0);
	return 0;
}

void HIKCapture::ProduceFrameItem(FrameRepository *ir, cv::Mat item)
{
	unique_lock<mutex> lock(ir->mtx);
	while (((ir->write_position + 1) % LEN)
		== ir->read_position) { // item buffer is full, just wait here.
		//std::cout << "FrameProducer is waiting for an empty slot...\n";
		(ir->repo_not_full).wait(lock); // 生产者等待"frame库缓冲区不为满"这一条件发生.
	}

	(ir->item_buffer)[ir->write_position] = item.clone();
	(ir->write_position)++; // 写入位置后移.

	if (ir->write_position == LEN) // 写入位置若是在队列最后则重新设置为初始位置.
		ir->write_position = 0;

	(ir->repo_not_empty).notify_all(); // 通知消费者frame库不为空.
	lock.unlock();
}

cv::Mat HIKCapture::GetFrameItem(FrameRepository *ir)
{
	unique_lock<mutex> lock(ir->mtx);
	// item buffer is empty, just wait here.
	while (ir->write_position == ir->read_position) {
		//std::cout << "waiting for frames...\n";
		(ir->repo_not_empty).wait(lock); // 消费者等待"frame库缓冲区不为空"这一条件发生.
	}
	cv::Mat data = (ir->item_buffer)[ir->read_position].clone();
	(ir->read_position)++; // 读取位置后移
	if (ir->read_position >= LEN) // 读取位置若移到最后，则重新置位.
		ir->read_position = 0;

	(ir->repo_not_full).notify_all(); // 通知消费者frame库不为满.
	lock.unlock(); // 解锁.
	return data; // 返回frame.
}

void HIKCapture::FrameDispTask() // 消费者任务
{
	while (1) {
		Sleep(1000 / (iFPS - 10)); // set the disp FPS here,  better waiting for frames insted of waiting for empty slot.
		cv::Mat data = GetFrameItem(&gFrameRepository);
		imshow("HIK Camera", data);
		cvWaitKey(1);
	}
}

void HIKCapture::InitFrameRepository(FrameRepository *ir)
{
	ir->write_position = 0; // 初始化frame写入位置.
	ir->read_position = 0; // 初始化frame读取位置.

	cv::Mat matrix = cv::Mat(iFrameRow, iFrameCol, CV_32F, cv::Scalar::all(0));

	for (int i = 0; i < LEN; i++)
	{
		ir->item_buffer[i] = matrix.clone(); // write in zeros
	}
}


void HIKCapture::initialHIKCamera()
{
	InitFrameRepository(&gFrameRepository);
	InitializeCriticalSection(&g_cs_frameList);
	cout << "start camera thread" << endl;
	hTh1 = (HANDLE)_beginthreadex(NULL, 0, &CameraThread, NULL, 0, &thID1);
	cout << "camera thread created" << endl;
	cout << "start imgConvert thread" << endl;
	hTh2 = (HANDLE)_beginthreadex(NULL, 0, &ImgConvertThread, NULL, 0, &thID2);
	cout << "imgConvert thread created" << endl;
	//boost::thread FrameConsumer(&HIKCapture::FrameDispTask);
	//FrameConsumer.join();

	//Sleep(-1);
}

void HIKCapture::clearHIKCameraSession()
{
	CloseHandle(hTh1);
	CloseHandle(hTh2);
	return;
}
