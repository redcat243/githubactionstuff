#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include "webview.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_HOME 1001
#define ID_TRAY_EXIT 1002

NOTIFYICONDATAA g_nid = { 0 };
HWND g_hwnd = NULL;
webview::webview* g_webview_ptr = nullptr;
std::string g_home_path = "";

// Helper to get executable directory on Windows
std::string get_executable_dir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    return path.substr(0, pos);
}

// Function to open any file (like an RTFD or TXT note) in Notepad
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
                g_webview_ptr->navigate(g_home_path);
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
// 1. Create invisible background window to handle System Tray events
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CatBrowserTrayClass";
    RegisterClassA(&wc);

    g_hwnd = CreateWindowA("CatBrowserTrayClass", "CatBrowser Tray Host", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    // 2. Initialize System Tray Icon (uses default application icon or custom icon.ico if present)
    g_nid.cbSize = sizeof(NOTIFYICONDATAA);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(hInstance, IDI_APPLICATION); // Replace with custom icon handle if compiling with .rc resource file
    lstrcpyA(g_nid.szTip, "CatBrowser");
    Shell_NotifyIconA(NIM_ADD, &g_nid);

    // 3. Setup Native WebView Window
    webview::webview w(true, nullptr);
    g_webview_ptr = &w;

    w.set_title("CatBrowser");
    w.set_size(1024, 768, WEBVIEW_HINT_NONE);

    std::string exe_dir = get_executable_dir();
    g_home_path = "file:///" + exe_dir + "/cathome.html";

    // JS Binding: Navigate Home
    w.bind("goHome", [&w](std::string seq, std::string req, void *arg) {
        w.navigate(g_home_path);
        w.resolve(seq, 0, "{}");
    });

    // JS Binding: Open specific RTFD / TXT document in Notepad directly
    w.bind("openInNotepad", [exe_dir](std::string seq, std::string req, void *arg) {
        // Trims JS array syntax or string quotes from incoming parameters
        std::string filename = req.substr(2, req.length() - 4); 
        std::string target_file = exe_dir + "\\" + filename;
        open_in_notepad(target_file);
        w.resolve(seq, 0, "{}");
    });

    // JS Binding: Open sammy.html in a new window
w.bind("openSammyWindow", [&hInstance](std::string seq, std::string req, void *arg) {
    std::string sammy_path = "file:///" + get_executable_dir() + "/sammy.html";
    
    // Launch a new thread so the new window runs independently
    std::thread([sammy_path]() {
        webview::webview sammy_win(true, nullptr);
        sammy_win.set_title("About Sammy");
        sammy_win.set_size(650, 700, WEBVIEW_HINT_NONE);
        sammy_win.navigate(sammy_path);
        sammy_win.run();
    }).detach();

    g_webview_ptr->resolve(seq, 0, "{}");
});

    w.navigate(g_home_path);
    w.run();

    return 0;
}