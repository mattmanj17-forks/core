#/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <SlideSorterViewShell.hxx>
#include <ViewShellImplementation.hxx>

#include <SlideSorter.hxx>
#include <controller/SlideSorterController.hxx>
#include <controller/SlsClipboard.hxx>
#include <controller/SlsScrollBarManager.hxx>
#include <controller/SlsPageSelector.hxx>
#include <controller/SlsSlotManager.hxx>
#include <controller/SlsCurrentSlideManager.hxx>
#include <controller/SlsSelectionManager.hxx>
#include <view/SlideSorterView.hxx>
#include <view/SlsLayouter.hxx>
#include <model/SlideSorterModel.hxx>
#include <model/SlsPageDescriptor.hxx>
#include <framework/FrameworkHelper.hxx>
#include <ViewShellBase.hxx>
#include <drawdoc.hxx>
#include <sdpage.hxx>
#include <app.hrc>
#include <AccessibleSlideSorterView.hxx>
#include <DrawDocShell.hxx>
#include <DrawViewShell.hxx>
#include <FrameView.hxx>
#include <SdUnoSlideView.hxx>
#include <ViewShellManager.hxx>
#include <Window.hxx>
#include <drawview.hxx>
#include <sfx2/msg.hxx>
#include <sfx2/objface.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/request.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/sidebar/SidebarChildWindow.hxx>
#include <sfx2/devtools/DevelopmentToolChildWindow.hxx>
#include <svx/svxids.hrc>
#include <vcl/EnumContext.hxx>
#include <svx/sidebar/ContextChangeEventMultiplexer.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <sfx2/sidebar/SidebarController.hxx>

using namespace ::sd::slidesorter;
#define ShellClass_SlideSorterViewShell
#include <sdslots.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

using ::sd::framework::FrameworkHelper;
using ::vcl::EnumContext;
using namespace sfx2::sidebar;

namespace sd::slidesorter {

namespace {

bool inChartOrMathContext(const sd::View* pView)
{
    if (!pView)
        return false;

    SfxViewShell* pViewShell = pView->GetSfxViewShell();
    SidebarController* pSidebar = SidebarController::GetSidebarControllerForView(pViewShell);
    if (pSidebar)
        return pSidebar->hasChartOrMathContextCurrently();

    return false;
}

} // anonymous namespace


SFX_IMPL_INTERFACE(SlideSorterViewShell, SfxShell)

void SlideSorterViewShell::InitInterface_Impl()
{
    GetStaticInterface()->RegisterChildWindow(::sfx2::sidebar::SidebarChildWindow::GetChildWindowId());
    GetStaticInterface()->RegisterChildWindow(DevelopmentToolChildWindow::GetChildWindowId());
}


std::shared_ptr<SlideSorterViewShell> SlideSorterViewShell::Create (
    ViewShellBase& rViewShellBase,
    vcl::Window* pParentWindow,
    FrameView* pFrameViewArgument)
{
    std::shared_ptr<SlideSorterViewShell> pViewShell;
    try
    {
        pViewShell.reset(
            new SlideSorterViewShell(rViewShellBase,pParentWindow,pFrameViewArgument));
        pViewShell->Initialize();
        if (pViewShell->mpSlideSorter == nullptr)
            pViewShell.reset();
    }
    catch(Exception&)
    {
        pViewShell.reset();
    }
    return pViewShell;
}

SlideSorterViewShell::SlideSorterViewShell (
    ViewShellBase& rViewShellBase,
    vcl::Window* pParentWindow,
    FrameView* pFrameViewArgument)
    : ViewShell (pParentWindow, rViewShellBase),
      mbIsArrangeGUIElementsPending(true)
{
    GetContentWindow()->set_id(u"slidesorter"_ustr);
    meShellType = ST_SLIDE_SORTER;

    if (pFrameViewArgument != nullptr)
        mpFrameView = pFrameViewArgument;
    else
        mpFrameView = new FrameView(GetDoc());
    GetFrameView()->Connect();

    SetName (u"SlideSorterViewShell"_ustr);

    pParentWindow->SetStyle(pParentWindow->GetStyle() | WB_DIALOGCONTROL);
}

SlideSorterViewShell::~SlideSorterViewShell()
{
    DisposeFunctions();

    try
    {
        ::sd::Window* pWindow = GetActiveWindow();
        if (pWindow!=nullptr)
        {
            rtl::Reference<comphelper::OAccessible> pAccessible = pWindow->GetAccessible(false);
            if (pAccessible.is())
                pAccessible->dispose();
        }
    }
    catch( css::uno::Exception& )
    {
        TOOLS_WARN_EXCEPTION( "sd", "sd::SlideSorterViewShell::~SlideSorterViewShell()" );
    }
    GetFrameView()->Disconnect();
}

void SlideSorterViewShell::Initialize()
{
    mpSlideSorter = SlideSorter::CreateSlideSorter(
        *this,
        mpContentWindow,
        mpHorizontalScrollBar,
        mpVerticalScrollBar);
    mpView = &mpSlideSorter->GetView();

    doShow();

    SetPool( &GetDoc()->GetPool() );
    SetUndoManager( GetDoc()->GetDocSh()->GetUndoManager() );

    // For accessibility we have to shortly hide the content window.
    // This triggers the construction of a new accessibility object for
    // the new view shell.  (One is created earlier while the constructor
    // of the base class is executed.  At that time the correct
    // accessibility object can not be constructed.)
    sd::Window *pWindow (mpSlideSorter->GetContentWindow().get());
    if (pWindow)
    {
        pWindow->Hide();
        pWindow->Show();
    }
}

void SlideSorterViewShell::Init (bool bIsMainViewShell)
{
    ViewShell::Init(bIsMainViewShell);

    // since the updatePageList will show focus, the window.show() must be called ahead. This show is deferred from Init()
    ::sd::Window* pActiveWindow = GetActiveWindow();
    if (pActiveWindow)
        pActiveWindow->Show();
    mpSlideSorter->GetModel().UpdatePageList();

    if (mpContentWindow)
        mpContentWindow->SetViewShell(this);
}

SlideSorterViewShell* SlideSorterViewShell::GetSlideSorter (ViewShellBase& rBase)
{
    SlideSorterViewShell* pViewShell = nullptr;

    // Test the center and left pane for showing a slide sorter.
    OUString aPaneURLs[] = {
        FrameworkHelper::msCenterPaneURL,
        FrameworkHelper::msFullScreenPaneURL,
        FrameworkHelper::msLeftImpressPaneURL,
        FrameworkHelper::msLeftDrawPaneURL,
        OUString()};

    try
    {
        std::shared_ptr<FrameworkHelper> pFrameworkHelper (FrameworkHelper::Instance(rBase));
        if (pFrameworkHelper->IsValid())
            for (int i=0; pViewShell==nullptr && !aPaneURLs[i].isEmpty(); ++i)
            {
                pViewShell = dynamic_cast<SlideSorterViewShell*>(
                    pFrameworkHelper->GetViewShell(aPaneURLs[i]).get());
            }
    }
    catch (RuntimeException&)
    {}

    return pViewShell;
}

Reference<drawing::XDrawSubController> SlideSorterViewShell::CreateSubController()
{
    Reference<drawing::XDrawSubController> xSubController;

    if (IsMainViewShell())
    {
        // Create uno controller for the main view shell.
        xSubController.set( new SdUnoSlideView( *mpSlideSorter));
    }

    return xSubController;
}

/** If there is a valid controller then create a new instance of
    <type>AccessibleSlideSorterView</type>.  Otherwise delegate this call
    to the base class to return a default object (probably an empty
    reference).
*/
rtl::Reference<comphelper::OAccessible>
SlideSorterViewShell::CreateAccessibleDocumentView(::sd::Window* pWindow)
{
    // When the view is not set then the initialization is not yet complete
    // and we can not yet provide an accessibility object.
    if (mpView == nullptr || mpSlideSorter == nullptr)
        return nullptr;

    assert(mpSlideSorter);

    rtl::Reference<::accessibility::AccessibleSlideSorterView> pAccessibleView =
    new ::accessibility::AccessibleSlideSorterView(
        *mpSlideSorter,
        pWindow);

    pAccessibleView->Init();

    return pAccessibleView;
}

void SlideSorterViewShell::SwitchViewFireFocus(const css::uno::Reference< css::accessibility::XAccessible >& xAcc )
{
    if (xAcc)
    {
        ::accessibility::AccessibleSlideSorterView* pBase = static_cast< ::accessibility::AccessibleSlideSorterView* >(xAcc.get());
        if (pBase)
        {
            pBase->SwitchViewActivated();
        }
    }
}

SlideSorter& SlideSorterViewShell::GetSlideSorter() const
{
    assert(mpSlideSorter);
    return *mpSlideSorter;
}

bool SlideSorterViewShell::RelocateToParentWindow (vcl::Window* pParentWindow)
{
    assert(mpSlideSorter);
    if ( ! mpSlideSorter)
        return false;

    mpSlideSorter->RelocateToWindow(pParentWindow);
    ReadFrameViewData(mpFrameView);

    return true;
}

SfxUndoManager* SlideSorterViewShell::ImpGetUndoManager() const
{
    SfxShell* pObjectBar = GetViewShellBase().GetViewShellManager()->GetTopShell();
    if (pObjectBar != nullptr)
    {
        // When it exists then return the undo manager of the currently
        // active object bar.  The object bar is missing when the
        // SlideSorterViewShell is not the main view shell.
        return pObjectBar->GetUndoManager();
    }
    else
    {
        // Return the undo manager of this  shell when there is no object or
        // tool bar.
        return const_cast<SlideSorterViewShell*>(this)->GetUndoManager();
    }
}

SdPage* SlideSorterViewShell::getCurrentPage() const
{
    // since SlideSorterViewShell::GetActualPage() currently also
    // returns master pages, which is a wrong behaviour for GetActualPage(),
    // we can just use that for now
    return const_cast<SlideSorterViewShell*>(this)->GetActualPage();
}

SdPage* SlideSorterViewShell::GetActualPage()
{
    SdPage* pCurrentPage = nullptr;

    // 1. Try to get the current page from the view shell in the center pane
    // (if we are that not ourself).
    if ( ! IsMainViewShell())
    {
        std::shared_ptr<ViewShell> pMainViewShell = GetViewShellBase().GetMainViewShell();
        if (pMainViewShell != nullptr)
            pCurrentPage = pMainViewShell->GetActualPage();
    }

    if (pCurrentPage == nullptr)
    {
        model::SharedPageDescriptor pDescriptor (
            mpSlideSorter->GetController().GetCurrentSlideManager().GetCurrentSlide());
        if (pDescriptor)
            pCurrentPage = pDescriptor->GetPage();
    }

    return pCurrentPage;
}

void SlideSorterViewShell::GetMenuState ( SfxItemSet& rSet)
{
    ViewShell::GetMenuState(rSet);
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetSlotManager().GetMenuState(rSet);
}

void SlideSorterViewShell::GetClipboardState ( SfxItemSet& rSet)
{
    ViewShell::GetMenuState(rSet);
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetSlotManager().GetClipboardState(rSet);
}

void SlideSorterViewShell::ExecCtrl (SfxRequest& rRequest)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().ExecCtrl(rRequest);
}

void SlideSorterViewShell::GetCtrlState (SfxItemSet& rSet)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetCtrlState(rSet);
}

void SlideSorterViewShell::FuSupport (SfxRequest& rRequest)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().FuSupport(rRequest);
}

/** We have to handle those slot calls here that need to have access to
    private or protected members and methods of this class.
*/
void SlideSorterViewShell::FuTemporary (SfxRequest& rRequest)
{
    assert(mpSlideSorter);
    switch (rRequest.GetSlot())
    {
        case SID_MODIFYPAGE:
        {
            SdPage* pCurrentPage = GetActualPage();
            if (pCurrentPage != nullptr)
                mpImpl->ProcessModifyPageSlot (
                    rRequest,
                    pCurrentPage,
                    PageKind::Standard);
            Cancel();
            rRequest.Done ();
        }
        break;

        default:
            mpSlideSorter->GetController().FuTemporary(rRequest);
            break;
    }
}

void SlideSorterViewShell::GetStatusBarState (SfxItemSet& rSet)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetStatusBarState(rSet);
}

void SlideSorterViewShell::FuPermanent (SfxRequest& rRequest)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().FuPermanent(rRequest);
}

void SlideSorterViewShell::GetAttrState (SfxItemSet& rSet)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetAttrState(rSet);
}

void SlideSorterViewShell::ExecStatusBar (SfxRequest& rReq)
{
    // nothing is executed during a slide show!
    if(HasCurrentFunction(SID_PRESENTATION))
        return;

    switch (rReq.GetSlot())
    {
        case SID_STATUS_PAGE:
        {
            GetViewFrame()->GetDispatcher()->Execute(SID_GO_TO_PAGE,
                                          SfxCallMode::SYNCHRON | SfxCallMode::RECORD);
        }
        break;
    }
}

void SlideSorterViewShell::Paint (
    const ::tools::Rectangle& rBBox,
    ::sd::Window* pWindow)
{
    SetActiveWindow (pWindow);
    assert(mpSlideSorter);
    if (mpSlideSorter)
        mpSlideSorter->GetController().Paint(rBBox,pWindow);
}

void SlideSorterViewShell::ArrangeGUIElements()
{
    if (IsActive())
    {
        assert(mpSlideSorter);
        mpSlideSorter->ArrangeGUIElements(maViewPos, maViewSize);
        mbIsArrangeGUIElementsPending = false;
    }
    else
        mbIsArrangeGUIElementsPending = true;
}

void SlideSorterViewShell::Activate (bool bIsMDIActivate)
{
    if(inChartOrMathContext(GetView()))
    {
        // Avoid context changes for chart/math during activation / deactivation.
        const bool bIsContextBroadcasterEnabled (SfxShell::SetContextBroadcasterEnabled(false));

        ViewShell::Activate(bIsMDIActivate);

        SfxShell::SetContextBroadcasterEnabled(bIsContextBroadcasterEnabled);
        return;
    }

    ViewShell::Activate(bIsMDIActivate);
    if (mbIsArrangeGUIElementsPending)
        ArrangeGUIElements();

    // Determine and broadcast the context that belongs to the main view shell.
    EnumContext::Context eContext = EnumContext::Context::Unknown;
    std::shared_ptr<ViewShell> pMainViewShell (GetViewShellBase().GetMainViewShell());
    ViewShell::ShellType eMainViewShellType (
        pMainViewShell
            ? pMainViewShell->GetShellType()
            : ViewShell::ST_NONE);
    switch (eMainViewShellType)
    {
        case ViewShell::ST_IMPRESS:
        case ViewShell::ST_SLIDE_SORTER:
        case ViewShell::ST_NOTES:
        case ViewShell::ST_DRAW:
            eContext = EnumContext::Context::DrawPage;
            if( nullptr != dynamic_cast< const DrawViewShell *>( pMainViewShell.get() ))
            {
                DrawViewShell* pDrawViewShell = dynamic_cast<DrawViewShell*>(pMainViewShell.get());
                if (pDrawViewShell != nullptr)
                    eContext = EnumContext::GetContextEnum(pDrawViewShell->GetSidebarContextName());
            }
            break;

        default:
            break;
    }
    ContextChangeEventMultiplexer::NotifyContextChange(
        &GetViewShellBase(),
        eContext);
}

void SlideSorterViewShell::Deactivate (bool /*bIsMDIActivate*/)
{
    // Save Settings - Specifically SlidesPerRow to retrieve it later
    WriteFrameViewData();
}

void SlideSorterViewShell::Command (
    const CommandEvent& rEvent,
    ::sd::Window* pWindow)
{
    assert(mpSlideSorter);
    if ( ! mpSlideSorter->GetController().Command (rEvent, pWindow))
        ViewShell::Command (rEvent, pWindow);
}

void SlideSorterViewShell::ReadFrameViewData (FrameView* pFrameView)
{
    assert(mpSlideSorter);
    if (pFrameView != nullptr)
    {
        view::SlideSorterView& rView (mpSlideSorter->GetView());

        sal_uInt16 nSlidesPerRow (pFrameView->GetSlidesPerRow());
        if (nSlidesPerRow > 0
            && rView.GetOrientation() == view::Layouter::GRID
            && IsMainViewShell())
        {
            rView.GetLayouter().SetColumnCount(nSlidesPerRow,nSlidesPerRow);
        }
        if (IsMainViewShell())
            mpSlideSorter->GetController().GetCurrentSlideManager().NotifyCurrentSlideChange(
                mpFrameView->GetSelectedPage());
        mpSlideSorter->GetController().Rearrange(true);

        // DrawMode for 'main' window
        if (GetActiveWindow()->GetOutDev()->GetDrawMode() != pFrameView->GetDrawMode() )
            GetActiveWindow()->GetOutDev()->SetDrawMode( pFrameView->GetDrawMode() );
    }

    // When this slide sorter is not displayed in the main window then we do
    // not share the same frame view and have to find other ways to acquire
    // certain values.
    if ( ! IsMainViewShell())
    {
        std::shared_ptr<ViewShell> pMainViewShell = GetViewShellBase().GetMainViewShell();
        if (pMainViewShell != nullptr)
            mpSlideSorter->GetController().GetCurrentSlideManager().NotifyCurrentSlideChange(
                pMainViewShell->getCurrentPage());
    }
}

void SlideSorterViewShell::WriteFrameViewData()
{
    assert(mpSlideSorter);
    if (mpFrameView == nullptr)
        return;

    view::SlideSorterView& rView (mpSlideSorter->GetView());
    mpFrameView->SetSlidesPerRow(static_cast<sal_uInt16>(rView.GetLayouter().GetColumnCount()));

    // DrawMode for 'main' window
    if( mpFrameView->GetDrawMode() != GetActiveWindow()->GetOutDev()->GetDrawMode() )
        mpFrameView->SetDrawMode( GetActiveWindow()->GetOutDev()->GetDrawMode() );

    SdPage* pActualPage = GetActualPage();
    if (pActualPage != nullptr)
    {
        if (IsMainViewShell())
            mpFrameView->SetSelectedPage((pActualPage->GetPageNum()- 1) / 2);
        // else
        // The slide sorter is not expected to switch the current page
        // other than by double clicks.  That is handled separately.
    }
    else
    {
        // We have no current page to set but at least we can make sure
        // that the index of the frame view has a legal value.
        if (mpFrameView->GetSelectedPage() >= mpSlideSorter->GetModel().GetPageCount())
            mpFrameView->SetSelectedPage(static_cast<sal_uInt16>(mpSlideSorter->GetModel().GetPageCount())-1);
    }
}

void SlideSorterViewShell::SetZoom (::tools::Long )
{
    // Ignored.
    // The zoom scale is adapted internally to fit a number of columns in
    // the window.
}

void SlideSorterViewShell::SetZoomRect (const ::tools::Rectangle& rZoomRect)
{
    assert(mpSlideSorter);
    Size aPageSize (mpSlideSorter->GetView().GetLayouter().GetPageObjectSize());

    ::tools::Rectangle aRect(rZoomRect);

    if (aRect.GetWidth()  < aPageSize.Width())
    {
        ::tools::Long nWidthDiff  = (aPageSize.Width() - aRect.GetWidth()) / 2;

        aRect.AdjustLeft( -nWidthDiff );
        aRect.AdjustRight(nWidthDiff );

        if (aRect.Left() < 0)
        {
            aRect.SetPos(Point(0, aRect.Top()));
        }
    }

    if (aRect.GetHeight()  < aPageSize.Height())
    {
        ::tools::Long nHeightDiff  = (aPageSize.Height() - aRect.GetHeight()) / 2;

        aRect.AdjustTop( -nHeightDiff );
        aRect.AdjustBottom(nHeightDiff );

        if (aRect.Top() < 0)
        {
            aRect.SetPos(Point(aRect.Left(), 0));
        }
    }

    ViewShell::SetZoomRect(aRect);

    GetViewFrame()->GetBindings().Invalidate( SID_ATTR_ZOOM );
    GetViewFrame()->GetBindings().Invalidate( SID_ATTR_ZOOMSLIDER );
}

void SlideSorterViewShell::UpdateScrollBars()
{
    // Do not call the overwritten method of the base class: We do all the
    // scroll bar setup by ourselves.
    mpSlideSorter->GetController().GetScrollBarManager().UpdateScrollBars(true);
}

void SlideSorterViewShell::StartDrag (
    const Point& rDragPt,
    vcl::Window* pWindow )
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetClipboard().StartDrag (
        rDragPt,
        pWindow);
}

sal_Int8 SlideSorterViewShell::AcceptDrop (
    const AcceptDropEvent& rEvt,
    DropTargetHelper& rTargetHelper,
    ::sd::Window* pTargetWindow,
    sal_uInt16 nPage,
    SdrLayerID nLayer)
{
    assert(mpSlideSorter);
    return mpSlideSorter->GetController().GetClipboard().AcceptDrop (
        rEvt,
        rTargetHelper,
        pTargetWindow,
        nPage,
        nLayer);
}

sal_Int8 SlideSorterViewShell::ExecuteDrop (
    const ExecuteDropEvent& rEvt,
    DropTargetHelper& rTargetHelper,
    ::sd::Window* pTargetWindow,
    sal_uInt16 nPage,
    SdrLayerID nLayer)
{
    assert(mpSlideSorter);
    return mpSlideSorter->GetController().GetClipboard().ExecuteDrop (
        rEvt,
        rTargetHelper,
        pTargetWindow,
        nPage,
        nLayer);
}

std::shared_ptr<SlideSorterViewShell::PageSelection>
    SlideSorterViewShell::GetPageSelection() const
{
    assert(mpSlideSorter);
    return mpSlideSorter->GetController().GetPageSelector().GetPageSelection();
}

void SlideSorterViewShell::SetPageSelection (
    const std::shared_ptr<PageSelection>& rSelection)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetPageSelector().SetPageSelection(rSelection, true);
}

void SlideSorterViewShell::AddSelectionChangeListener (
    const Link<LinkParamNone*,void>& rCallback)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetSelectionManager()->AddSelectionChangeListener(rCallback);
}

void SlideSorterViewShell::RemoveSelectionChangeListener (
    const Link<LinkParamNone*,void>& rCallback)
{
    assert(mpSlideSorter);
    mpSlideSorter->GetController().GetSelectionManager()->RemoveSelectionChangeListener(rCallback);
}

void SlideSorterViewShell::MainViewEndEditAndUnmarkAll()
{
    std::shared_ptr<ViewShell> pMainViewShell = GetViewShellBase().GetMainViewShell();
    DrawViewShell* pDrawViewShell = dynamic_cast<DrawViewShell*>(pMainViewShell.get());
    SdrView* pView = pDrawViewShell ? pDrawViewShell->GetDrawView() : nullptr;
    if (pView)
    {
        pView->SdrEndTextEdit();
        pView->UnmarkAll();
    }
}

std::pair<sal_uInt16, sal_uInt16> SlideSorterViewShell::SyncPageSelectionToDocument(const std::shared_ptr<SlideSorterViewShell::PageSelection> &rpSelection)
{
    sal_uInt16 firstSelectedPageNo = SAL_MAX_UINT16;
    sal_uInt16 lastSelectedPageNo = 0;

    GetDoc()->UnselectAllPages();
    for (auto& rpPage : *rpSelection)
    {
        // Check page number
        sal_uInt16 pageNo = rpPage->GetPageNum();
        if (pageNo > lastSelectedPageNo)
            lastSelectedPageNo = pageNo;
        if (pageNo < firstSelectedPageNo)
            firstSelectedPageNo = pageNo;
        GetDoc()->SetSelected(rpPage, true);
    }

    return std::make_pair(firstSelectedPageNo, lastSelectedPageNo);
}

void SlideSorterViewShell::ExecMovePageFirst (SfxRequest& /*rReq*/)
{
    MainViewEndEditAndUnmarkAll();

    std::shared_ptr<SlideSorterViewShell::PageSelection> xSelection(GetPageSelection());

    // SdDrawDocument MovePages is based on SdPage IsSelected, so
    // transfer the SlideSorter selection to SdPages
    SyncPageSelectionToDocument(xSelection);

    // Moves selected pages after page -1
    GetDoc()->MoveSelectedPages( sal_uInt16(-1) );

    PostMoveSlidesActions(xSelection);
}

void SlideSorterViewShell::GetStateMovePageFirst (SfxItemSet& rSet)
{
    if ( ! IsMainViewShell())
    {
        std::shared_ptr<ViewShell> pMainViewShell = GetViewShellBase().GetMainViewShell();
        DrawViewShell* pDrawViewShell = dynamic_cast<DrawViewShell*>(pMainViewShell.get());
        if (pDrawViewShell != nullptr && pDrawViewShell->GetPageKind() == PageKind::Handout)
        {
            rSet.DisableItem( SID_MOVE_PAGE_FIRST );
            rSet.DisableItem( SID_MOVE_PAGE_UP );
            return;
        }
    }

    std::shared_ptr<SlideSorterViewShell::PageSelection> xSelection(GetPageSelection());

    // SdDrawDocument MovePages is based on SdPage IsSelected, so
    // transfer the SlideSorter selection to SdPages
    sal_uInt16 firstSelectedPageNo = SyncPageSelectionToDocument(xSelection).first;
    // Now compute human page number from internal page number
    firstSelectedPageNo = (firstSelectedPageNo - 1) / 2;

    if (firstSelectedPageNo == 0)
    {
        rSet.DisableItem( SID_MOVE_PAGE_FIRST );
        rSet.DisableItem( SID_MOVE_PAGE_UP );
    }
}

void SlideSorterViewShell::ExecMovePageUp (SfxRequest& /*rReq*/)
{
    MainViewEndEditAndUnmarkAll();

    std::shared_ptr<SlideSorterViewShell::PageSelection> xSelection(GetPageSelection());

    // SdDrawDocument MovePages is based on SdPage IsSelected, so
    // transfer the SlideSorter selection to SdPages
    sal_uInt16 firstSelectedPageNo = SyncPageSelectionToDocument(xSelection).first;

    // In case no slide is selected
    if (firstSelectedPageNo == SAL_MAX_UINT16)
        return;

    // Now compute human page number from internal page number
    firstSelectedPageNo = (firstSelectedPageNo - 1) / 2;

    if (firstSelectedPageNo == 0)
        return;

    // Move pages before firstSelectedPageNo - 1 (so after firstSelectedPageNo - 2),
    // remembering that -1 means at first, which is good.
    GetDoc()->MoveSelectedPages( firstSelectedPageNo - 2 );

    PostMoveSlidesActions(xSelection);
}

void SlideSorterViewShell::GetStateMovePageUp (SfxItemSet& rSet)
{
    GetStateMovePageFirst(rSet);
}

void SlideSorterViewShell::ExecMovePageDown (SfxRequest& /*rReq*/)
{
    MainViewEndEditAndUnmarkAll();

    std::shared_ptr<SlideSorterViewShell::PageSelection> xSelection(GetPageSelection());

    // SdDrawDocument MovePages is based on SdPage IsSelected, so
    // transfer the SlideSorter selection to SdPages
    sal_uInt16 lastSelectedPageNo = SyncPageSelectionToDocument(xSelection).second;

    // Get page number of the last page
    sal_uInt16 nNoOfPages = GetDoc()->GetSdPageCount(PageKind::Standard);

    // Now compute human page number from internal page number
    lastSelectedPageNo = (lastSelectedPageNo - 1) / 2;
    if (lastSelectedPageNo == nNoOfPages - 1)
        return;

    // Move to position after lastSelectedPageNo
    GetDoc()->MoveSelectedPages( lastSelectedPageNo + 1 );

    PostMoveSlidesActions(xSelection);
}

void SlideSorterViewShell::GetStateMovePageDown (SfxItemSet& rSet)
{
    GetStateMovePageLast( rSet );
}

void SlideSorterViewShell::ExecMovePageLast (SfxRequest& /*rReq*/)
{
    MainViewEndEditAndUnmarkAll();

    std::shared_ptr<SlideSorterViewShell::PageSelection> xSelection(GetPageSelection());

    // SdDrawDocument MovePages is based on SdPage IsSelected, so
    // transfer the SlideSorter selection to SdPages
    SyncPageSelectionToDocument(xSelection);

    // Get page number of the last page
    sal_uInt16 nNoOfPages = GetDoc()->GetSdPageCount(PageKind::Standard);

    // Move to position after last page No (=Number of pages - 1)
    GetDoc()->MoveSelectedPages( nNoOfPages - 1 );

    PostMoveSlidesActions(xSelection);
}

void SlideSorterViewShell::GetStateMovePageLast (SfxItemSet& rSet)
{
    std::shared_ptr<ViewShell> pMainViewShell = GetViewShellBase().GetMainViewShell();
    DrawViewShell* pDrawViewShell = dynamic_cast<DrawViewShell*>(pMainViewShell.get());
    if (pDrawViewShell != nullptr && pDrawViewShell->GetPageKind() == PageKind::Handout)
    {
        rSet.DisableItem( SID_MOVE_PAGE_LAST );
        rSet.DisableItem( SID_MOVE_PAGE_DOWN );
        return;
    }

    std::shared_ptr<SlideSorterViewShell::PageSelection> xSelection(GetPageSelection());

    // SdDrawDocument MovePages is based on SdPage IsSelected, so
    // transfer the SlideSorter selection to SdPages
    sal_uInt16 lastSelectedPageNo = SyncPageSelectionToDocument(xSelection).second;

    // Get page number of the last page
    sal_uInt16 nNoOfPages = GetDoc()->GetSdPageCount(PageKind::Standard);

    // Now compute human page number from internal page number
    lastSelectedPageNo = (lastSelectedPageNo - 1) / 2;
    if (lastSelectedPageNo == nNoOfPages - 1)
    {
        rSet.DisableItem( SID_MOVE_PAGE_LAST );
        rSet.DisableItem( SID_MOVE_PAGE_DOWN );
    }
}

void SlideSorterViewShell::PostMoveSlidesActions(const std::shared_ptr<SlideSorterViewShell::PageSelection> &rpSelection)
{
    sal_uInt16 nNoOfPages = GetDoc()->GetSdPageCount(PageKind::Standard);
    for (sal_uInt16 nPage = 0; nPage < nNoOfPages; nPage++)
    {
        SdPage* pPage = GetDoc()->GetSdPage(nPage, PageKind::Standard);
        GetDoc()->SetSelected(pPage, false);
    }

    mpSlideSorter->GetController().GetPageSelector().DeselectAllPages();
    for (const auto& rpPage : *rpSelection)
    {
        mpSlideSorter->GetController().GetPageSelector().SelectPage(rpPage);
    }

    // Refresh toolbar icons
    SfxBindings& rBindings = GetViewFrame()->GetBindings();
    rBindings.Invalidate(SID_MOVE_PAGE_FIRST);
    rBindings.Invalidate(SID_MOVE_PAGE_UP);
    rBindings.Invalidate(SID_MOVE_PAGE_DOWN);
    rBindings.Invalidate(SID_MOVE_PAGE_LAST);

}

} // end of namespace ::sd::slidesorter

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
