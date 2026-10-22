#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cwchar>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

namespace
{
    constexpr wchar_t kWindowClass[] = L"BGE_DoubleH_Fix_Window";
    constexpr wchar_t kAppTitle[] = L"BGE Double H Fix";

    constexpr char kStatusField[] = "Perso_ID_status";
    constexpr size_t kStatusFieldLen = sizeof(kStatusField) - 1;

    constexpr std::uint8_t kExpectedArrayMarker = 0x8C;
    constexpr size_t kStatusCount = 12;
    constexpr size_t kDoubleHIndex = 1;
    constexpr std::uint8_t kBugState = 4;
    constexpr std::uint8_t kFollowState = 0;

    enum ControlId
    {
        IDC_PATH = 1001,
        IDC_CHOOSE = 1002,
        IDC_STATUS = 1003,
        IDC_DETAILS = 1004,
        IDC_REPAIR = 1005,
        IDC_EXIT = 1006,
        IDC_HEADER_TITLE = 1007,
        IDC_HEADER_SUBTITLE = 1008,
        IDC_HINT = 1009,
        IDC_DETAILS_LABEL = 1010,
        IDC_FILE_LABEL = 1011,
    };

    HWND g_path = nullptr;
    HWND g_status = nullptr;
    HWND g_details = nullptr;
    HWND g_repair = nullptr;
    HWND g_hint = nullptr;
    std::wstring g_selectedPath;

    struct SaveInfo
    {
        std::vector<std::uint8_t> data;
        size_t valuesStart{};
        std::vector<std::uint8_t> status;
    };

    std::wstring Utf8ish(const std::string& s)
    {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (len <= 0)
            return std::wstring(s.begin(), s.end());

        std::wstring out(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
        return out;
    }

    std::vector<std::uint8_t> ReadAllBytes(const std::wstring& path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            throw std::runtime_error("Could not open save file.");

        f.seekg(0, std::ios::end);
        const auto size = f.tellg();
        if (size < 0)
            throw std::runtime_error("Could not determine save file size.");

        f.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> data(static_cast<size_t>(size));

        if (!data.empty())
            f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

        if (!f && !data.empty())
            throw std::runtime_error("Could not read the complete save file.");

        return data;
    }

    void WriteAllBytes(const std::wstring& path, const std::vector<std::uint8_t>& data)
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error("Could not open save file for writing.");

        if (!data.empty())
            f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

        if (!f)
            throw std::runtime_error("Could not write the complete save file.");
    }

    std::vector<size_t> FindAll(const std::vector<std::uint8_t>& data)
    {
        std::vector<size_t> positions;

        if (data.size() < kStatusFieldLen)
            return positions;

        for (size_t i = 0; i + kStatusFieldLen <= data.size(); ++i)
        {
            bool match = true;
            for (size_t j = 0; j < kStatusFieldLen; ++j)
            {
                if (data[i + j] != static_cast<std::uint8_t>(kStatusField[j]))
                {
                    match = false;
                    break;
                }
            }

            if (match)
                positions.push_back(i);
        }

        return positions;
    }

    SaveInfo InspectSave(const std::wstring& path)
    {
        SaveInfo info;
        info.data = ReadAllBytes(path);

        const auto positions = FindAll(info.data);

        if (positions.empty())
            throw std::runtime_error("Field \"Perso_ID_status\" was not found.");

        if (positions.size() != 1)
            throw std::runtime_error("Expected exactly one \"Perso_ID_status\" field.");

        const size_t markerPos = positions[0] + kStatusFieldLen;

        if (markerPos >= info.data.size())
            throw std::runtime_error("Save ends immediately after Perso_ID_status.");

        if (info.data[markerPos] != kExpectedArrayMarker)
            throw std::runtime_error(
                "Unexpected data after Perso_ID_status. "
                "This save format/version is not recognized safely.");

        info.valuesStart = markerPos + 1;

        if (info.valuesStart + kStatusCount > info.data.size())
            throw std::runtime_error("Perso_ID_status array is truncated.");

        info.status.assign(
            info.data.begin() + static_cast<std::ptrdiff_t>(info.valuesStart),
            info.data.begin() + static_cast<std::ptrdiff_t>(info.valuesStart + kStatusCount));

        for (auto v : info.status)
        {
            if (v > 0x17)
                throw std::runtime_error(
                    "Perso_ID_status does not contain the expected one-byte values. "
                    "Refusing to patch.");
        }

        return info;
    }

    std::vector<size_t> DifferentOffsets(
        const std::vector<std::uint8_t>& a,
        const std::vector<std::uint8_t>& b)
    {
        if (a.size() != b.size())
            throw std::runtime_error("File length changed unexpectedly.");

        std::vector<size_t> diffs;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i] != b[i])
                diffs.push_back(i);
        }
        return diffs;
    }

    std::wstring MakeBackup(const std::wstring& path)
    {
        std::wstring candidate = path + L".doubleh-backup";

        DWORD attrs = GetFileAttributesW(candidate.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            if (!CopyFileW(path.c_str(), candidate.c_str(), TRUE))
                throw std::runtime_error("Could not create backup file.");
            return candidate;
        }

        for (unsigned i = 1; i < 10000; ++i)
        {
            candidate = path + L".doubleh-backup." + std::to_wstring(i);
            attrs = GetFileAttributesW(candidate.c_str());

            if (attrs == INVALID_FILE_ATTRIBUTES)
            {
                if (!CopyFileW(path.c_str(), candidate.c_str(), TRUE))
                    throw std::runtime_error("Could not create backup file.");
                return candidate;
            }
        }

        throw std::runtime_error("Could not find an available backup filename.");
    }

    std::wstring StatusArrayText(const std::vector<std::uint8_t>& values)
    {
        std::wostringstream out;
        out << L"[";
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i) out << L", ";
            out << static_cast<unsigned>(values[i]);
        }
        out << L"]";
        return out.str();
    }

    void SetText(HWND hwnd, const std::wstring& text)
    {
        SetWindowTextW(hwnd, text.c_str());
        // Force the control to repaint immediately after its content changes.
        // This prevents stale glyphs from remaining visible on some Windows themes.
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }

    void LoadSave(const std::wstring& path)
    {
        g_selectedPath = path;
        SetText(g_path, path);
        EnableWindow(g_repair, FALSE);

        try
        {
            const auto info = InspectSave(path);
            const auto state = info.status[kDoubleHIndex];

            std::wostringstream details;
            details << L"File: " << path.substr(path.find_last_of(L"\\/") + 1) << L"\r\n";
            details << L"Size: " << info.data.size() << L" bytes\r\n\r\n";
            details << L"Perso_ID_status:\r\n";
            details << StatusArrayText(info.status) << L"\r\n\r\n";
            details << L"Double H state: " << static_cast<unsigned>(state) << L"\r\n";

            if (state == kFollowState)
            {
                SetText(g_status, L"No repair needed");
                SetText(g_hint, L"Everything looks good. Double H is already set to Follow.");
                details << L"\r\nDouble H is already in Follow state (0).";
            }
            else if (state == kBugState)
            {
                SetText(g_status, L"Lost Partner repair pattern detected");
                SetText(g_hint, L"A safe repair is available. A backup is created automatically.");
                details << L"\r\nConfirmed repair candidate:\r\n";
                details << L"Perso_ID_status[1]: 4 -> 0\r\n\r\n";
                details << L"A backup will be created automatically before any change.";
                EnableWindow(g_repair, TRUE);
            }
            else
            {
                SetText(g_status, L"Different Double H state detected");
                SetText(g_hint, L"No changes are offered for this state to protect your save.");
                details << L"\r\nThis tool only auto-repairs the confirmed 4 -> 0 case.\r\n";
                details << L"No automatic change will be offered for state "
                        << static_cast<unsigned>(state) << L".";
            }

            SetText(g_details, details.str());
        }
        catch (const std::exception& e)
        {
            SetText(g_status, L"Save format not recognized");
            SetText(g_hint, L"The file does not match the expected structure. Nothing was changed.");
            SetText(g_details, L"ERROR\r\n\r\n" + Utf8ish(e.what()));
        }
    }

    void ChooseSave(HWND owner)
    {
        wchar_t fileName[MAX_PATH] = L"";

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter =
            L"BGE savegames (*.sav)\0*.sav\0"
            L"All files (*.*)\0*.*\0\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Select BGE savegame";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameW(&ofn))
            LoadSave(fileName);
    }

    void RepairSave(HWND owner)
    {
        if (g_selectedPath.empty())
            return;

        const int answer = MessageBoxW(
            owner,
            L"This will create a backup and change exactly one byte:\n\n"
            L"Perso_ID_status[1]: 4 -> 0\n\n"
            L"Continue?",
            L"Repair Double H",
            MB_YESNO | MB_ICONQUESTION);

        if (answer != IDYES)
            return;

        try
        {
            const auto info = InspectSave(g_selectedPath);
            const auto state = info.status[kDoubleHIndex];

            if (state == kFollowState)
            {
                MessageBoxW(owner, L"Double H is already in Follow state (0).",
                            L"No change made", MB_OK | MB_ICONINFORMATION);
                LoadSave(g_selectedPath);
                return;
            }

            if (state != kBugState)
            {
                MessageBoxW(owner,
                            L"This save is not in the confirmed bug state (4). "
                            L"No changes were made.",
                            L"No change made", MB_OK | MB_ICONINFORMATION);
                LoadSave(g_selectedPath);
                return;
            }

            const size_t patchOffset = info.valuesStart + kDoubleHIndex;
            auto patched = info.data;
            patched[patchOffset] = kFollowState;

            const auto diffs = DifferentOffsets(info.data, patched);
            if (diffs.size() != 1 || diffs[0] != patchOffset)
                throw std::runtime_error("Internal safety check failed; refusing to write.");

            const std::wstring backup = MakeBackup(g_selectedPath);

            try
            {
                WriteAllBytes(g_selectedPath, patched);

                const auto reread = ReadAllBytes(g_selectedPath);
                const auto changed = DifferentOffsets(info.data, reread);

                if (changed.size() != 1 || changed[0] != patchOffset)
                    throw std::runtime_error("Verification failed after writing.");

                const auto verified = InspectSave(g_selectedPath);
                if (verified.status[kDoubleHIndex] != kFollowState)
                    throw std::runtime_error("Follow-state verification failed.");
            }
            catch (...)
            {
                CopyFileW(backup.c_str(), g_selectedPath.c_str(), FALSE);
                throw;
            }

            std::wostringstream msg;
            msg << L"Double H was repaired successfully.\n\n";
            msg << L"Backup:\n" << backup << L"\n\n";
            msg << L"Changed byte offset: 0x"
                << std::uppercase << std::hex << patchOffset;

            MessageBoxW(owner, msg.str().c_str(), L"Repair successful",
                        MB_OK | MB_ICONINFORMATION);

            LoadSave(g_selectedPath);
        }
        catch (const std::exception& e)
        {
            const auto text = Utf8ish(e.what());
            MessageBoxW(owner, text.c_str(), L"Repair failed", MB_OK | MB_ICONERROR);
            LoadSave(g_selectedPath);
        }
    }

    HFONT CreateGuiFont(int size, bool bold = false, const wchar_t* face = L"Segoe UI")
    {
        HDC hdc = GetDC(nullptr);
        const int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);

        return CreateFontW(
            -MulDiv(size, dpi, 72),
            0, 0, 0,
            bold ? FW_BOLD : FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            face);
    }

    void ApplyFont(HWND hwnd, HFONT font)
    {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        static HFONT font = nullptr;
        static HFONT fontTitle = nullptr;
        static HFONT fontStatus = nullptr;
        static HFONT fontMono = nullptr;
        static HFONT fontSection = nullptr;
        static HBRUSH lightBackgroundBrush = nullptr;
        static HBRUSH darkHeaderBrush = nullptr;

        switch (msg)
        {
        case WM_CREATE:
        {
            font = CreateGuiFont(10);
            fontTitle = CreateGuiFont(17, true);
            fontStatus = CreateGuiFont(12, true);
            fontMono = CreateGuiFont(9, false, L"Consolas");
            fontSection = CreateGuiFont(9, true);
            lightBackgroundBrush = CreateSolidBrush(RGB(246, 248, 251));
            darkHeaderBrush = CreateSolidBrush(RGB(23, 36, 58));

            HWND title = CreateWindowW(
                L"STATIC", L"BGE Double H Fix",
                WS_CHILD | WS_VISIBLE,
                24, 14, 580, 30,
                hwnd, reinterpret_cast<HMENU>(IDC_HEADER_TITLE), nullptr, nullptr);
            ApplyFont(title, fontTitle);

            HWND subtitle = CreateWindowW(
                L"STATIC", L"Beyond Good & Evil | 20th Anniversary Edition",
                WS_CHILD | WS_VISIBLE | SS_NOPREFIX,
                25, 46, 650, 20,
                hwnd, reinterpret_cast<HMENU>(IDC_HEADER_SUBTITLE), nullptr, nullptr);
            ApplyFont(subtitle, font);

            HWND fileLabel = CreateWindowW(
                L"STATIC", L"SAVE FILE",
                WS_CHILD | WS_VISIBLE,
                24, 96, 250, 18,
                hwnd, reinterpret_cast<HMENU>(IDC_FILE_LABEL), nullptr, nullptr);
            ApplyFont(fileLabel, fontSection);

            g_path = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"No save selected",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                24, 120, 545, 30,
                hwnd, reinterpret_cast<HMENU>(IDC_PATH), nullptr, nullptr);
            ApplyFont(g_path, font);

            HWND choose = CreateWindowW(
                L"BUTTON", L"Choose save...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                582, 119, 180, 32,
                hwnd, reinterpret_cast<HMENU>(IDC_CHOOSE), nullptr, nullptr);
            ApplyFont(choose, font);

            g_status = CreateWindowW(
                L"STATIC", L"Ready to inspect",
                WS_CHILD | WS_VISIBLE,
                24, 181, 720, 26,
                hwnd, reinterpret_cast<HMENU>(IDC_STATUS), nullptr, nullptr);
            ApplyFont(g_status, fontStatus);

            g_hint = CreateWindowW(
                L"STATIC", L"Choose a save file or drag it onto this window.",
                WS_CHILD | WS_VISIBLE,
                24, 210, 720, 22,
                hwnd, reinterpret_cast<HMENU>(IDC_HINT), nullptr, nullptr);
            ApplyFont(g_hint, font);

            HWND detailsLabel = CreateWindowW(
                L"STATIC", L"SAVE INSPECTION",
                WS_CHILD | WS_VISIBLE,
                24, 270, 250, 18,
                hwnd, reinterpret_cast<HMENU>(IDC_DETAILS_LABEL), nullptr, nullptr);
            ApplyFont(detailsLabel, fontSection);

            g_details = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT",
                L"Save details and repair eligibility will appear here.",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
                ES_READONLY | WS_VSCROLL,
                24, 294, 738, 135,
                hwnd, reinterpret_cast<HMENU>(IDC_DETAILS), nullptr, nullptr);
            ApplyFont(g_details, fontMono);

            HWND footer = CreateWindowW(
                L"STATIC", L"Original save is backed up before any repair.",
                WS_CHILD | WS_VISIBLE,
                24, 457, 400, 24,
                hwnd, nullptr, nullptr, nullptr);
            ApplyFont(footer, font);

            g_repair = CreateWindowW(
                L"BUTTON", L"Repair Double H",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_DISABLED,
                548, 451, 146, 34,
                hwnd, reinterpret_cast<HMENU>(IDC_REPAIR), nullptr, nullptr);
            ApplyFont(g_repair, font);

            HWND exitBtn = CreateWindowW(
                L"BUTTON", L"Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                704, 451, 58, 34,
                hwnd, reinterpret_cast<HMENU>(IDC_EXIT), nullptr, nullptr);
            ApplyFont(exitBtn, font);

            DragAcceptFiles(hwnd, TRUE);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);

            HBRUSH background = CreateSolidBrush(RGB(246, 248, 251));
            FillRect(hdc, &client, background);
            DeleteObject(background);

            RECT header{ 0, 0, client.right, 80 };
            HBRUSH headerBrush = CreateSolidBrush(RGB(23, 36, 58));
            FillRect(hdc, &header, headerBrush);
            DeleteObject(headerBrush);

            RECT accent{ 0, 0, 5, 80 };
            HBRUSH accentBrush = CreateSolidBrush(RGB(48, 199, 169));
            FillRect(hdc, &accent, accentBrush);
            DeleteObject(accentBrush);

            HPEN divider = CreatePen(PS_SOLID, 1, RGB(222, 228, 237));
            HGDIOBJ oldPen = SelectObject(hdc, divider);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            MoveToEx(hdc, 24, 164, nullptr);
            LineTo(hdc, client.right - 24, 164);
            MoveToEx(hdc, 24, 249, nullptr);
            LineTo(hdc, client.right - 24, 249);
            MoveToEx(hdc, 24, 440, nullptr);
            LineTo(hdc, client.right - 24, 440);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(divider);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);
            const int controlId = GetDlgCtrlID(control);

            // These STATIC controls change text after a save is loaded. Give each
            // control an opaque background so old text is erased before repainting.
            // Returning NULL_BRUSH with a transparent background left stale glyphs
            // visible underneath the new status and hint text.
            if (controlId == IDC_HEADER_TITLE || controlId == IDC_HEADER_SUBTITLE)
            {
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, RGB(23, 36, 58));
                SetTextColor(hdc, controlId == IDC_HEADER_TITLE
                    ? RGB(250, 251, 253)
                    : RGB(190, 202, 219));
                return reinterpret_cast<LRESULT>(darkHeaderBrush);
            }

            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(246, 248, 251));

            if (controlId == IDC_FILE_LABEL || controlId == IDC_DETAILS_LABEL)
                SetTextColor(hdc, RGB(91, 108, 131));
            else if (controlId == IDC_HINT)
                SetTextColor(hdc, RGB(98, 113, 134));
            else if (control == g_status)
            {
                wchar_t statusText[256]{};
                GetWindowTextW(control, statusText, 256);
                if (wcsstr(statusText, L"not recognized") || wcsstr(statusText, L"Different"))
                    SetTextColor(hdc, RGB(180, 55, 55));
                else if (wcsstr(statusText, L"Lost Partner"))
                    SetTextColor(hdc, RGB(30, 92, 174));
                else if (wcsstr(statusText, L"No repair needed"))
                    SetTextColor(hdc, RGB(25, 125, 89));
                else
                    SetTextColor(hdc, RGB(28, 43, 64));
            }
            else
                SetTextColor(hdc, RGB(70, 84, 104));

            return reinterpret_cast<LRESULT>(lightBackgroundBrush);
        }

        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(35, 48, 66));
            SetBkColor(reinterpret_cast<HDC>(wParam), RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_CHOOSE:
                ChooseSave(hwnd);
                return 0;
            case IDC_REPAIR:
                RepairSave(hwnd);
                return 0;
            case IDC_EXIT:
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_DROPFILES:
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            wchar_t path[MAX_PATH]{};

            if (DragQueryFileW(drop, 0, path, MAX_PATH))
                LoadSave(path);

            DragFinish(drop);
            return 0;
        }

        case WM_DESTROY:
            if (font) DeleteObject(font);
            if (fontTitle) DeleteObject(fontTitle);
            if (fontStatus) DeleteObject(fontStatus);
            if (fontMono) DeleteObject(fontMono);
            if (fontSection) DeleteObject(fontSection);
            if (lightBackgroundBrush) DeleteObject(lightBackgroundBrush);
            if (darkHeaderBrush) DeleteObject(darkHeaderBrush);
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int show)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc))
        return 1;

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        kAppTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 550,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd)
        return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    // Support dragging a .sav directly onto the EXE.
    if (cmdLine && *cmdLine)
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv)
        {
            if (argc > 1)
            {
                DWORD attrs = GetFileAttributesW(argv[1]);
                if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
                    LoadSave(argv[1]);
            }
            LocalFree(argv);
        }
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
