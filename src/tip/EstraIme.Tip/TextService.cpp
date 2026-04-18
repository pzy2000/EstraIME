#include "TextService.h"
#include "TipGuids.h"

#include "../../common/EstraIme.Common/Log.h"
#include "../../common/EstraIme.Common/TextUtils.h"

#include <windows.h>

#include <format>

namespace
{
    constexpr HRESULT CheckResult(const HRESULT hr)
    {
        return hr;
    }

    bool HasCtrlPressed()
    {
        return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    }

    bool HasAltPressed()
    {
        return (GetKeyState(VK_MENU) & 0x8000) != 0;
    }

    bool HasWinPressed()
    {
        return (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
    }

    bool IsShiftKey(const WPARAM vkey)
    {
        return vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT;
    }

    bool IsNavigationShortcut(const WPARAM vkey)
    {
        return vkey == VK_LEFT || vkey == VK_RIGHT || vkey == VK_HOME || vkey == VK_END;
    }

    bool IsPageShortcut(const WPARAM vkey)
    {
        return vkey == VK_PRIOR || vkey == VK_NEXT || vkey == VK_OEM_MINUS || vkey == VK_OEM_PLUS;
    }

    bool TryGetPinyinChar(const WPARAM vkey, wchar_t& output)
    {
        if (vkey >= 'A' && vkey <= 'Z')
        {
            output = static_cast<wchar_t>(towlower(static_cast<wchar_t>(vkey)));
            return true;
        }

        if (vkey == VK_OEM_7)
        {
            output = L'\'';
            return true;
        }

        return false;
    }

    bool TryGetMappedPunctuation(const WPARAM vkey, wchar_t& output)
    {
        switch (vkey)
        {
        case VK_OEM_COMMA:
            output = L',';
            return true;
        case VK_OEM_PERIOD:
            output = L'.';
            return true;
        case VK_OEM_1:
            output = (GetKeyState(VK_SHIFT) & 0x8000) ? L':' : L';';
            return true;
        case VK_OEM_2:
            output = (GetKeyState(VK_SHIFT) & 0x8000) ? L'?' : L'/';
            return (output == L'?');
        case '1':
            if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
            {
                output = L'!';
                return true;
            }
            break;
        default:
            break;
        }

        return false;
    }
}

class EstraIme::Tip::TextService::EditSession final : public ITfEditSession
{
public:
    EditSession(TextService* owner, EditAction action, std::wstring text)
        : owner_(owner), action_(action), text_(std::move(text))
    {
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (!ppvObject)
        {
            return E_INVALIDARG;
        }
        if (riid == IID_IUnknown || riid == IID_ITfEditSession)
        {
            *ppvObject = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return ++refCount_;
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        const auto value = --refCount_;
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    std::atomic<ULONG> refCount_{1};
    TextService* owner_;
    EditAction action_;
    std::wstring text_;
};

namespace EstraIme::Tip
{
    TextService::TextService()
        : config_(Common::ConfigStore::Load())
    {
        chineseMode_ = config_.defaultMode != L"english";
    }

    STDMETHODIMP TextService::QueryInterface(REFIID riid, void** ppvObject)
    {
        if (!ppvObject)
        {
            return E_INVALIDARG;
        }

        if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor || riid == IID_ITfTextInputProcessorEx)
        {
            *ppvObject = static_cast<ITfTextInputProcessorEx*>(this);
        }
        else if (riid == IID_ITfKeyEventSink)
        {
            *ppvObject = static_cast<ITfKeyEventSink*>(this);
        }
        else if (riid == IID_ITfThreadMgrEventSink)
        {
            *ppvObject = static_cast<ITfThreadMgrEventSink*>(this);
        }
        else if (riid == IID_ITfCompositionSink)
        {
            *ppvObject = static_cast<ITfCompositionSink*>(this);
        }
        else
        {
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) TextService::AddRef()
    {
        return ++refCount_;
    }

    STDMETHODIMP_(ULONG) TextService::Release()
    {
        const auto value = --refCount_;
        if (value == 0)
        {
            delete this;
        }
        return value;
    }

    STDMETHODIMP TextService::Activate(ITfThreadMgr* threadMgr, const TfClientId clientId)
    {
        return ActivateEx(threadMgr, clientId, 0);
    }

    STDMETHODIMP TextService::Deactivate()
    {
        ShutdownSinks();
        if (debounceThread_.joinable())
        {
            debounceThread_.request_stop();
        }
        threadMgr_ = nullptr;
        compositionRange_ = nullptr;
        ResetCandidates();
        candidateWindow_.Hide();
        return S_OK;
    }

    STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* threadMgr, const TfClientId clientId, DWORD)
    {
        threadMgr_.copy_from(threadMgr);
        clientId_ = clientId;
        return InitializeSinks();
    }

    STDMETHODIMP TextService::OnSetFocus(const BOOL)
    {
        return S_OK;
    }

    STDMETHODIMP TextService::OnTestKeyDown(ITfContext*, const WPARAM wParam, LPARAM, BOOL* eaten)
    {
        *eaten = FALSE;
        const bool hasComposition = !composition_.empty();
        const bool ctrlPressed = HasCtrlPressed();
        const bool altPressed = HasAltPressed();
        const bool winPressed = HasWinPressed();

        if (((GetKeyState(VK_SHIFT) & 0x8000) != 0) && !IsShiftKey(wParam))
        {
            shiftChordUsed_ = true;
        }

        if (IsShiftKey(wParam))
        {
            shiftChordUsed_ = false;
            Common::Logger::Info(std::format(L"Shift test-down: vkey=0x{:X}", static_cast<unsigned>(wParam)));
            return S_OK;
        }

        if (ctrlPressed || altPressed || winPressed)
        {
            return S_OK;
        }

        wchar_t pinyinChar = L'\0';
        if (chineseMode_ && TryGetPinyinChar(wParam, pinyinChar))
        {
            *eaten = TRUE;
        }

        if (hasComposition && (wParam == VK_BACK || wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_SPACE || (wParam >= '1' && wParam <= '9') || IsPageShortcut(wParam) || wParam == VK_UP || wParam == VK_DOWN))
        {
            *eaten = TRUE;
        }

        wchar_t punctuation = L'\0';
        if (chineseMode_ && !hasComposition && TryGetMappedPunctuation(wParam, punctuation))
        {
            *eaten = TRUE;
        }

        return S_OK;
    }

    STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM wParam, LPARAM, BOOL* eaten)
    {
        *eaten = FALSE;

        if (IsShiftKey(wParam))
        {
            Common::Logger::Info(std::format(L"Shift test-up: vkey=0x{:X}", static_cast<unsigned>(wParam)));
        }

        return S_OK;
    }

    STDMETHODIMP TextService::OnKeyDown(ITfContext* context, const WPARAM wParam, LPARAM, BOOL* eaten)
    {
        *eaten = FALSE;
        const bool ctrlPressed = HasCtrlPressed();
        const bool altPressed = HasAltPressed();
        const bool winPressed = HasWinPressed();
        const bool hasComposition = !composition_.empty();

        if (IsShiftKey(wParam))
        {
            return S_OK;
        }

        if (ctrlPressed || altPressed || winPressed)
        {
            return S_OK;
        }

        if (!chineseMode_)
        {
            return S_OK;
        }

        wchar_t pinyinChar = L'\0';
        if (TryGetPinyinChar(wParam, pinyinChar))
        {
            {
                std::scoped_lock lock(stateMutex_);
                composition_.push_back(pinyinChar);
                ++generation_;
                asyncCandidates_.clear();
                selectedIndex_ = 0;
                currentPageIndex_ = 0;
            }
            RebuildCandidates();
            UpdateComposition(context, false, composition_);
            ScheduleAsyncAutocomplete();
            *eaten = TRUE;
            return S_OK;
        }

        wchar_t punctuation = L'\0';
        if (composition_.empty() && TryGetMappedPunctuation(wParam, punctuation))
        {
            InsertMappedPunctuation(context, punctuation);
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam == VK_BACK && !composition_.empty())
        {
            {
                std::scoped_lock lock(stateMutex_);
                composition_.pop_back();
                ++generation_;
                asyncCandidates_.clear();
                selectedIndex_ = 0;
                currentPageIndex_ = 0;
            }
            if (composition_.empty())
            {
                CancelComposition(context);
            }
            else
            {
                RebuildCandidates();
                UpdateComposition(context, false, composition_);
                ScheduleAsyncAutocomplete();
            }
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam >= '1' && wParam <= '9' && !visibleCandidates_.empty())
        {
            const auto index = static_cast<std::size_t>(wParam - '1');
            if (index < visibleCandidates_.size())
            {
                CommitCandidate(context, visibleCandidates_[index].text);
            }
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam == VK_SPACE && !visibleCandidates_.empty())
        {
            CommitCandidate(context, visibleCandidates_[selectedIndex_].text);
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam == VK_RETURN && !composition_.empty())
        {
            const auto text = visibleCandidates_.empty() ? composition_ : visibleCandidates_[selectedIndex_].text;
            CommitCandidate(context, text);
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam == VK_ESCAPE && !composition_.empty())
        {
            CancelComposition(context);
            *eaten = TRUE;
            return S_OK;
        }

        if ((wParam == VK_PRIOR || wParam == VK_OEM_MINUS) && !visibleCandidates_.empty())
        {
            MovePage(-1);
            *eaten = TRUE;
            return S_OK;
        }

        if ((wParam == VK_NEXT || wParam == VK_OEM_PLUS) && !visibleCandidates_.empty())
        {
            MovePage(1);
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam == VK_UP && !visibleCandidates_.empty())
        {
            MoveSelection(-1);
            *eaten = TRUE;
            return S_OK;
        }

        if (wParam == VK_DOWN && !visibleCandidates_.empty())
        {
            MoveSelection(1);
            *eaten = TRUE;
            return S_OK;
        }

        if (hasComposition && IsNavigationShortcut(wParam))
        {
            return S_OK;
        }

        return S_OK;
    }

    STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM wParam, LPARAM, BOOL* eaten)
    {
        *eaten = FALSE;

        if (IsShiftKey(wParam))
        {
            shiftChordUsed_ = false;
        }

        return S_OK;
    }

    STDMETHODIMP TextService::OnPreservedKey(ITfContext*, REFGUID guid, BOOL* eaten)
    {
        *eaten = FALSE;
        if (guid == GUID_EstraImeShiftToggle)
        {
            const bool shouldToggle = !shiftChordUsed_ && composition_.empty();
            Common::Logger::Info(std::format(L"OnPreservedKey shift toggle fired: shouldToggle={}", shouldToggle ? L"true" : L"false"));
            shiftChordUsed_ = false;
            if (shouldToggle)
            {
                chineseMode_ = !chineseMode_;
                candidateWindow_.Hide();
                *eaten = TRUE;
            }
        }
        return S_OK;
    }

    STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr*)
    {
        return S_OK;
    }

    STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr*)
    {
        return S_OK;
    }

    STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*)
    {
        return S_OK;
    }

    STDMETHODIMP TextService::OnPushContext(ITfContext*)
    {
        return S_OK;
    }

    STDMETHODIMP TextService::OnPopContext(ITfContext*)
    {
        return S_OK;
    }

    STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie, ITfComposition*)
    {
        compositionRange_ = nullptr;
        return S_OK;
    }

    HRESULT TextService::InitializeSinks()
    {
        auto keystrokeMgr = threadMgr_.as<ITfKeystrokeMgr>();
        if (keystrokeMgr)
        {
            const auto hr = keystrokeMgr->AdviseKeyEventSink(clientId_, this, TRUE);
            if (FAILED(hr))
            {
                return hr;
            }

            TF_PRESERVEDKEY shiftKey{};
            shiftKey.uVKey = VK_SHIFT;
            shiftKey.uModifiers = TF_MOD_ON_KEYUP;
            const auto preserveHr = keystrokeMgr->PreserveKey(clientId_, GUID_EstraImeShiftToggle, &shiftKey, L"Toggle Chinese/English", static_cast<ULONG>(wcslen(L"Toggle Chinese/English")));
            if (FAILED(preserveHr))
            {
                Common::Logger::Warn(std::format(L"PreserveKey(VK_SHIFT, ON_KEYUP) failed: 0x{:08X}", static_cast<unsigned>(preserveHr)));
            }
            else
            {
                Common::Logger::Info(L"PreserveKey(VK_SHIFT, ON_KEYUP) registered");
            }
        }

        auto source = threadMgr_.as<ITfSource>();
        if (source)
        {
            const auto hr = source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadMgrEventCookie_);
            if (FAILED(hr))
            {
                return hr;
            }
        }

        return S_OK;
    }

    void TextService::ShutdownSinks()
    {
        if (threadMgr_)
        {
            if (auto keystrokeMgr = threadMgr_.try_as<ITfKeystrokeMgr>())
            {
                TF_PRESERVEDKEY shiftKey{};
                shiftKey.uVKey = VK_SHIFT;
                shiftKey.uModifiers = TF_MOD_ON_KEYUP;
                keystrokeMgr->UnpreserveKey(GUID_EstraImeShiftToggle, &shiftKey);
                keystrokeMgr->UnadviseKeyEventSink(clientId_);
            }

            if (threadMgrEventCookie_ != TF_INVALID_COOKIE)
            {
                if (auto source = threadMgr_.try_as<ITfSource>())
                {
                    source->UnadviseSink(threadMgrEventCookie_);
                }
                threadMgrEventCookie_ = TF_INVALID_COOKIE;
            }
        }
    }

    void TextService::RebuildCandidates()
    {
        std::scoped_lock lock(stateMutex_);
        traditionalCandidates_ = engine_.Query(composition_, generation_, kCandidateBudget);
        allCandidates_ = fusion_.Fuse(traditionalCandidates_, asyncCandidates_, kCandidateBudget, generation_);
        if (currentPageIndex_ >= PageCount())
        {
            currentPageIndex_ = 0;
        }
        RefreshVisiblePage();
    }

    void TextService::ScheduleAsyncAutocomplete()
    {
        if (!ShouldTriggerAsync())
        {
            return;
        }

        const auto requestGeneration = generation_;
        debounceGeneration_.store(requestGeneration);

        if (debounceThread_.joinable())
        {
            debounceThread_.request_stop();
        }

        debounceThread_ = std::jthread([this, requestGeneration](std::stop_token stopToken) {
            const auto step = 10;
            for (int waited = 0; waited < config_.trigger.pauseMs; waited += step)
            {
                if (stopToken.stop_requested())
                {
                    return;
                }
                Sleep(step);
            }

            if (stopToken.stop_requested() || debounceGeneration_.load() != requestGeneration)
            {
                return;
            }

            EstraIme::AutocompleteRequest request{};
            {
                std::scoped_lock lock(stateMutex_);
                request.sessionId = L"default-session";
                request.requestId = std::format(L"req-{}", requestGeneration);
                request.generationId = requestGeneration;
                request.compositionText = composition_;
                request.contextBefore = L"";
                request.supportsUi = true;
                request.secureInput = false;
            }

            sidecar_.RequestAutocompleteAsync(request, [this, requestGeneration](const EstraIme::AutocompleteResponse& response) {
                std::scoped_lock lock(stateMutex_);
                if (!response.ok || response.generationId != generation_ || response.generationId != requestGeneration)
                {
                    return;
                }

                asyncCandidates_ = response.suggestions;
                allCandidates_ = fusion_.Fuse(traditionalCandidates_, asyncCandidates_, kCandidateBudget, generation_);
                if (currentPageIndex_ >= PageCount())
                {
                    currentPageIndex_ = 0;
                    selectedIndex_ = 0;
                }
                RefreshVisiblePage();
            });
        });
    }

    void TextService::CommitCandidate(ITfContext* context, const std::wstring& text)
    {
        engine_.CommitSelection(composition_, text);
        UpdateComposition(context, true, text);
        if (debounceThread_.joinable())
        {
            debounceThread_.request_stop();
        }
        std::scoped_lock lock(stateMutex_);
        ResetCandidates();
        candidateWindow_.Hide();
    }

    void TextService::CancelComposition(ITfContext* context)
    {
        UpdateComposition(context, false, L"");
        if (debounceThread_.joinable())
        {
            debounceThread_.request_stop();
        }
        std::scoped_lock lock(stateMutex_);
        ResetCandidates();
        candidateWindow_.Hide();
    }

    void TextService::UpdateComposition(ITfContext* context, const bool commit, const std::wstring& text)
    {
        HRESULT editSessionResult = S_OK;
        auto* editSession = new EditSession(this, commit ? EditAction::Commit : (text.empty() ? EditAction::Clear : EditAction::Update), text);
        context->RequestEditSession(clientId_, editSession, TF_ES_SYNC | TF_ES_READWRITE, &editSessionResult);
        editSession->Release();
    }

    void TextService::InsertMappedPunctuation(ITfContext* context, const wchar_t ch)
    {
        UpdateComposition(context, true, Engine::PinyinEngine::MapPunctuation(ch));
    }

    bool TextService::ShouldTriggerAsync() const
    {
        return config_.llmEnabled &&
            config_.llmProvider != L"cloud" &&
            composition_.size() >= static_cast<std::size_t>(config_.trigger.minChars);
    }

    void TextService::MovePage(const int delta)
    {
        std::scoped_lock lock(stateMutex_);
        const auto count = PageCount();
        if (count <= 1)
        {
            return;
        }

        const auto next = static_cast<int>(currentPageIndex_) + delta;
        if (next < 0)
        {
            currentPageIndex_ = count - 1;
        }
        else if (next >= static_cast<int>(count))
        {
            currentPageIndex_ = 0;
        }
        else
        {
            currentPageIndex_ = static_cast<std::size_t>(next);
        }
        selectedIndex_ = 0;
        RefreshVisiblePage();
    }

    void TextService::MoveSelection(const int delta)
    {
        std::scoped_lock lock(stateMutex_);
        if (visibleCandidates_.empty())
        {
            return;
        }

        const auto next = static_cast<int>(selectedIndex_) + delta;
        if (next < 0)
        {
            selectedIndex_ = visibleCandidates_.size() - 1;
        }
        else if (next >= static_cast<int>(visibleCandidates_.size()))
        {
            selectedIndex_ = 0;
        }
        else
        {
            selectedIndex_ = static_cast<std::size_t>(next);
        }

        candidateWindow_.Show(ResolveAnchorPoint(), visibleCandidates_, selectedIndex_, currentPageIndex_, PageCount(), allCandidates_.size());
    }

    std::size_t TextService::PageCount() const
    {
        return allCandidates_.empty() ? 0 : ((allCandidates_.size() + kPageSize - 1) / kPageSize);
    }

    POINT TextService::ResolveAnchorPoint() const
    {
        POINT point{24, 24};
        const auto hwnd = GetForegroundWindow();
        if (GetCaretPos(&point) && hwnd)
        {
            ClientToScreen(hwnd, &point);
            return point;
        }

        RECT rc{};
        if (hwnd && GetWindowRect(hwnd, &rc))
        {
            return {rc.left + 24, rc.top + 48};
        }

        return point;
    }

    void TextService::RefreshVisiblePage()
    {
        visibleCandidates_.clear();
        if (allCandidates_.empty())
        {
            candidateWindow_.Hide();
            return;
        }

        const auto begin = currentPageIndex_ * kPageSize;
        const auto end = std::min(begin + kPageSize, allCandidates_.size());
        visibleCandidates_.assign(allCandidates_.begin() + static_cast<std::ptrdiff_t>(begin), allCandidates_.begin() + static_cast<std::ptrdiff_t>(end));
        if (selectedIndex_ >= visibleCandidates_.size())
        {
            selectedIndex_ = 0;
        }
        candidateWindow_.Show(ResolveAnchorPoint(), visibleCandidates_, selectedIndex_, currentPageIndex_, PageCount(), allCandidates_.size());
    }

    void TextService::ResetCandidates()
    {
        composition_.clear();
        traditionalCandidates_.clear();
        asyncCandidates_.clear();
        allCandidates_.clear();
        visibleCandidates_.clear();
        currentPageIndex_ = 0;
        selectedIndex_ = 0;
        ++generation_;
    }
}

STDMETHODIMP EstraIme::Tip::TextService::EditSession::DoEditSession(const TfEditCookie ec)
{
    auto threadMgr = owner_->threadMgr_;
    if (!threadMgr)
    {
        return E_FAIL;
    }

    winrt::com_ptr<ITfDocumentMgr> documentMgr;
    auto hr = threadMgr->GetFocus(documentMgr.put());
    if (FAILED(hr) || !documentMgr)
    {
        return FAILED(hr) ? hr : E_FAIL;
    }

    winrt::com_ptr<ITfContext> context;
    hr = documentMgr->GetTop(context.put());
    if (FAILED(hr) || !context)
    {
        return FAILED(hr) ? hr : E_FAIL;
    }

    auto insertAtSelection = context.try_as<ITfInsertAtSelection>();
    auto contextComposition = context.try_as<ITfContextComposition>();
    if (!insertAtSelection || !contextComposition)
    {
        return E_FAIL;
    }

    const auto collapseSelectionToRangeEnd = [&](ITfRange* sourceRange) -> HRESULT {
        winrt::com_ptr<ITfRange> caretRange;
        auto selectionHr = sourceRange->Clone(caretRange.put());
        if (FAILED(selectionHr))
        {
            return selectionHr;
        }

        selectionHr = caretRange->Collapse(ec, TF_ANCHOR_END);
        if (FAILED(selectionHr))
        {
            return selectionHr;
        }

        TF_SELECTION selection{};
        selection.range = caretRange.get();
        selection.style.ase = TF_AE_NONE;
        selection.style.fInterimChar = FALSE;
        return context->SetSelection(ec, 1, &selection);
    };

    if (action_ == EditAction::Clear)
    {
        if (owner_->compositionRange_)
        {
            owner_->compositionRange_->EndComposition(ec);
            owner_->compositionRange_ = nullptr;
        }
        return S_OK;
    }

    if (!owner_->compositionRange_ && action_ == EditAction::Update)
    {
        winrt::com_ptr<ITfRange> range;
        hr = insertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, L"", 0, range.put());
        if (FAILED(hr))
        {
            return hr;
        }
        hr = contextComposition->StartComposition(ec, range.get(), static_cast<ITfCompositionSink*>(owner_), owner_->compositionRange_.put());
        if (FAILED(hr))
        {
            return hr;
        }
    }

    if (owner_->compositionRange_)
    {
        winrt::com_ptr<ITfRange> range;
        hr = owner_->compositionRange_->GetRange(range.put());
        if (FAILED(hr))
        {
            return hr;
        }
        hr = range->SetText(ec, 0, text_.c_str(), static_cast<LONG>(text_.size()));
        if (FAILED(hr))
        {
            return hr;
        }

        hr = collapseSelectionToRangeEnd(range.get());
        if (FAILED(hr))
        {
            return hr;
        }

        if (action_ == EditAction::Commit)
        {
            owner_->compositionRange_->EndComposition(ec);
            owner_->compositionRange_ = nullptr;
        }
        return S_OK;
    }

    if (action_ == EditAction::Commit)
    {
        winrt::com_ptr<ITfRange> range;
        return insertAtSelection->InsertTextAtSelection(ec, 0, text_.c_str(), static_cast<LONG>(text_.size()), range.put());
    }

    return S_OK;
}
