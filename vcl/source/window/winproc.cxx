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

#include <sal/config.h>

#include <o3tl/safeint.hxx>
#include <tools/debug.hxx>
#include <tools/time.hxx>
#include <sal/log.hxx>

#include <unotools/localedatawrapper.hxx>

#include <dndeventdispatcher.hxx>
#include <comphelper/lok.hxx>
#include <vcl/QueueInfo.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/timer.hxx>
#include <vcl/event.hxx>
#include <vcl/GestureEventPan.hxx>
#include <vcl/GestureEventZoom.hxx>
#include <vcl/GestureEventRotate.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/cursor.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/toolkit/floatwin.hxx>
#include <vcl/toolkit/dialog.hxx>
#include <vcl/toolkit/edit.hxx>
#include <vcl/help.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/menu.hxx>
#include <vcl/virdev.hxx>
#include <vcl/uitest/logger.hxx>
#include <vcl/ptrstyle.hxx>

#include <svdata.hxx>
#include <salwtype.hxx>
#include <salframe.hxx>
#include <accmgr.hxx>
#include <print.h>
#include <window.h>
#include <helpwin.hxx>
#include <brdwin.hxx>

#include <com/sun/star/datatransfer/dnd/DNDConstants.hpp>
#include <com/sun/star/datatransfer/dnd/XDragSource.hpp>
#include <com/sun/star/awt/MouseEvent.hpp>

#define IMPL_MIN_NEEDSYSWIN         49

namespace
{

class GenericDropTargetDropContext
    : public ::cppu::WeakImplHelper<css::datatransfer::dnd::XDropTargetDropContext>
{
public:
    GenericDropTargetDropContext() {}

    // XDropTargetDropContext
    virtual void SAL_CALL acceptDrop(sal_Int8 /*dragOperation*/) override {}
    virtual void SAL_CALL rejectDrop() override {}
    virtual void SAL_CALL dropComplete(sal_Bool /*success*/) override {}
};

class GenericDropTargetDragContext
    : public ::cppu::WeakImplHelper<css::datatransfer::dnd::XDropTargetDragContext>
{
public:
    GenericDropTargetDragContext() {}

    // XDropTargetDragContext
    virtual void SAL_CALL acceptDrag(sal_Int8 /*dragOperation*/) override {}
    virtual void SAL_CALL rejectDrag() override {}
};
}

bool ImplCallPreNotify( NotifyEvent& rEvt )
{
    return rEvt.GetWindow()->CompatPreNotify( rEvt );
}

static bool ImplHandleMouseFloatMode( vcl::Window* pChild, const Point& rMousePos,
                                      sal_uInt16 nCode, NotifyEventType nSVEvent,
                                      bool bMouseLeave )
{
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpFirstFloat && !pSVData->mpWinData->mpCaptureWin
        && !pSVData->mpWinData->mpFirstFloat->ImplIsFloatPopupModeWindow(pChild))
    {
        /*
         *  #93895# since floats are system windows, coordinates have
         *  to be converted to float relative for the hittest
         */
        bool            bHitTestInsideRect = false;
        FloatingWindow* pFloat = pSVData->mpWinData->mpFirstFloat->ImplFloatHitTest( pChild, rMousePos, bHitTestInsideRect );
        if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        {
            if ( bMouseLeave )
                return true;

            if ( !pFloat || bHitTestInsideRect )
            {
                if ( ImplGetSVHelpData().mpHelpWin && !ImplGetSVHelpData().mbKeyboardHelp )
                    ImplDestroyHelpWindow( true );
                pChild->ImplGetFrame()->SetPointer( PointerStyle::Arrow );
                return true;
            }
        }
        else
        {
            if ( nCode & MOUSE_LEFT )
            {
                if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
                {
                    if ( !pFloat )
                    {
                        FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                        pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                        return true;
                    }
                    else if ( bHitTestInsideRect )
                    {
                        pFloat->ImplSetMouseDown();
                        return true;
                    }
                }
                else
                {
                    if ( pFloat )
                    {
                        if ( bHitTestInsideRect )
                        {
                            if ( pFloat->ImplIsMouseDown() )
                                pFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel );
                            return true;
                        }
                    }
                    else
                    {
                        FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                        FloatWinPopupFlags nPopupFlags = pLastLevelFloat->GetPopupModeFlags();
                        if ( !(nPopupFlags & FloatWinPopupFlags::NoMouseUpClose) )
                        {
                            pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                            return true;
                        }
                    }
                }
            }
            else
            {
                if ( !pFloat )
                {
                    FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                    FloatWinPopupFlags nPopupFlags = pLastLevelFloat->GetPopupModeFlags();
                    if ( nPopupFlags & FloatWinPopupFlags::AllMouseButtonClose )
                    {
                        if ( (nPopupFlags & FloatWinPopupFlags::NoMouseUpClose) &&
                             (nSVEvent == NotifyEventType::MOUSEBUTTONUP) )
                            return true;
                        pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                        return true;
                    }
                    else
                        return true;
                }
            }
        }
    }

    return false;
}

static void ImplHandleMouseHelpRequest( vcl::Window* pChild, const Point& rMousePos )
{
    ImplSVHelpData& aHelpData = ImplGetSVHelpData();
    if ( aHelpData.mpHelpWin &&
         ( aHelpData.mpHelpWin->IsWindowOrChild( pChild ) ||
           pChild->IsWindowOrChild( aHelpData.mpHelpWin ) ))
        return;

    HelpEventMode nHelpMode = HelpEventMode::NONE;
    if ( aHelpData.mbQuickHelp )
        nHelpMode = HelpEventMode::QUICK;
    if ( aHelpData.mbBalloonHelp )
        nHelpMode |= HelpEventMode::BALLOON;
    if ( !(bool(nHelpMode)) )
        return;

    if ( pChild->IsInputEnabled() && !pChild->IsInModalMode() )
    {
        HelpEvent aHelpEvent( rMousePos, nHelpMode );
        aHelpData.mbRequestingHelp = true;
        pChild->RequestHelp( aHelpEvent );
        aHelpData.mbRequestingHelp = false;
    }
    // #104172# do not kill keyboard activated tooltips
    else if ( aHelpData.mpHelpWin && !aHelpData.mbKeyboardHelp)
    {
        ImplDestroyHelpWindow( true );
    }
}

static void ImplSetMousePointer( vcl::Window const * pChild )
{
    if ( ImplGetSVHelpData().mbExtHelpMode )
        pChild->ImplGetFrame()->SetPointer( PointerStyle::Help );
    else
        pChild->ImplGetFrame()->SetPointer( pChild->ImplGetMousePointer() );
}

static bool ImplCallCommand( const VclPtr<vcl::Window>& pChild, CommandEventId nEvt, void const * pData = nullptr,
                             bool bMouse = false, Point const * pPos = nullptr )
{
    Point aPos;
    if ( pPos )
        aPos = *pPos;
    else
    {
        if( bMouse )
            aPos = pChild->GetPointerPosPixel();
        else
        {
            // simulate mouseposition at center of window
            Size aSize( pChild->GetOutputSizePixel() );
            aPos = Point( aSize.getWidth()/2, aSize.getHeight()/2 );
        }
    }

    CommandEvent    aCEvt( aPos, nEvt, bMouse, pData );
    NotifyEvent     aNCmdEvt( NotifyEventType::COMMAND, pChild, &aCEvt );
    bool bPreNotify = ImplCallPreNotify( aNCmdEvt );
    if ( pChild->isDisposed() )
        return false;
    if ( !bPreNotify )
    {
        pChild->ImplGetWindowImpl()->mbCommand = false;
        pChild->Command( aCEvt );

        if( pChild->isDisposed() )
            return false;
        pChild->ImplNotifyKeyMouseCommandEventListeners( aNCmdEvt );
        if ( pChild->isDisposed() )
            return false;
        if ( pChild->ImplGetWindowImpl()->mbCommand )
            return true;
    }

    return false;
}

/*  #i34277# delayed context menu activation;
*   necessary if there already was a popup menu running.
*/

namespace {

struct ContextMenuEvent
{
    VclPtr<vcl::Window>  pWindow;
    Point           aChildPos;
};

}

static void ContextMenuEventLink( void* pCEvent, void* )
{
    ContextMenuEvent* pEv = static_cast<ContextMenuEvent*>(pCEvent);

    if( ! pEv->pWindow->isDisposed() )
    {
        ImplCallCommand( pEv->pWindow, CommandEventId::ContextMenu, nullptr, true, &pEv->aChildPos );
    }
    delete pEv;
}

bool ImplHandleMouseEvent( const VclPtr<vcl::Window>& xWindow, NotifyEventType nSVEvent, bool bMouseLeave,
                           tools::Long nX, tools::Long nY, sal_uInt64 nMsgTime,
                           sal_uInt16 nCode, MouseEventModifiers nMode )
{
    SAL_INFO( "vcl.debugevent",
              "mouse event "
               "(NotifyEventType " << static_cast<sal_uInt16>(nSVEvent) << ") "
               "(MouseLeave " << bMouseLeave << ") "
               "(X, Y " << nX << ", " << nY << ") "
               "(Code " << nCode << ") "
               "(Modifiers " << static_cast<sal_uInt16>(nMode) << ")");
    ImplSVHelpData& aHelpData = ImplGetSVHelpData();
    ImplSVData* pSVData = ImplGetSVData();
    Point       aMousePos( nX, nY );
    VclPtr<vcl::Window> pChild;
    bool        bRet(false);
    sal_uInt16  nClicks(0);
    ImplFrameData* pWinFrameData = xWindow->ImplGetFrameData();
    sal_uInt16      nOldCode = pWinFrameData->mnMouseCode;

    if (comphelper::LibreOfficeKit::isActive() && AllSettings::GetLayoutRTL()
        && xWindow->GetOutDev() && !xWindow->GetOutDev()->ImplIsAntiparallel())
    {
        xWindow->GetOutDev()->ReMirror(aMousePos);
        nX = aMousePos.X();
        nY = aMousePos.Y();
    }

    // we need a mousemove event, before we get a mousebuttondown or a
    // mousebuttonup event
    if ( (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN) || (nSVEvent == NotifyEventType::MOUSEBUTTONUP) )
    {
        if ( (nSVEvent == NotifyEventType::MOUSEBUTTONUP) && aHelpData.mbExtHelpMode )
            Help::EndExtHelp();
        if ( aHelpData.mpHelpWin )
        {
            if( xWindow->ImplGetWindow() == aHelpData.mpHelpWin )
            {
                ImplDestroyHelpWindow( false );
                return true; // xWindow is dead now - avoid crash!
            }
            else
                ImplDestroyHelpWindow( true );
        }

        if ( (pWinFrameData->mnLastMouseX != nX) ||
             (pWinFrameData->mnLastMouseY != nY) )
        {
            sal_uInt16 nMoveCode = nCode & ~(MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);
            ImplHandleMouseEvent(xWindow, NotifyEventType::MOUSEMOVE, false, nX, nY, nMsgTime, nMoveCode, nMode);
        }
    }

    // update frame data
    pWinFrameData->mnBeforeLastMouseX = pWinFrameData->mnLastMouseX;
    pWinFrameData->mnBeforeLastMouseY = pWinFrameData->mnLastMouseY;
    pWinFrameData->mnLastMouseX = nX;
    pWinFrameData->mnLastMouseY = nY;
    pWinFrameData->mnMouseCode  = nCode;
    MouseEventModifiers const nTmpMask = MouseEventModifiers::SYNTHETIC | MouseEventModifiers::MODIFIERCHANGED;
    pWinFrameData->mnMouseMode  = nMode & ~nTmpMask;
    if ( bMouseLeave )
    {
        pWinFrameData->mbMouseIn = false;
        if ( ImplGetSVHelpData().mpHelpWin && !ImplGetSVHelpData().mbKeyboardHelp )
        {
            ImplDestroyHelpWindow( true );

            if ( xWindow->isDisposed() )
                return true; // xWindow is dead now - avoid crash! (#122045#)
        }
    }
    else
        pWinFrameData->mbMouseIn = true;

    DBG_ASSERT(!pSVData->mpWinData->mpTrackWin
                   || (pSVData->mpWinData->mpTrackWin == pSVData->mpWinData->mpCaptureWin),
               "ImplHandleMouseEvent: TrackWin != CaptureWin");

    // AutoScrollMode
    if (pSVData->mpWinData->mpAutoScrollWin && (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN))
    {
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
        return true;
    }

    // find mouse window
    if (pSVData->mpWinData->mpCaptureWin)
    {
        pChild = pSVData->mpWinData->mpCaptureWin;

        SAL_WARN_IF( xWindow != pChild->ImplGetFrameWindow(), "vcl",
                    "ImplHandleMouseEvent: mouse event is not sent to capture window" );

        // java client cannot capture mouse correctly
        if ( xWindow != pChild->ImplGetFrameWindow() )
            return false;

        if ( bMouseLeave )
            return false;
    }
    else
    {
        if ( bMouseLeave )
            pChild = nullptr;
        else
            pChild = xWindow->ImplFindWindow( aMousePos );
    }

    // test this because mouse events are buffered in the remote version
    // and size may not be in sync
    if ( !pChild && !bMouseLeave )
        return false;

    // execute a few tests and catch the message or implement the status
    if ( pChild )
    {
        if( pChild->GetOutDev()->ImplIsAntiparallel() )
        {
            // re-mirror frame pos at pChild
            const OutputDevice *pChildWinOutDev = pChild->GetOutDev();
            pChildWinOutDev->ReMirror( aMousePos );
        }

        // no mouse messages to disabled windows
        // #106845# if the window was disabled during capturing we have to pass the mouse events to release capturing
        if (pSVData->mpWinData->mpCaptureWin.get() != pChild
            && (!pChild->IsEnabled() || !pChild->IsInputEnabled() || pChild->IsInModalMode()))
        {
            ImplHandleMouseFloatMode( pChild, aMousePos, nCode, nSVEvent, bMouseLeave );
            if ( nSVEvent == NotifyEventType::MOUSEMOVE )
            {
                ImplHandleMouseHelpRequest( pChild, aMousePos );
                if( pWinFrameData->mpMouseMoveWin.get() != pChild )
                    nMode |= MouseEventModifiers::ENTERWINDOW;
            }

            // Call the hook also, if Window is disabled

            if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
                return true;
            else
            {
                // Set normal MousePointer for disabled windows
                if ( nSVEvent == NotifyEventType::MOUSEMOVE )
                    ImplSetMousePointer( pChild );

                return false;
            }
        }

        // End ExtTextInput-Mode, if the user click in the same TopLevel Window
        if (pSVData->mpWinData->mpExtTextInputWin
            && ((nSVEvent == NotifyEventType::MOUSEBUTTONDOWN)
                || (nSVEvent == NotifyEventType::MOUSEBUTTONUP)))
            pSVData->mpWinData->mpExtTextInputWin->EndExtTextInput();
    }

    // determine mouse event data
    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        // check if MouseMove belongs to same window and if the
        // status did not change
        if ( pChild )
        {
            Point aChildMousePos = pChild->ScreenToOutputPixel( aMousePos );
            if ( !bMouseLeave &&
                 (pChild == pWinFrameData->mpMouseMoveWin) &&
                 (aChildMousePos.X() == pWinFrameData->mnLastMouseWinX) &&
                 (aChildMousePos.Y() == pWinFrameData->mnLastMouseWinY) &&
                 (nOldCode == pWinFrameData->mnMouseCode) )
            {
                // set mouse pointer anew, as it could have changed
                // due to the mode switch
                ImplSetMousePointer( pChild );
                return false;
            }

            pWinFrameData->mnLastMouseWinX = aChildMousePos.X();
            pWinFrameData->mnLastMouseWinY = aChildMousePos.Y();
        }

        // mouse click
        nClicks = pWinFrameData->mnClickCount;

        // call Start-Drag handler if required
        // Warning: should be called before Move, as otherwise during
        // fast mouse movements the applications move to the selection state
        vcl::Window* pMouseDownWin = pWinFrameData->mpMouseDownWin;
        if ( pMouseDownWin )
        {
            // check for matching StartDrag mode. We only compare
            // the status of the mouse buttons, such that e. g. Mod1 can
            // change immediately to the copy mode
            const MouseSettings& rMSettings = pMouseDownWin->GetSettings().GetMouseSettings();
            if ( (nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) ==
                 (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) )
            {
                if ( !pMouseDownWin->ImplGetFrameData()->mbStartDragCalled )
                {
                    tools::Long nDragW  = rMSettings.GetStartDragWidth();
                    tools::Long nDragH  = rMSettings.GetStartDragHeight();
                    //long nMouseX = nX;
                    //long nMouseY = nY;
                    tools::Long nMouseX = aMousePos.X(); // #106074# use the possibly re-mirrored coordinates (RTL) ! nX,nY are unmodified !
                    tools::Long nMouseY = aMousePos.Y();
                    if ( (((nMouseX-nDragW) > pMouseDownWin->ImplGetFrameData()->mnFirstMouseX) ||
                           ((nMouseX+nDragW) < pMouseDownWin->ImplGetFrameData()->mnFirstMouseX)) ||
                         (((nMouseY-nDragH) > pMouseDownWin->ImplGetFrameData()->mnFirstMouseY) ||
                           ((nMouseY+nDragH) < pMouseDownWin->ImplGetFrameData()->mnFirstMouseY)) )
                    {
                        pMouseDownWin->ImplGetFrameData()->mbStartDragCalled  = true;

                        // Check if drag source provides its own recognizer
                        if( pMouseDownWin->ImplGetFrameData()->mbInternalDragGestureRecognizer )
                        {
                            // query DropTarget from child window
                            rtl::Reference< DNDListenerContainer > xDragGestureRecognizer(
                                    pMouseDownWin->ImplGetWindowImpl()->mxDNDListenerContainer );

                            if( xDragGestureRecognizer.is() )
                            {
                                // retrieve mouse position relative to mouse down window
                                Point relLoc = pMouseDownWin->ScreenToOutputPixel( Point(
                                    pMouseDownWin->ImplGetFrameData()->mnFirstMouseX,
                                    pMouseDownWin->ImplGetFrameData()->mnFirstMouseY ) );

                                // create a UNO mouse event out of the available data
                                css::awt::MouseEvent aMouseEvent( static_cast < css::uno::XInterface * > ( nullptr ),
#ifdef MACOSX
                                    nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3),
#else
                                    nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2),
#endif
                                    nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE),
                                    nMouseX,
                                    nMouseY,
                                    nClicks,
                                    false );

                                SolarMutexReleaser aReleaser;

                                // FIXME: where do I get Action from ?
                                css::uno::Reference< css::datatransfer::dnd::XDragSource > xDragSource = pMouseDownWin->GetDragSource();

                                if( xDragSource.is() )
                                {
                                    xDragGestureRecognizer->fireDragGestureEvent( 0,
                                        relLoc.X(), relLoc.Y(), xDragSource, css::uno::Any( aMouseEvent ) );
                                }
                            }
                        }
                    }
                }
            }
            else
                pMouseDownWin->ImplGetFrameData()->mbStartDragCalled  = true;
        }

        if (xWindow->isDisposed())
            return true;
        // test for mouseleave and mouseenter
        VclPtr<vcl::Window> pMouseMoveWin = pWinFrameData->mpMouseMoveWin;
        if ( pChild != pMouseMoveWin )
        {
            if ( pMouseMoveWin )
            {
                Point       aLeaveMousePos = pMouseMoveWin->ScreenToOutputPixel( aMousePos );
                MouseEvent  aMLeaveEvt( aLeaveMousePos, nClicks, nMode | MouseEventModifiers::LEAVEWINDOW, nCode, nCode );
                NotifyEvent aNLeaveEvt( NotifyEventType::MOUSEMOVE, pMouseMoveWin, &aMLeaveEvt );
                pWinFrameData->mbInMouseMove = true;
                pMouseMoveWin->ImplGetWinData()->mbMouseOver = false;

                // A MouseLeave can destroy this window
                if ( !ImplCallPreNotify( aNLeaveEvt ) )
                {
                    pMouseMoveWin->MouseMove( aMLeaveEvt );
                    if( !pMouseMoveWin->isDisposed() )
                        aNLeaveEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners( aNLeaveEvt );
                }

                pWinFrameData->mpMouseMoveWin = nullptr;
                pWinFrameData->mbInMouseMove = false;

                if ( pChild && pChild->isDisposed() )
                    pChild = nullptr;
                if ( pMouseMoveWin->isDisposed() )
                    return true;
            }

            nMode |= MouseEventModifiers::ENTERWINDOW;
        }
        pWinFrameData->mpMouseMoveWin = pChild;
        if( pChild )
            pChild->ImplGetWinData()->mbMouseOver = true;

        // MouseLeave
        if ( !pChild )
            return false;
    }
    else
    {
        if (pChild)
        {
            // mouse click
            if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
            {
                const MouseSettings& rMSettings = pChild->GetSettings().GetMouseSettings();
                sal_uInt64 nDblClkTime = rMSettings.GetDoubleClickTime();
                tools::Long    nDblClkW    = rMSettings.GetDoubleClickWidth();
                tools::Long    nDblClkH    = rMSettings.GetDoubleClickHeight();
                //long    nMouseX     = nX;
                //long    nMouseY     = nY;
                tools::Long nMouseX = aMousePos.X();   // #106074# use the possibly re-mirrored coordinates (RTL) ! nX,nY are unmodified !
                tools::Long nMouseY = aMousePos.Y();

                if ( (pChild == pChild->ImplGetFrameData()->mpMouseDownWin) &&
                     (nCode == pChild->ImplGetFrameData()->mnFirstMouseCode) &&
                     ((nMsgTime-pChild->ImplGetFrameData()->mnMouseDownTime) < nDblClkTime) &&
                     ((nMouseX-nDblClkW) <= pChild->ImplGetFrameData()->mnFirstMouseX) &&
                     ((nMouseX+nDblClkW) >= pChild->ImplGetFrameData()->mnFirstMouseX) &&
                     ((nMouseY-nDblClkH) <= pChild->ImplGetFrameData()->mnFirstMouseY) &&
                     ((nMouseY+nDblClkH) >= pChild->ImplGetFrameData()->mnFirstMouseY) )
                {
                    pChild->ImplGetFrameData()->mnClickCount++;
                    pChild->ImplGetFrameData()->mbStartDragCalled  = true;
                }
                else
                {
                    pChild->ImplGetFrameData()->mpMouseDownWin     = pChild;
                    pChild->ImplGetFrameData()->mnClickCount       = 1;
                    pChild->ImplGetFrameData()->mnFirstMouseX      = nMouseX;
                    pChild->ImplGetFrameData()->mnFirstMouseY      = nMouseY;
                    pChild->ImplGetFrameData()->mnFirstMouseCode   = nCode;
                    pChild->ImplGetFrameData()->mbStartDragCalled  = (nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) !=
                                                                     (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE));
                }
                pChild->ImplGetFrameData()->mnMouseDownTime = nMsgTime;
            }
            nClicks = pChild->ImplGetFrameData()->mnClickCount;
        }

        pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();
    }

    SAL_WARN_IF( !pChild, "vcl", "ImplHandleMouseEvent: pChild == NULL" );

    if (!pChild)
        return false;

    // create mouse event
    Point aChildPos = pChild->ScreenToOutputPixel( aMousePos );
    MouseEvent aMEvt( aChildPos, nClicks, nMode, nCode, nCode );


    // tracking window gets the mouse events
    if (pSVData->mpWinData->mpTrackWin)
        pChild = pSVData->mpWinData->mpTrackWin;

    // handle FloatingMode
    if (!pSVData->mpWinData->mpTrackWin && pSVData->mpWinData->mpFirstFloat)
    {
        if ( ImplHandleMouseFloatMode( pChild, aMousePos, nCode, nSVEvent, bMouseLeave ) )
        {
            if ( !pChild->isDisposed() )
            {
                pChild->ImplGetFrameData()->mbStartDragCalled = true;
            }
            return true;
        }
    }

    // call handler
    bool bCallHelpRequest = true;
    SAL_WARN_IF( !pChild, "vcl", "ImplHandleMouseEvent: pChild is NULL" );

    if (!pChild)
        return false;

    NotifyEvent aNEvt( nSVEvent, pChild, &aMEvt );
    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        pChild->ImplGetFrameData()->mbInMouseMove = true;

    // bring window into foreground on mouseclick
    if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
    {
        if (!pSVData->mpWinData->mpFirstFloat
            && // totop for floating windows in popup would change the focus and would close them immediately
            !(pChild->ImplGetFrameWindow()->GetStyle()
              & WB_OWNERDRAWDECORATION)) // ownerdrawdecorated windows must never grab focus
            pChild->ToTop();
        if ( pChild->isDisposed() )
            return true;
    }

    if ( ImplCallPreNotify( aNEvt ) || pChild->isDisposed() )
        bRet = true;
    else
    {
        bRet = false;
        if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        {
            if (pSVData->mpWinData->mpTrackWin)
            {
                TrackingEvent aTEvt( aMEvt );
                pChild->Tracking( aTEvt );
                if ( !pChild->isDisposed() )
                {
                    // When ScrollRepeat, we restart the timer
                    if (pSVData->mpWinData->mpTrackTimer
                        && (pSVData->mpWinData->mnTrackFlags & StartTrackingFlags::ScrollRepeat))
                        pSVData->mpWinData->mpTrackTimer->Start();
                }
                bCallHelpRequest = false;
                bRet = true;
            }
            else
            {
                if( pChild->isDisposed() )
                    bCallHelpRequest = false;
                else
                {
                    // if the MouseMove handler changes the help window's visibility
                    // the HelpRequest handler should not be called anymore
                    vcl::Window* pOldHelpTextWin = ImplGetSVHelpData().mpHelpWin;
                    pChild->MouseMove( aMEvt );
                    if ( pOldHelpTextWin != ImplGetSVHelpData().mpHelpWin )
                        bCallHelpRequest = false;
                }
            }
        }
        else if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
        {
            if ( pSVData->mpWinData->mpTrackWin )
                bRet = true;
            else
            {
                pChild->ImplGetWindowImpl()->mbMouseButtonDown = false;
                pChild->MouseButtonDown( aMEvt );
            }
        }
        else
        {
            if (pSVData->mpWinData->mpTrackWin)
            {
                pChild->EndTracking();
                bRet = true;
            }
            else
            {
                pChild->ImplGetWindowImpl()->mbMouseButtonUp = false;
                pChild->MouseButtonUp( aMEvt );
            }
        }

        assert(aNEvt.GetWindow() == pChild);

        if (!pChild->isDisposed())
            pChild->ImplNotifyKeyMouseCommandEventListeners( aNEvt );
    }

    if (pChild->isDisposed())
        return true;

    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        pChild->ImplGetWindowImpl()->mpFrameData->mbInMouseMove = false;

    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        if ( bCallHelpRequest && !ImplGetSVHelpData().mbKeyboardHelp )
            ImplHandleMouseHelpRequest( pChild, pChild->OutputToScreenPixel( aMEvt.GetPosPixel() ) );
        bRet = true;
    }
    else if ( !bRet )
    {
        if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
        {
            if ( !pChild->ImplGetWindowImpl()->mbMouseButtonDown )
                bRet = true;
        }
        else
        {
            if ( !pChild->ImplGetWindowImpl()->mbMouseButtonUp )
                bRet = true;
        }
    }

    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        // set new mouse pointer
        if ( !bMouseLeave )
            ImplSetMousePointer( pChild );
    }
    else if ( (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN) || (nSVEvent == NotifyEventType::MOUSEBUTTONUP) )
    {
        // Command-Events
        if ( /*!bRet &&*/ (nClicks == 1) && (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN) &&
             (nCode == MOUSE_MIDDLE) )
        {
            MouseMiddleButtonAction nMiddleAction = pChild->GetSettings().GetMouseSettings().GetMiddleButtonAction();
            if ( nMiddleAction == MouseMiddleButtonAction::AutoScroll )
                bRet = !ImplCallCommand( pChild, CommandEventId::StartAutoScroll, nullptr, true, &aChildPos );
            else if ( nMiddleAction == MouseMiddleButtonAction::PasteSelection )
                bRet = !ImplCallCommand( pChild, CommandEventId::PasteSelection, nullptr, true, &aChildPos );
        }
        else
        {
            // ContextMenu
            if ( (nCode == MouseSettings::GetContextMenuCode()) &&
                 (nClicks == MouseSettings::GetContextMenuClicks()) )
            {
                bool bContextMenu = (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN);
                if ( bContextMenu )
                {
                    if( pSVData->maAppData.mpActivePopupMenu )
                    {
                        /*  #i34277# there already is a context menu open
                        *   that was probably just closed with EndPopupMode.
                        *   We need to give the eventual corresponding
                        *   PopupMenu::Execute a chance to end properly.
                        *   Therefore delay context menu command and
                        *   issue only after popping one frame of the
                        *   Yield stack.
                        */
                        ContextMenuEvent* pEv = new ContextMenuEvent;
                        pEv->pWindow = std::move(pChild);
                        pEv->aChildPos = aChildPos;
                        Application::PostUserEvent( LINK_NONMEMBER( pEv, ContextMenuEventLink ) );
                    }
                    else
                        bRet = ! ImplCallCommand( pChild, CommandEventId::ContextMenu, nullptr, true, &aChildPos );
                }
            }
        }
    }

    return bRet;
}

bool ImplLOKHandleMouseEvent(const VclPtr<vcl::Window>& xWindow, NotifyEventType nEvent, bool /*bMouseLeave*/,
                             tools::Long nX, tools::Long nY, sal_uInt64 /*nMsgTime*/,
                             sal_uInt16 nCode, MouseEventModifiers nMode, sal_uInt16 nClicks)
{
    Point aMousePos(nX, nY);

    if (!xWindow)
        return false;

    if (xWindow->isDisposed())
        return false;

    ImplFrameData* pFrameData = xWindow->ImplGetFrameData();
    if (!pFrameData)
        return false;

    Point aWinPos = xWindow->ScreenToOutputPixel(aMousePos);

    pFrameData->mnLastMouseX = nX;
    pFrameData->mnLastMouseY = nY;
    pFrameData->mnClickCount = nClicks;
    pFrameData->mnMouseCode = nCode;
    pFrameData->mbMouseIn = false;

    vcl::Window* pDragWin = pFrameData->mpMouseDownWin;
    if (pDragWin &&
        nEvent == NotifyEventType::MOUSEMOVE &&
        pFrameData->mbDragging)
    {
        css::uno::Reference<css::datatransfer::dnd::XDropTargetDragContext> xDropTargetDragContext =
            new GenericDropTargetDragContext();
        rtl::Reference<DNDListenerContainer> xDropTarget(
            pDragWin->ImplGetWindowImpl()->mxDNDListenerContainer);

        if (!xDropTarget.is() ||
            !xDropTargetDragContext.is() ||
            (nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) !=
            (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)))
        {
            pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
            return false;
        }

        xDropTarget->fireDragOverEvent(
            xDropTargetDragContext,
            css::datatransfer::dnd::DNDConstants::ACTION_MOVE,
            aWinPos.X(),
            aWinPos.Y(),
            (css::datatransfer::dnd::DNDConstants::ACTION_COPY |
             css::datatransfer::dnd::DNDConstants::ACTION_MOVE |
             css::datatransfer::dnd::DNDConstants::ACTION_LINK));

        return true;
    }

    if (pDragWin &&
        nEvent == NotifyEventType::MOUSEBUTTONUP &&
        pFrameData->mbDragging)
    {
        css::uno::Reference<css::datatransfer::XTransferable> xTransfer;
        css::uno::Reference<css::datatransfer::dnd::XDropTargetDropContext> xDropTargetDropContext =
            new GenericDropTargetDropContext();
        rtl::Reference<DNDListenerContainer> xDropTarget(
            pDragWin->ImplGetWindowImpl()->mxDNDListenerContainer);

        if (!xDropTarget.is() || !xDropTargetDropContext.is())
        {
            pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
            return false;
        }

        Point dragOverPos = pDragWin->ScreenToOutputPixel(aMousePos);
        xDropTarget->fireDropEvent(
            xDropTargetDropContext,
            css::datatransfer::dnd::DNDConstants::ACTION_MOVE,
            dragOverPos.X(),
            dragOverPos.Y(),
            (css::datatransfer::dnd::DNDConstants::ACTION_COPY |
             css::datatransfer::dnd::DNDConstants::ACTION_MOVE |
             css::datatransfer::dnd::DNDConstants::ACTION_LINK),
            xTransfer);

        pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
        return true;
    }

    if (pFrameData->mbDragging)
    {
        // wrong status, reset
        pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
        return false;
    }

    vcl::Window* pDownWin = pFrameData->mpMouseDownWin;
    if (pDownWin && nEvent == NotifyEventType::MOUSEMOVE)
    {
        const MouseSettings& aSettings = pDownWin->GetSettings().GetMouseSettings();
        if ((nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) ==
            (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) )
        {
            if (!pFrameData->mbStartDragCalled)
            {
                tools::Long nDragWidth = aSettings.GetStartDragWidth();
                tools::Long nDragHeight = aSettings.GetStartDragHeight();
                tools::Long nMouseX = aMousePos.X();
                tools::Long nMouseY = aMousePos.Y();

                if ((((nMouseX - nDragWidth) > pFrameData->mnFirstMouseX) ||
                     ((nMouseX + nDragWidth) < pFrameData->mnFirstMouseX)) ||
                    (((nMouseY - nDragHeight) > pFrameData->mnFirstMouseY) ||
                     ((nMouseY + nDragHeight) < pFrameData->mnFirstMouseY)))
                {
                    pFrameData->mbStartDragCalled  = true;

                    if (pFrameData->mbInternalDragGestureRecognizer)
                    {
                        // query DropTarget from child window
                        rtl::Reference<DNDListenerContainer> xDragGestureRecognizer(
                            pDownWin->ImplGetWindowImpl()->mxDNDListenerContainer );

                        if (xDragGestureRecognizer.is())
                        {
                            // create a UNO mouse event out of the available data
                            css::awt::MouseEvent aEvent(
                                static_cast < css::uno::XInterface * > ( nullptr ),
 #ifdef MACOSX
                                nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3),
 #else
                                nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2),
 #endif
                                nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE),
                                nMouseX,
                                nMouseY,
                                nClicks,
                                false);
                            css::uno::Reference< css::datatransfer::dnd::XDragSource > xDragSource =
                                pDownWin->GetDragSource();

                            if (xDragSource.is())
                            {
                                xDragGestureRecognizer->
                                    fireDragGestureEvent(
                                        0,
                                        aWinPos.X(),
                                        aWinPos.Y(),
                                        xDragSource,
                                        css::uno::Any(aEvent));
                            }
                        }
                    }
                }
            }
        }
    }

    MouseEvent aMouseEvent(aWinPos, nClicks, nMode, nCode, nCode);
    if (nEvent == NotifyEventType::MOUSEMOVE)
    {
        if (pFrameData->mpTrackWin)
        {
            TrackingEvent aTrackingEvent(aMouseEvent);
            pFrameData->mpTrackWin->Tracking(aTrackingEvent);
        }
        else
            xWindow->MouseMove(aMouseEvent);
    }
    else if (nEvent == NotifyEventType::MOUSEBUTTONDOWN &&
        !pFrameData->mpTrackWin)
    {
        pFrameData->mpMouseDownWin = xWindow;
        pFrameData->mnFirstMouseX = aMousePos.X();
        pFrameData->mnFirstMouseY = aMousePos.Y();

        xWindow->MouseButtonDown(aMouseEvent);
    }
    else
    {
        if (pFrameData->mpTrackWin)
        {
            pFrameData->mpTrackWin->EndTracking();
        }

        pFrameData->mpMouseDownWin = nullptr;
        pFrameData->mpMouseMoveWin = nullptr;
        pFrameData->mbStartDragCalled = false;
        xWindow->MouseButtonUp(aMouseEvent);
    }

    if (nEvent == NotifyEventType::MOUSEBUTTONDOWN)
    {
         // ContextMenu
         if ( (nCode == MouseSettings::GetContextMenuCode()) &&
              (nClicks == MouseSettings::GetContextMenuClicks()) )
         {
            ImplCallCommand(xWindow, CommandEventId::ContextMenu, nullptr, true, &aWinPos);
         }
    }

    return true;
}

static vcl::Window* ImplGetKeyInputWindow( vcl::Window* pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();

    // determine last input time
    pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();

    // #127104# workaround for destroyed windows
    if( pWindow->ImplGetWindowImpl() == nullptr )
        return nullptr;

    // find window - is every time the window which has currently the
    // focus or the last time the focus.

    // the first floating window always has the focus, try it, or any parent floating windows, first
    vcl::Window* pChild = pSVData->mpWinData->mpFirstFloat;
    while (pChild)
    {
        if (pChild->ImplGetWindowImpl())
        {
            if (pChild->ImplGetWindowImpl()->mbFloatWin)
            {
                if (static_cast<FloatingWindow *>(pChild)->GrabsFocus())
                    break;
            }
            else if (pChild->ImplGetWindowImpl()->mbDockWin)
            {
                vcl::Window* pParent = pChild->GetWindow(GetWindowType::RealParent);
                if (pParent && pParent->ImplGetWindowImpl()->mbFloatWin &&
                    static_cast<FloatingWindow *>(pParent)->GrabsFocus())
                    break;
            }
        }
        pChild = pChild->GetParent();
    }

    if (!pChild)
        pChild = pWindow;

    pChild = pChild->ImplGetWindowImpl() && pChild->ImplGetWindowImpl()->mpFrameData ? pChild->ImplGetWindowImpl()->mpFrameData->mpFocusWin.get() : nullptr;

    // no child - then no input
    if ( !pChild )
        return nullptr;

    // We call also KeyInput if we haven't the focus, because on Unix
    // system this is often the case when a Lookup Choice Window has
    // the focus - because this windows send the KeyInput directly to
    // the window without resetting the focus

    // no keyinput to disabled windows
    if ( !pChild->IsEnabled() || !pChild->IsInputEnabled() || pChild->IsInModalMode() )
        return nullptr;

    return pChild;
}

static bool ImplHandleKey( vcl::Window* pWindow, NotifyEventType nSVEvent,
                           sal_uInt16 nKeyCode, sal_uInt16 nCharCode, sal_uInt16 nRepeat, bool bForward )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::KeyCode aKeyCode( nKeyCode, nKeyCode );
    sal_uInt16 nEvCode = aKeyCode.GetCode();

    // allow application key listeners to remove the key event
    // but make sure we're not forwarding external KeyEvents, (ie where bForward is false)
    // because those are coming back from the listener itself and MUST be processed
    if( bForward )
    {
        VclEventId nVCLEvent;
        switch( nSVEvent )
        {
            case NotifyEventType::KEYINPUT:
                nVCLEvent = VclEventId::WindowKeyInput;
                break;
            case NotifyEventType::KEYUP:
                nVCLEvent = VclEventId::WindowKeyUp;
                break;
            default:
                nVCLEvent = VclEventId::NONE;
                break;
        }
        KeyEvent aKeyEvent(static_cast<sal_Unicode>(nCharCode), aKeyCode, nRepeat);
        if (nVCLEvent != VclEventId::NONE && Application::HandleKey(nVCLEvent, pWindow, &aKeyEvent))
            return true;
    }

    bool bCtrlF6 = (aKeyCode.GetCode() == KEY_F6) && aKeyCode.IsMod1();

    // determine last input time
    pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();

    // handle tracking window
    if ( nSVEvent == NotifyEventType::KEYINPUT )
    {
        if ( ImplGetSVHelpData().mbExtHelpMode )
        {
            Help::EndExtHelp();
            if ( nEvCode == KEY_ESCAPE )
                return true;
        }
        if ( ImplGetSVHelpData().mpHelpWin )
            ImplDestroyHelpWindow( false );

        // AutoScrollMode
        if (pSVData->mpWinData->mpAutoScrollWin)
        {
            pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
            if ( nEvCode == KEY_ESCAPE )
                return true;
        }

        if (pSVData->mpWinData->mpTrackWin)
        {
            sal_uInt16 nOrigCode = aKeyCode.GetCode();

            if ( nOrigCode == KEY_ESCAPE )
            {
                pSVData->mpWinData->mpTrackWin->EndTracking( TrackingEventFlags::Cancel | TrackingEventFlags::Key );
                if (pSVData->mpWinData->mpFirstFloat)
                {
                    FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                    if ( !(pLastLevelFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoKeyClose) )
                    {
                        sal_uInt16 nEscCode = aKeyCode.GetCode();

                        if ( nEscCode == KEY_ESCAPE )
                            pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                    }
                }
                return true;
            }
            else if ( nOrigCode == KEY_RETURN )
            {
                pSVData->mpWinData->mpTrackWin->EndTracking( TrackingEventFlags::Key );
                return true;
            }
            else
                return true;
        }

        // handle FloatingMode
        if (pSVData->mpWinData->mpFirstFloat)
        {
            FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
            if ( !(pLastLevelFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoKeyClose) )
            {
                sal_uInt16 nCode = aKeyCode.GetCode();

                if ( (nCode == KEY_ESCAPE) || bCtrlF6)
                {
                    pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                    if( !bCtrlF6 )
                        return true;
                }
            }
        }

        // test for accel
        if ( pSVData->maAppData.mpAccelMgr )
        {
            if ( pSVData->maAppData.mpAccelMgr->IsAccelKey( aKeyCode ) )
                return true;
        }
    }

    // find window
    VclPtr<vcl::Window> pChild = ImplGetKeyInputWindow( pWindow );
    if ( !pChild )
        return false;

    // #i1820# use locale specific decimal separator
    if (nEvCode == KEY_DECIMAL)
    {
        // tdf#138932: don't modify the meaning of the key for password box
        bool bPass = false;
        if (auto pEdit = dynamic_cast<Edit*>(pChild.get()))
            bPass = pEdit->IsPassword();
        if (!bPass && Application::GetSettings().GetMiscSettings().GetEnableLocalizedDecimalSep())
        {
            OUString aSep(pWindow->GetSettings().GetLocaleDataWrapper().getNumDecimalSep());
            nCharCode = static_cast<sal_uInt16>(aSep[0]);
        }
    }

    // RTL: mirror cursor keys
    if( (aKeyCode.GetCode() == KEY_LEFT || aKeyCode.GetCode() == KEY_RIGHT) &&
      pChild->IsRTLEnabled() && pChild->GetOutDev()->HasMirroredGraphics() )
        aKeyCode = vcl::KeyCode( aKeyCode.GetCode() == KEY_LEFT ? KEY_RIGHT : KEY_LEFT, aKeyCode.GetModifier() );

    KeyEvent    aKeyEvt( static_cast<sal_Unicode>(nCharCode), aKeyCode, nRepeat );
    NotifyEvent aNotifyEvt( nSVEvent, pChild, &aKeyEvt );
    bool bKeyPreNotify = ImplCallPreNotify( aNotifyEvt );
    bool bRet = true;

    if ( !bKeyPreNotify && !pChild->isDisposed() )
    {
        if ( nSVEvent == NotifyEventType::KEYINPUT )
        {
            UITestLogger::getInstance().logKeyInput(pChild, aKeyEvt);
            pChild->ImplGetWindowImpl()->mbKeyInput = false;
            pChild->KeyInput( aKeyEvt );
        }
        else
        {
            pChild->ImplGetWindowImpl()->mbKeyUp = false;
            pChild->KeyUp( aKeyEvt );
        }
        if( !pChild->isDisposed() )
            aNotifyEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners( aNotifyEvt );
    }

    if ( pChild->isDisposed() )
        return true;

    if ( nSVEvent == NotifyEventType::KEYINPUT )
    {
        if ( !bKeyPreNotify && pChild->ImplGetWindowImpl()->mbKeyInput )
        {
            sal_uInt16 nCode = aKeyCode.GetCode();

            // #101999# is focus in or below toolbox
            bool bToolboxFocus=false;
            if( (nCode == KEY_F1) && aKeyCode.IsShift() )
            {
                vcl::Window *pWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
                while( pWin )
                {
                    if( pWin->ImplGetWindowImpl()->mbToolBox )
                    {
                        bToolboxFocus = true;
                        break;
                    }
                    else
                        pWin = pWin->GetParent();
                }
            }

            // ContextMenu
            if ( (nCode == KEY_CONTEXTMENU) || ((nCode == KEY_F10) && aKeyCode.IsShift() && !aKeyCode.IsMod1() && !aKeyCode.IsMod2() ) )
                bRet = !ImplCallCommand( pChild, CommandEventId::ContextMenu );
            else if ( ( (nCode == KEY_F2) && aKeyCode.IsShift() ) || ( (nCode == KEY_F1) && aKeyCode.IsMod1() ) ||
                // #101999# no active help when focus in toolbox, simulate BalloonHelp instead
                ( (nCode == KEY_F1) && aKeyCode.IsShift() && bToolboxFocus ) )
            {
                // TipHelp via Keyboard (Shift-F2 or Ctrl-F1)
                // simulate mouseposition at center of window

                Size aSize = pChild->GetOutDev()->GetOutputSize();
                Point aPos( aSize.getWidth()/2, aSize.getHeight()/2 );
                aPos = pChild->OutputToScreenPixel( aPos );

                HelpEvent aHelpEvent( aPos, HelpEventMode::BALLOON );
                aHelpEvent.SetKeyboardActivated( true );
                ImplGetSVHelpData().mbSetKeyboardHelp = true;
                pChild->RequestHelp( aHelpEvent );
                ImplGetSVHelpData().mbSetKeyboardHelp = false;
            }
            else if ( (nCode == KEY_F1) || (nCode == KEY_HELP) )
            {
                if ( !aKeyCode.GetModifier() )
                {
                    if ( ImplGetSVHelpData().mbContextHelp )
                    {
                        Point       aMousePos = pChild->OutputToScreenPixel( pChild->GetPointerPosPixel() );
                        HelpEvent   aHelpEvent( aMousePos, HelpEventMode::CONTEXT );
                        pChild->RequestHelp( aHelpEvent );
                    }
                    else
                        bRet = false;
                }
                else if ( aKeyCode.IsShift() )
                {
                    if ( ImplGetSVHelpData().mbExtHelp )
                        Help::StartExtHelp();
                    else
                        bRet = false;
                }
            }
            else
                bRet = false;
        }
    }
    else
    {
        if ( !bKeyPreNotify && pChild->ImplGetWindowImpl()->mbKeyUp )
            bRet = false;
    }

    // #105591# send keyinput to parent if we are a floating window and the key was not processed yet
    if( !bRet && pWindow->ImplGetWindowImpl() && pWindow->ImplGetWindowImpl()->mbFloatWin && pWindow->GetParent() && (pWindow->ImplGetWindowImpl()->mpFrame != pWindow->GetParent()->ImplGetWindowImpl()->mpFrame) )
    {
        pChild = pWindow->GetParent();

        // call handler
        NotifyEvent aNEvt( nSVEvent, pChild, &aKeyEvt );
        bool bPreNotify = ImplCallPreNotify( aNEvt );
        if ( pChild->isDisposed() )
            return true;

        if ( !bPreNotify )
        {
            if ( nSVEvent == NotifyEventType::KEYINPUT )
            {
                pChild->ImplGetWindowImpl()->mbKeyInput = false;
                pChild->KeyInput( aKeyEvt );
            }
            else
            {
                pChild->ImplGetWindowImpl()->mbKeyUp = false;
                pChild->KeyUp( aKeyEvt );
            }

            if( !pChild->isDisposed() )
                aNEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners( aNEvt );
            if ( pChild->isDisposed() )
                return true;
        }

        if( bPreNotify || !pChild->ImplGetWindowImpl()->mbKeyInput )
            bRet = true;
    }

    return bRet;
}

static bool ImplHandleExtTextInput( vcl::Window* pWindow,
                                    const OUString& rText,
                                    const ExtTextInputAttr* pTextAttr,
                                    sal_Int32 nCursorPos, sal_uInt16 nCursorFlags )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window*     pChild = nullptr;

    int nTries = 200;
    while( nTries-- )
    {
        pChild = pSVData->mpWinData->mpExtTextInputWin;
        if ( !pChild )
        {
            pChild = ImplGetKeyInputWindow( pWindow );
            if ( !pChild )
                return false;
        }
        if( !pChild->ImplGetWindowImpl()->mpFrameData->mnFocusId )
            break;

        if (comphelper::LibreOfficeKit::isActive())
        {
            SAL_WARN("vcl", "Failed to get ext text input context");
            break;
        }
        Application::Yield();
    }

    // If it is the first ExtTextInput call, we inform the information
    // and allocate the data, which we must store in this mode
    ImplWinData* pWinData = pChild->ImplGetWinData();
    if ( !pChild->ImplGetWindowImpl()->mbExtTextInput )
    {
        pChild->ImplGetWindowImpl()->mbExtTextInput = true;
        pWinData->mpExtOldText = OUString();
        pWinData->mpExtOldAttrAry.reset();
        pSVData->mpWinData->mpExtTextInputWin = pChild;
        ImplCallCommand( pChild, CommandEventId::StartExtTextInput );
    }

    // be aware of being recursively called in StartExtTextInput
    if ( !pChild->ImplGetWindowImpl()->mbExtTextInput )
        return false;

    // Test for changes
    bool bOnlyCursor = false;
    sal_Int32 nMinLen = std::min( pWinData->mpExtOldText->getLength(), rText.getLength() );
    sal_Int32 nDeltaStart = 0;
    while ( nDeltaStart < nMinLen )
    {
        if ( (*pWinData->mpExtOldText)[nDeltaStart] != rText[nDeltaStart] )
            break;
        nDeltaStart++;
    }
    if ( pWinData->mpExtOldAttrAry || pTextAttr )
    {
        if ( !pWinData->mpExtOldAttrAry || !pTextAttr )
            nDeltaStart = 0;
        else
        {
            sal_Int32 i = 0;
            while ( i < nDeltaStart )
            {
                if ( pWinData->mpExtOldAttrAry[i] != pTextAttr[i] )
                {
                    nDeltaStart = i;
                    break;
                }
                i++;
            }
        }
    }
    if ( (nDeltaStart >= nMinLen) &&
         (pWinData->mpExtOldText->getLength() == rText.getLength()) )
        bOnlyCursor = true;

    // Call Event and store the information
    CommandExtTextInputData aData( rText, pTextAttr,
                                   nCursorPos, nCursorFlags,
                                   bOnlyCursor );
    *pWinData->mpExtOldText = rText;
    pWinData->mpExtOldAttrAry.reset();
    if ( pTextAttr )
    {
        pWinData->mpExtOldAttrAry.reset( new ExtTextInputAttr[rText.getLength()] );
        memcpy( pWinData->mpExtOldAttrAry.get(), pTextAttr, rText.getLength()*sizeof( ExtTextInputAttr ) );
    }
    return !ImplCallCommand( pChild, CommandEventId::ExtTextInput, &aData );
}

static bool ImplHandleEndExtTextInput()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;
    bool bRet = false;

    if ( pChild )
    {
        pChild->ImplGetWindowImpl()->mbExtTextInput = false;
        pSVData->mpWinData->mpExtTextInputWin = nullptr;
        ImplWinData* pWinData = pChild->ImplGetWinData();
        pWinData->mpExtOldText.reset();
        pWinData->mpExtOldAttrAry.reset();
        bRet = !ImplCallCommand( pChild, CommandEventId::EndExtTextInput );
    }

    return bRet;
}

static void ImplHandleExtTextInputPos( vcl::Window* pWindow,
                                       tools::Rectangle& rRect, tools::Long& rInputWidth,
                                       bool * pVertical )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;

    if ( !pChild )
        pChild = ImplGetKeyInputWindow( pWindow );
    else
    {
        // Test, if the Window is related to the frame
        if ( !pWindow->ImplIsWindowOrChild( pChild ) )
            pChild = ImplGetKeyInputWindow( pWindow );
    }

    if ( pChild )
    {
        const OutputDevice *pChildOutDev = pChild->GetOutDev();
        ImplCallCommand( pChild, CommandEventId::CursorPos );
        const tools::Rectangle* pRect = pChild->GetCursorRect();
        if ( pRect )
            rRect = pChildOutDev->ImplLogicToDevicePixel( *pRect );
        else
        {
            vcl::Cursor* pCursor = pChild->GetCursor();
            if ( pCursor )
            {
                Point aPos = pChildOutDev->ImplLogicToDevicePixel( pCursor->GetPos() );
                Size aSize = pChild->LogicToPixel( pCursor->GetSize() );
                if ( !aSize.Width() )
                    aSize.setWidth( pChild->GetSettings().GetStyleSettings().GetCursorSize() );
                rRect = tools::Rectangle( aPos, aSize );
            }
            else
                rRect = tools::Rectangle( Point( pChild->GetOutOffXPixel(), pChild->GetOutOffYPixel() ), Size() );
        }
        rInputWidth = pChild->GetOutDev()->ImplLogicWidthToDevicePixel( pChild->GetCursorExtTextInputWidth() );
        if ( !rInputWidth )
            rInputWidth = rRect.GetWidth();
    }
    if (pVertical != nullptr)
        *pVertical
            = pChild != nullptr && pChild->GetInputContext().GetFont().IsVertical();
}

static bool ImplHandleInputContextChange( vcl::Window* pWindow )
{
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );
    CommandInputContextData aData;
    return !ImplCallCommand( pChild, CommandEventId::InputContextChange, &aData );
}

static bool ImplCallWheelCommand( const VclPtr<vcl::Window>& pWindow, const Point& rPos,
                                  const CommandWheelData* pWheelData )
{
    Point               aCmdMousePos = pWindow->ScreenToOutputPixel( rPos );
    CommandEvent        aCEvt( aCmdMousePos, CommandEventId::Wheel, true, pWheelData );
    NotifyEvent         aNCmdEvt( NotifyEventType::COMMAND, pWindow, &aCEvt );
    bool bPreNotify = ImplCallPreNotify( aNCmdEvt );
    if ( pWindow->isDisposed() )
        return false;
    if ( !bPreNotify )
    {
        pWindow->ImplGetWindowImpl()->mbCommand = false;
        pWindow->Command( aCEvt );
        if ( pWindow->isDisposed() )
            return false;
        if ( pWindow->ImplGetWindowImpl()->mbCommand )
            return true;
    }
    return false;
}

static bool acceptableWheelScrollTarget(const vcl::Window *pMouseWindow)
{
    return (pMouseWindow && !pMouseWindow->isDisposed() && pMouseWindow->IsInputEnabled() && !pMouseWindow->IsInModalMode());
}

//If the last event at the same absolute screen position was handled by a
//different window then reuse that window if the event occurs within 1/2 a
//second, i.e. so scrolling down something like the calc sidebar that contains
//widgets that respond to wheel events will continue to send the event to the
//scrolling widget in favour of the widget that happens to end up under the
//mouse.
static bool shouldReusePreviousMouseWindow(const SalWheelMouseEvent& rPrevEvt, const SalWheelMouseEvent& rEvt)
{
    return (rEvt.mnX == rPrevEvt.mnX && rEvt.mnY == rPrevEvt.mnY && rEvt.mnTime-rPrevEvt.mnTime < 500/*ms*/);
}

namespace {

class HandleGestureEventBase
{
protected:
    ImplSVData* m_pSVData;
    VclPtr<vcl::Window> m_pWindow;
    Point m_aMousePos;

public:
    HandleGestureEventBase(vcl::Window *pWindow, const Point &rMousePos)
        : m_pSVData(ImplGetSVData())
        , m_pWindow(pWindow)
        , m_aMousePos(rMousePos)
    {
    }
    bool Setup();
    vcl::Window* FindTarget();
    vcl::Window* Dispatch(vcl::Window* pTarget);
    virtual bool CallCommand(vcl::Window *pWindow, const Point &rMousePos) = 0;
    virtual ~HandleGestureEventBase() {}
};

}

bool HandleGestureEventBase::Setup()
{

    if (m_pSVData->mpWinData->mpAutoScrollWin)
        m_pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
    if (ImplGetSVHelpData().mpHelpWin)
        ImplDestroyHelpWindow( true );
    return !m_pWindow->isDisposed();
}

vcl::Window* HandleGestureEventBase::FindTarget()
{
    // first check any floating window ( eg. drop down listboxes)
    vcl::Window *pMouseWindow = nullptr;

    if (m_pSVData->mpWinData->mpFirstFloat && !m_pSVData->mpWinData->mpCaptureWin &&
         !m_pSVData->mpWinData->mpFirstFloat->ImplIsFloatPopupModeWindow( m_pWindow ) )
    {
        bool bHitTestInsideRect = false;
        pMouseWindow = m_pSVData->mpWinData->mpFirstFloat->ImplFloatHitTest( m_pWindow, m_aMousePos, bHitTestInsideRect );
        if (!pMouseWindow)
            pMouseWindow = m_pSVData->mpWinData->mpFirstFloat;
    }
    // then try the window directly beneath the mouse
    if( !pMouseWindow )
    {
        pMouseWindow = m_pWindow->ImplFindWindow( m_aMousePos );
    }
    else
    {
        // transform coordinates to float window frame coordinates
        pMouseWindow = pMouseWindow->ImplFindWindow(
                 pMouseWindow->OutputToScreenPixel(
                  pMouseWindow->AbsoluteScreenToOutputPixel(
                   m_pWindow->OutputToAbsoluteScreenPixel(
                    m_pWindow->ScreenToOutputPixel( m_aMousePos ) ) ) ) );
    }

    while (acceptableWheelScrollTarget(pMouseWindow))
    {
        if (pMouseWindow->IsEnabled())
            break;
        //try the parent if this one is disabled
        pMouseWindow = pMouseWindow->GetParent();
    }

    return pMouseWindow;
}

vcl::Window *HandleGestureEventBase::Dispatch(vcl::Window* pMouseWindow)
{
    vcl::Window *pDispatchedTo = nullptr;

    if (acceptableWheelScrollTarget(pMouseWindow) && pMouseWindow->IsEnabled())
    {
        // transform coordinates to float window frame coordinates
        Point aRelMousePos( pMouseWindow->OutputToScreenPixel(
                             pMouseWindow->AbsoluteScreenToOutputPixel(
                              m_pWindow->OutputToAbsoluteScreenPixel(
                               m_pWindow->ScreenToOutputPixel( m_aMousePos ) ) ) ) );
        bool bPropagate = CallCommand(pMouseWindow, aRelMousePos);
        if (!bPropagate)
            pDispatchedTo = pMouseWindow;
    }

    // if the command was not handled try the focus window
    if (!pDispatchedTo)
    {
        vcl::Window* pFocusWindow = m_pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
        if ( pFocusWindow && (pFocusWindow != pMouseWindow) &&
             (pFocusWindow == m_pSVData->mpWinData->mpFocusWin) )
        {
            // no wheel-messages to disabled windows
            if ( pFocusWindow->IsEnabled() && pFocusWindow->IsInputEnabled() && ! pFocusWindow->IsInModalMode() )
            {
                // transform coordinates to focus window frame coordinates
                Point aRelMousePos( pFocusWindow->OutputToScreenPixel(
                                     pFocusWindow->AbsoluteScreenToOutputPixel(
                                      m_pWindow->OutputToAbsoluteScreenPixel(
                                       m_pWindow->ScreenToOutputPixel( m_aMousePos ) ) ) ) );
                bool bPropagate = CallCommand(pFocusWindow, aRelMousePos);
                if (!bPropagate)
                    pDispatchedTo = pMouseWindow;
            }
        }
    }
    return pDispatchedTo;
}

namespace {

class HandleWheelEvent : public HandleGestureEventBase
{
private:
    CommandWheelData m_aWheelData;
public:
    HandleWheelEvent(vcl::Window *pWindow, const SalWheelMouseEvent& rEvt)
        : HandleGestureEventBase(pWindow, Point(rEvt.mnX, rEvt.mnY))
    {
        CommandWheelMode nMode;
        sal_uInt16 nCode = rEvt.mnCode;
        bool bHorz = rEvt.mbHorz;
        bool bPixel = rEvt.mbDeltaIsPixel;
        if ( nCode & KEY_MOD1 )
            nMode = CommandWheelMode::ZOOM;
        else if ( nCode & KEY_MOD2 )
            nMode = CommandWheelMode::DATAZOOM;
        else
        {
            nMode = CommandWheelMode::SCROLL;
            // #i85450# interpret shift-wheel as horizontal wheel action
            if( (nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3)) == KEY_SHIFT )
                bHorz = true;
        }

        m_aWheelData = CommandWheelData(rEvt.mnDelta, rEvt.mnNotchDelta, rEvt.mnScrollLines, nMode, nCode, bHorz, bPixel);

    }
    virtual bool CallCommand(vcl::Window *pWindow, const Point &rMousePos) override
    {
        return ImplCallWheelCommand(pWindow, rMousePos, &m_aWheelData);
    }
    bool HandleEvent(const SalWheelMouseEvent& rEvt);
};

}

bool HandleWheelEvent::HandleEvent(const SalWheelMouseEvent& rEvt)
{
    if (!Setup())
        return false;

    VclPtr<vcl::Window> xMouseWindow = FindTarget();

    ImplSVData* pSVData = ImplGetSVData();

    // avoid the problem that scrolling via wheel to this point brings a widget
    // under the mouse that also accepts wheel commands, so stick with the old
    // widget if the time gap is very small
    if (shouldReusePreviousMouseWindow(pSVData->mpWinData->maLastWheelEvent, rEvt) &&
        acceptableWheelScrollTarget(pSVData->mpWinData->mpLastWheelWindow))
    {
        xMouseWindow = pSVData->mpWinData->mpLastWheelWindow;
    }

    pSVData->mpWinData->maLastWheelEvent = rEvt;

    pSVData->mpWinData->mpLastWheelWindow = Dispatch(xMouseWindow);

    return pSVData->mpWinData->mpLastWheelWindow;
}

namespace {

class HandleGestureEvent : public HandleGestureEventBase
{
public:
    HandleGestureEvent(vcl::Window *pWindow, const Point &rMousePos)
        : HandleGestureEventBase(pWindow, rMousePos)
    {
    }
    bool HandleEvent();
};

}

bool HandleGestureEvent::HandleEvent()
{
    if (!Setup())
        return false;

    vcl::Window *pTarget = FindTarget();

    bool bHandled = Dispatch(pTarget) != nullptr;
    return bHandled;
}

static bool ImplHandleWheelEvent(vcl::Window* pWindow, const SalWheelMouseEvent& rEvt)
{
    HandleWheelEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent(rEvt);
}

namespace {

class HandleGestureSwipeEvent : public HandleGestureEvent
{
private:
    CommandGestureSwipeData m_aSwipeData;
public:
    HandleGestureSwipeEvent(vcl::Window *pWindow, const SalGestureSwipeEvent& rEvt)
        : HandleGestureEvent(pWindow, Point(rEvt.mnX, rEvt.mnY)),
          m_aSwipeData(rEvt.mnVelocityX)
    {
    }
    virtual bool CallCommand(vcl::Window *pWindow, const Point &/*rMousePos*/) override
    {
        return ImplCallCommand(pWindow, CommandEventId::GestureSwipe, &m_aSwipeData);
    }
};

}

static bool ImplHandleSwipe(vcl::Window *pWindow, const SalGestureSwipeEvent& rEvt)
{
    HandleGestureSwipeEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent();
}

namespace {

class HandleGestureLongPressEvent : public HandleGestureEvent
{
private:
    CommandGestureLongPressData m_aLongPressData;
public:
    HandleGestureLongPressEvent(vcl::Window *pWindow, const SalGestureLongPressEvent& rEvt)
        : HandleGestureEvent(pWindow, Point(rEvt.mnX, rEvt.mnY)),
          m_aLongPressData(rEvt.mnX, rEvt.mnY)
    {
    }
    virtual bool CallCommand(vcl::Window *pWindow, const Point &/*rMousePos*/) override
    {
        return ImplCallCommand(pWindow, CommandEventId::GestureLongPress, &m_aLongPressData);
    }
};

}

static bool ImplHandleLongPress(vcl::Window *pWindow, const SalGestureLongPressEvent& rEvt)
{
    HandleGestureLongPressEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent();
}

namespace {

class HandleGesturePanEvent : public HandleGestureEvent
{
private:
    CommandGesturePanData m_aGestureData;

public:
    HandleGesturePanEvent(vcl::Window* pWindow, const SalGestureEvent& rEvent)
        : HandleGestureEvent(pWindow, Point(rEvent.mnX, rEvent.mnY))
        , m_aGestureData(rEvent.mnX, rEvent.mnY, rEvent.meEventType, rEvent.mfOffset, rEvent.meOrientation)
    {
    }

    virtual bool CallCommand(vcl::Window* pWindow, const Point& /*rMousePos*/) override
    {
        return ImplCallCommand(pWindow, CommandEventId::GesturePan, &m_aGestureData);
    }
};

}

static bool ImplHandleGestureEvent(vcl::Window* pWindow, const SalGestureEvent& rEvent)
{
    HandleGesturePanEvent aHandler(pWindow, rEvent);
    return aHandler.HandleEvent();
}

namespace {

class HandleGestureZoomEvent : public HandleGestureEvent
{
private:
    CommandGestureZoomData m_aGestureData;

public:
    HandleGestureZoomEvent(vcl::Window* pWindow, const SalGestureZoomEvent& rEvent)
        : HandleGestureEvent(pWindow, Point(rEvent.mnX, rEvent.mnY))
        , m_aGestureData(rEvent.mnX, rEvent.mnY, rEvent.meEventType, rEvent.mfScaleDelta)
    {
    }

    virtual bool CallCommand(vcl::Window* pWindow, const Point& /*rMousePos*/) override
    {
        return ImplCallCommand(pWindow, CommandEventId::GestureZoom, &m_aGestureData);
    }
};

}

static bool ImplHandleGestureZoomEvent(vcl::Window* pWindow, const SalGestureZoomEvent& rEvent)
{
    HandleGestureZoomEvent aHandler(pWindow, rEvent);
    return aHandler.HandleEvent();
}

namespace {

class HandleGestureRotateEvent : public HandleGestureEvent
{
private:
    CommandGestureRotateData m_aGestureData;

public:
    HandleGestureRotateEvent(vcl::Window* pWindow, const SalGestureRotateEvent& rEvent)
        : HandleGestureEvent(pWindow, Point(rEvent.mnX, rEvent.mnY))
        , m_aGestureData(rEvent.mnX, rEvent.mnY, rEvent.meEventType, rEvent.mfAngleDelta)
    {
    }

    virtual bool CallCommand(vcl::Window* pWindow, const Point& /*rMousePos*/) override
    {
        return ImplCallCommand(pWindow, CommandEventId::GestureRotate, &m_aGestureData);
    }
};

}

static bool ImplHandleGestureRotateEvent(vcl::Window* pWindow, const SalGestureRotateEvent& rEvent)
{
    HandleGestureRotateEvent aHandler(pWindow, rEvent);
    return aHandler.HandleEvent();
}

static void ImplHandlePaint( vcl::Window* pWindow, const tools::Rectangle& rBoundRect, bool bImmediateUpdate )
{
    // system paint events must be checked for re-mirroring
    pWindow->ImplGetWindowImpl()->mnPaintFlags |= ImplPaintFlags::CheckRtl;

    // trigger paint for all windows that live in the new paint region
    vcl::Region aRegion( rBoundRect );
    pWindow->ImplInvalidateOverlapFrameRegion( aRegion );
    if( bImmediateUpdate )
    {
        // #i87663# trigger possible pending resize notifications
        // (GetSizePixel does that for us)
        pWindow->GetSizePixel();
        // force drawing immediately
        pWindow->PaintImmediately();
    }
}

static void KillOwnPopups( vcl::Window const * pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window *pParent = pWindow->ImplGetWindowImpl()->mpFrameWindow;
    vcl::Window *pChild = pSVData->mpWinData->mpFirstFloat;
    if ( pChild && pParent->ImplIsWindowOrChild( pChild, true ) )
    {
        if (!(pSVData->mpWinData->mpFirstFloat->GetPopupModeFlags()
              & FloatWinPopupFlags::NoAppFocusClose))
            pSVData->mpWinData->mpFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel
                                                           | FloatWinPopupEndFlags::CloseAll);
    }
}

void ImplHandleResize( vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight )
{
    const bool bChanged = (nNewWidth != pWindow->GetOutputSizePixel().Width()) || (nNewHeight != pWindow->GetOutDev()->GetOutputHeightPixel());
    if (bChanged && pWindow->GetStyle() & (WB_MOVEABLE|WB_SIZEABLE))
    {
        KillOwnPopups( pWindow );
        if( pWindow->ImplGetWindow() != ImplGetSVHelpData().mpHelpWin )
            ImplDestroyHelpWindow( true );
    }

    if (
         (nNewWidth > 0 && nNewHeight > 0) ||
         pWindow->ImplGetWindow()->ImplGetWindowImpl()->mbAllResize
       )
    {
        if (bChanged)
        {
            pWindow->GetOutDev()->mnOutWidth  = nNewWidth;
            pWindow->GetOutDev()->mnOutHeight = nNewHeight;
            pWindow->ImplGetWindowImpl()->mbWaitSystemResize = false;
            if ( pWindow->IsReallyVisible() )
                pWindow->ImplSetClipFlag();
            if ( pWindow->IsVisible() || pWindow->ImplGetWindow()->ImplGetWindowImpl()->mbAllResize ||
                ( pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplGetWindowImpl()->mpClientWindow ) )   // propagate resize for system border windows
            {
                bool bStartTimer = true;
                // use resize buffering for user resizes
                // ownerdraw decorated windows and floating windows can be resized immediately (i.e. synchronously)
                if( pWindow->ImplGetWindowImpl()->mbFrame && (pWindow->GetStyle() & WB_SIZEABLE)
                    && !(pWindow->GetStyle() & WB_OWNERDRAWDECORATION)  // synchronous resize for ownerdraw decorated windows (toolbars)
                    && !pWindow->ImplGetWindowImpl()->mbFloatWin )             // synchronous resize for floating windows, #i43799#
                {
                    if( pWindow->ImplGetWindowImpl()->mpClientWindow )
                    {
                        // #i42750# presentation wants to be informed about resize
                        // as early as possible
                        WorkWindow* pWorkWindow = dynamic_cast<WorkWindow*>(pWindow->ImplGetWindowImpl()->mpClientWindow.get());
                        if( ! pWorkWindow || pWorkWindow->IsPresentationMode() )
                            bStartTimer = false;
                    }
                    else
                    {
                        WorkWindow* pWorkWindow = dynamic_cast<WorkWindow*>(pWindow);
                        if( ! pWorkWindow || pWorkWindow->IsPresentationMode() )
                            bStartTimer = false;
                    }
                }
                else
                    bStartTimer = false;

                if( bStartTimer )
                    pWindow->ImplGetWindowImpl()->mpFrameData->maResizeIdle.Start();
                else
                    pWindow->ImplCallResize(); // otherwise menus cannot be positioned
            }
            else
                pWindow->ImplGetWindowImpl()->mbCallResize = true;

            if (pWindow->SupportsDoubleBuffering() && pWindow->ImplGetWindowImpl()->mbFrame)
            {
                // Propagate resize for the frame's buffer.
                pWindow->ImplGetWindowImpl()->mpFrameData->mpBuffer->SetOutputSizePixel(pWindow->GetOutputSizePixel());
            }
        }
    }

    pWindow->ImplGetWindowImpl()->mpFrameData->mbNeedSysWindow = (nNewWidth < IMPL_MIN_NEEDSYSWIN) ||
                                            (nNewHeight < IMPL_MIN_NEEDSYSWIN);
    bool bMinimized = (nNewWidth <= 0) || (nNewHeight <= 0);
    if( bMinimized != pWindow->ImplGetWindowImpl()->mpFrameData->mbMinimized )
        pWindow->ImplGetWindowImpl()->mpFrameWindow->ImplNotifyIconifiedState( bMinimized );
    pWindow->ImplGetWindowImpl()->mpFrameData->mbMinimized = bMinimized;
}

static void ImplHandleMove( vcl::Window* pWindow )
{
    if( pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplIsFloatingWindow() && pWindow->IsReallyVisible() )
    {
        static_cast<FloatingWindow*>(pWindow)->EndPopupMode( FloatWinPopupEndFlags::TearOff );
        pWindow->ImplCallMove();
    }

    if( pWindow->GetStyle() & (WB_MOVEABLE|WB_SIZEABLE) )
    {
        KillOwnPopups( pWindow );
        if( pWindow->ImplGetWindow() != ImplGetSVHelpData().mpHelpWin )
            ImplDestroyHelpWindow( true );
    }

    if ( pWindow->IsVisible() )
        pWindow->ImplCallMove();
    else
        pWindow->ImplGetWindowImpl()->mbCallMove = true; // make sure the framepos will be updated on the next Show()

    if ( pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplGetWindowImpl()->mpClientWindow )
        pWindow->ImplGetWindowImpl()->mpClientWindow->ImplCallMove();   // notify client to update geometry

}

static void ImplHandleMoveResize( vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight )
{
    ImplHandleMove( pWindow );
    ImplHandleResize( pWindow, nNewWidth, nNewHeight );
}

static void ImplActivateFloatingWindows( vcl::Window const * pWindow, bool bActive )
{
    // First check all overlapping windows
    vcl::Window* pTempWindow = pWindow->ImplGetWindowImpl()->mpFirstOverlap;
    while ( pTempWindow )
    {
        if ( pTempWindow->GetActivateMode() == ActivateModeFlags::NONE )
        {
            if ( (pTempWindow->GetType() == WindowType::BORDERWINDOW) &&
                 (pTempWindow->ImplGetWindow()->GetType() == WindowType::FLOATINGWINDOW) )
                static_cast<ImplBorderWindow*>(pTempWindow)->SetDisplayActive( bActive );
        }

        ImplActivateFloatingWindows( pTempWindow, bActive );
        pTempWindow = pTempWindow->ImplGetWindowImpl()->mpNext;
    }
}

IMPL_LINK_NOARG(vcl::Window, ImplAsyncFocusHdl, void*, void)
{
    if (!ImplGetWindowImpl() || !ImplGetWindowImpl()->mpFrameData)
        return;

    ImplGetWindowImpl()->mpFrameData->mnFocusId = nullptr;

    // If the status has been preserved, because we got back the focus
    // in the meantime, we do nothing
    bool bHasFocus = ImplGetWindowImpl()->mpFrameData->mbHasFocus || ImplGetWindowImpl()->mpFrameData->mbSysObjFocus;

    // next execute the delayed functions
    if ( bHasFocus )
    {
        // redraw all floating windows inactive
        if ( ImplGetWindowImpl()->mpFrameData->mbStartFocusState != bHasFocus )
            ImplActivateFloatingWindows( this, bHasFocus );

        if ( ImplGetWindowImpl()->mpFrameData->mpFocusWin )
        {
            bool bHandled = false;
            if ( ImplGetWindowImpl()->mpFrameData->mpFocusWin->IsInputEnabled() &&
                 ! ImplGetWindowImpl()->mpFrameData->mpFocusWin->IsInModalMode() )
            {
                if ( ImplGetWindowImpl()->mpFrameData->mpFocusWin->IsEnabled() )
                {
                    ImplGetWindowImpl()->mpFrameData->mpFocusWin->GrabFocus();
                    bHandled = true;
                }
                else if( ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplHasDlgCtrl() )
                {
                // #109094# if the focus is restored to a disabled dialog control (was disabled meanwhile)
                // try to move it to the next control
                    ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplDlgCtrlNextWindow();
                    bHandled = true;
                }
            }
            if ( !bHandled )
            {
                ImplSVData* pSVData = ImplGetSVData();
                vcl::Window*     pTopLevelWindow = ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

                if ((!pTopLevelWindow->IsInputEnabled() || pTopLevelWindow->IsInModalMode())
                    && !pSVData->mpWinData->mpExecuteDialogs.empty())
                    pSVData->mpWinData->mpExecuteDialogs.back()->ToTop(ToTopFlags::RestoreWhenMin | ToTopFlags::GrabFocusOnly);
                else
                    pTopLevelWindow->GrabFocus();
            }
        }
        else
            GrabFocus();
    }
    else
    {
        vcl::Window* pFocusWin = ImplGetWindowImpl()->mpFrameData->mpFocusWin;
        if ( pFocusWin )
        {
            ImplSVData* pSVData = ImplGetSVData();

            if (pSVData->mpWinData->mpFocusWin == pFocusWin)
            {
                // transfer the FocusWindow
                vcl::Window* pOverlapWindow = pFocusWin->ImplGetFirstOverlapWindow();
                if ( pOverlapWindow && pOverlapWindow->ImplGetWindowImpl() )
                    pOverlapWindow->ImplGetWindowImpl()->mpLastFocusWindow = pFocusWin;
                pSVData->mpWinData->mpFocusWin = nullptr;

                if ( pFocusWin->ImplGetWindowImpl() && pFocusWin->ImplGetWindowImpl()->mpCursor )
                    pFocusWin->ImplGetWindowImpl()->mpCursor->ImplHide();

                // call the Deactivate
                vcl::Window* pOldOverlapWindow = pFocusWin->ImplGetFirstOverlapWindow();
                vcl::Window* pOldRealWindow = pOldOverlapWindow->ImplGetWindow();

                if (pOldOverlapWindow && pOldOverlapWindow->ImplGetWindowImpl() &&
                    pOldRealWindow && pOldRealWindow->ImplGetWindowImpl())
                {
                    pOldOverlapWindow->ImplGetWindowImpl()->mbActive = false;
                    pOldOverlapWindow->Deactivate();
                    if ( pOldRealWindow != pOldOverlapWindow )
                    {
                        pOldRealWindow->ImplGetWindowImpl()->mbActive = false;
                        pOldRealWindow->Deactivate();
                    }
                }

                // TrackingMode is ended in ImplHandleLoseFocus
#ifdef _WIN32
                // To avoid problems with the Unix IME
                pFocusWin->EndExtTextInput();
#endif

                NotifyEvent aNEvt(NotifyEventType::LOSEFOCUS, pFocusWin);
                if (!ImplCallPreNotify(aNEvt))
                    pFocusWin->CompatLoseFocus();
                pFocusWin->ImplCallDeactivateListeners(nullptr);
            }
        }

        // Redraw all floating window inactive
        if ( ImplGetWindowImpl()->mpFrameData->mbStartFocusState != bHasFocus )
            ImplActivateFloatingWindows( this, bHasFocus );
    }
}

static void ImplHandleGetFocus( vcl::Window* pWindow )
{
    if (!pWindow || !pWindow->ImplGetWindowImpl() || !pWindow->ImplGetWindowImpl()->mpFrameData)
        return;

    pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus = true;

    // execute Focus-Events after a delay, such that SystemChildWindows
    // do not blink when they receive focus
    if ( !pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId )
    {
        pWindow->ImplGetWindowImpl()->mpFrameData->mbStartFocusState = !pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus;
        pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId = Application::PostUserEvent( LINK( pWindow, vcl::Window, ImplAsyncFocusHdl ), nullptr, true);
        vcl::Window* pFocusWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
        if ( pFocusWin && pFocusWin->ImplGetWindowImpl()->mpCursor )
            pFocusWin->ImplGetWindowImpl()->mpCursor->ImplShow();
    }
}

static void ImplHandleLoseFocus( vcl::Window* pWindow )
{
    if (!pWindow)
        return;

    ImplSVData* pSVData = ImplGetSVData();

    // Abort the autoscroll if the frame loses focus
    if (pSVData->mpWinData->mpAutoScrollWin)
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();

    // Abort tracking if the frame loses focus
    if (pSVData->mpWinData->mpTrackWin)
    {
        if (pSVData->mpWinData->mpTrackWin->ImplGetWindowImpl() &&
            pSVData->mpWinData->mpTrackWin->ImplGetWindowImpl()->mpFrameWindow == pWindow)
            pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel);
    }

    if (pWindow->ImplGetWindowImpl() && pWindow->ImplGetWindowImpl()->mpFrameData)
    {
        pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus = false;

        // execute Focus-Events after a delay, such that SystemChildWindows
        // do not flicker when they receive focus
        if ( !pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId )
        {
            pWindow->ImplGetWindowImpl()->mpFrameData->mbStartFocusState = !pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus;
            pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId = Application::PostUserEvent( LINK( pWindow, vcl::Window, ImplAsyncFocusHdl ), nullptr, true );
        }

        vcl::Window* pFocusWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
        if ( pFocusWin && pFocusWin->ImplGetWindowImpl()->mpCursor )
            pFocusWin->ImplGetWindowImpl()->mpCursor->ImplHide();
    }

    // Make sure that no menu is visible when a toplevel window loses focus.
    VclPtr<FloatingWindow> pFirstFloat = pSVData->mpWinData->mpFirstFloat;
    if (pFirstFloat && pFirstFloat->IsMenuFloatingWindow() && !pWindow->GetParent())
    {
        pFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll);
    }
}

namespace {

struct DelayedCloseEvent
{
    VclPtr<vcl::Window> pWindow;
};

}

static void DelayedCloseEventLink( void* pCEvent, void* )
{
    DelayedCloseEvent* pEv = static_cast<DelayedCloseEvent*>(pCEvent);

    if( ! pEv->pWindow->isDisposed() )
    {
        // dispatch to correct window type
        if( pEv->pWindow->IsSystemWindow() )
            static_cast<SystemWindow*>(pEv->pWindow.get())->Close();
        else if( pEv->pWindow->IsDockingWindow() )
            static_cast<DockingWindow*>(pEv->pWindow.get())->Close();
    }
    delete pEv;
}

static void ImplHandleClose( const vcl::Window* pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();

    bool bWasPopup = false;
    if( pWindow->ImplIsFloatingWindow() &&
        static_cast<const FloatingWindow*>(pWindow)->ImplIsInPrivatePopupMode() )
    {
        bWasPopup = true;
    }

    // on Close stop all floating modes and end popups
    if (pSVData->mpWinData->mpFirstFloat)
    {
        FloatingWindow* pLastLevelFloat;
        pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
        pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
    }
    if ( ImplGetSVHelpData().mbExtHelpMode )
        Help::EndExtHelp();
    if ( ImplGetSVHelpData().mpHelpWin )
        ImplDestroyHelpWindow( false );
    // AutoScrollMode
    if (pSVData->mpWinData->mpAutoScrollWin)
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();

    if (pSVData->mpWinData->mpTrackWin)
        pSVData->mpWinData->mpTrackWin->EndTracking( TrackingEventFlags::Cancel | TrackingEventFlags::Key );

    if (bWasPopup)
        return;

    vcl::Window *pWin = pWindow->ImplGetWindow();
    SystemWindow* pSysWin = dynamic_cast<SystemWindow*>(pWin);
    if (pSysWin)
    {
        // See if the custom close handler is set.
        const Link<SystemWindow&,void>& rLink = pSysWin->GetCloseHdl();
        if (rLink.IsSet())
        {
            rLink.Call(*pSysWin);
            return;
        }
    }

    // check whether close is allowed
    if ( pWin->IsEnabled() && pWin->IsInputEnabled() && !pWin->IsInModalMode() )
    {
        DelayedCloseEvent* pEv = new DelayedCloseEvent;
        pEv->pWindow = pWin;
        Application::PostUserEvent( LINK_NONMEMBER( pEv, DelayedCloseEventLink ) );
    }
}

static void ImplHandleUserEvent( ImplSVEvent* pSVEvent )
{
    if ( pSVEvent )
    {
        if ( pSVEvent->mbCall )
        {
            pSVEvent->maLink.Call( pSVEvent->mpData );
        }

        delete pSVEvent;
    }
}

MouseEventModifiers ImplGetMouseMoveMode( SalMouseEvent const * pEvent )
{
    MouseEventModifiers nMode = MouseEventModifiers::NONE;
    if ( !pEvent->mnCode )
        nMode |= MouseEventModifiers::SIMPLEMOVE;
    if ( (pEvent->mnCode & MOUSE_LEFT) && !(pEvent->mnCode & KEY_MOD1) )
        nMode |= MouseEventModifiers::DRAGMOVE;
    if ( (pEvent->mnCode & MOUSE_LEFT) && (pEvent->mnCode & KEY_MOD1) )
        nMode |= MouseEventModifiers::DRAGCOPY;
    return nMode;
}

MouseEventModifiers ImplGetMouseButtonMode( SalMouseEvent const * pEvent )
{
    MouseEventModifiers nMode = MouseEventModifiers::NONE;
    if ( pEvent->mnButton == MOUSE_LEFT )
        nMode |= MouseEventModifiers::SIMPLECLICK;
    if ( (pEvent->mnButton == MOUSE_LEFT) && !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT)) )
        nMode |= MouseEventModifiers::SELECT;
    if ( (pEvent->mnButton == MOUSE_LEFT) && (pEvent->mnCode & KEY_MOD1) &&
         !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT | KEY_SHIFT)) )
        nMode |= MouseEventModifiers::MULTISELECT;
    if ( (pEvent->mnButton == MOUSE_LEFT) && (pEvent->mnCode & KEY_SHIFT) &&
         !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT | KEY_MOD1)) )
        nMode |= MouseEventModifiers::RANGESELECT;
    return nMode;
}

static bool ImplHandleSalMouseLeave( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEMOVE, true,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime, pEvent->mnCode,
                                 ImplGetMouseMoveMode( pEvent ) );
}

static bool ImplHandleSalMouseMove( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEMOVE, false,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime, pEvent->mnCode,
                                 ImplGetMouseMoveMode( pEvent ) );
}

static bool ImplHandleSalMouseButtonDown( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEBUTTONDOWN, false,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime,
#ifdef MACOSX
                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3)),
#else
                                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2)),
#endif
                                 ImplGetMouseButtonMode( pEvent ) );
}

static bool ImplHandleSalMouseButtonUp( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEBUTTONUP, false,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime,
#ifdef MACOSX
                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3)),
#else
                                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2)),
#endif
                                 ImplGetMouseButtonMode( pEvent ) );
}

static bool ImplHandleMenuEvent( vcl::Window const * pWindow, SalMenuEvent* pEvent, SalEvent nEvent )
{
    // Find SystemWindow and its Menubar and let it dispatch the command
    bool bRet = false;
    vcl::Window *pWin = pWindow->ImplGetWindowImpl()->mpFirstChild;
    while ( pWin )
    {
        if ( pWin->ImplGetWindowImpl()->mbSysWin )
            break;
        pWin = pWin->ImplGetWindowImpl()->mpNext;
    }
    if( pWin )
    {
        MenuBar *pMenuBar = static_cast<SystemWindow*>(pWin)->GetMenuBar();
        if( pMenuBar )
        {
            switch( nEvent )
            {
                case SalEvent::MenuActivate:
                    pMenuBar->HandleMenuActivateEvent( static_cast<Menu*>(pEvent->mpMenu) );
                    bRet = true;
                    break;
                case SalEvent::MenuDeactivate:
                    pMenuBar->HandleMenuDeActivateEvent( static_cast<Menu*>(pEvent->mpMenu) );
                    bRet = true;
                    break;
                case SalEvent::MenuHighlight:
                    bRet = pMenuBar->HandleMenuHighlightEvent( static_cast<Menu*>(pEvent->mpMenu), pEvent->mnId );
                    break;
                case SalEvent::MenuButtonCommand:
                    bRet = pMenuBar->HandleMenuButtonEvent( pEvent->mnId );
                    break;
                case SalEvent::MenuCommand:
                    bRet = pMenuBar->HandleMenuCommandEvent( static_cast<Menu*>(pEvent->mpMenu), pEvent->mnId );
                    break;
                default:
                    break;
            }
        }
    }
    return bRet;
}

static void ImplHandleSalKeyMod( vcl::Window* pWindow, SalKeyModEvent const * pEvent )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pTrackWin = pSVData->mpWinData->mpTrackWin;
    if ( pTrackWin )
        pWindow = pTrackWin;
#ifdef MACOSX
    sal_uInt16 nOldCode = pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3);
#else
    sal_uInt16 nOldCode = pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2);
#endif
    sal_uInt16 nNewCode = pEvent->mnCode;
    if ( nOldCode != nNewCode )
    {
#ifdef MACOSX
        nNewCode |= pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & ~(KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3);
#else
        nNewCode |= pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & ~(KEY_SHIFT | KEY_MOD1 | KEY_MOD2);
#endif
        pWindow->ImplGetWindowImpl()->mpFrameWindow->ImplCallMouseMove( nNewCode, true );
    }

    // #105224# send commandevent to allow special treatment of Ctrl-LeftShift/Ctrl-RightShift etc.
    // + auto-accelerator feature, tdf#92630

    // try to find a key input window...
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );
    //...otherwise fail safe...
    if (!pChild)
        pChild = pWindow;

    CommandModKeyData data( pEvent->mnModKeyCode, pEvent->mbDown );
    ImplCallCommand( pChild, CommandEventId::ModKeyChange, &data );
}

static void ImplHandleInputLanguageChange( vcl::Window* pWindow )
{
    // find window
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );
    if ( !pChild )
        return;

    ImplCallCommand( pChild, CommandEventId::InputLanguageChange );
}

static void ImplHandleSalSettings( SalEvent nEvent )
{
    Application* pApp = GetpApp();
    if ( !pApp )
        return;

    if ( nEvent == SalEvent::SettingsChanged )
    {
        AllSettings aSettings = Application::GetSettings();
        Application::MergeSystemSettings( aSettings );
        pApp->OverrideSystemSettings( aSettings );
        Application::SetSettings( aSettings );
    }
    else
    {
        DataChangedEventType nType;
        switch ( nEvent )
        {
            case SalEvent::PrinterChanged:
                ImplDeletePrnQueueList();
                nType = DataChangedEventType::PRINTER;
                break;
            case SalEvent::DisplayChanged:
                nType = DataChangedEventType::DISPLAY;
                break;
            case SalEvent::FontChanged:
                OutputDevice::ImplUpdateAllFontData( true );
                nType = DataChangedEventType::FONTS;
                break;
            default:
                nType = DataChangedEventType::NONE;
                break;
        }

        if ( nType != DataChangedEventType::NONE )
        {
            DataChangedEvent aDCEvt( nType );
            Application::ImplCallEventListenersApplicationDataChanged(&aDCEvt);
            Application::NotifyAllWindows( aDCEvt );
        }
    }
}

static void ImplHandleSalExtTextInputPos( vcl::Window* pWindow, SalExtTextInputPosEvent* pEvt )
{
    tools::Rectangle aCursorRect;
    ImplHandleExtTextInputPos( pWindow, aCursorRect, pEvt->mnExtWidth, &pEvt->mbVertical );
    if ( aCursorRect.IsEmpty() )
    {
        pEvt->mnX       = -1;
        pEvt->mnY       = -1;
        pEvt->mnWidth   = -1;
        pEvt->mnHeight  = -1;
    }
    else
    {
        pEvt->mnX       = aCursorRect.Left();
        pEvt->mnY       = aCursorRect.Top();
        pEvt->mnWidth   = aCursorRect.GetWidth();
        pEvt->mnHeight  = aCursorRect.GetHeight();
    }
}

static bool ImplHandleShowDialog( vcl::Window* pWindow, ShowDialogId nDialogId )
{
    if( ! pWindow )
        return false;

    if( pWindow->GetType() == WindowType::BORDERWINDOW )
    {
        vcl::Window* pWrkWin = pWindow->GetWindow( GetWindowType::Client );
        if( pWrkWin )
            pWindow = pWrkWin;
    }
    CommandDialogData aCmdData( nDialogId );
    return ImplCallCommand( pWindow, CommandEventId::ShowDialog, &aCmdData );
}

static void ImplHandleSurroundingTextRequest( vcl::Window *pWindow,
                          OUString& rText,
                          Selection &rSelRange )
{
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );

    if ( !pChild )
    {
        rText.clear();
        rSelRange.setMin( 0 );
        rSelRange.setMax( 0 );
    }
    else
    {
        rText = pChild->GetSurroundingText();
        Selection aSel = pChild->GetSurroundingTextSelection();
        rSelRange.setMin( aSel.Min() );
        rSelRange.setMax( aSel.Max() );
    }
}

static void ImplHandleSalSurroundingTextRequest( vcl::Window *pWindow,
                         SalSurroundingTextRequestEvent *pEvt )
{
    Selection aSelRange;
    ImplHandleSurroundingTextRequest( pWindow, pEvt->maText, aSelRange );

    aSelRange.Normalize();

    if( aSelRange.Min() < 0 )
        pEvt->mnStart = 0;
    else if( aSelRange.Min() > pEvt->maText.getLength() )
        pEvt->mnStart = pEvt->maText.getLength();
    else
        pEvt->mnStart = aSelRange.Min();

    if( aSelRange.Max() < 0 )
        pEvt->mnStart = 0;
    else if( aSelRange.Max() > pEvt->maText.getLength() )
        pEvt->mnEnd = pEvt->maText.getLength();
    else
        pEvt->mnEnd = aSelRange.Max();
}

static void ImplHandleSalDeleteSurroundingTextRequest( vcl::Window *pWindow,
                         SalSurroundingTextSelectionChangeEvent *pEvt )
{
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );

    Selection aSelection(pEvt->mnStart, pEvt->mnEnd);
    if (pChild && pChild->DeleteSurroundingText(aSelection))
    {
        pEvt->mnStart = aSelection.Min();
        pEvt->mnEnd = aSelection.Max();
    }
    else
    {
        pEvt->mnStart = pEvt->mnEnd = SAL_MAX_UINT32;
    }
}

static void ImplHandleSurroundingTextSelectionChange( vcl::Window *pWindow,
                              sal_uLong nStart,
                              sal_uLong nEnd )
{
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );
    if( pChild )
    {
        CommandSelectionChangeData data( nStart, nEnd );
        ImplCallCommand( pChild, CommandEventId::SelectionChange, &data );
    }
}

static void ImplHandleStartReconversion( vcl::Window *pWindow )
{
    vcl::Window* pChild = ImplGetKeyInputWindow( pWindow );
    if( pChild )
        ImplCallCommand( pChild, CommandEventId::PrepareReconversion );
}

static void ImplHandleSalQueryCharPosition( vcl::Window *pWindow,
                                            SalQueryCharPositionEvent *pEvt )
{
    pEvt->mbValid = false;
    pEvt->mbVertical = false;
    pEvt->maCursorBound = AbsoluteScreenPixelRectangle();

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;

    if ( !pChild )
        pChild = ImplGetKeyInputWindow( pWindow );
    else
    {
        // Test, if the Window is related to the frame
        if ( !pWindow->ImplIsWindowOrChild( pChild ) )
            pChild = ImplGetKeyInputWindow( pWindow );
    }

    if( !pChild )
        return;

    ImplCallCommand( pChild, CommandEventId::QueryCharPosition );

    ImplWinData* pWinData = pChild->ImplGetWinData();
    if ( !(pWinData->mpCompositionCharRects && pEvt->mnCharPos < o3tl::make_unsigned( pWinData->mnCompositionCharRects )) )
        return;

    const OutputDevice *pChildOutDev = pChild->GetOutDev();
    const tools::Rectangle& aRect = pWinData->mpCompositionCharRects[ pEvt->mnCharPos ];
    tools::Rectangle aDeviceRect = pChildOutDev->ImplLogicToDevicePixel( aRect );
    AbsoluteScreenPixelPoint aAbsScreenPos = pChild->OutputToAbsoluteScreenPixel( pChild->ScreenToOutputPixel(aDeviceRect.TopLeft()) );
    pEvt->maCursorBound = AbsoluteScreenPixelRectangle(aAbsScreenPos, aDeviceRect.GetSize());
    pEvt->mbVertical = pWinData->mbVertical;
    pEvt->mbValid = true;
}

bool ImplWindowFrameProc( vcl::Window* _pWindow, SalEvent nEvent, const void* pEvent )
{
    DBG_TESTSOLARMUTEX();

    // Ensure the window survives during this method.
    VclPtr<vcl::Window> pWindow( _pWindow );

    bool bRet = false;

    // #119709# for some unknown reason it is possible to receive events (in this case key events)
    // although the corresponding VCL window must have been destroyed already
    // at least ImplGetWindowImpl() was NULL in these cases, so check this here
    if( pWindow->ImplGetWindowImpl() == nullptr )
        return false;

    switch ( nEvent )
    {
        case SalEvent::MouseMove:
            bRet = ImplHandleSalMouseMove( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::ExternalMouseMove:
        {
            MouseEvent const * pMouseEvt = static_cast<MouseEvent const *>(pEvent);
            SalMouseEvent   aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = 0;
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = ImplHandleSalMouseMove( pWindow, &aSalMouseEvent );
        }
        break;
        case SalEvent::MouseLeave:
            bRet = ImplHandleSalMouseLeave( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::MouseButtonDown:
            bRet = ImplHandleSalMouseButtonDown( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::ExternalMouseButtonDown:
        {
            MouseEvent const * pMouseEvt = static_cast<MouseEvent const *>(pEvent);
            SalMouseEvent   aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = pMouseEvt->GetButtons();
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = ImplHandleSalMouseButtonDown( pWindow, &aSalMouseEvent );
        }
        break;
        case SalEvent::MouseButtonUp:
            bRet = ImplHandleSalMouseButtonUp( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::ExternalMouseButtonUp:
        {
            MouseEvent const * pMouseEvt = static_cast<MouseEvent const *>(pEvent);
            SalMouseEvent   aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = pMouseEvt->GetButtons();
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = ImplHandleSalMouseButtonUp( pWindow, &aSalMouseEvent );
        }
        break;
        case SalEvent::MouseActivate:
            bRet = false;
            break;
        case SalEvent::KeyInput:
            {
            SalKeyEvent const * pKeyEvt = static_cast<SalKeyEvent const *>(pEvent);
            bRet = ImplHandleKey( pWindow, NotifyEventType::KEYINPUT,
                pKeyEvt->mnCode, pKeyEvt->mnCharCode, pKeyEvt->mnRepeat, true );
            }
            break;
        case SalEvent::ExternalKeyInput:
            {
            KeyEvent const * pKeyEvt = static_cast<KeyEvent const *>(pEvent);
            bRet = ImplHandleKey( pWindow, NotifyEventType::KEYINPUT,
                pKeyEvt->GetKeyCode().GetFullCode(), pKeyEvt->GetCharCode(), pKeyEvt->GetRepeat(), false );
            }
            break;
        case SalEvent::KeyUp:
            {
            SalKeyEvent const * pKeyEvt = static_cast<SalKeyEvent const *>(pEvent);
            bRet = ImplHandleKey( pWindow, NotifyEventType::KEYUP,
                pKeyEvt->mnCode, pKeyEvt->mnCharCode, pKeyEvt->mnRepeat, true );
            }
            break;
        case SalEvent::ExternalKeyUp:
            {
            KeyEvent const * pKeyEvt = static_cast<KeyEvent const *>(pEvent);
            bRet = ImplHandleKey( pWindow, NotifyEventType::KEYUP,
                pKeyEvt->GetKeyCode().GetFullCode(), pKeyEvt->GetCharCode(), pKeyEvt->GetRepeat(), false );
            }
            break;
        case SalEvent::KeyModChange:
            ImplHandleSalKeyMod( pWindow, static_cast<SalKeyModEvent const *>(pEvent) );
            break;

        case SalEvent::InputLanguageChange:
            ImplHandleInputLanguageChange( pWindow );
            break;

        case SalEvent::MenuActivate:
        case SalEvent::MenuDeactivate:
        case SalEvent::MenuHighlight:
        case SalEvent::MenuCommand:
        case SalEvent::MenuButtonCommand:
            bRet = ImplHandleMenuEvent( pWindow, const_cast<SalMenuEvent *>(static_cast<SalMenuEvent const *>(pEvent)), nEvent );
            break;

        case SalEvent::WheelMouse:
            bRet = ImplHandleWheelEvent( pWindow, *static_cast<const SalWheelMouseEvent*>(pEvent));
            break;

        case SalEvent::Paint:
            {
            SalPaintEvent const * pPaintEvt = static_cast<SalPaintEvent const *>(pEvent);

            if( AllSettings::GetLayoutRTL() )
            {
                SalFrame* pSalFrame = pWindow->ImplGetWindowImpl()->mpFrame;
                const_cast<SalPaintEvent *>(pPaintEvt)->mnBoundX = pSalFrame->GetWidth() - pPaintEvt->mnBoundWidth - pPaintEvt->mnBoundX;
            }

            tools::Rectangle aBoundRect( Point( pPaintEvt->mnBoundX, pPaintEvt->mnBoundY ),
                                  Size( pPaintEvt->mnBoundWidth, pPaintEvt->mnBoundHeight ) );
            ImplHandlePaint( pWindow, aBoundRect, pPaintEvt->mbImmediateUpdate );
            }
            break;

        case SalEvent::Move:
            ImplHandleMove( pWindow );
            break;

        case SalEvent::Resize:
            {
            tools::Long nNewWidth;
            tools::Long nNewHeight;
            pWindow->ImplGetWindowImpl()->mpFrame->GetClientSize( nNewWidth, nNewHeight );
            ImplHandleResize( pWindow, nNewWidth, nNewHeight );
            }
            break;

        case SalEvent::MoveResize:
            {
            SalFrameGeometry g = pWindow->ImplGetWindowImpl()->mpFrame->GetGeometry();
            ImplHandleMoveResize(pWindow, g.width(), g.height());
            }
            break;

        case SalEvent::ClosePopups:
            {
            KillOwnPopups( pWindow );
            }
            break;

        case SalEvent::GetFocus:
            ImplHandleGetFocus( pWindow );
            break;
        case SalEvent::LoseFocus:
            ImplHandleLoseFocus( pWindow );
            break;

        case SalEvent::Close:
            ImplHandleClose( pWindow );
            break;

        case SalEvent::Shutdown:
            {
                static bool bInQueryExit = false;
                if( !bInQueryExit )
                {
                    bInQueryExit = true;
                    if ( GetpApp()->QueryExit() )
                    {
                        // end the message loop
                        Application::Quit();
                        return false;
                    }
                    else
                    {
                        bInQueryExit = false;
                        return true;
                    }
                }
                return false;
            }

        case SalEvent::SettingsChanged:
        case SalEvent::PrinterChanged:
        case SalEvent::DisplayChanged:
        case SalEvent::FontChanged:
            ImplHandleSalSettings( nEvent );
            break;

        case SalEvent::UserEvent:
            ImplHandleUserEvent( const_cast<ImplSVEvent *>(static_cast<ImplSVEvent const *>(pEvent)) );
            break;

        case SalEvent::ExtTextInput:
            {
            SalExtTextInputEvent const * pEvt = static_cast<SalExtTextInputEvent const *>(pEvent);
            bRet = ImplHandleExtTextInput( pWindow,
                                           pEvt->maText, pEvt->mpTextAttr,
                                           pEvt->mnCursorPos, pEvt->mnCursorFlags );
            }
            break;
        case SalEvent::EndExtTextInput:
            bRet = ImplHandleEndExtTextInput();
            break;
        case SalEvent::ExtTextInputPos:
            ImplHandleSalExtTextInputPos( pWindow, const_cast<SalExtTextInputPosEvent *>(static_cast<SalExtTextInputPosEvent const *>(pEvent)) );
            break;
        case SalEvent::InputContextChange:
            bRet = ImplHandleInputContextChange( pWindow );
            break;
        case SalEvent::ShowDialog:
            {
                ShowDialogId nLOKWindowId = static_cast<ShowDialogId>(reinterpret_cast<sal_IntPtr>(pEvent));
                bRet = ImplHandleShowDialog( pWindow, nLOKWindowId );
            }
            break;
        case SalEvent::SurroundingTextRequest:
            ImplHandleSalSurroundingTextRequest( pWindow, const_cast<SalSurroundingTextRequestEvent *>(static_cast<SalSurroundingTextRequestEvent const *>(pEvent)) );
            break;
        case SalEvent::DeleteSurroundingTextRequest:
            ImplHandleSalDeleteSurroundingTextRequest( pWindow, const_cast<SalSurroundingTextSelectionChangeEvent *>(static_cast<SalSurroundingTextSelectionChangeEvent const *>(pEvent)) );
            break;
        case SalEvent::SurroundingTextSelectionChange:
        {
            SalSurroundingTextSelectionChangeEvent const * pEvt
             = static_cast<SalSurroundingTextSelectionChangeEvent const *>(pEvent);
            ImplHandleSurroundingTextSelectionChange( pWindow,
                              pEvt->mnStart,
                              pEvt->mnEnd );
            [[fallthrough]]; // TODO: Fallthrough really intended?
        }
        case SalEvent::StartReconversion:
            ImplHandleStartReconversion( pWindow );
            break;

        case SalEvent::QueryCharPosition:
            ImplHandleSalQueryCharPosition( pWindow, const_cast<SalQueryCharPositionEvent *>(static_cast<SalQueryCharPositionEvent const *>(pEvent)) );
            break;

        case SalEvent::GestureSwipe:
            bRet = ImplHandleSwipe(pWindow, *static_cast<const SalGestureSwipeEvent*>(pEvent));
            break;

        case SalEvent::GestureLongPress:
            bRet = ImplHandleLongPress(pWindow, *static_cast<const SalGestureLongPressEvent*>(pEvent));
            break;

        case SalEvent::ExternalGesture:
        {
            auto const * pGestureEvent = static_cast<GestureEventPan const *>(pEvent);

            SalGestureEvent aSalGestureEvent;
            aSalGestureEvent.mfOffset = pGestureEvent->mnOffset;
            aSalGestureEvent.mnX = pGestureEvent->mnX;
            aSalGestureEvent.mnY = pGestureEvent->mnY;
            aSalGestureEvent.meEventType = pGestureEvent->meEventType;
            aSalGestureEvent.meOrientation = pGestureEvent->meOrientation;

            bRet = ImplHandleGestureEvent(pWindow, aSalGestureEvent);
        }
        break;
        case SalEvent::GesturePan:
        {
            auto const * aSalGestureEvent = static_cast<SalGestureEvent const *>(pEvent);
            bRet = ImplHandleGestureEvent(pWindow, *aSalGestureEvent);
        }
        break;
        case SalEvent::GestureZoom:
        {
            const auto * pGestureEvent = static_cast<SalGestureZoomEvent const *>(pEvent);
            bRet = ImplHandleGestureZoomEvent(pWindow, *pGestureEvent);
        }
        break;
        case SalEvent::GestureRotate:
        {
            const auto * pGestureEvent = static_cast<SalGestureRotateEvent const *>(pEvent);
            bRet = ImplHandleGestureRotateEvent(pWindow, *pGestureEvent);
        }
        break;
        default:
            SAL_WARN( "vcl.layout", "ImplWindowFrameProc(): unknown event (" << static_cast<int>(nEvent) << ")" );
            break;
    }

    return bRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
