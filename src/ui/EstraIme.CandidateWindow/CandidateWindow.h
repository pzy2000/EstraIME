#pragma once

#include "EstraImeShared/CommonTypes.h"

#include <Windows.h>

#include <mutex>
#include <vector>

namespace EstraIme::UI
{
    class CandidateWindow
    {
    public:
        CandidateWindow();
        ~CandidateWindow();

        void Show(const POINT& screenPoint, const std::vector<EstraIme::Candidate>& candidates, std::size_t selectedIndex, std::size_t pageNumber, std::size_t pageCount, std::size_t totalCount);
        void Hide();
        void UpdateCandidatesAsync(const std::vector<EstraIme::Candidate>& candidates, std::size_t selectedIndex, std::size_t pageNumber, std::size_t pageCount, std::size_t totalCount);
        bool IsVisible() const;

    private:
        static constexpr UINT kRefreshMessage = WM_APP + 91;

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        void EnsureWindow();
        void Paint(HDC hdc);

        HWND hwnd_{nullptr};
        POINT anchor_{};
        std::vector<EstraIme::Candidate> candidates_;
        std::size_t selectedIndex_{0};
        std::size_t pageNumber_{0};
        std::size_t pageCount_{0};
        std::size_t totalCount_{0};
        mutable std::mutex mutex_;
    };
}
