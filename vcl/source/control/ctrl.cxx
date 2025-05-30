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

#include <comphelper/lok.hxx>

#include <vcl/DocWindow.hxx>
#include <vcl/ctrl.hxx>
#include <vcl/decoview.hxx>
#include <vcl/event.hxx>
#include <vcl/mnemonic.hxx>

#include <svdata.hxx>
#include <textlayout.hxx>

using namespace vcl;

void Control::ImplInitControlData()
{
    mbHasControlFocus       = false;
    mbShowAccelerator       = false;
}

Control::Control(WindowType eType)
    : Window(eType)
{
    ImplInitControlData();
}

Control::Control(vcl::Window* pParent, WinBits eStyle)
    : Window(WindowType::CONTROL)
{
    ImplInitControlData();
    ImplInit(pParent, eStyle, nullptr);
}

Control::~Control()
{
    disposeOnce();
}

void Control::dispose()
{
    mxLayoutData.reset();
    mpReferenceDevice.reset();
    Window::dispose();
}

void Control::EnableRTL( bool bEnable )
{
    // convenience: for controls also switch layout mode
    GetOutDev()->SetLayoutMode( bEnable ? vcl::text::ComplexTextLayoutFlags::BiDiRtl | vcl::text::ComplexTextLayoutFlags::TextOriginLeft :
                                vcl::text::ComplexTextLayoutFlags::TextOriginLeft );
    CompatStateChanged( StateChangedType::Mirroring );
    Window::EnableRTL(bEnable);
}

void Control::Resize()
{
    ImplClearLayoutData();
    Window::Resize();
}

void Control::FillLayoutData() const
{
}

void Control::CreateLayoutData() const
{
    SAL_WARN_IF( mxLayoutData, "vcl", "Control::CreateLayoutData: should be called with non-existent layout data only!" );
    mxLayoutData.emplace();
}

bool Control::HasLayoutData() const
{
    return bool(mxLayoutData);
}

void Control::SetText( const OUString& rStr )
{
    ImplClearLayoutData();
    Window::SetText( rStr );
}

ControlLayoutData::ControlLayoutData() : m_pParent( nullptr )
{
}

tools::Rectangle ControlLayoutData::GetCharacterBounds( tools::Long nIndex ) const
{
    return (nIndex >= 0 && o3tl::make_unsigned(nIndex) < m_aUnicodeBoundRects.size()) ? m_aUnicodeBoundRects[ nIndex ] : tools::Rectangle();
}

tools::Rectangle Control::GetCharacterBounds( tools::Long nIndex ) const
{
    if( !HasLayoutData() )
        FillLayoutData();
    return mxLayoutData ? mxLayoutData->GetCharacterBounds( nIndex ) : tools::Rectangle();
}

tools::Long ControlLayoutData::GetIndexForPoint( const Point& rPoint ) const
{
    tools::Long nIndex = -1;
    for( tools::Long i = m_aUnicodeBoundRects.size()-1; i >= 0; i-- )
    {
        Point aTopLeft = m_aUnicodeBoundRects[i].TopLeft();
        Point aBottomRight = m_aUnicodeBoundRects[i].BottomRight();
        if (rPoint.X() >= aTopLeft.X() && rPoint.Y() >= aTopLeft.Y() &&
            rPoint.X() <= aBottomRight.X() && rPoint.Y() <= aBottomRight.Y())
        {
            nIndex = i;
            break;
        }
    }
    return nIndex;
}

tools::Long Control::GetIndexForPoint( const Point& rPoint ) const
{
    if( ! HasLayoutData() )
        FillLayoutData();
    return mxLayoutData ? mxLayoutData->GetIndexForPoint( rPoint ) : -1;
}

Pair ControlLayoutData::GetLineStartEnd( tools::Long nLine ) const
{
    Pair aPair( -1, -1 );

    int nDisplayLines = m_aLineIndices.size();
    if( nLine >= 0 && nLine < nDisplayLines )
    {
        aPair.A() = m_aLineIndices[nLine];
        if( nLine+1 < nDisplayLines )
            aPair.B() = m_aLineIndices[nLine+1]-1;
        else
            aPair.B() = m_aDisplayText.getLength()-1;
    }
    else if( nLine == 0 && nDisplayLines == 0 && !m_aDisplayText.isEmpty() )
    {
        // special case for single line controls so the implementations
        // in that case do not have to fill in the line indices
        aPair.A() = 0;
        aPair.B() = m_aDisplayText.getLength()-1;
    }
    return aPair;
}

Pair Control::GetLineStartEnd( tools::Long nLine ) const
{
    if( !HasLayoutData() )
        FillLayoutData();
    return mxLayoutData ? mxLayoutData->GetLineStartEnd( nLine ) : Pair( -1, -1 );
}

tools::Long ControlLayoutData::ToRelativeLineIndex( tools::Long nIndex ) const
{
    // is the index sensible at all ?
    if( nIndex >= 0 && nIndex < m_aDisplayText.getLength() )
    {
        int nDisplayLines = m_aLineIndices.size();
        // if only 1 line exists, then absolute and relative index are
        // identical -> do nothing
        if( nDisplayLines > 1 )
        {
            int nLine;
            for( nLine = nDisplayLines-1; nLine >= 0; nLine-- )
            {
                if( m_aLineIndices[nLine] <= nIndex )
                {
                    nIndex -= m_aLineIndices[nLine];
                    break;
                }
            }
            if( nLine < 0 )
            {
                SAL_WARN_IF( nLine < 0, "vcl", "ToRelativeLineIndex failed" );
                nIndex = -1;
            }
        }
    }
    else
        nIndex = -1;

    return nIndex;
}

tools::Long Control::ToRelativeLineIndex( tools::Long nIndex ) const
{
    if( !HasLayoutData() )
        FillLayoutData();
    return mxLayoutData ? mxLayoutData->ToRelativeLineIndex( nIndex ) : -1;
}

OUString Control::GetDisplayText() const
{
    if( !HasLayoutData() )
        FillLayoutData();
    return mxLayoutData ? mxLayoutData->m_aDisplayText : GetText();
}

bool Control::FocusWindowBelongsToControl(const vcl::Window* pFocusWin) const
{
    return ImplIsWindowOrChild(pFocusWin);
}

bool Control::EventNotify( NotifyEvent& rNEvt )
{
    if ( rNEvt.GetType() == NotifyEventType::GETFOCUS )
    {
        if ( !mbHasControlFocus )
        {
            mbHasControlFocus = true;
            CompatStateChanged( StateChangedType::ControlFocus );
            if ( ImplCallEventListenersAndHandler( VclEventId::ControlGetFocus, {} ) )
                // been destroyed within the handler
                return true;
        }
    }
    else
    {
        if ( rNEvt.GetType() == NotifyEventType::LOSEFOCUS )
        {
            vcl::Window* pFocusWin = Application::GetFocusWindow();
            if ( !pFocusWin || !FocusWindowBelongsToControl(pFocusWin) )
            {
                mbHasControlFocus = false;
                CompatStateChanged( StateChangedType::ControlFocus );
                if ( ImplCallEventListenersAndHandler( VclEventId::ControlLoseFocus, [this] () { maLoseFocusHdl.Call(*this); } ) )
                    // been destroyed within the handler
                    return true;
            }
        }
    }
    return Window::EventNotify( rNEvt );
}

void Control::StateChanged( StateChangedType nStateChange )
{
    if( nStateChange == StateChangedType::InitShow   ||
        nStateChange == StateChangedType::Visible    ||
        nStateChange == StateChangedType::Zoom       ||
        nStateChange == StateChangedType::ControlFont
        )
    {
        ImplClearLayoutData();
    }
    Window::StateChanged( nStateChange );
}

void Control::AppendLayoutData( const Control& rSubControl ) const
{
    if( !rSubControl.HasLayoutData() )
        rSubControl.FillLayoutData();
    if( !rSubControl.HasLayoutData() || rSubControl.mxLayoutData->m_aDisplayText.isEmpty() )
        return;

    tools::Long nCurrentIndex = mxLayoutData->m_aDisplayText.getLength();
    mxLayoutData->m_aDisplayText += rSubControl.mxLayoutData->m_aDisplayText;
    int nLines = rSubControl.mxLayoutData->m_aLineIndices.size();
    int n;
    mxLayoutData->m_aLineIndices.push_back( nCurrentIndex );
    for( n = 1; n < nLines; n++ )
        mxLayoutData->m_aLineIndices.push_back( rSubControl.mxLayoutData->m_aLineIndices[n] + nCurrentIndex );
    int nRectangles = rSubControl.mxLayoutData->m_aUnicodeBoundRects.size();
    tools::Rectangle aRel = rSubControl.GetWindowExtentsRelative(*this);
    for( n = 0; n < nRectangles; n++ )
    {
        tools::Rectangle aRect = rSubControl.mxLayoutData->m_aUnicodeBoundRects[n];
        aRect.Move( aRel.Left(), aRel.Top() );
        mxLayoutData->m_aUnicodeBoundRects.push_back( aRect );
    }
}

void Control::CallEventListeners( VclEventId nEvent, void* pData)
{
    VclPtr<Control> xThis(this);
    UITestLogger::getInstance().logAction(xThis, nEvent);

    vcl::Window::CallEventListeners(nEvent, pData);
}

bool Control::ImplCallEventListenersAndHandler( VclEventId nEvent, std::function<void()> const & callHandler )
{
    VclPtr<Control> xThis(this);

    Control::CallEventListeners( nEvent );

    if ( !xThis->isDisposed() )
    {
        if (callHandler)
        {
            callHandler();
        }

        if ( !xThis->isDisposed() )
            return false;
    }
    return true;
}

void Control::SetLayoutDataParent( const Control* pParent ) const
{
    if( HasLayoutData() )
        mxLayoutData->m_pParent = pParent;
}

void Control::ImplClearLayoutData() const
{
    mxLayoutData.reset();
}

void Control::ImplDrawFrame( OutputDevice* pDev, tools::Rectangle& rRect )
{
    // use a deco view to draw the frame
    // However, since there happens a lot of magic there, we need to fake some (style) settings
    // on the device
    AllSettings aOriginalSettings( pDev->GetSettings() );

    AllSettings aNewSettings( aOriginalSettings );
    StyleSettings aStyle( aNewSettings.GetStyleSettings() );

    // The *only known* clients of the Draw methods of the various VCL-controls are form controls:
    // During print preview, and during printing, Draw is called. Thus, drawing always happens with a
    // mono (colored) border
    aStyle.SetOptions( aStyle.GetOptions() | StyleSettingsOptions::Mono );
    aStyle.SetMonoColor( GetSettings().GetStyleSettings().GetMonoColor() );

    aNewSettings.SetStyleSettings( aStyle );
    // #i67023# do not call data changed listeners for this fake
    // since they may understandably invalidate on settings changed
    pDev->OutputDevice::SetSettings( aNewSettings );

    DecorationView aDecoView( pDev );
    rRect = aDecoView.DrawFrame( rRect, DrawFrameStyle::Out, DrawFrameFlags::WindowBorder );

    pDev->OutputDevice::SetSettings( aOriginalSettings );
}

void Control::SetShowAccelerator(bool bVal)
{
    mbShowAccelerator = bVal;
};

ControlLayoutData::~ControlLayoutData()
{
    if( m_pParent )
        m_pParent->ImplClearLayoutData();
}

Size Control::GetOptimalSize() const
{
    return Size( GetTextWidth( GetText() ) + 2 * 12,
                 GetTextHeight() + 2 * 6 );
}

void Control::SetReferenceDevice( OutputDevice* _referenceDevice )
{
    if ( mpReferenceDevice == _referenceDevice )
        return;

    mpReferenceDevice = _referenceDevice;
    Invalidate();
}

OutputDevice* Control::GetReferenceDevice() const
{
    // tdf#118377 It can happen that mpReferenceDevice is already disposed and
    // stays disposed (see task, even when Dialog is closed). I have no idea if
    // this may be very bad - someone who knows more about lifetime of OutputDevice's
    // will have to decide.
    // To secure this, I changed all accesses to mpControlData->mpReferenceDevice to
    // use Control::GetReferenceDevice() - only use mpControlData->mpReferenceDevice
    // inside Control::SetReferenceDevice and Control::GetReferenceDevice().
    // Control::GetReferenceDevice() will now reset mpReferenceDevice if it is already
    // disposed. This way all usages will do a kind of 'test-and-get' call.
    if(nullptr != mpReferenceDevice && mpReferenceDevice->isDisposed())
    {
        const_cast<Control*>(this)->SetReferenceDevice(nullptr);
    }

    return mpReferenceDevice;
}

const vcl::Font& Control::GetCanonicalFont( const StyleSettings& _rStyle ) const
{
    return _rStyle.GetLabelFont();
}

const Color& Control::GetCanonicalTextColor( const StyleSettings& _rStyle ) const
{
    return _rStyle.GetLabelTextColor();
}

void Control::ApplySettings(vcl::RenderContext& rRenderContext)
{
    const StyleSettings& rStyleSettings = rRenderContext.GetSettings().GetStyleSettings();

    ApplyControlFont(rRenderContext, GetCanonicalFont(rStyleSettings));

    ApplyControlForeground(rRenderContext, GetCanonicalTextColor(rStyleSettings));
    rRenderContext.SetTextFillColor();
}

void Control::ImplInitSettings()
{
    ApplySettings(*GetOutDev());
}

tools::Rectangle Control::DrawControlText( OutputDevice& _rTargetDevice, const tools::Rectangle& rRect, const OUString& _rStr,
    DrawTextFlags _nStyle, std::vector< tools::Rectangle >* _pVector, OUString* _pDisplayText, const Size* i_pDeviceSize ) const
{
    OUString rPStr = _rStr;
    DrawTextFlags nPStyle = _nStyle;

    bool autoacc = ImplGetSVData()->maNWFData.mbAutoAccel;

    if (autoacc && !mbShowAccelerator)
        rPStr = removeMnemonicFromString( _rStr );

    if( !GetReferenceDevice() || ( GetReferenceDevice() == &_rTargetDevice ) )
    {
        const tools::Rectangle aRet = _rTargetDevice.GetTextRect(rRect, rPStr, nPStyle);
        _rTargetDevice.DrawText(aRet, rPStr, nPStyle, _pVector, _pDisplayText);
        return aRet;
    }

    ControlTextRenderer aRenderer( *this, _rTargetDevice, *GetReferenceDevice() );
    return aRenderer.DrawText(rRect, rPStr, nPStyle, _pVector, _pDisplayText, i_pDeviceSize);
}

tools::Rectangle Control::GetControlTextRect( OutputDevice& _rTargetDevice, const tools::Rectangle & rRect,
                                       const OUString& _rStr, DrawTextFlags _nStyle, Size* o_pDeviceSize ) const
{
    OUString rPStr = _rStr;
    DrawTextFlags nPStyle = _nStyle;

    bool autoacc = ImplGetSVData()->maNWFData.mbAutoAccel;

    if (autoacc && !mbShowAccelerator)
        rPStr = removeMnemonicFromString( _rStr );

    if ( !GetReferenceDevice() || ( GetReferenceDevice() == &_rTargetDevice ) )
    {
        tools::Rectangle aRet = _rTargetDevice.GetTextRect( rRect, rPStr, nPStyle );
        if (o_pDeviceSize)
        {
            *o_pDeviceSize = aRet.GetSize();
        }
        return aRet;
    }

    ControlTextRenderer aRenderer( *this, _rTargetDevice, *GetReferenceDevice() );
    return aRenderer.GetTextRect(rRect, rPStr, nPStyle, o_pDeviceSize);
}

Font
Control::GetUnzoomedControlPointFont() const
{
    Font aFont(GetCanonicalFont(GetSettings().GetStyleSettings()));
    if (IsControlFont())
        aFont.Merge(GetControlFont());
    return aFont;
}

void Control::LogicInvalidate(const tools::Rectangle* pRectangle)
{
    VclPtr<vcl::Window> pParent = GetParentWithLOKNotifier();
    if (!pParent || !dynamic_cast<vcl::DocWindow*>(GetParent()))
    {
        // if control doesn't belong to a DocWindow, the overridden base class
        // method has to be invoked
        Window::LogicInvalidate(pRectangle);
        return;
    }

    // avoid endless paint/invalidate loop in Impress
    if (comphelper::LibreOfficeKit::isTiledPainting())
        return;

    tools::Rectangle aResultRectangle;
    if (!pRectangle)
    {
        // we have to invalidate the whole control area not the whole document
        aResultRectangle = PixelToLogic(tools::Rectangle(GetPosPixel(), GetSizePixel()), MapMode(MapUnit::MapTwip));
    }
    else
    {
        aResultRectangle = OutputDevice::LogicToLogic(*pRectangle, GetMapMode(), MapMode(MapUnit::MapTwip));
    }

    pParent->GetLOKNotifier()->notifyInvalidation(&aResultRectangle);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
