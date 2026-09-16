#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <thread>

#define WEBVIEW_IMPLEMENTATION
#define WEBVIEW_WINAPI
#include "webview.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_HOME 1001
#define ID_TRAY_EXIT 1002

NOTIFYICONDATAA g_nid = { 0 };
HWND g_hwnd = NULL;
webview_t g_webview_ptr = NULL;
std::string g_home_path = "";

// Helper to get executable directory on Windows
std::string get_executable_dir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    return path.substr(0, pos);
}

// Function to open any file in Notepad
void open_in_notepad(const std::string& filepath) {
    ShellExecuteA(NULL, "open", "notepad.exe", filepath.c_str(), NULL, SW_SHOWNORMAL);
}

// System Tray Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuA(hMenu, MF_STRING, ID_TRAY_HOME, "Go Home");
            AppendMenuA(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit CatBrowser");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_HOME:
            if (g_webview_ptr) {
                webview_navigate(g_webview_ptr, g_home_path.c_str());
            }
            break;
        case ID_TRAY_EXIT:
            Shell_NotifyIconA(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Create background window for System Tray events
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CatBrowserTrayClass";
    RegisterClassA(&wc);

    g_hwnd = CreateWindowA("CatBrowserTrayClass", "CatBrowser Tray Host", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    // 2. Initialize System Tray Icon
    g_nid.cbSize = sizeof(NOTIFYICONDATAA);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    lstrcpyA(g_nid.szTip, "CatBrowser");
    Shell_NotifyIconA(NIM_ADD, &g_nid);

    // 3. Setup Native WebView Window
    webview_t w = webview_create(0, NULL);
    g_webview_ptr = w;

    webview_set_title(w, "CatBrowser");
    webview_set_size(w, 1024, 768, WEBVIEW_HINT_NONE);

    std::string exe_dir = get_executable_dir();
    g_home_path = "file:///" + exe_dir + "/cathome.html";

    // JS Binding: Navigate Home
    webview_bind(w, "goHome", [](const char *seq, const char *req, void *arg) {
        webview_t instance = (webview_t)arg;
        webview_navigate(instance, g_home_path.c_str());
        webview_return(instance, seq, 0, "{}");
    }, w);

    // JS Binding: Open file in Notepad
    webview_bind(w, "openInNotepad", [](const char *seq, const char *req, void *arg) {
        webview_t instance = (webview_t)arg;
        std::string req_str = req;
        std::string filename = req_str.length() > 4 ? req_str.substr(2, req_str.length() - 4) : ""; 
        std::string target_file = get_executable_dir() + "\\" + filename;
        open_in_notepad(target_file);
        webview_return(instance, seq, 0, "{}");
    }, w);

    // JS Binding: Open Sammy page in secondary window
    webview_bind(w, "openSammyWindow", [](const char *seq, const char *req, void *arg) {
        webview_t instance = (webview_t)arg;
        std::string sammy_path = "file:///" + get_executable_dir() + "/sammy.html";
        
        std::thread([sammy_path]() {
            webview_t sammy_win = webview_create(0, NULL);
            webview_set_title(sammy_win, "About Sammy");
            webview_set_size(sammy_win, 650, 700, WEBVIEW_HINT_NONE);
            webview_navigate(sammy_win, sammy_path.c_str());
            webview_run(sammy_win);
            webview_destroy(sammy_win);
        }).detach();

        webview_return(instance, seq, 0, "{}");
    }, w);

    webview_navigate(w, g_home_path.c_str());
    webview_run(w);
    webview_destroy(w);

    return 0;
}
