/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */
#ifndef INCLUDED_SFX2_VIEWSH_HXX
#define INCLUDED_SFX2_VIEWSH_HXX

#include <sal/config.h>
#include <memory>
#include <optional>
#include <sfx2/dllapi.h>
#include <sal/types.h>
#include <com/sun/star/uno/Reference.h>
#include <svl/lstner.hxx>
#include <sfx2/shell.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <vcl/IDialogRenderable.hxx>
#include <comphelper/errcode.hxx>
#include <o3tl/typed_flags_set.hxx>
#include <vcl/vclptr.hxx>
#include <editeng/outliner.hxx>
#include <functional>
#include <unordered_set>
#include <unordered_map>

class SfxTabPage;
class SfxBaseController;
class Fraction;
namespace weld {
    class Container;
    class DialogController;
    class Window;
}
class KeyEvent;
class SvBorder;
class SdrView;
class SfxModule;
class SfxViewFrame;
class Printer;
class SfxPrinter;
class NotifyEvent;
class SfxInPlaceClient;
class SfxLokCallbackInterface;
class LOKDocumentFocusListener;
class SfxStoringHelper;
class VCLXPopupMenu;
namespace rtl { class OStringBuffer; }
namespace vcl { class PrinterController; }

namespace com::sun::star::beans { struct PropertyValue; }
namespace com::sun::star::datatransfer::clipboard { class XClipboardListener; }
namespace com::sun::star::datatransfer::clipboard { class XClipboardNotifier; }
namespace com::sun::star::embed { class XEmbeddedObject; }
namespace com::sun::star::frame { class XController; }
namespace com::sun::star::frame { class XModel; }
namespace com::sun::star::ui { class XContextMenuInterceptor; }
namespace com::sun::star::ui { struct ContextMenuExecuteEvent; }
namespace com::sun::star::view { class XRenderable; }
namespace com::sun::star::security { class XCertificate; }
namespace svtools { enum ColorConfigEntry : int; }
namespace svl::crypto { class CertificateOrName; }

enum class SfxPrinterChangeFlags
{
    NONE             = 0,
    PRINTER          = 1,  // without JOB SETUP => Temporary
    JOBSETUP         = 2,
    OPTIONS          = 4,
    CHG_ORIENTATION  = 8,
    CHG_SIZE         = 16
};
namespace o3tl
{
    template<> struct typed_flags<SfxPrinterChangeFlags> : is_typed_flags<SfxPrinterChangeFlags, 31> {};
}
#define SFX_PRINTER_ALL             (SfxPrinterChangeFlags::PRINTER | SfxPrinterChangeFlags::JOBSETUP | SfxPrinterChangeFlags::OPTIONS | SfxPrinterChangeFlags::CHG_ORIENTATION | SfxPrinterChangeFlags::CHG_SIZE)

#define SFX_PRINTERROR_BUSY          1

// "Verified" using www.apple.com and Netscape 3.01
#define DEFAULT_MARGIN_WIDTH 8
#define DEFAULT_MARGIN_HEIGHT 12


//  @[SfxViewShell-Flags]

enum class SfxViewShellFlags
{
    NONE              = 0x0000,
    HAS_PRINTOPTIONS  = 0x0010, /* Options-Button and Options-Dialog in PrintDialog */
    NO_NEWWINDOW      = 0x0100, /* Allow N View */
};
namespace o3tl
{
    template<> struct typed_flags<SfxViewShellFlags> : is_typed_flags<SfxViewShellFlags, 0x0110> {};
}

/*  [Description]

    The SfxViewShell flags control the behavior of SfxViewShell for the
    duration of its lifetime. They are defined in the constructor of
    <SfxViewShell>.
*/

enum class LOKDeviceFormFactor
{
    UNKNOWN     = 0,
    DESKTOP     = 1,
    TABLET      = 2,
    MOBILE      = 3
};

class SfxViewFactory;
#define SFX_DECL_VIEWFACTORY(Class) \
private: \
    static SfxViewFactory *s_pFactory; \
public: \
    static SfxViewShell  *CreateInstance(SfxViewFrame& rFrame, SfxViewShell *pOldView); \
    static void           RegisterFactory( SfxInterfaceId nPrio ); \
    static SfxViewFactory*Factory() { return s_pFactory; } \
    static void           InitFactory()

#define SFX_IMPL_NAMED_VIEWFACTORY(Class, AsciiViewName) \
    SfxViewFactory* Class::s_pFactory; \
    SfxViewShell* Class::CreateInstance(SfxViewFrame& rFrame, SfxViewShell *pOldView) \
    { return new Class(rFrame, pOldView); } \
    void Class::RegisterFactory( SfxInterfaceId nPrio ) \
    { \
        s_pFactory = new SfxViewFactory(&CreateInstance,nPrio,AsciiViewName);\
        InitFactory(); \
    } \
    void Class::InitFactory()

#define SFX_VIEW_REGISTRATION(DocClass) \
            DocClass::Factory().RegisterViewFactory( *Factory() )

template<class T> bool checkSfxViewShell(const SfxViewShell& pShell)
{
    return dynamic_cast<const T*>(&pShell) != nullptr;
}

typedef std::unordered_map<OUString, std::pair<Color, int>> StylesSpotlightColorMap;

/**
 * One SfxViewShell more or less represents one edit window for a document, there can be multiple
 * ones for a single opened document (SfxObjectShell).
 */
class SFX2_DLLPUBLIC SfxViewShell: public SfxShell, public SfxListener, public OutlinerViewShell, public vcl::ILibreOfficeKitNotifier
{
friend class SfxViewFrame;
friend class SfxBaseController;
friend class SfxPrinterController;

    std::unique_ptr<struct SfxViewShell_Impl>   pImpl;
    SfxViewFrame&               rFrame;
    VclPtr<vcl::Window>         pWindow;
    bool                        bNoNewWindow;
    bool                        mbPrinterSettingsModified;
    LanguageTag                 maLOKLanguageTag;
    LanguageTag                 maLOKLocale;
    LOKDeviceFormFactor         maLOKDeviceFormFactor;
    bool                        mbLOKAccessibilityEnabled;
    rtl::Reference<LOKDocumentFocusListener>   mpLOKDocumentFocusListener;
    std::unordered_set<OUString>    mvLOKBlockedCommandList;
    OUString maLOKTimezone;
    bool maLOKIsTimezoneSet;

    /// Used to set the DocId at construction time. See SetCurrentDocId.
    static ViewShellDocId       mnCurrentDocId;

    /// Used for async export
    std::shared_ptr<SfxStoringHelper> m_xHelper;

    StylesSpotlightColorMap ParaStylesSpotlightColorMap;
    StylesSpotlightColorMap CharStylesSpotlightColorMap;

protected:
    virtual void                Activate(bool IsMDIActivate) override;
    virtual void                Deactivate(bool IsMDIActivate) override;

    virtual void                InnerResizePixel( const Point &rOfs, const Size &rSize, bool inplaceEditModeChange );
    virtual void                OuterResizePixel( const Point &rOfs, const Size &rSize );
    virtual void                SetZoomFactor( const Fraction &rZoomX, const Fraction &rZoomY );

    virtual void                Move();

    virtual void                Notify( SfxBroadcaster& rBC, const SfxHint& rHint ) override;

public:
    // Iteration
    SAL_RET_MAYBENULL static SfxViewShell* GetFirst( bool bOnlyVisible = true, const std::function<bool ( const SfxViewShell& )>& isViewShell = nullptr );
    SAL_RET_MAYBENULL static SfxViewShell* GetNext( const SfxViewShell& rPrev,
                   bool bOnlyVisible = true,
                   const std::function<bool ( const SfxViewShell& )>& isViewShell = nullptr );
    SAL_RET_MAYBENULL static SfxViewShell* Current();
    SAL_WARN_UNUSED_RESULT static bool IsCurrentLokViewReadOnly();

    SAL_RET_MAYBENULL static SfxViewShell* Get( const css::uno::Reference< css::frame::XController>& i_rController );

    // Initialize Constructors/Destructors
                                SFX_DECL_INTERFACE(SFX_INTERFACE_SFXVIEWSH)

private:
    /// SfxInterface initializer.
    static void InitInterface_Impl();

    LOKDocumentFocusListener& GetLOKDocumentFocusListener();
    const LOKDocumentFocusListener& GetLOKDocumentFocusListener() const;
    bool lokReadOnlyView : 1 = false; // When true, this is a LOK readonly view
    bool allowChangeComments : 1 = false; // Allow editing comments in readonly view mode
    bool allowManageRedlines : 1 = false; // Allow accepting/rejecting changes in readonly view mode

public:

                                SfxViewShell( SfxViewFrame& rFrame, SfxViewShellFlags nFlags );
    virtual                     ~SfxViewShell() override;

    /// Informs the view shell that it'll be deleted before the main loop processes the next user
    /// input.
    virtual void SetDying() {}

    SfxInPlaceClient*           GetIPClient() const;
    SfxInPlaceClient*           GetUIActiveClient() const;
    SfxInPlaceClient*           FindIPClient( const css::uno::Reference < css::embed::XEmbeddedObject >&  xObj, vcl::Window *pObjParentWin ) const;

    virtual ErrCode             DoVerb(sal_Int32 nVerb);

    void                        OutplaceActivated( bool bActive );
    virtual void                UIActivating( SfxInPlaceClient* pClient );
    virtual void                UIDeactivated( SfxInPlaceClient* pClient );

    void                        JumpToMark( const OUString& rMark );
    void                        VisAreaChanged();

    void                        SetLokReadOnlyView(bool readOnlyView) { lokReadOnlyView = readOnlyView; };
    bool                        IsLokReadOnlyView() const { return lokReadOnlyView; };
    void                        SetAllowChangeComments(bool allow) { allowChangeComments = allow; }
    bool                        IsAllowChangeComments() const { return allowChangeComments; }
    void                        SetAllowManageRedlines(bool allow) { allowManageRedlines = allow; }
    bool                        IsAllowManageRedlines() const { return allowManageRedlines; }

    // Misc

    /**
     * Initialize is called after the frame has been loaded and the controller
     * has been set.  By the time this is called the document has been fully
     * imported.
     * @param bOnlyASample used by some dialogs to avoid constructing monster strings e.g. in calc
     */
    virtual bool                PrepareClose( bool bUI = true );
    virtual OUString            GetSelectionText( bool bCompleteWords = false, bool bOnlyASample = false );
    virtual bool                HasSelection( bool bText = true ) const;
    virtual SdrView*            GetDrawView() const;

    void                        AddSubShell( SfxShell& rShell );
    void                        RemoveSubShell( SfxShell *pShell=nullptr );
    SfxShell*                   GetSubShell( sal_uInt16 );

    virtual       SfxShell*     GetFormShell()       { return nullptr; };
    virtual const SfxShell*     GetFormShell() const { return nullptr; };

    // ILibreOfficeKitNotifier
    virtual void                notifyWindow(vcl::LOKWindowId nLOKWindowId, const OUString& rAction, const std::vector<vcl::LOKPayloadItem>& rPayload = std::vector<vcl::LOKPayloadItem>()) const override;
    virtual OString             dumpNotifyState() const override;

    // Focus, KeyInput, Cursor
    virtual void                ShowCursor( bool bOn = true );
    virtual bool                KeyInput( const KeyEvent &rKeyEvent );
    bool                        Escape();

    // Viewing Interface
    vcl::Window*                GetWindow() const { return pWindow; }
    weld::Window*               GetFrameWeld() const;
    void                        SetWindow( vcl::Window *pViewPort );
    const SvBorder&             GetBorderPixel() const;
    void                        SetBorderPixel( const SvBorder &rBorder );
    void                        InvalidateBorder();

    /*  [Description]

        This method returns a reference to the <SfxViewFrame> Instance in which
        this SfxViewShell is displayed. This is the instance that was passed
        on in the constructor. It is guaranteed that the returned reference
        is a valid SfxViewFrame instance.

        [Cross-reference]

        <SfxShell::GetFrame()const>
    */
    SfxViewFrame& GetViewFrame() const
    {
        return rFrame;
    }

    // Printing Interface
    virtual SfxPrinter*         GetPrinter( bool bCreate = false );
    virtual sal_uInt16          SetPrinter( SfxPrinter *pNewPrinter, SfxPrinterChangeFlags nDiffFlags = SFX_PRINTER_ALL );
    virtual bool                HasPrintOptionsPage() const;
    virtual std::unique_ptr<SfxTabPage>  CreatePrintOptionsPage(weld::Container* pPage, weld::DialogController* pController, const SfxItemSet &rOptions);
    Printer*                    GetActivePrinter() const;

    // Working set
    virtual void                WriteUserData( OUString&, bool bBrowse = false );
    virtual void                ReadUserData( const OUString&, bool bBrowse = false );
    virtual void                WriteUserDataSequence ( css::uno::Sequence < css::beans::PropertyValue >& );
    virtual void                ReadUserDataSequence ( const css::uno::Sequence < css::beans::PropertyValue >& );
    virtual void                QueryObjAreaPixel( tools::Rectangle& rRect ) const;

    virtual SfxObjectShell*     GetObjectShell() override;

    /** retrieves the document which shall be considered the "current document" when the frame is active

        The default implementation simply returns the XModel of the associated SfxObjectShell. You will rarely
        need to overwrite this behavior.
    */
    virtual css::uno::Reference< css::frame::XModel >
                                GetCurrentDocument() const;

    /** forwards the current document, as returned by ->GetCurrentDocument, to SfxObjectShell::SetWorkingDocument
    */
    void                        SetCurrentDocument() const;

    /** get an XRenderable instance that can render this document
    */
    virtual css::uno::Reference< css::view::XRenderable > GetRenderable();


    virtual void                MarginChanged();
    const Size&                 GetMargin() const;
    void                        SetMargin( const Size& );
    void                        DisconnectAllClients();
    bool                        NewWindowAllowed() const            { return !bNoNewWindow; }
    void                        SetNewWindowAllowed( bool bSet )    { bNoNewWindow = !bSet; }

    void                        SetController( SfxBaseController* pController );
    css::uno::Reference<css::frame::XController> GetController() const;

    bool                        TryContextMenuInterception(const rtl::Reference<VCLXPopupMenu>& rIn,
                                                           const OUString& rMenuIdentifier,
                                                           rtl::Reference<VCLXPopupMenu>& rOut,
                                                           css::ui::ContextMenuExecuteEvent aEvent);
    bool                        TryContextMenuInterception(const rtl::Reference<VCLXPopupMenu>&,
                                                           const OUString& rMenuIdentifier,
                                                           css::ui::ContextMenuExecuteEvent aEvent);

    void                        ExecPrint( const css::uno::Sequence < css::beans::PropertyValue >&, bool, bool );
    // Like ExecPrint(), but only sets up for printing. Use Printer::ExecutePrintJob() and Printer::FinishPrintJob() afterwards.
    void                        StartPrint( const css::uno::Sequence < css::beans::PropertyValue >&, bool, bool );
    const std::shared_ptr< vcl::PrinterController >& GetPrinterController() const;

    void                        AddRemoveClipboardListener( const css::uno::Reference < css::datatransfer::clipboard::XClipboardListener>&, bool );
    css::uno::Reference< css::datatransfer::clipboard::XClipboardNotifier > GetClipboardNotifier() const;

    SAL_DLLPRIVATE SfxInPlaceClient* GetUIActiveIPClient_Impl() const;
    SAL_DLLPRIVATE void AddContextMenuInterceptor_Impl( const css::uno::Reference < css::ui::XContextMenuInterceptor >& xInterceptor );
    SAL_DLLPRIVATE void RemoveContextMenuInterceptor_Impl( const css::uno::Reference < css::ui::XContextMenuInterceptor >& xInterceptor );
    SAL_DLLPRIVATE bool GlobalKeyInput_Impl( const KeyEvent &rKeyEvent );

    SAL_DLLPRIVATE void NewIPClient_Impl( SfxInPlaceClient *pIPClient );
    SAL_DLLPRIVATE void IPClientGone_Impl( SfxInPlaceClient const *pIPClient );
    SAL_DLLPRIVATE void ResetAllClients_Impl( SfxInPlaceClient const *pIP );

    SAL_DLLPRIVATE void SetPrinter_Impl( VclPtr<SfxPrinter>& pNewPrinter );

    SAL_DLLPRIVATE bool HandleNotifyEvent_Impl( NotifyEvent const & rEvent );
    SAL_DLLPRIVATE bool HasKeyListeners_Impl() const;
    SAL_DLLPRIVATE bool HasMouseClickListeners_Impl() const;

    SAL_DLLPRIVATE SfxBaseController*   GetBaseController_Impl() const;

    // Shell Interface
    SAL_DLLPRIVATE void ExecPrint_Impl(SfxRequest &);
    SAL_DLLPRIVATE void ExecMisc_Impl(SfxRequest &);
    SAL_DLLPRIVATE void GetState_Impl(SfxItemSet&);
    SAL_DLLPRIVATE void CheckIPClient_Impl(SfxInPlaceClient const *, const tools::Rectangle&);
    SAL_DLLPRIVATE void PushSubShells_Impl( bool bPush=true );
    SAL_DLLPRIVATE void PopSubShells_Impl() { PushSubShells_Impl( false ); }
    SAL_DLLPRIVATE bool ExecKey_Impl(const KeyEvent& aKey);

    /// Set up a more efficient internal callback instead of LibreOfficeKitCallback.
    void setLibreOfficeKitViewCallback(SfxLokCallbackInterface* pCallback);
    SfxLokCallbackInterface* getLibreOfficeKitViewCallback() const;
    /// dump view state for diagnostics
    void dumpLibreOfficeKitViewState(rtl::OStringBuffer &rState);
    /// Invokes the registered callback, if there are any.
    virtual void libreOfficeKitViewCallback(int nType, const OString& pPayload) const override;
    virtual void libreOfficeKitViewCallbackWithViewId(int nType, const OString& pPayload, int nViewId) const override;
    virtual void libreOfficeKitViewInvalidateTilesCallback(const tools::Rectangle* pRect, int nPart, int nMode) const override;
    virtual void libreOfficeKitViewUpdatedCallback(int nType) const override;
    virtual void libreOfficeKitViewUpdatedCallbackPerViewId(int nType, int nViewId, int nSourceViewId) const override;
    // Performs any pending calls to libreOfficeKitViewInvalidateTilesCallback() as necessary.
    virtual void flushPendingLOKInvalidateTiles();
    virtual void libreOfficeKitViewAddPendingInvalidateTiles() override;
    // Returns current payload for nType, after libreOfficeKitViewUpdatedCallback() or
    // libreOfficeKitViewUpdatedCallbackPerViewId() were called. If no payload should
    // be generated, the ignore flag should be set.
    virtual std::optional<OString> getLOKPayload(int nType, int nViewId) const;

    /// Set if we are doing tiled searching.
    void setTiledSearching(bool bTiledSearching);
    /// See lok::Document::getPart().
    virtual int getPart() const;
    /// See lok::Document::getMode().
    virtual int getEditMode() const;
    virtual void dumpAsXml(xmlTextWriterPtr pWriter) const;
    /// See OutlinerViewShell::GetViewShellId().
    ViewShellId GetViewShellId() const override;

    /// Set the current DocId, which is used by Mobile LOKit to
    /// load multiple documents and yet identify the views of each.
    /// There are events that are fired while creating a new view,
    /// and if we don't have a DocId, we can't know which other views
    /// within the same document (if any) should get those events.
    /// By setting this static value, we are able to set the DocId
    /// of each SfxViewShell at construction time.
    static void SetCurrentDocId(ViewShellDocId nId);
    /// Get the DocId used by Mobile LOKit to load multiple documents.
    ViewShellDocId GetDocId() const override;

    /// ILibreOfficeKitNotifier. Emits a LOK_CALLBACK_INVALIDATE_TILES.
    virtual void notifyInvalidation(tools::Rectangle const *) const override;
    /// See OutlinerViewShell::NotifyOtherViews().
    void NotifyOtherViews(int nType, const OString& rKey, const OString& rPayload) override;
    /// See OutlinerViewShell::NotifyOtherView().
    void NotifyOtherView(OutlinerViewShell* pOtherShell, int nType, const OString& rKey, const OString& rPayload) override;
    /// Ask this view to send its cursor position to pViewShell.
    virtual void NotifyCursor(SfxViewShell* /*pViewShell*/) const;
    /// Where a new view can perform some update/initialization soon after the callback has been registered.
    virtual void afterCallbackRegistered();
    /// See OutlinerViewShell::GetEditWindowForActiveOLEObj().
    virtual vcl::Window* GetEditWindowForActiveOLEObj() const override;
    /// Get a color config color from this view
    virtual ::Color GetColorConfigColor(svtools::ColorConfigEntry nColorType) const;

    /// Set the LibreOfficeKit language of this view.
    void SetLOKLanguageTag(const OUString& rBcp47LanguageTag);
    /// Get the LibreOfficeKit language of this view.
    const LanguageTag& GetLOKLanguageTag() const { return maLOKLanguageTag; }
    /// Enable/Disable LibreOfficeKit AT support for this view.
    void SetLOKAccessibilityState(bool bEnabled);
    /// Get LibreOfficeKit AT support state for this view.
    bool GetLOKAccessibilityState() const { return mbLOKAccessibilityEnabled; }

    /// Get the LibreOfficeKit timezone of this view. See @SetLOKTimezone.
    std::pair<bool, OUString> GetLOKTimezone() const
    {
        return { maLOKIsTimezoneSet, maLOKTimezone };
    }

    /// Set the LibreOfficeKit timezone of this view.
    /// @isSet true to use @rTimezone, even if it's empty. Otherwise, no timezone.
    /// @rTimezone the value to set (which could be empty).
    void SetLOKTimezone(bool isSet, const OUString& rTimezone)
    {
        maLOKIsTimezoneSet = isSet;
        maLOKTimezone = rTimezone;
    }

    /// Set the LibreOfficeKit locale of this view.
    void SetLOKLocale(const OUString& rBcp47LanguageTag);
    /// Get the LibreOfficeKit locale of this view.
    const LanguageTag& GetLOKLocale() const { return maLOKLocale; }
    /// Get the form factor of the device where the lok client is running.
    LOKDeviceFormFactor GetLOKDeviceFormFactor() const { return maLOKDeviceFormFactor; }
    /// Check if the lok client is running on a desktop machine.
    bool isLOKDesktop() const { return maLOKDeviceFormFactor == LOKDeviceFormFactor::DESKTOP; }
    /// Check if the lok client is running on a tablet.
    bool isLOKTablet() const  { return maLOKDeviceFormFactor == LOKDeviceFormFactor::TABLET; }
    /// Check if the lok client is running on a mobile device.
    bool isLOKMobilePhone() const { return maLOKDeviceFormFactor == LOKDeviceFormFactor::MOBILE; }

    virtual tools::Rectangle getLOKVisibleArea() const { return tools::Rectangle(); }

    // Blocked Command view settings
    void setBlockedCommandList(const char* blockedCommandList);
    bool isBlockedCommand(const OUString & command) const;

    void SetStoringHelper(const std::shared_ptr<SfxStoringHelper>& xHelper) { m_xHelper = xHelper; }

    StylesSpotlightColorMap& GetStylesSpotlightParaColorMap() { return ParaStylesSpotlightColorMap; }
    StylesSpotlightColorMap& GetStylesSpotlightCharColorMap() { return CharStylesSpotlightColorMap; }

    OUString getA11yFocusedParagraph() const;
    int getA11yCaretPosition() const;
    void SetSigningCertificate(const svl::crypto::CertificateOrName& rCertificateOrName);
    svl::crypto::CertificateOrName GetSigningCertificate() const;

    // These are used for visual signing: SetSignPDFCertificate() is called when the signature
    // line is inserted, and GetSignPDFCertificate() is used by the signing code to get the already
    // selected certificate.
    void SetSignPDFCertificate(const svl::crypto::CertificateOrName& rCertificateOrName);
    svl::crypto::CertificateOrName GetSignPDFCertificate() const;
};

#endif // INCLUDED_SFX2_VIEWSH_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
