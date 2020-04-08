#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <commctrl.h>

#pragma comment( lib, "comctl32.lib")

  
// Loaded Jpeg details
static HDC ImageDc = 0;
static uint16_t ImageWth = 0;
static uint16_t ImageHt = 0;



// ** LdB the list box had an ID of 7000 it coccurs a few places I dragged it up here to a constant which is how it should be done
//  I have replaced the number 7000 where it occurred
#define ID_LISTBOX 7000
// this is the second listbox ID
#define ID_LISTBOX1 7001
 

/*-[ CreateFullFilename ]---------------------------------------------------}
.  Creates a full filename from a shortname.
.--------------------------------------------------------------------------*/
bool CreateFullFilename (const char* ShortName, char* NameBuffer, uint16_t BufSize)
{
	if (NameBuffer && ShortName && BufSize > 0)							// Check pointers and buffer size
	{
		uint16_t len = (uint16_t)GetModuleFileName(GetModuleHandle(NULL),
			NameBuffer, BufSize);										// Transfer full path to EXE to buffer
		if (len && GetLastError() == 0)									// It copied name to buffer and it fitted
		{
			char* pStr = strrchr(NameBuffer, '\\');						// Find directory slash between EXE name and path		
			if (pStr) *(++pStr) = '\0';									// Terminate the string there
			if (strcat_s(NameBuffer, BufSize, ShortName) == 0)			// Add shortname to path and check it worked  
				return true;											// Return success
		}
	}
	return false;														// Any failure will return false
}

/*-[ MemDCFromJPGFile ]-----------------------------------------------------}
.  Creates a Memory Device context from the the JPEG filename. Due to the
.  way OLE works the filename must be fully qualified (ie c:\folder\a.jpg)
.  Width and height of the JPG can be return by providing valid pointers or
.  NULL can be used if the values are not required. The function will return
.  NULL for any error, otherwise returns a valid Memory Device Contest.
.  Caller must take responsibility for disposal of the Device Context.
.--------------------------------------------------------------------------*/
#include "olectl.h"													// OLE is used for JPG image loading
#define HIMETRIC_PER_INCH ( 2540 )
HDC MemDCFromJPGFile (const char* Filename, uint16_t* Wth, uint16_t* Ht)
{
	HDC MemDC = 0;													// Preset return value to fail
	if (Filename)													// Check filename pointer valid 
	{
		IPicture* Ipic = NULL;
		WCHAR OleFQN[_MAX_PATH];
		size_t len = strlen(Filename);								// Hold the filename string length
		MultiByteToWideChar(850, 0, Filename, len + 1,
			&OleFQN[0], _countof(OleFQN));							// Convert filename to unicode
		HRESULT hr = OleLoadPicturePath(OleFQN, NULL, 0, 0,
			&IID_IPicture, (LPVOID*)&Ipic);							// Load the picture
		if ((hr == S_OK) && (Ipic != 0)) 							// Check picture loaded okay	
		{
			SIZE sizeInHiMetric;
			HDC Dc = GetDC(NULL);									// Get screen DC
			int nPixelsPerInchX = GetDeviceCaps(Dc, LOGPIXELSX);	// Screen X pixels per inch 
			int nPixelsPerInchY = GetDeviceCaps(Dc, LOGPIXELSY);	// Screen Y pixels per inch
			ReleaseDC(NULL, Dc);									// Release screen DC
			Ipic->lpVtbl->get_Width(Ipic, &sizeInHiMetric.cx);		// Get picture witdh in HiMetric
			Ipic->lpVtbl->get_Height(Ipic, &sizeInHiMetric.cy);		// Get picture height in HiMetric
			int Tw = (nPixelsPerInchX * sizeInHiMetric.cx +
				HIMETRIC_PER_INCH / 2) / HIMETRIC_PER_INCH;			// Calculate picture width in pixels
			int Th = (nPixelsPerInchY * sizeInHiMetric.cy +
				HIMETRIC_PER_INCH / 2) / HIMETRIC_PER_INCH;			// Calculate picture height in pixels
			if (Wth) (*Wth) = Tw;									// User requested width result
			if (Ht) (*Ht) = Th;										// User request height result
			MemDC = CreateCompatibleDC(0);							// Create a memory device context
			if (MemDC)												// Check memory context created				
			{
				// Create A Temporary Bitmap
				BITMAPINFO	bi = { 0 };								// The Type Of Bitmap We Request
				DWORD* pBits = 0;									// Pointer To The Bitmap Bits
				bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);		// Set Structure Size
				bi.bmiHeader.biBitCount = 32;						// 32 Bit
				bi.bmiHeader.biWidth = Tw;							// Image width
				bi.bmiHeader.biHeight = Th;							// Make Image Top Up (Positive Y-Axis)
				bi.bmiHeader.biCompression = BI_RGB;				// RGB Encoding
				bi.bmiHeader.biPlanes = 1;							// 1 Bitplane
				HBITMAP bmp = CreateDIBSection(MemDC, &bi,
					DIB_RGB_COLORS, (void**)&pBits, 0, 0);			// Create a DIB section
				if (bmp)
				{
					SelectObject(MemDC, bmp);						// Select the bitmap to the memory context															
					hr = Ipic->lpVtbl->Render(Ipic, MemDC, 0, 0, Tw, Th,
						0, sizeInHiMetric.cy, sizeInHiMetric.cx,
						-sizeInHiMetric.cy, NULL);					// Render the image to the DC
					DeleteObject(bmp);								// Delete the bitmap section we are finished with
				}
				else {												// DIB section create failed	
					DeleteObject(MemDC);							// Delete the memory DC
					MemDC = 0;										// Reset the memory DC back to zero to fail out
				}
			}
			Ipic->lpVtbl->Release(Ipic);							// Finised with Ipic
		}
	}
	return MemDC;													// Return the memory context
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_CREATE:
		{
			HWND List;
			List = CreateWindowEx(WS_EX_CLIENTEDGE, // extended window styles
				WC_LISTBOX,					 // list box window class name
				NULL,
				WS_CHILD | WS_VISIBLE | WS_VSCROLL,		 // window styles
				50,							 // horizontal position
				10,							 // vertical position
				200,						 // width
				200,						 // height
				hwnd,
				NULL,
				0,
				NULL);
			char buf[256];
			int i;
			for (i = 0; i < 20; i++)
			{
				sprintf_s(buf, 256, "List1 Item %i", i);
				SendMessage(List, LB_ADDSTRING, i, (LPARAM)&buf[0]);
			}
			List = CreateWindowEx(WS_EX_CLIENTEDGE, // extended window styles
				WC_LISTBOX,					 // list box window class name
				NULL,
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_GROUP,		 // window styles
				350,						 // horizontal position
				10,							 // vertical position
				200,						 // width
				200,						 // height
				hwnd,
				NULL,
				0,
				NULL);
			for (i = 0; i < 15; i++)
			{
				sprintf_s(buf, 256, "List2 Item %i", i);
				SendMessage(List, LB_ADDSTRING, i, (LPARAM)&buf[0]);
			}
			return 0L;
		}
		// **LdB We accept this message so we can set a minimum window size the users can drag it down too
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpInfo = (LPMINMAXINFO)lParam;
			if (lpInfo) {
				lpInfo->ptMinTrackSize.x = ImageWth + 50;
				lpInfo->ptMinTrackSize.y = ImageHt + 50;
			}
			return 0;
		}
		/** LdB  **/
		// These next two messages are better to use rather than WM_MOVE/WM_SIZE.
		// Remember WM_MOVE/WM_SIZE are from 16bit windows. In 32bit windows the window
		// manager only sends these two messages and the DefWindowProc() handler actually
		// accepts them and converts them to WM_MOVE/WM_SIZE.
		// 
		// We accept this so we can scale our controls to the client size.
		case WM_WINDOWPOSCHANGING:
		case WM_WINDOWPOSCHANGED:
			{
				HDWP hDWP;
				RECT rc;
				
				// Create a deferred window handle.
				// **LdB Deferring 2 child control at the moment (you now have 2 controls in window listbox, listbox1)
				if(hDWP = BeginDeferWindowPos(2)){ 
					GetClientRect(hwnd, &rc);
					
					// **LdB This time I use the SWP_NOMOVE & SWP_NOSIZE so the position and size isn't change 
					// from what you set in the WM_CREATE
					hDWP = DeferWindowPos(hDWP, GetDlgItem(hwnd, ID_LISTBOX), NULL,
						0, 0, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);

					// **LdB This time I use the SWP_NOMOVE & SWP_NOSIZE so the position and size isn't change 
					// from what you set in the WM_CREATE
					hDWP = DeferWindowPos(hDWP, GetDlgItem(hwnd, ID_LISTBOX1), NULL,
						0, 0, 0, 0, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);

					// Resize all windows under the deferred window handled at the same time.
					EndDeferWindowPos(hDWP);
					
	                // Now invalidate the window area to force redraw
					InvalidateRect(hwnd, 0, TRUE);
				}
			}
			return 0;
		case WM_ERASEBKGND:											// WM_ERASEBKGND
			if (ImageDc) 											// Context is valid
			{
				RECT r;
				GetClientRect(hwnd, &r);							// Get client area
				StretchBlt((HDC)wParam, 0, 0, r.right - r.left,
					r.bottom - r.top, ImageDc, 0, 0, ImageWth, ImageHt,
					SRCCOPY);										// Transfer bitmap
				return 1;											// Return nonzero if it erases the background else zero. 
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
 
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
 
	// initialize common controls
	INITCOMMONCONTROLSEX iccex = { 0 };
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES ;
	InitCommonControlsEx(&iccex);

	char Filename[MAX_PATH];
	CreateFullFilename("beauty.jpg", Filename, MAX_PATH);
	ImageDc = MemDCFromJPGFile(Filename, &ImageWth, &ImageHt);

	WNDCLASSEX wc = { 0 };
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = 0;
	wc.lpfnWndProc	 = WndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hInstance;
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "ListViewDemo";
	wc.hIconSm		 = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&wc);

	CreateWindowEx( WS_EX_CLIENTEDGE, wc.lpszClassName,
		"The title of my window", 
        WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 
		ImageWth+50, ImageHt+50, NULL, NULL, hInstance, NULL);
 
	MSG Msg;
	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}