// vim: set ts=2 sts=2 sw=2 et ai cin:
/*
 * Copyright (c) 2013 Reitaro IZU
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define _WIN32_WINNT 0x0500
#define _UNICODE
#define UNICODE
#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <tchar.h>

using namespace Gdiplus;

/////////////////////

// Emulating Unicode handling of Visual Studio.
// Ugh! Is this correct?

#ifndef _T
# ifdef UNICODE
#  define _T(x) L##x
# else
#  define _T(x) (x)
# endif
#endif

#ifndef _tcscpy
# ifdef UNICODE
#  define _tcscpy wcscpy
# else
#  define _tcscpy strcpy
# endif
#endif

#ifndef _tcslen
# ifdef UNICODE
#  define _tcslen wcslen
# else
#  define _tcslen strlen
# endif
#endif

#ifndef _tcscat
# ifdef UNICODE
#  define _tcscat wcscat
# else
#  define _tcscat strcat
# endif
#endif

#ifndef _tsplitpath
# ifdef UNICODE
#  define _tsplitpath _wsplitpath
# else
#  define _tsplitpath _splitpath
# endif
#endif

/////////////////////

int g_opacity = 128;
HBITMAP g_hBitmap = NULL;
int g_bitmapWidth = 0;
int g_bitmapHeight = 0;
TCHAR g_filePath[MAX_PATH] = { 0 };

NOTIFYICONDATA g_nif;
HMENU g_hMenu;
HWND g_hOpacityControlWindow = NULL;

const int OPACITY_LIMIT = 10;
const int OPACITY_MIN = OPACITY_LIMIT;
const int OPACITY_MAX = 255 - OPACITY_LIMIT;
#define CLAMP_OPACITY(x) (max(OPACITY_MIN, min(OPACITY_MAX, (x))))
#define CONVERT_TRACKBAR_OPACITY(x) (255 - (x))

/////////////////////

const TCHAR* CONFIG_SECTION_NAME = _T("viewgrafy");

bool privateProfileFilePath(TCHAR* buf, int buflen) {
  const TCHAR* ini_name = _T("config.ini");
  TCHAR fullpath[MAX_PATH];
  TCHAR drive[_MAX_DRIVE];
  TCHAR dir[MAX_PATH];
  TCHAR filename[_MAX_FNAME];
  TCHAR ext[_MAX_EXT];

  if (!GetModuleFileName(NULL, fullpath, MAX_PATH))
    return false;
  _tsplitpath(fullpath, drive, dir, filename, ext);
  if (_tcslen(drive) + _tcslen(dir) + _tcslen(ini_name) + 1 > (unsigned int)buflen)
    return false;

  _tcscpy(buf, drive);
  _tcscat(buf, dir);
  _tcscat(buf, ini_name);
  //wprintf(_T("result: %s\n"), buf);
  return true;
}

void loadConfigure() {
  TCHAR path[MAX_PATH];
  privateProfileFilePath(path, MAX_PATH);

  GetPrivateProfileString(CONFIG_SECTION_NAME, _T("path"), _T(""), g_filePath, MAX_PATH, path);
  int val = GetPrivateProfileInt(CONFIG_SECTION_NAME, _T("opacity"), 128, path);
  g_opacity = CLAMP_OPACITY(val);
}

void saveConfigure() {
  TCHAR path[MAX_PATH];
  privateProfileFilePath(path, MAX_PATH);

  WritePrivateProfileString(CONFIG_SECTION_NAME, _T("path"), g_filePath, path);

  int val = CLAMP_OPACITY(g_opacity);
  TCHAR buf[4];
  _itot(val, buf, 10);
  WritePrivateProfileString(CONFIG_SECTION_NAME, _T("opacity"), buf, path);
}

/////////////////////

class GdiplusInitializer {
public:
  GdiplusInitializer() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_token, &gdiplusStartupInput, NULL);
  }
  ~GdiplusInitializer() {
    GdiplusShutdown(m_token);
  }
private:
  ULONG_PTR m_token;
};

/////////////////////

HBITMAP prepareBitmap(HWND hwnd, const TCHAR* filepath, int& ow, int& oh) {
  Bitmap* bmp = new Bitmap(filepath);
  if (!bmp)
    return NULL;

  int iw = bmp->GetWidth();
  int ih = bmp->GetHeight();
  int fw = GetSystemMetrics(SM_CXSCREEN);
  int fh = GetSystemMetrics(SM_CYSCREEN);
  double rw = (double)fw / iw;
  double rh = (double)fh / ih;
  int w, h;
  if (rw > rh) {
    w = fw; // == iw * rw
    h = ih * rw;
    //printf("%dx%d x %lf = %dx%d (%lf:%lf | %dx%d)\n", iw, ih, rw, w, h, rw, rh, fw, fh);
  } else {
    w = iw * rh;
    h = fh; // == ih * rh
    //printf("%dx%d x %lf = %dx%d (%lf:%lf | %dx%d)\n", iw, ih, rh, w, h, rw, rh, fw, fh);
  }
  ow = w;
  oh = h;

  BITMAPV5HEADER bi;

  ZeroMemory(&bi,sizeof(BITMAPV5HEADER));
  bi.bV5Size        = sizeof(BITMAPV5HEADER);
  bi.bV5Width       = w;
  bi.bV5Height      = h;
  bi.bV5Planes      = 1;
  bi.bV5BitCount    = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask     = 0x00FF0000;
  bi.bV5GreenMask   = 0x0000FF00;
  bi.bV5BlueMask    = 0x000000FF;
  bi.bV5AlphaMask   = 0xFF000000; 

  void *lpBits;
  HDC hdc, hmemdc;
  Graphics* g;

  HBITMAP hbmp = CreateDIBSection(NULL, (BITMAPINFO *)&bi, DIB_RGB_COLORS, 
                                  (void **)&lpBits, NULL, (DWORD)0);
  if (!hbmp)
    goto error_exit0;

  hdc = GetDC(hwnd);
  hmemdc = CreateCompatibleDC(hdc);
  SelectObject(hmemdc, hbmp);
  g = new Graphics(hmemdc);
  if (!g)
    goto error_exit1;
  g->Clear(Color::Transparent);
  g->SetCompositingMode(CompositingModeSourceOver);
  g->DrawImage(bmp, 0, 0, w, h);
  delete g;
  DeleteDC(hmemdc);
  ReleaseDC(hwnd, hdc);

  delete bmp;
  return hbmp;

error_exit1:
  DeleteObject(hbmp);
  // fall through
error_exit0:
  delete bmp;
  return NULL;
}

bool updateWindow(HWND hWnd, const TCHAR* filepath, int opacity) {
  int uWidth = 0, uHeight = 0;
  HBITMAP hBitmap;

  if (filepath) {
    hBitmap = prepareBitmap(hWnd, filepath, uWidth, uHeight);
    g_hBitmap = hBitmap;
    g_bitmapWidth = uWidth;
    g_bitmapHeight = uHeight;
  } else {
    hBitmap = g_hBitmap;
    uWidth = g_bitmapWidth;
    uHeight = g_bitmapHeight;
  }
  if (!hBitmap) {
    //MessageBox(hWnd, _T("Could not read a image file."), _T("Error"), MB_OK | MB_ICONSTOP);
    return false;
  }

  SetWindowPos(hWnd, 0, 0, 0, uWidth, uHeight, SWP_NOACTIVATE | SWP_NOZORDER);

  HDC hmemdc, hdc, hsdc;
  hsdc = GetDC(0);
  hdc = GetDC(hWnd);
  hmemdc = CreateCompatibleDC(hdc);

  SIZE  wndSize;
  wndSize.cx = uWidth;
  wndSize.cy = uHeight;

  POINT po;
  po.x = po.y = 0;

  BLENDFUNCTION blend;
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = opacity;
  blend.AlphaFormat = AC_SRC_ALPHA;

  HGDIOBJ hOldObj = SelectObject(hmemdc, hBitmap);
  BitBlt(hdc, 0, 0, uWidth, uHeight, hmemdc, 0, 0, SRCCOPY | CAPTUREBLT);

  if (!UpdateLayeredWindow(hWnd, hsdc, NULL, &wndSize, hmemdc, &po, 0, &blend, ULW_ALPHA)) {
    //TCHAR strErrMes[80];
    //DWORD err = GetLastError();
    //wsprintf(strErrMes, _T("fail to UpdateLayeredWindow(): %d"), err);
    //MessageBox(hWnd, strErrMes, _T("error"), MB_OK | MB_ICONSTOP);
    return false;
  }

  SelectObject(hmemdc, hOldObj);
  DeleteDC(hmemdc);
  ReleaseDC(hWnd, hdc);
  ReleaseDC(0, hsdc);
  return true;
}

///////////////////////

#define WM_TASKTRAY (WM_USER + 1)

bool registerTaskTrayIcon(HWND hWnd) {
  // Create an entry to the task tray.

  const int ID_TASKTRAY = 1; // The value is a "don't care."  Since we have just one icon,
                             // no need to distinguish icons in the WM_TASKTRAY handler.
  g_nif.cbSize = sizeof(NOTIFYICONDATA);
  g_nif.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), _T("MAINICON"), IMAGE_ICON,
                                 0, 0, LR_DEFAULTCOLOR);
  g_nif.hWnd = hWnd;
  g_nif.uCallbackMessage = WM_TASKTRAY;
  g_nif.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  g_nif.uID = ID_TASKTRAY;
  wcscpy(g_nif.szTip, _T("Viewgrafy"));

  if (!Shell_NotifyIcon(NIM_ADD, &g_nif) && (GetLastError() == ERROR_TIMEOUT)) {
    do {
      Sleep(2000);
      if (Shell_NotifyIcon(NIM_MODIFY, &g_nif)) // Confirm it is not registered yet.
        break;
    } while (!Shell_NotifyIcon(NIM_ADD, &g_nif));
  }
  return true;
}

void unregisterTaskTrayIcon() {
  Shell_NotifyIcon(NIM_DELETE, &g_nif);
}

/////////////////////////

#define ID_EXIT 1001
#define ID_SELECT 1002
#define ID_CHANGEOPACITY 1003

bool createPopupMenu() {
  g_hMenu = CreatePopupMenu();
  if (!g_hMenu)
    return false;
  InsertMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, _T("Exit / 終了(&X)"));
  InsertMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_SELECT, _T("Change Image... / 画像の変更... (&I)"));
  InsertMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_CHANGEOPACITY, _T("Change Opacity... / 透明度を変更... (&O)"));
  return true;
}

void deletePopupMenu() {
  if (g_hMenu)
    DestroyMenu(g_hMenu);
}

/////////////////////////

bool selectFile(HWND hWnd, TCHAR* filepath, const int len) {
  OPENFILENAME ofn;
  TCHAR filename[1];

  ZeroMemory(&ofn, sizeof(ofn));
  ZeroMemory(filename, sizeof(filename));
  ZeroMemory(filepath, len);
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hWnd;
  ofn.lpstrFilter = _T("Image Files\0*.jpeg;*.jpg;*.png;*.bmp\0All files(*.*)\0*.*\0\0");
  ofn.lpstrFile = filepath;
  ofn.lpstrFileTitle = filename;
  ofn.nMaxFile = len;
  ofn.nMaxFileTitle = sizeof(filename);
  ofn.Flags = OFN_FILEMUSTEXIST;
  ofn.lpstrTitle = _T("Choose a File to Show");
  ofn.lpstrDefExt = _T("*.jpeg;*.jpg;*.png;*.bmp");

  bool ret = GetOpenFileName(&ofn);
  return ret;
}

/////////////////////////

LRESULT CALLBACK wndProc_opacityControl(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
  case WM_ACTIVATE:
    if (LOWORD(wParam) == WA_INACTIVE) {
      ShowWindow(hWnd, SW_HIDE);
    }
    break;

  case WM_VSCROLL:
    {
      HWND hSliderWnd = GetWindow(hWnd, GW_CHILD);
      int val = SendMessage(hSliderWnd, TBM_GETPOS, 0, 0);
      int opacity = CONVERT_TRACKBAR_OPACITY(val);
      if (opacity == g_opacity)
        break;
      g_opacity = opacity;
      if (_tcslen(g_filePath) == 0) // File is not specified yet.
        break;
      HWND hParentWnd = GetParent(hWnd);
      if (!updateWindow(hParentWnd, NULL, g_opacity)) {
        DestroyWindow(hParentWnd);
        return 0;
      }
    }
    break;

  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

HWND createOpacityControlWindow(HWND hParentWnd, HINSTANCE hInstance) {
  const TCHAR* OPACITY_CONTROL_WINDOW_CLASS_NAME = _T("viewgrafy_window_opacity");

  // Register the class
  WNDCLASSEX wcex;
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = wndProc_opacityControl;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = NULL;
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
  wcex.lpszMenuName = NULL;
  wcex.lpszClassName = OPACITY_CONTROL_WINDOW_CLASS_NAME;
  wcex.hIconSm = NULL;
  RegisterClassEx(&wcex);

  // Create and set up the window
  HWND hWnd;
  const int WIDTH = 50, HEIGHT = 150;
  hWnd = CreateWindowEx(WS_EX_TOOLWINDOW,
                        OPACITY_CONTROL_WINDOW_CLASS_NAME,
                        _T("ViewgrafyTrack"),
                        WS_POPUP | WS_DLGFRAME,
                        CW_USEDEFAULT, 0,
                        WIDTH, HEIGHT,
                        hParentWnd, NULL, hInstance, NULL);
  if (!hWnd)
    return NULL;

  HWND hSliderWnd;
  RECT rect;
  GetClientRect(hWnd, &rect);
  const int hpadding = 0;

  // TODO Align it center...
  hSliderWnd = CreateWindow(_T("msctls_trackbar32"),
                            NULL,
                            WS_VISIBLE | WS_CHILD | TBS_VERT | TBS_BOTH,
                            rect.left + hpadding,
                            rect.top,
                            rect.right - rect.left - (hpadding * 2),
                            rect.bottom - rect.top,
                            hWnd, (HMENU)10001, hInstance, NULL);
  if (!hSliderWnd)
    return NULL;

  SendMessage(hSliderWnd, TBM_SETRANGE, TRUE, MAKELPARAM(OPACITY_MIN, OPACITY_MAX));
  SendMessage(hSliderWnd, TBM_SETPOS, TRUE, CONVERT_TRACKBAR_OPACITY(g_opacity));
  return hWnd;
}

/////////////////////////

LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
    {
      if (_tcslen(g_filePath) == 0) {
        MessageBox(hWnd, _T("Specify the image file. / 画像ファイルを指定してください。"), _T("First run / 初回起動"), MB_OK);
        TCHAR filepath[MAX_PATH];
        bool b = selectFile(hWnd, filepath, MAX_PATH);
        if (b) {
          _tcscpy(g_filePath, filepath);
        } else {
          break;
        }
      }
      if (!updateWindow(hWnd, g_filePath, g_opacity)) {
        MessageBox(hWnd, _T("Could not open the image file. Specify it again. / 画像ファイルが開けませんでした。指定しなおしてください。"), _T("Error / エラー"), MB_OK | MB_ICONSTOP);
        _tcscpy(g_filePath, _T(""));
        DestroyWindow(hWnd);
        break;
      }
    }
    break;

  case WM_TASKTRAY:
    switch (lParam) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
      {
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hWnd);
        TrackPopupMenu(g_hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
        return 0;
      }
      break;
    default:
      break;
    }
    break;

  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case ID_EXIT: 
      DestroyWindow(hWnd);
      break;
    case ID_SELECT:
      {
        TCHAR filepath[MAX_PATH];
        bool b = selectFile(hWnd, filepath, MAX_PATH);
        if (b) {
          //MessageBox(hWnd, filepath, b ? _T("OK") : _T("NG"), MB_OK);
          if (!updateWindow(hWnd, filepath, g_opacity)) {
            DestroyWindow(hWnd);
            return 0;
          }
          _tcscpy(g_filePath, filepath);
        }
      }
      break;
    case ID_CHANGEOPACITY:
      {
        POINT pt;
        RECT rect;
        GetCursorPos(&pt);
        GetWindowRect(g_hOpacityControlWindow, &rect);
        SetWindowPos(g_hOpacityControlWindow, HWND_TOP,
                     pt.x - (rect.right - rect.left),
                     pt.y - (rect.bottom - rect.top),
                     0, 0,
                     SWP_NOSIZE | SWP_SHOWWINDOW);
        UpdateWindow(g_hOpacityControlWindow);
      }
      return DefWindowProc(hWnd, message, wParam, lParam);
      break;
    default:
      break;
    }
    break;

  case WM_DESTROY:
    {
      saveConfigure();
      unregisterTaskTrayIcon();
      deletePopupMenu();
      PostQuitMessage(0);
    }
    break;

  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nShow) {
  const TCHAR* WINDOW_CLASS_NAME = _T("viewgrafy_window");

  GdiplusInitializer gdiplus;
  InitCommonControls();

  loadConfigure();

  // Register the class
  WNDCLASSEX wcex;
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = wndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = NULL;
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
  wcex.lpszMenuName = NULL;
  wcex.lpszClassName = WINDOW_CLASS_NAME;
  wcex.hIconSm = NULL;
  RegisterClassEx(&wcex);

  // Create and set up the window
  HWND hWnd;
  hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                        WINDOW_CLASS_NAME,
                        _T("Viewgrafy"),
                        WS_POPUP,
                        CW_USEDEFAULT, 0,
                        CW_USEDEFAULT, 0,
                        NULL, NULL, hInstance, NULL);
  if (!hWnd)
    return FALSE;

  ShowWindow(hWnd, nShow);
  UpdateWindow(hWnd);
  SetForegroundWindow(GetDesktopWindow()); // To avoid weird shuffling z-order of windows...

  g_hOpacityControlWindow = createOpacityControlWindow(hWnd, hInstance);
  if (!g_hOpacityControlWindow)
    return FALSE;

  if (!registerTaskTrayIcon(hWnd))
    return FALSE;
  if (!createPopupMenu())
    return FALSE;

  // The message loop
  MSG msg;
  for (;;) {
    BOOL bRet = GetMessage(&msg, NULL, 0, 0);
    if (bRet == 0 || bRet == -1)
      break;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}

#if defined(UNICODE) && defined(__MINGW32__)
// When defined(UNICODE), the current Mingw does not provie
// the startup routine that calls WinMain()...
int main(int, char**) {
  return WinMain(GetModuleHandle(NULL), NULL, (TCHAR*)0, SW_SHOWNORMAL);
}
#endif

