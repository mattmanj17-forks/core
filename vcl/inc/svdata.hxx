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

#pragma once

#include <sal/config.h>

#include <o3tl/lru_map.hxx>
#include <o3tl/hash_combine.hxx>
#include <o3tl/sorted_vector.hxx>
#include <osl/conditn.hxx>
#include <tools/fldunit.hxx>
#include <unotools/options.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/cvtgrf.hxx>
#include <vcl/dropcache.hxx>
#include <vcl/image.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/print.hxx>
#include <vcl/uitest/logger.hxx>
#include <vcl/virdev.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/window.hxx>
#include <vcl/task.hxx>
#include <LibreOfficeKit/LibreOfficeKitTypes.h>
#include <unotools/resmgr.hxx>

#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/i18n/XCharacterClassification.hpp>
#include "vcleventlisteners.hxx"
#include "print.h"
#include "salwtype.hxx"
#include "windowdev.hxx"
#include "displayconnectiondispatch.hxx"

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>
#include <unordered_map>
#include "schedulerimpl.hxx"
#include <basegfx/DrawCommands.hxx>

struct ImplPostEventData;
struct ImplTimerData;
struct ImplIdleData;
struct ImplConfigData;
namespace rtl
{
    class OStringBuffer;
}
namespace vcl::font
{
    class DirectFontSubstitution;
    class PhysicalFontCollection;
}
struct BlendFrameCache;
struct ImplHotKey;
struct ImplEventHook;
class Point;
class ImplAccelManager;
class ImplFontCache;
class HelpTextWindow;
class ImplTBDragMgr;
class ImplIdleMgr;
class FloatingWindow;
class AllSettings;
class NotifyEvent;
class Timer;
class AutoTimer;
class Idle;
class Help;
class PopupMenu;
class Application;
class OutputDevice;
class SvFileStream;
class SystemWindow;
class WorkWindow;
class Dialog;
class VirtualDevice;
class Printer;
class SalFrame;
class SalInstance;
class SalSystem;
class ImplPrnQueueList;
class UnoWrapperBase;
class GraphicConverter;
class ImplWheelWindow;
class SalTimer;
class DockingManager;
class VclEventListeners2;
class SalData;
class OpenGLContext;
class UITestLogger;

#define SV_ICON_ID_OFFICE                               1
#define SV_ICON_ID_TEXT                                 2
#define SV_ICON_ID_TEXT_TEMPLATE                        3
#define SV_ICON_ID_SPREADSHEET                          4
#define SV_ICON_ID_SPREADSHEET_TEMPLATE                 5
#define SV_ICON_ID_DRAWING                              6
#define SV_ICON_ID_PRESENTATION                         8
#define SV_ICON_ID_MASTER_DOCUMENT                     10
#define SV_ICON_ID_TEMPLATE                            11
#define SV_ICON_ID_DATABASE                            12
#define SV_ICON_ID_FORMULA                             13

const FloatWinPopupFlags LISTBOX_FLOATWINPOPUPFLAGS = FloatWinPopupFlags::Down |
    FloatWinPopupFlags::NoHorzPlacement | FloatWinPopupFlags::AllMouseButtonClose;

namespace com::sun::star::datatransfer::clipboard { class XClipboard; }

namespace vcl
{
    class DisplayConnectionDispatch;
    class Window;
}

namespace basegfx
{
    class SystemDependentDataManager;
}

class LocaleConfigurationListener final : public utl::ConfigurationListener
{
public:
    virtual void ConfigurationChanged( utl::ConfigurationBroadcaster*, ConfigurationHints ) override;
};

typedef std::pair<VclPtr<vcl::Window>, ImplPostEventData *> ImplPostEventPair;

struct ImplSVAppData
{
    ImplSVAppData();
    ~ImplSVAppData();

    std::optional<AllSettings> mxSettings;           // Application settings
    LocaleConfigurationListener* mpCfgListener = nullptr;
    VclEventListeners       maEventListeners;     // listeners for vcl events (eg, extended toolkit)
    std::vector<Link<VclWindowEvent&,bool> >
                            maKeyListeners;       // listeners for key events only (eg, extended toolkit)
    std::vector<ImplPostEventPair> maPostedEventList;
    ImplAccelManager*       mpAccelMgr = nullptr; // Accelerator Manager
    std::optional<OUString> mxAppName;            // Application name
    std::optional<OUString> mxAppFileName;        // Abs. Application FileName
    std::optional<OUString> mxDisplayName;        // Application Display Name
    std::optional<OUString> mxToolkitName;        // Toolkit Name
    Help*                   mpHelp = nullptr;               // Application help
    VclPtr<PopupMenu>       mpActivePopupMenu;              // Actives Popup-Menu (in Execute)
    VclPtr<ImplWheelWindow> mpWheelWindow;                  // WheelWindow
    sal_uInt64              mnLastInputTime = 0;            // GetLastInputTime()
    sal_uInt16              mnDispatchLevel = 0;            // DispatchLevel
    sal_uInt16              mnModalMode = 0;                // ModalMode Count
    SystemWindowFlags       mnSysWinMode = SystemWindowFlags(0); // Mode, when SystemWindows should be created
    bool                    mbInAppMain = false;            // is Application::Main() on stack
    bool                    mbInAppExecute = false;         // is Application::Execute() on stack
    std::atomic<bool>       mbAppQuit = false;              // is Application::Quit() called, volatile because we read/write from different threads
    bool                    mbSettingsInit = false;         // true: Settings are initialized
    DialogCancelMode meDialogCancel = DialogCancelMode::Off; // true: All Dialog::Execute() calls will be terminated immediately with return false
    bool mbRenderToBitmaps = false; // set via svp / headless plugin
    bool m_bUseSystemLoop = false;

    DECL_STATIC_LINK(ImplSVAppData, ImplQuitMsg, void*, void);
};

/// Cache multiple scalings for the same bitmap
struct ScaleCacheKey {
    SalBitmap *mpBitmap;
    Size       maDestSize;
    ScaleCacheKey(SalBitmap *pBitmap, const Size &aDestSize)
    {
        mpBitmap = pBitmap;
        maDestSize = aDestSize;
    }
    ScaleCacheKey(const ScaleCacheKey &key)
    {
        mpBitmap = key.mpBitmap;
        maDestSize = key.maDestSize;
    }
    bool operator==(ScaleCacheKey const& rOther) const
    {
        return mpBitmap == rOther.mpBitmap && maDestSize == rOther.maDestSize;
    }
};

namespace std
{
template <> struct hash<ScaleCacheKey>
{
    std::size_t operator()(ScaleCacheKey const& k) const noexcept
    {
        std::size_t seed = 0;
        o3tl::hash_combine(seed, k.mpBitmap);
        o3tl::hash_combine(seed, k.maDestSize.getWidth());
        o3tl::hash_combine(seed, k.maDestSize.getHeight());
        return seed;
    }
};

} // end std namespace

typedef o3tl::lru_map<ScaleCacheKey, BitmapEx> lru_scale_cache;

struct ImplSVGDIData
{
    ~ImplSVGDIData();

    VclPtr<vcl::WindowOutputDevice> mpFirstWinGraphics;     // First OutputDevice with a Frame Graphics
    VclPtr<vcl::WindowOutputDevice> mpLastWinGraphics;      // Last OutputDevice with a Frame Graphics
    VclPtr<OutputDevice>    mpFirstVirGraphics;             // First OutputDevice with a VirtualDevice Graphics
    VclPtr<OutputDevice>    mpLastVirGraphics;              // Last OutputDevice with a VirtualDevice Graphics
    VclPtr<Printer>         mpFirstPrnGraphics;             // First OutputDevice with an InfoPrinter Graphics
    VclPtr<Printer>         mpLastPrnGraphics;              // Last OutputDevice with an InfoPrinter Graphics
    VclPtr<VirtualDevice>   mpFirstVirDev;                  // First VirtualDevice
    OpenGLContext*          mpLastContext = nullptr;        // Last OpenGLContext
    VclPtr<Printer>         mpFirstPrinter;                 // First Printer
    std::unique_ptr<ImplPrnQueueList> mpPrinterQueueList;   // List of all printer queue
    std::shared_ptr<vcl::font::PhysicalFontCollection> mxScreenFontList; // Screen-Font-List
    std::shared_ptr<ImplFontCache> mxScreenFontCache;       // Screen-Font-Cache
    lru_scale_cache         maScaleCache = lru_scale_cache(10); // Cache for scaled images
    vcl::font::DirectFontSubstitution* mpDirectFontSubst = nullptr; // Font-Substitutions defined in Tools->Options->Fonts
    std::unique_ptr<GraphicConverter> mxGrfConverter;       // Converter for graphics
    tools::Long                    mnAppFontX = 0;                 // AppFont X-Numenator for 40/tel Width
    tools::Long                    mnAppFontY = 0;                 // AppFont Y-Numenator for 80/tel Height
    bool                    mbFontSubChanged = false;       // true: FontSubstitution was changed between Begin/End

    o3tl::lru_map<OUString, BitmapEx> maThemeImageCache = o3tl::lru_map<OUString, BitmapEx>(10);
    o3tl::lru_map<OUString, gfx::DrawRoot> maThemeDrawCommandsCache = o3tl::lru_map<OUString, gfx::DrawRoot>(50);
};

struct ImplSVFrameData
{
    ~ImplSVFrameData();
    VclPtr<vcl::Window>     mpFirstFrame;                   // First FrameWindow
    VclPtr<vcl::Window>     mpActiveApplicationFrame;       // the last active application frame, can be used as DefModalDialogParent if no focuswin set
    VclPtr<WorkWindow>      mpAppWin;                       // Application-Window

    std::unique_ptr<UITestLogger> m_pUITestLogger;
};

struct ImplSVWinData
{
    ~ImplSVWinData();
    VclPtr<vcl::Window>     mpFocusWin;                     // window, that has the focus
    VclPtr<vcl::Window>     mpCaptureWin;                   // window, that has the mouse capture
    VclPtr<vcl::Window>     mpLastDeacWin;                  // Window, that need a deactivate (FloatingWindow-Handling)
    VclPtr<FloatingWindow>  mpFirstFloat;                   // First FloatingWindow in PopupMode
    std::vector<VclPtr<Dialog>> mpExecuteDialogs;           ///< Stack of dialogs that are Execute()'d - the last one is the top most one.
    VclPtr<vcl::Window>     mpExtTextInputWin;              // Window, which is in ExtTextInput
    VclPtr<vcl::Window>     mpTrackWin;                     // window, that is in tracking mode
    std::unique_ptr<AutoTimer> mpTrackTimer;                // tracking timer
    std::vector<Image>      maMsgBoxImgList;                // ImageList for MessageBox
    VclPtr<vcl::Window>     mpAutoScrollWin;                // window, that is in AutoScrollMode mode
    VclPtr<vcl::Window>     mpLastWheelWindow;              // window, that last received a mouse wheel event
    SalWheelMouseEvent      maLastWheelEvent;               // the last received mouse wheel event

    StartTrackingFlags      mnTrackFlags = StartTrackingFlags::NONE; // tracking flags
    StartAutoScrollFlags    mnAutoScrollFlags = StartAutoScrollFlags::NONE; // auto scroll flags
    bool                    mbNoDeactivate = false;         // true: do not execute Deactivate
    bool                    mbNoSaveFocus = false;          // true: menus must not save/restore focus
    bool                    mbIsLiveResize = false;         // true: skip waiting for events and low priority timers
    bool                    mbIsWaitingForNativeEvent = false; // true: code is executing via a native callback while waiting for the next native event
};

typedef std::vector< std::pair< OUString, FieldUnit > > FieldUnitStringList;

struct ImplSVCtrlData
{
    std::vector<Image>      maCheckImgList;                 // ImageList for CheckBoxes
    std::vector<Image>      maRadioImgList;                 // ImageList for RadioButtons
    std::optional<Image>    moDisclosurePlus;
    std::optional<Image>    moDisclosureMinus;
    ImplTBDragMgr*          mpTBDragMgr = nullptr;          // DragMgr for ToolBox
    sal_uInt16              mnCheckStyle = 0;               // CheckBox-Style for ImageList-Update
    sal_uInt16              mnRadioStyle = 0;               // Radio-Style for ImageList-Update
    Color                   mnLastCheckFColor;              // Last FaceColor for CheckImage
    Color                   mnLastCheckWColor;              // Last WindowColor for CheckImage
    Color                   mnLastCheckLColor;              // Last LightColor for CheckImage
    Color                   mnLastRadioFColor;              // Last FaceColor for RadioImage
    Color                   mnLastRadioWColor;              // Last WindowColor for RadioImage
    Color                   mnLastRadioLColor;              // Last LightColor for RadioImage
    FieldUnitStringList     maFieldUnitStrings;   // list with field units
    FieldUnitStringList     maCleanUnitStrings;   // same list but with some "fluff" like spaces removed
};

struct ImplSVHelpData
{
    ~ImplSVHelpData();
    bool                    mbContextHelp = false;          // is ContextHelp enabled
    bool                    mbExtHelp = false;              // is ExtendedHelp enabled
    bool                    mbExtHelpMode = false;          // is in ExtendedHelp Mode
    bool                    mbOldBalloonMode = false;       // BalloonMode, before ExtHelpMode started
    bool                    mbBalloonHelp = false;          // is BalloonHelp enabled
    bool                    mbQuickHelp = false;            // is QuickHelp enabled
    bool                    mbSetKeyboardHelp = false;      // tiphelp was activated by keyboard
    bool                    mbKeyboardHelp = false;         // tiphelp was activated by keyboard
    bool                    mbRequestingHelp = false;       // In Window::RequestHelp
    VclPtr<HelpTextWindow>  mpHelpWin;                      // HelpWindow
    sal_uInt64              mnLastHelpHideTime = 0;         // ticks of last show
};

// "NWF" means "Native Widget Framework" and was the term used for the
// idea that StarView/OOo "widgets" should *look* (and feel) like the
// "native widgets" on each platform, even if not at all implemented
// using them. See http://people.redhat.com/dcbw/ooo-nwf.html .

struct ImplSVNWFData
{
    int                     mnStatusBarLowerRightOffset = 0; // amount in pixel to avoid in the lower righthand corner
    int                     mnMenuFormatBorderX = 0;        // horizontal inner popup menu border
    int                     mnMenuFormatBorderY = 0;        // vertical inner popup menu border
    ::Color                 maMenuBarHighlightTextColor = COL_TRANSPARENT; // override highlight text color
                                                            // in menubar if not transparent
    bool                    mbMenuBarDockingAreaCommonBG = false; // e.g. WinXP default theme
    bool                    mbDockingAreaSeparateTB = false; // individual toolbar backgrounds
                                                            // instead of one for docking area
    bool                    mbDockingAreaAvoidTBFrames = false; ///< don't draw frames around the individual toolbars if mbDockingAreaSeparateTB is false
    bool                    mbFlatMenu = false;             // no popup 3D border
    bool                    mbNoFocusRects = false;         // on Aqua/Gtk3 use native focus rendering, except for flat buttons
    bool                    mbNoFocusRectsForFlatButtons = false; // on Gtk3 native focusing is also preferred for flat buttons
    bool                    mbNoFrameJunctionForPopups = false; // on Gtk4 popups are done via popovers and a toolbar menu won't align to its toolitem, so
                                                                // omit the effort the creation a visual junction
    bool                    mbCenteredTabs = false;         // on Aqua, tabs are centered
    bool                    mbNoActiveTabTextRaise = false; // on Aqua the text for the selected tab
                                                            // should not "jump up" a pixel
    bool                    mbProgressNeedsErase = false;   // set true for platforms that should draw the
                                                            // window background before drawing the native
                                                            // progress bar
    bool                    mbCanDrawWidgetAnySize = false; // set to true currently on gtk

    /// entire drop down listbox resembles a button, no textarea/button parts (as currently on Windows)
    bool                    mbDDListBoxNoTextArea = false;
    bool                    mbAutoAccel = false;            // whether accelerators are only shown when Alt is held down
    bool                    mbRolloverMenubar = false;      // theming engine supports rollover in menubar
    // gnome#768128 I cannot see a route under wayland at present to support
    // floating toolbars that can be redocked because there's no way to track
    // that the toolbar is over a dockable area.
    bool                    mbCanDetermineWindowPosition = true;

    int mnListBoxEntryMargin = 0;
};

struct ImplSchedulerContext
{
    ImplSchedulerData*      mpFirstSchedulerData[PRIO_COUNT] = { nullptr, }; ///< list of all active tasks per priority
    ImplSchedulerData*      mpLastSchedulerData[PRIO_COUNT] = { nullptr, };  ///< last item of each mpFirstSchedulerData list
    ImplSchedulerData*      mpSchedulerStack = nullptr;     ///< stack of invoked tasks
    ImplSchedulerData*      mpSchedulerStackTop = nullptr;  ///< top most stack entry to detect needed rescheduling during pop
    SalTimer*               mpSalTimer = nullptr;           ///< interface to sal event loop / system timer
    sal_uInt64              mnTimerStart = 0;               ///< start time of the timer
    sal_uInt64              mnTimerPeriod = SAL_MAX_UINT64; ///< current timer period
    std::mutex              maMutex;                        ///< the "scheduler mutex" (see
                                                            ///< vcl/README.scheduler)
    bool                    mbActive = true;                ///< is the scheduler active?
    oslInterlockedCount     mnIdlesLockCount = 0;           ///< temporary ignore idles
};

struct ImplSVData
{
    ImplSVData();
    ~ImplSVData();
    SalData*                mpSalData = nullptr;
    SalInstance*            mpDefInst = nullptr;            // Default SalInstance
    Application*            mpApp = nullptr;                // pApp
    VclPtr<WorkWindow>      mpDefaultWin;                   // Default-Window
    bool                    mbDeInit = false;               // Is VCL deinitializing
    std::unique_ptr<SalSystem> mpSalSystem;                 // SalSystem interface
    bool                    mbResLocaleSet = false;         // SV-Resource-Manager
    std::locale             maResLocale;                    // Resource locale
    ImplSchedulerContext    maSchedCtx;                     // Data for class Scheduler
    ImplSVAppData           maAppData;                      // Data for class Application
    ImplSVGDIData           maGDIData;                      // Data for Output classes
    ImplSVFrameData         maFrameData;                    // Data for Frame classes
    ImplSVWinData*          mpWinData = nullptr;            // Data for per-view Windows classes
    ImplSVCtrlData          maCtrlData;                     // Data for Control classes
    ImplSVHelpData*         mpHelpData;                     // Data for Help classes
    ImplSVNWFData           maNWFData;
    UnoWrapperBase*         mpUnoWrapper = nullptr;
    VclPtr<vcl::Window>     mpIntroWindow;                  // the splash screen
    std::unique_ptr<DockingManager> mpDockingManager;
    std::unique_ptr<BlendFrameCache> mpBlendFrameCache;

    oslThreadIdentifier     mnMainThreadId = 0;
    rtl::Reference< vcl::DisplayConnectionDispatch > mxDisplayConnection;

    css::uno::Reference< css::lang::XComponent > mxAccessBridge;
    std::unordered_map< int, OUString > maPaperNames;
    o3tl::sorted_vector<CacheOwner*> maCacheOwners;

    css::uno::Reference<css::i18n::XCharacterClassification> m_xCharClass;

#if defined _WIN32
    css::uno::Reference<css::datatransfer::clipboard::XClipboard> m_xSystemClipboard;
#endif

    osl::Condition m_inExecuteCondtion; // Set when code returns to Application::Execute,
                                        // i.e. no nested message loops run

    Link<LinkParamNone*,void> maDeInitHook;

    // LOK & headless backend specific hooks
    LibreOfficeKitPollCallback mpPollCallback = nullptr;
    LibreOfficeKitWakeCallback mpWakeCallback = nullptr;
    void *mpPollClosure = nullptr;

    void registerCacheOwner(CacheOwner&);
    void deregisterCacheOwner(CacheOwner&);
    void dropCaches();
    void dumpState(rtl::OStringBuffer &rState);
};

css::uno::Reference<css::i18n::XCharacterClassification> const& ImplGetCharClass();

void        ImplDeInitSVData();
VCL_PLUGIN_PUBLIC basegfx::SystemDependentDataManager& ImplGetSystemDependentDataManager();
VCL_PLUGIN_PUBLIC vcl::Window* ImplGetDefaultWindow();
vcl::Window* ImplGetDefaultContextWindow();
const std::locale& ImplGetResLocale();
VCL_PLUGIN_PUBLIC OUString VclResId(TranslateId sContextAndId);
DockingManager*     ImplGetDockingManager();
void GenerateAutoMnemonicsOnHierarchy(const vcl::Window* pWindow);

VCL_PLUGIN_PUBLIC ImplSVHelpData& ImplGetSVHelpData();

VCL_DLLPUBLIC bool        ImplCallPreNotify( NotifyEvent& rEvt );

VCL_PLUGIN_PUBLIC ImplSVData* ImplGetSVData();
VCL_PLUGIN_PUBLIC void ImplHideSplash();

const FieldUnitStringList& ImplGetFieldUnits();
const FieldUnitStringList& ImplGetCleanedFieldUnits();

struct ImplSVEvent
{
    void*               mpData;
    Link<void*,void>    maLink;
    VclPtr<vcl::Window> mpInstanceRef;
    VclPtr<vcl::Window> mpWindow;
    bool                mbCall;
};

extern int nImplSysDialog;

inline SalData* GetSalData() { return ImplGetSVData()->mpSalData; }
inline void SetSalData(SalData* pData) { ImplGetSVData()->mpSalData = pData; }
inline SalInstance* GetSalInstance() { return ImplGetSVData()->mpDefInst; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
