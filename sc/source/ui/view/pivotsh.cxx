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

#include <scitems.hxx>
#include <sfx2/objface.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/request.hxx>
#include <svl/whiter.hxx>
#include <vcl/EnumContext.hxx>

#include <sc.hrc>
#include <pivotsh.hxx>
#include <tabvwsh.hxx>
#include <docsh.hxx>
#include <document.hxx>
#include <dpobject.hxx>
#include <dpshttab.hxx>
#include <dbdocfun.hxx>
#include <uiitems.hxx>
#include <scabstdlg.hxx>

#define ShellClass_ScPivotShell
#include <scslots.hxx>


SFX_IMPL_INTERFACE(ScPivotShell, SfxShell)

void ScPivotShell::InitInterface_Impl()
{
    GetStaticInterface()->RegisterPopupMenu(u"pivot"_ustr);
}

ScPivotShell::ScPivotShell( ScTabViewShell* pViewSh ) :
    SfxShell(pViewSh),
    pViewShell( pViewSh )
{
    SetPool( &pViewSh->GetPool() );
    ScViewData& rViewData = pViewSh->GetViewData();
    SfxUndoManager* pMgr = rViewData.GetSfxDocShell().GetUndoManager();
    SetUndoManager( pMgr );
    if ( !rViewData.GetDocument().IsUndoEnabled() )
    {
        pMgr->SetMaxUndoActionCount( 0 );
    }
    SetName(u"Pivot"_ustr);
    SfxShell::SetContextName(vcl::EnumContext::GetContextName(vcl::EnumContext::Context::Pivot));
}

ScPivotShell::~ScPivotShell()
{
}

void ScPivotShell::Execute( const SfxRequest& rReq )
{
    switch ( rReq.GetSlot() )
    {
        case SID_PIVOT_RECALC:
            pViewShell->RecalcPivotTable();
            break;

        case SID_PIVOT_KILL:
            pViewShell->DeletePivotTable();
            break;

        case SID_DP_FILTER:
        {
            ScDPObject* pDPObj = GetCurrDPObject();
            if( pDPObj )
            {
                ScQueryParam aQueryParam;
                SCTAB nSrcTab = 0;
                const ScSheetSourceDesc* pDesc = pDPObj->GetSheetDesc();
                OSL_ENSURE( pDesc, "no sheet source for DP filter dialog" );
                if( pDesc )
                {
                    aQueryParam = pDesc->GetQueryParam();
                    nSrcTab = pDesc->GetSourceRange().aStart.Tab();
                }

                ScViewData& rViewData = pViewShell->GetViewData();
                SfxItemSetFixed<SCITEM_QUERYDATA, SCITEM_QUERYDATA> aArgSet( pViewShell->GetPool() );
                aArgSet.Put( ScQueryItem( SCITEM_QUERYDATA, &aQueryParam ) );

                ScAbstractDialogFactory* pFact = ScAbstractDialogFactory::Create();

                ScopedVclPtr<AbstractScPivotFilterDlg> pDlg(pFact->CreateScPivotFilterDlg(
                    pViewShell->GetFrameWeld(), aArgSet, rViewData, nSrcTab));

                if( pDlg->Execute() == RET_OK )
                {
                    ScSheetSourceDesc aNewDesc(&rViewData.GetDocument());
                    if( pDesc )
                        aNewDesc = *pDesc;

                    const ScQueryItem& rQueryItem = pDlg->GetOutputItem();
                    aNewDesc.SetQueryParam(rQueryItem.GetQueryData());

                    ScDPObject aNewObj( *pDPObj );
                    aNewObj.SetSheetDesc( aNewDesc );
                    ScDBDocFunc aFunc( rViewData.GetDocShell() );
                    aFunc.DataPilotUpdate( pDPObj, &aNewObj, true, false );
                    rViewData.GetView()->CursorPosChanged();       // shells may be switched
                }
            }
        }
        break;
    }
}

void ScPivotShell::GetState( SfxItemSet& rSet )
{
    ScDocShell& rDocSh = pViewShell->GetViewData().GetDocShell();
    ScDocument& rDoc = rDocSh.GetDocument();
    bool bDisable = rDocSh.IsReadOnly() || rDoc.GetChangeTrack();
    bool bFilterDisable = bDisable;
    if (!bDisable)
    {
        SCCOL nTab = pViewShell->GetViewData().GetTabNo();
        const ScTableProtection* pTabProt = rDoc.GetTabProtection(nTab);
        if (pTabProt && pTabProt->isProtected())
        {
            bDisable = true;
            if (!pTabProt->isOptionEnabled(ScTableProtection::PIVOT_TABLES))
                bFilterDisable = true;
        }
    }

    SfxWhichIter aIter(rSet);
    sal_uInt16 nWhich = aIter.FirstWhich();
    while (nWhich)
    {
        switch (nWhich)
        {
            case SID_PIVOT_RECALC:
            case SID_PIVOT_KILL:
            {
                //! move ReadOnly check to idl flags
                if ( bDisable )
                {
                    rSet.DisableItem( nWhich );
                }
            }
            break;
            case SID_DP_FILTER:
            {
                ScDPObject* pDPObj = GetCurrDPObject();
                if( bFilterDisable || !pDPObj || !pDPObj->IsSheetData() )
                    rSet.DisableItem( nWhich );
            }
            break;
        }
        nWhich = aIter.NextWhich();
    }
}

ScDPObject* ScPivotShell::GetCurrDPObject()
{
    const ScViewData& rViewData = pViewShell->GetViewData();
    return rViewData.GetDocument().GetDPAtCursor(
        rViewData.GetCurX(), rViewData.GetCurY(), rViewData.GetTabNo() );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
