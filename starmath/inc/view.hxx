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
#include <rtl/ref.hxx>
#include <sfx2/docinsert.hxx>
#include <sfx2/dockwin.hxx>
#include <sfx2/viewsh.hxx>
#include <sfx2/ctrlitem.hxx>
#include <sfx2/shell.hxx>
#include <sfx2/viewfrm.hxx>
#include <vcl/InterimItemWindow.hxx>
#include <vcl/timer.hxx>
#include "document.hxx"
#include "edit.hxx"

class SmViewShell;
class SmGraphicAccessible;
class SmGraphicWidget;

constexpr sal_uInt16 MINZOOM = 25;
constexpr sal_uInt16 MAXZOOM = 800;

class SmGraphicWindow final : public InterimItemWindow
{
private:
    Point aPixOffset; // offset to virtual window (pixel)
    Size aTotPixSz; // total size of virtual window (pixel)
    tools::Long nLinePixH; // size of a line/column (pixel)
    tools::Long nColumnPixW;
    sal_uInt16 nZoom;

    std::unique_ptr<weld::ScrolledWindow> mxScrolledWindow;
    std::unique_ptr<SmGraphicWidget> mxGraphic;
    std::unique_ptr<weld::CustomWeld> mxGraphicWin;

    DECL_LINK(ScrollHdl, weld::ScrolledWindow&, void);

public:
    explicit SmGraphicWindow(SmViewShell& rShell);
    virtual void dispose() override;
    virtual ~SmGraphicWindow() override;

    virtual bool IsStarMath() const override { return true; }

    void SetTotalSize(const Size& rNewSize);
    Size GetTotalSize() const;

    void SetZoom(sal_uInt16 Factor);
    sal_uInt16 GetZoom() const { return nZoom; }

    void ZoomToFitInWindow();

    virtual void Resize() override;
    void ShowContextMenu(const CommandEvent& rCEvt);

    void SetGraphicMapMode(const MapMode& rNewMapMode);
    MapMode GetGraphicMapMode() const;

    SmGraphicWidget& GetGraphicWidget()
    {
        return *mxGraphic;
    }

    const SmGraphicWidget& GetGraphicWidget() const
    {
        return *mxGraphic;
    }
};

class SmGraphicWidget final : public weld::CustomWidgetController
{
public:
    bool IsCursorVisible() const
    {
        return bIsCursorVisible;
    }
    void ShowCursor(bool bShow);
    bool IsLineVisible() const
    {
        return bIsLineVisible;
    }
    void ShowLine(bool bShow);
    const SmNode * SetCursorPos(sal_uInt16 nRow, sal_uInt16 nCol);

    explicit SmGraphicWidget(SmViewShell& rShell, SmGraphicWindow& rGraphicWindow);
    virtual ~SmGraphicWidget() override;

    // CustomWidgetController
    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    virtual bool MouseButtonDown(const MouseEvent &rMEvt) override;
    virtual bool MouseMove(const MouseEvent &rMEvt) override;
    virtual void GetFocus() override;
    virtual void LoseFocus() override;
    virtual bool KeyInput(const KeyEvent& rKEvt) override;

    void SetTotalSize();

    SmViewShell& GetView() { return mrViewShell; }
    SmDocShell* GetDoc();
    SmCursor& GetCursor();

    const Point& GetFormulaDrawPos() const
    {
        return aFormulaDrawPos;
    }

    // for Accessibility
    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible() override;

    SmGraphicAccessible* GetAccessible_Impl()
    {
        return mxAccessible.get();
    }

    OutputDevice& GetOutputDevice()
    {
        assert(GetDrawingArea());
        return GetDrawingArea()->get_ref_device();
    }

private:
    void SetIsCursorVisible(bool bVis);
    void SetCursor(const SmNode *pNode);
    void SetCursor(const tools::Rectangle &rRect);

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override;
    virtual bool Command(const CommandEvent& rCEvt) override;

    void RepaintViewShellDoc();
    DECL_LINK(CaretBlinkTimerHdl, Timer *, void);
    void CaretBlinkInit();
    void CaretBlinkStart();
    void CaretBlinkStop();

    SmGraphicWindow& mrGraphicWindow;

    Point aFormulaDrawPos;
    // old style editing pieces
    tools::Rectangle aCursorRect;
    bool bIsCursorVisible;
    bool bIsLineVisible;
    AutoTimer aCaretBlinkTimer;
    rtl::Reference<SmGraphicAccessible> mxAccessible;
    SmViewShell& mrViewShell;
    double mfLastZoomScale = 0;
    double mfAccumulatedZoom = 0;
};

class SmGraphicController final : public SfxControllerItem
{
    SmGraphicWidget &rGraphic;
public:
    SmGraphicController(SmGraphicWidget &, sal_uInt16, SfxBindings & );
    virtual void StateChangedAtToolBoxControl(sal_uInt16             nSID,
                              SfxItemState       eState,
                              const SfxPoolItem* pState) override;
};

class SmEditController final : public SfxControllerItem
{
    SmEditWindow &rEdit;

public:
    SmEditController(SmEditWindow &, sal_uInt16, SfxBindings  & );

    virtual void StateChangedAtToolBoxControl(sal_uInt16 nSID, SfxItemState eState, const SfxPoolItem* pState) override;
};

class SmCmdBoxWindow final : public SfxDockingWindow
{
    std::unique_ptr<SmEditWindow, o3tl::default_delete<SmEditWindow>> m_xEdit;
    SmEditController    aController;
    bool                bExiting;

    Timer               aInitialFocusTimer;

    DECL_LINK(InitialFocusTimerHdl, Timer *, void);

    virtual Size CalcDockingSize(SfxChildAlignment eAlign) override;
    virtual SfxChildAlignment CheckAlignment(SfxChildAlignment eActual,
                                             SfxChildAlignment eWish) override;

    virtual void    ToggleFloatingMode() override;

public:
    SmCmdBoxWindow(SfxBindings    *pBindings,
                   SfxChildWindow *pChildWindow,
                   Window         *pParent);

    virtual ~SmCmdBoxWindow () override;
    virtual void dispose() override;

    // Window
    virtual void GetFocus() override;
    virtual void StateChanged( StateChangedType nStateChange ) override;
    virtual void Command(const CommandEvent& rCEvt) override;

    Point WidgetToWindowPos(const weld::Widget& rWidget, const Point& rPos);

    void ShowContextMenu(const Point& rPos);

    void AdjustPosition();

    SmEditWindow& GetEditWindow()
    {
        return *m_xEdit;
    }
    SmViewShell* GetView();
};

class SmCmdBoxWrapper final : public SfxChildWindow
{
    SFX_DECL_CHILDWINDOW_WITHID(SmCmdBoxWrapper);

    SmCmdBoxWrapper(vcl::Window* pParentWindow, sal_uInt16 nId, SfxBindings* pBindings, SfxChildWinInfo* pInfo);

public:

    SmEditWindow& GetEditWindow()
    {
        return static_cast<SmCmdBoxWindow *>(GetWindow())->GetEditWindow();
    }
};

namespace sfx2 { class FileDialogHelper; }

class SmViewShell final : public SfxViewShell
{
    std::unique_ptr<sfx2::DocumentInserter> mpDocInserter;
    std::unique_ptr<SfxRequest> mpRequest;
    VclPtr<SmGraphicWindow> mxGraphicWindow;
    SmGraphicController maGraphicController;
    OUString maStatusText;
    bool mbPasteState;

    DECL_LINK( DialogClosedHdl, sfx2::FileDialogHelper*, void );
    virtual void            Notify( SfxBroadcaster& rBC, const SfxHint& rHint ) override;

    virtual SfxPrinter *GetPrinter(bool bCreate = false) override;
    virtual sal_uInt16 SetPrinter(SfxPrinter *pNewPrinter,
                              SfxPrinterChangeFlags nDiffFlags = SFX_PRINTER_ALL) override;

    void Insert( SfxMedium& rMedium );
    void InsertFrom(SfxMedium &rMedium);

    virtual bool HasPrintOptionsPage() const override;
    virtual std::unique_ptr<SfxTabPage> CreatePrintOptionsPage(weld::Container* pPage, weld::DialogController* pController,
                                                               const SfxItemSet &rOptions) override;
    virtual void Deactivate(bool IsMDIActivate) override;
    virtual void Activate(bool IsMDIActivate) override;
    virtual void InnerResizePixel(const Point &rOfs, const Size  &rSize, bool inplaceEditModeChange) override;
    virtual void OuterResizePixel(const Point &rOfs, const Size  &rSize) override;
    virtual void QueryObjAreaPixel( tools::Rectangle& rRect ) const override;
    virtual void SetZoomFactor( const Fraction &rX, const Fraction &rY ) override;
    virtual std::optional<OString> getLOKPayload(int nType, int nViewId) const override;

public:

    SmViewShell(SfxViewFrame& rFrame, SfxViewShell *pOldSh);
    virtual ~SmViewShell() override;

    SmDocShell * GetDoc() const
    {
        return static_cast<SmDocShell *>( GetViewFrame().GetObjectShell() );
    }

    SAL_RET_MAYBENULL SmEditWindow * GetEditWindow();

    SmGraphicWidget& GetGraphicWidget()
    {
        return mxGraphicWindow->GetGraphicWidget();
    }
    const SmGraphicWidget& GetGraphicWidget() const
    {
        return mxGraphicWindow->GetGraphicWidget();
    }

    SmGraphicWindow& GetGraphicWindow()
    {
        return *mxGraphicWindow;
    }

    void        SetStatusText(const OUString& rText);

    void        ShowError( const SmErrorDesc *pErrorDesc );
    void        NextError();
    void        PrevError();

    SFX_DECL_INTERFACE(SFX_INTERFACE_SMA_START+SfxInterfaceId(2))
    SFX_DECL_VIEWFACTORY(SmViewShell);

    void SendCaretToLOK() const;

    void InvalidateSlots();

private:
    /// SfxInterface initializer.
    static void InitInterface_Impl();

public:
    void Execute( SfxRequest& rReq );
    void GetState(SfxItemSet &);

    static bool IsInlineEditEnabled();

    // Opens the main help page for the Math module
    void StartMainHelp();

private:
    void ZoomByItemSet(const SfxItemSet *pSet);
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
