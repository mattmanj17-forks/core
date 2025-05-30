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

#include <fuconcs.hxx>
#include <rtl/ustring.hxx>

#include <svx/svxids.hrc>

#include <sfx2/viewfrm.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/request.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/eeitem.hxx>
#include <svx/svdoashp.hxx>
#include <svx/sdtagitm.hxx>

#include <ViewShell.hxx>
#include <ViewShellBase.hxx>
#include <ToolBarManager.hxx>
#include <svx/gallery.hxx>
#include <svx/sdooitm.hxx>
#include <svl/itempool.hxx>
#include <svl/stritem.hxx>

#include <View.hxx>
#include <Window.hxx>
#include <drawdoc.hxx>

namespace sd {


FuConstructCustomShape::FuConstructCustomShape (
        ViewShell&          rViewSh,
        ::sd::Window*       pWin,
        ::sd::View*         pView,
        SdDrawDocument&     rDoc,
        SfxRequest&         rReq ) :
    FuConstruct(rViewSh, pWin, pView, rDoc, rReq)
{
}

rtl::Reference<FuPoor> FuConstructCustomShape::Create( ViewShell& rViewSh, ::sd::Window* pWin, ::sd::View* pView, SdDrawDocument& rDoc, SfxRequest& rReq, bool bPermanent )
{
    FuConstructCustomShape* pFunc;
    rtl::Reference<FuPoor> xFunc( pFunc = new FuConstructCustomShape( rViewSh, pWin, pView, rDoc, rReq ) );
    xFunc->DoExecute(rReq);
    pFunc->SetPermanent( bPermanent );
    return xFunc;
}

void FuConstructCustomShape::DoExecute( SfxRequest& rReq )
{
    FuConstruct::DoExecute( rReq );

    const SfxItemSet* pArgs = rReq.GetArgs();
    if ( pArgs )
    {
        const SfxStringItem& rItm = static_cast<const SfxStringItem&>(pArgs->Get( rReq.GetSlot() ));
        aCustomShape = rItm.GetValue();
    }

    mrViewShell.GetViewShellBase().GetToolBarManager()->SetToolBar(
        ToolBarManager::ToolBarGroup::Function,
        ToolBarManager::msDrawingObjectToolBar);
}

bool FuConstructCustomShape::MouseButtonDown(const MouseEvent& rMEvt)
{
    bool bReturn = FuConstruct::MouseButtonDown(rMEvt);

    if ( rMEvt.IsLeft() && !mpView->IsAction() )
    {
        Point aPnt( mpWindow->PixelToLogic( rMEvt.GetPosPixel() ) );

        mpWindow->CaptureMouse();
        sal_uInt16 nDrgLog = sal_uInt16 ( mpWindow->PixelToLogic(Size(mpView->GetDragThresholdPixels(),0)).Width() );

        mpView->BegCreateObj(aPnt, nullptr, nDrgLog);

        SdrObject* pObj = mpView->GetCreateObj();
        if ( pObj )
        {
            SetAttributes( pObj );
            bool bForceFillStyle = true;
            bool bForceNoFillStyle = false;
            if ( static_cast<SdrObjCustomShape*>(pObj)->UseNoFillStyle() )
            {
                bForceFillStyle = false;
                bForceNoFillStyle = true;
            }
            SfxItemSet aAttr(mrDoc.GetPool());
            SetStyleSheet( aAttr, pObj, bForceFillStyle, bForceNoFillStyle );
            pObj->SetMergedItemSet(aAttr);
        }
    }

    return bReturn;
}

bool FuConstructCustomShape::MouseButtonUp(const MouseEvent& rMEvt)
{
    if (rMEvt.IsLeft() && IsIgnoreUnexpectedMouseButtonUp())
        return false;

    bool bReturn(false);

    if(mpView->IsCreateObj() && rMEvt.IsLeft())
    {
        SdrObject* pObj = mpView->GetCreateObj();
        if( pObj && mpView->EndCreateObj( SdrCreateCmd::ForceEnd ) )
        {
            bReturn = true;
        }
        else
        {
            //Drag was too small to create object, so insert default object at click pos
            Point aClickPos(mpWindow->PixelToLogic(rMEvt.GetPosPixel()));
            sal_uInt32 nDefaultObjectSize(1000);
            sal_Int32 nCenterOffset(-sal_Int32(nDefaultObjectSize / 2));
            aClickPos.AdjustX(nCenterOffset);
            aClickPos.AdjustY(nCenterOffset);

            SdrPageView *pPV = mpView->GetSdrPageView();
            if(mpView->IsSnapEnabled())
                aClickPos = mpView->GetSnapPos(aClickPos, pPV);

            ::tools::Rectangle aNewObjectRectangle(aClickPos, Size(nDefaultObjectSize, nDefaultObjectSize));
            rtl::Reference<SdrObject> pObjDefault = CreateDefaultObject(nSlotId, aNewObjectRectangle);

            bReturn = mpView->InsertObjectAtView(pObjDefault.get(), *pPV, SdrInsertFlags::SETDEFLAYER | SdrInsertFlags::SETDEFATTR);
        }
    }
    bReturn = FuConstruct::MouseButtonUp (rMEvt) || bReturn;

    if (!bPermanent)
        mrViewShell.GetViewFrame()->GetDispatcher()->Execute(SID_OBJECT_SELECT, SfxCallMode::ASYNCHRON);

    return bReturn;
}

void FuConstructCustomShape::Activate()
{
    mpView->SetCurrentObj( SdrObjKind::CustomShape );
    FuConstruct::Activate();
}

/**
 * set attribute for the object to be created
 */
void FuConstructCustomShape::SetAttributes( SdrObject* pObj )
{
    bool bAttributesAppliedFromGallery = false;

    if ( GalleryExplorer::GetSdrObjCount( GALLERY_THEME_POWERPOINT ) )
    {
        std::vector< OUString > aObjList;
        if ( GalleryExplorer::FillObjListTitle( GALLERY_THEME_POWERPOINT, aObjList ) )
        {
            for ( std::vector<OUString>::size_type i = 0; i < aObjList.size(); i++ )
            {
                if ( aObjList[ i ].equalsIgnoreAsciiCase( aCustomShape ) )
                {
                    FmFormModel aFormModel;

                    if ( GalleryExplorer::GetSdrObj( GALLERY_THEME_POWERPOINT, i, &aFormModel ) )
                    {
                        const SdrPage* pPage = aFormModel.GetPage( 0 );
                        if ( pPage )
                        {
                            const SdrObject* pSourceObj = pPage->GetObj( 0 );
                            if( pSourceObj )
                            {
                                const SfxItemSet& rSource = pSourceObj->GetMergedItemSet();
                                SfxItemSet aDest(SfxItemSet::makeFixedSfxItemSet<
                                    // Ranges from SdrAttrObj:
                                    SDRATTR_START, SDRATTR_SHADOW_LAST, SDRATTR_MISC_FIRST, SDRATTR_MISC_LAST, SDRATTR_TEXTDIRECTION, SDRATTR_TEXTDIRECTION,
                                    // Graphic attributes, 3D properties,
                                    // CustomShape properties:
                                    SDRATTR_GRAF_FIRST, SDRATTR_CUSTOMSHAPE_LAST,
                                    // Range from SdrTextObj:
                                    EE_ITEMS_START, EE_ITEMS_END>(pObj->getSdrModelFromSdrObject().GetItemPool()));
                                aDest.Set( rSource );
                                pObj->SetMergedItemSet( aDest );
                                // Enables Word-wrap by default (tdf#134369)
                                pObj->SetMergedItem( SdrOnOffItem( SDRATTR_TEXT_WORDWRAP, true ) );
                                Degree100 nAngle = pSourceObj->GetRotateAngle();
                                if ( nAngle )
                                    pObj->NbcRotate( pObj->GetSnapRect().Center(), nAngle );
                                bAttributesAppliedFromGallery = true;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    if ( !bAttributesAppliedFromGallery )
    {
        pObj->SetMergedItem( SvxAdjustItem( SvxAdjust::Center, EE_PARA_JUST ) );
        pObj->SetMergedItem( SdrTextVertAdjustItem( SDRTEXTVERTADJUST_CENTER ) );
        pObj->SetMergedItem( SdrTextHorzAdjustItem( SDRTEXTHORZADJUST_BLOCK ) );
        pObj->SetMergedItem( makeSdrTextAutoGrowHeightItem( false ) );
        static_cast<SdrObjCustomShape*>(pObj)->MergeDefaultAttributes( &aCustomShape );
    }
}

const OUString& FuConstructCustomShape::GetShapeType() const
{
    return aCustomShape;
}

rtl::Reference<SdrObject> FuConstructCustomShape::CreateDefaultObject(const sal_uInt16, const ::tools::Rectangle& rRectangle)
{
    rtl::Reference<SdrObject> pObj(SdrObjFactory::MakeNewObject(
        mpView->getSdrModelFromSdrView(),
        mpView->GetCurrentObjInventor(),
        mpView->GetCurrentObjIdentifier()));

    if( pObj )
    {
        ::tools::Rectangle aRect( rRectangle );
        if ( doConstructOrthogonal() )
            ImpForceQuadratic( aRect );
        pObj->SetLogicRect( aRect );
        SetAttributes( pObj.get() );
        SfxItemSet aAttr(mrDoc.GetPool());
        SetStyleSheet(aAttr, pObj.get());
        pObj->SetMergedItemSet(aAttr);
    }
    return pObj;
}

// #i33136#
bool FuConstructCustomShape::doConstructOrthogonal() const
{
    return SdrObjCustomShape::doConstructOrthogonal(aCustomShape);
}

} // end of namespace sd

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
