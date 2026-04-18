#include "CandidateWindow.h"

#include <Windowsx.h>

#include <algorithm>
#include <format>

namespace
{
    constexpr wchar_t kWindowClassName[] = L"EstraImeCandidateWindow";
}

namespace EstraIme::UI
{
    CandidateWindow::CandidateWindow() = default;

    CandidateWindow::~CandidateWindow()
    {
        Hide();
    }

    void CandidateWindow::Show(const POINT& screenPoint, const std::vector<EstraIme::Candidate>& candidates, const std::size_t selectedIndex, const std::size_t pageNumber, const std::size_t pageCount, const std::size_t totalCount)
    {
        EnsureWindow();

        {
            std::scoped_lock lock(mutex_);
            anchor_ = screenPoint;
            candidates_ = candidates;
            selectedIndex_ = selectedIndex;
            pageNumber_ = pageNumber;
            pageCount_ = pageCount;
            totalCount_ = totalCount;
        }

        SetWindowPos(hwnd_, HWND_TOPMOST, screenPoint.x, screenPoint.y + 24, 320, 32 + static_cast<int>(candidates.size()) * 24, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        InvalidateRect(hwnd_, nullptr, TRUE);
    }

    void CandidateWindow::Hide()
    {
        if (hwnd_)
        {
            ShowWindow(hwnd_, SW_HIDE);
        }
    }

    void CandidateWindow::UpdateCandidatesAsync(const std::vector<EstraIme::Candidate>& candidates, const std::size_t selectedIndex, const std::size_t pageNumber, const std::size_t pageCount, const std::size_t totalCount)
    {
        if (!hwnd_)
        {
            return;
        }

        {
            std::scoped_lock lock(mutex_);
            candidates_ = candidates;
            selectedIndex_ = selectedIndex;
            pageNumber_ = pageNumber;
            pageCount_ = pageCount;
            totalCount_ = totalCount;
        }

        PostMessageW(hwnd_, kRefreshMessage, 0, 0);
    }

    bool CandidateWindow::IsVisible() const
    {
        return hwnd_ && IsWindowVisible(hwnd_);
    }

    LRESULT CALLBACK CandidateWindow::WindowProc(HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
    {
        auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
            return TRUE;
        }

        switch (message)
        {
        case WM_PAINT:
            if (self)
            {
                PAINTSTRUCT ps{};
                const auto hdc = BeginPaint(hwnd, &ps);
                self->Paint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;
        case kRefreshMessage:
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void CandidateWindow::EnsureWindow()
    {
        if (hwnd_)
        {
            return;
        }

        WNDCLASSW wc{};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kWindowClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);

        hwnd_ = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kWindowClassName,
            L"EstraIME Candidates",
            WS_POPUP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            320,
            200,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            this);
    }

    void CandidateWindow::Paint(HDC hdc)
    {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        HBRUSH background = CreateSolidBrush(RGB(252, 252, 252));
        FillRect(hdc, &rc, background);
        DeleteObject(background);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(20, 20, 20));

        std::vector<EstraIme::Candidate> snapshot;
        std::size_t selectedIndex = 0;
        std::size_t pageNumber = 0;
        std::size_t pageCount = 0;
        std::size_t totalCount = 0;
        {
            std::scoped_lock lock(mutex_);
            snapshot = candidates_;
            selectedIndex = selectedIndex_;
            pageNumber = pageNumber_;
            pageCount = pageCount_;
            totalCount = totalCount_;
        }

        RECT header = {8, 6, rc.right - 8, 26};
        const auto headerText = pageCount == 0
            ? std::wstring(L"EstraIME Alpha")
            : std::format(L"EstraIME Alpha  {}/{}  ({} items)", pageNumber + 1, pageCount, totalCount);
        DrawTextW(hdc, headerText.c_str(), -1, &header, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        for (std::size_t index = 0; index < snapshot.size(); ++index)
        {
            RECT item = {8, 30 + static_cast<LONG>(index) * 24, rc.right - 8, 52 + static_cast<LONG>(index) * 24};
            if (index == selectedIndex)
            {
                HBRUSH selection = CreateSolidBrush(RGB(225, 239, 255));
                FillRect(hdc, &item, selection);
                DeleteObject(selection);
            }

            const auto line = std::format(L"{}. {}{}", index + 1, snapshot[index].text, snapshot[index].annotation.empty() ? L"" : L"  [AI]");
            DrawTextW(hdc, line.c_str(), -1, &item, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        FrameRect(hdc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    }
}
