#pragma once

#include "../../common/EstraIme.Common/ConfigStore.h"
#include "../../common/EstraIme.Common/IpcClient.h"
#include "../../engine/EstraIme.Engine/PinyinEngine.h"
#include "../../fusion/EstraIme.Fusion/CandidateFusion.h"
#include "../../ui/EstraIme.CandidateWindow/CandidateWindow.h"

#include <msctf.h>
#include <winrt/base.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace EstraIme::Tip
{
    class TextService final :
        public ITfTextInputProcessorEx,
        public ITfKeyEventSink,
        public ITfThreadMgrEventSink,
        public ITfCompositionSink
    {
    public:
        TextService();

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
        STDMETHODIMP_(ULONG) AddRef() override;
        STDMETHODIMP_(ULONG) Release() override;

        // ITfTextInputProcessor
        STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override;
        STDMETHODIMP Deactivate() override;

        // ITfTextInputProcessorEx
        STDMETHODIMP ActivateEx(ITfThreadMgr* threadMgr, TfClientId clientId, DWORD flags) override;

        // ITfKeyEventSink
        STDMETHODIMP OnSetFocus(BOOL foreground) override;
        STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
        STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
        STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
        STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM wParam, LPARAM lParam, BOOL* eaten) override;
        STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;

        // ITfThreadMgrEventSink
        STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* documentMgr) override;
        STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* documentMgr) override;
        STDMETHODIMP OnSetFocus(ITfDocumentMgr* documentMgrFocus, ITfDocumentMgr* documentMgrPrevFocus) override;
        STDMETHODIMP OnPushContext(ITfContext* context) override;
        STDMETHODIMP OnPopContext(ITfContext* context) override;

        // ITfCompositionSink
        STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* composition) override;

    private:
        enum class EditAction
        {
            Update,
            Commit,
            Clear
        };

        class EditSession;
        friend class EditSession;

        HRESULT InitializeSinks();
        void ShutdownSinks();
        void RebuildCandidates();
        void RefreshVisiblePage();
        void ScheduleAsyncAutocomplete();
        void CommitCandidate(ITfContext* context, const std::wstring& text);
        void CancelComposition(ITfContext* context);
        void UpdateComposition(ITfContext* context, bool commit, const std::wstring& text);
        void InsertMappedPunctuation(ITfContext* context, wchar_t ch);
        bool ShouldTriggerAsync() const;
        void MovePage(int delta);
        void MoveSelection(int delta);
        std::size_t PageCount() const;
        POINT ResolveAnchorPoint() const;
        void ResetCandidates();

        static constexpr std::size_t kPageSize = 9;
        static constexpr std::size_t kCandidateBudget = 48;

        std::atomic<ULONG> refCount_{1};
        winrt::com_ptr<ITfThreadMgr> threadMgr_{};
        TfClientId clientId_{TF_CLIENTID_NULL};
        DWORD threadMgrEventCookie_{TF_INVALID_COOKIE};
        bool chineseMode_{true};

        std::wstring composition_;
        std::vector<EstraIme::Candidate> traditionalCandidates_;
        std::vector<EstraIme::Candidate> asyncCandidates_;
        std::vector<EstraIme::Candidate> allCandidates_;
        std::vector<EstraIme::Candidate> visibleCandidates_;
        std::size_t currentPageIndex_{0};
        std::size_t selectedIndex_{0};
        std::uint64_t generation_{0};

        winrt::com_ptr<ITfComposition> compositionRange_{};
        mutable std::mutex stateMutex_;

        EstraIme::ImeConfig config_{};
        EstraIme::Engine::PinyinEngine engine_{};
        EstraIme::Fusion::CandidateFusion fusion_{};
        EstraIme::Common::SidecarClient sidecar_{};
        EstraIme::UI::CandidateWindow candidateWindow_{};

        std::jthread debounceThread_{};
        std::atomic<std::uint64_t> debounceGeneration_{0};
    };
}
