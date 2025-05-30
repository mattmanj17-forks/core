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

#include <sfx2/bindings.hxx>
#include <sfx2/viewfrm.hxx>
#include <svx/sdtacitm.hxx>
#include <svx/svdobj.hxx>
#include <svx/sdtagitm.hxx>
#include <svx/sdtakitm.hxx>
#include <svx/sdtaditm.hxx>
#include <svx/sdtaaitm.hxx>
#include <svx/svdview.hxx>
#include <svx/svdocapt.hxx>
#include <editeng/outlobj.hxx>
#include <cmdid.h>
#include <view.hxx>
#include <edtwin.hxx>
#include <wrtsh.hxx>
#include <drawbase.hxx>
#include <conrect.hxx>

ConstRectangle::ConstRectangle( SwWrtShell* pWrtShell, SwEditWin* pEditWin,
                                SwView& rSwView )
    : SwDrawBase( pWrtShell, pEditWin, rSwView )
    , m_bMarquee(false)
    , m_bCapVertical(false)
    , mbVertical(false)
{
}

bool ConstRectangle::MouseButtonDown(const MouseEvent& rMEvt)
{
    bool bReturn = SwDrawBase::MouseButtonDown(rMEvt);

    if (bReturn)
    {
        if (m_pWin->GetSdrDrawMode() == SdrObjKind::Caption)
        {
            m_rView.NoRotate();
            if (m_rView.IsDrawSelMode())
            {
                m_rView.FlipDrawSelMode();
                m_pSh->GetDrawView()->SetFrameDragSingles(m_rView.IsDrawSelMode());
            }
        }
        else
        {
            SdrObject* pObj = m_rView.GetDrawView()->GetCreateObj();
            if (pObj)
            {
                SfxItemSet aAttr(pObj->getSdrModelFromSdrObject().GetItemPool());
                SwFEShell::SetLineEnds(aAttr, *pObj, m_nSlotId);
                pObj->SetMergedItemSet(aAttr);
            }
        }
    }

    return bReturn;
}

bool ConstRectangle::MouseButtonUp(const MouseEvent& rMEvt)
{
    bool bRet = SwDrawBase::MouseButtonUp(rMEvt);
    if( bRet )
    {
        SdrView *pSdrView = m_pSh->GetDrawView();
        const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
        SdrObject* pObj = rMarkList.GetMark(0) ? rMarkList.GetMark(0)->GetMarkedSdrObj()
                                               : nullptr;
        switch( m_pWin->GetSdrDrawMode() )
        {
        case SdrObjKind::Text:
            if( m_bMarquee )
            {
                m_pSh->ChgAnchor(RndStdIds::FLY_AS_CHAR);

                if( pObj )
                {
                    // Set the attributes needed for scrolling
                    SfxItemSetFixed<SDRATTR_MISC_FIRST, SDRATTR_MISC_LAST>
                        aItemSet(pSdrView->GetModel().GetItemPool());

                    aItemSet.Put( makeSdrTextAutoGrowWidthItem( false ) );
                    aItemSet.Put( makeSdrTextAutoGrowHeightItem( false ) );
                    aItemSet.Put( SdrTextAniKindItem( SdrTextAniKind::Scroll ) );
                    aItemSet.Put( SdrTextAniDirectionItem( SdrTextAniDirection::Left ) );
                    aItemSet.Put( SdrTextAniCountItem( 0 ) );
                    aItemSet.Put( SdrTextAniAmountItem(
                            static_cast<sal_Int16>(m_pWin->PixelToLogic(Size(2,1)).Width())) );

                    pObj->SetMergedItemSetAndBroadcast(aItemSet);
                }
            }
            else if(mbVertical)
            {
                if (SdrTextObj* pText = DynCastSdrTextObj(pObj))
                {
                    SfxItemSet aSet(pSdrView->GetModel().GetItemPool());

                    pText->SetVerticalWriting(true);

                    aSet.Put(makeSdrTextAutoGrowWidthItem(true));
                    aSet.Put(makeSdrTextAutoGrowHeightItem(false));
                    aSet.Put(SdrTextVertAdjustItem(SDRTEXTVERTADJUST_TOP));
                    aSet.Put(SdrTextHorzAdjustItem(SDRTEXTHORZADJUST_RIGHT));

                    pText->SetMergedItemSet(aSet);
                }
            }

            if( pObj )
            {
                SdrPageView* pPV = pSdrView->GetSdrPageView();
                m_rView.BeginTextEdit( pObj, pPV, m_pWin, true );
            }
            m_rView.LeaveDrawCreate();  // Switch to selection mode
            m_pSh->GetView().GetViewFrame().GetBindings().Invalidate(SID_INSERT_DRAW);
            break;

        case SdrObjKind::Caption:
        {
            SdrCaptionObj* pCaptObj = dynamic_cast<SdrCaptionObj*>(pObj);
            if( m_bCapVertical && pCaptObj )
            {
                pCaptObj->ForceOutlinerParaObject();
                OutlinerParaObject* pOPO = pCaptObj->GetOutlinerParaObject();
                if( pOPO && !pOPO->IsEffectivelyVertical() )
                    pOPO->SetVertical( true );
            }
        }
        break;
        default:; //prevent warning
        }
    }
    return bRet;
}

void ConstRectangle::Activate(const sal_uInt16 nSlotId)
{
    m_bMarquee = m_bCapVertical = false;
    mbVertical = false;

    switch (nSlotId)
    {
    case SID_LINE_ARROW_END:
    case SID_LINE_ARROW_CIRCLE:
    case SID_LINE_ARROW_SQUARE:
    case SID_LINE_ARROW_START:
    case SID_LINE_CIRCLE_ARROW:
    case SID_LINE_SQUARE_ARROW:
    case SID_LINE_ARROWS:
    case SID_DRAW_LINE:
    case SID_DRAW_XLINE:
        m_pWin->SetSdrDrawMode(SdrObjKind::Line);
        break;

    case SID_DRAW_MEASURELINE:
        m_pWin->SetSdrDrawMode(SdrObjKind::Measure);
        break;

    case SID_DRAW_RECT:
        m_pWin->SetSdrDrawMode(SdrObjKind::Rectangle);
        break;

    case SID_DRAW_ELLIPSE:
        m_pWin->SetSdrDrawMode(SdrObjKind::CircleOrEllipse);
        break;

    case SID_DRAW_TEXT_MARQUEE:
        m_bMarquee = true;
        m_pWin->SetSdrDrawMode(SdrObjKind::Text);
        break;

    case SID_DRAW_TEXT_VERTICAL:
        mbVertical = true;
        m_pWin->SetSdrDrawMode(SdrObjKind::Text);
        break;

    case SID_DRAW_TEXT:
        m_pWin->SetSdrDrawMode(SdrObjKind::Text);
        break;

    case SID_DRAW_CAPTION_VERTICAL:
        m_bCapVertical = true;
        [[fallthrough]];
    case SID_DRAW_CAPTION:
        m_pWin->SetSdrDrawMode(SdrObjKind::Caption);
        break;

    default:
        m_pWin->SetSdrDrawMode(SdrObjKind::NONE);
        break;
    }

    SwDrawBase::Activate(nSlotId);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
