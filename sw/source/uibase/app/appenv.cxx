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

#include <hintids.hxx>

#include <comphelper/string.hxx>
#include <sfx2/request.hxx>

#include <sfx2/bindings.hxx>
#include <sfx2/printer.hxx>
#include <sfx2/viewfrm.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/pbinitem.hxx>
#include <editeng/paperinf.hxx>
#include <osl/diagnose.h>
#include <fmthdft.hxx>
#include <swwait.hxx>
#include <swmodule.hxx>
#include <wrtsh.hxx>
#include <view.hxx>
#include <docsh.hxx>
#include <frmatr.hxx>
#include <fldbas.hxx>
#include <swundo.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <dialoghelp.hxx>
#include <fmtcol.hxx>
#include <frmmgr.hxx>
#include <fldmgr.hxx>
#include <pagedesc.hxx>
#include <poolfmt.hxx>
#include <expfld.hxx>
#include <SwStyleNameMapper.hxx>
#include <fmtpdsc.hxx>

#include <cmdid.h>
#include <strings.hrc>
#include <swabstdlg.hxx>
#include <envimg.hxx>
#include "appenv.hxx"

#define ENV_NEWDOC      RET_OK
#define ENV_INSERT      RET_USER

// Function used for labels and envelopes in applab.cxx and appenv.cxx
OUString InsertLabEnvText( SwWrtShell& rSh, SwFieldMgr& rFieldMgr, const OUString& rText )
{
    OUString sRet;
    OUString aText = rText.replaceAll("\r", "");

    sal_Int32 nTokenPos = 0;
    while( -1 != nTokenPos )
    {
        OUString aLine = aText.getToken( 0, '\n', nTokenPos );
        while ( !aLine.isEmpty() )
        {
            OUString sTmpText;
            bool bField = false;

            sal_Int32 nPos = aLine.indexOf( '<' );
            if (0 != nPos)
            {
                sal_Int32 const nCopy((nPos != -1) ? nPos : aLine.getLength());
                sTmpText = aLine.copy(0, nCopy);
                aLine = aLine.copy(nCopy);
            }
            else
            {
                nPos = aLine.indexOf( '>' );
                if ( nPos == -1 )
                {
                    sTmpText = aLine;
                    aLine.clear();
                }
                else
                {
                    sTmpText = aLine.copy( 0, nPos + 1);
                    aLine = aLine.copy( nPos + 1);

                    // Database fields must contain at least 3 points!
                    OUString sDBName( sTmpText.copy( 1, sTmpText.getLength() - 2));
                    if (comphelper::string::getTokenCount(sDBName, '.') >= 3)
                    {
                        sDBName = ::ReplacePoint(sDBName, true);
                        SwInsertField_Data aData(SwFieldTypesEnum::Database, 0, sDBName, OUString(), 0, &rSh);
                        rFieldMgr.InsertField( aData );
                        sRet = sDBName;
                        bField = true;
                    }
                }
            }
            if ( !bField )
                rSh.Insert( sTmpText );
        }
        rSh.SplitNode();
    }
    rSh.DelLeft();  // Again remove last linebreak

    return sRet;
}

static void lcl_CopyCollAttr(SwWrtShell const * pOldSh, SwWrtShell* pNewSh, sal_uInt16 nCollId)
{
    sal_uInt16 nCollCnt = pOldSh->GetTextFormatCollCount();
    for( sal_uInt16 nCnt = 0; nCnt < nCollCnt; ++nCnt )
    {
        SwTextFormatColl* pColl = &pOldSh->GetTextFormatColl(nCnt);
        if(nCollId == pColl->GetPoolFormatId())
            pNewSh->GetTextCollFromPool(nCollId)->SetFormatAttr(pColl->GetAttrSet());
    }
}

void SwModule::InsertEnv( SfxRequest& rReq )
{
    static sal_uInt16 nTitleNo = 0;

    SwDocShell      *pMyDocSh;
    SfxViewFrame    *pFrame;
    SwView          *pNewView;
    SwWrtShell      *pOldSh,
                    *pSh;

    // Get current shell
    pMyDocSh = static_cast<SwDocShell*>( SfxObjectShell::Current());
    pOldSh   = pMyDocSh ? pMyDocSh->GetWrtShell() : nullptr;

    // Create new document (don't show!)
    SfxObjectShellLock xDocSh( new SwDocShell( SfxObjectCreateMode::STANDARD ) );
    xDocSh->DoInitNew();
    pFrame = SfxViewFrame::LoadHiddenDocument( *xDocSh, SFX_INTERFACE_NONE );
    pNewView = static_cast<SwView*>( pFrame->GetViewShell());
    pNewView->AttrChangedNotify(nullptr); // so that SelectShell is being called
    pSh = pNewView->GetWrtShellPtr();

    if (!pSh)
        return;

    OUString aTmp = SwResId(STR_ENV_TITLE) + OUString::number( ++nTitleNo );
    xDocSh->SetTitle( aTmp );

    // if applicable, copy the old Collections "Sender" and "Receiver" to
    // a new document
    if ( pOldSh )
    {
        ::lcl_CopyCollAttr(pOldSh, pSh, RES_POOLCOLL_ENVELOPE_ADDRESS);
        ::lcl_CopyCollAttr(pOldSh, pSh, RES_POOLCOLL_SEND_ADDRESS);
    }

    // Read SwEnvItem from config
    SwEnvCfgItem aEnvCfg;

    // Check if there's already an envelope.
    bool bEnvChange = false;

    SfxItemSetFixed<FN_ENVELOP, FN_ENVELOP> aSet(GetPool());
    aSet.Put(aEnvCfg.GetItem());

    SfxPrinter* pTempPrinter = pSh->getIDocumentDeviceAccess().getPrinter( true );
    if(pOldSh )
    {
        const SwPageDesc& rCurPageDesc = pOldSh->GetPageDesc(pOldSh->GetCurPageDesc());
        UIName sEnvelope;
        SwStyleNameMapper::FillUIName( RES_POOLPAGE_ENVELOPE, sEnvelope );
        bEnvChange = rCurPageDesc.GetName() == sEnvelope;

        IDocumentDeviceAccess& rIDDA_old = pOldSh->getIDocumentDeviceAccess();
        if( rIDDA_old.getPrinter( false ) )
        {
            IDocumentDeviceAccess& rIDDA = pSh->getIDocumentDeviceAccess();
            rIDDA.setJobsetup( *rIDDA_old.getJobsetup() );
            //#69563# if it isn't the same printer then the pointer has been invalidated!
            pTempPrinter = rIDDA.getPrinter( true );
        }
        pTempPrinter->SetPaperBin(rCurPageDesc.GetMaster().GetPaperBin().GetValue());

    }

    ScopedVclPtr<SfxAbstractTabDialog> pDlg;
    short nMode = ENV_INSERT;

    const SwEnvItem* pItem = rReq.GetArg<SwEnvItem>(FN_ENVELOP);
    if ( !pItem )
    {
        SwAbstractDialogFactory* pFact = SwAbstractDialogFactory::Create();
        pDlg.disposeAndReset(pFact->CreateSwEnvDlg(GetFrameWeld(pMyDocSh), aSet, pOldSh, pTempPrinter, !bEnvChange));
        nMode = pDlg->Execute();
    }
    else
    {
        const SfxBoolItem* pBoolItem = rReq.GetArg<SfxBoolItem>(FN_PARAM_1);
        if ( pBoolItem && pBoolItem->GetValue() )
            nMode = ENV_NEWDOC;
    }

    if (nMode == ENV_NEWDOC || nMode == ENV_INSERT)
    {
        SwWait aWait( static_cast<SwDocShell&>(*xDocSh), true );

        // Read dialog and save item to config
        const SwEnvItem& rItem = pItem ? *pItem : static_cast<const SwEnvItem&>( pDlg->GetOutputItemSet()->Get(FN_ENVELOP) );
        aEnvCfg.GetItem() = rItem;
        aEnvCfg.Commit();

        // When we print we take the Jobsetup that is set up in the dialog.
        // Information has to be set here, before a possible destruction of
        // the new shell because the shell's printer has been handed to the
        // dialog.
        if ( nMode != ENV_NEWDOC )
        {
            OSL_ENSURE(pOldSh, "No document - wasn't 'Insert' disabled???");
            SvxPaperBinItem aItem( RES_PAPER_BIN );
            aItem.SetValue(static_cast<sal_uInt8>(pSh->getIDocumentDeviceAccess().getPrinter(true)->GetPaperBin()));
            pOldSh->GetPageDescFromPool(RES_POOLPAGE_ENVELOPE)->GetMaster().SetFormatAttr(aItem);
        }

        SwWrtShell *pTmp = nMode == ENV_INSERT ? pOldSh : pSh;
        const SwPageDesc* pFollow = nullptr;
        SwTextFormatColl *pSend = pTmp->GetTextCollFromPool(RES_POOLCOLL_SEND_ADDRESS),
                     *pAddr = pTmp->GetTextCollFromPool(RES_POOLCOLL_ENVELOPE_ADDRESS);
        const UIName sSendMark = pSend->GetName();
        const UIName sAddrMark = pAddr->GetName();

        if (nMode == ENV_INSERT)
        {

            SetView(&pOldSh->GetView()); // Set pointer to top view

            // Delete new document
            xDocSh->DoClose();
            pSh = pOldSh;
            //#i4251# selected text or objects in the document should
            //not be deleted on inserting envelopes
            pSh->EnterStdMode();
            // Here it goes (insert)
            pSh->StartUndo(SwUndoId::UI_INSERT_ENVELOPE);
            pSh->StartAllAction();
            pSh->SttEndDoc(true);

            if (bEnvChange)
            {
                // followup template: page 2
                pFollow = pSh->GetPageDesc(pSh->GetCurPageDesc()).GetFollow();

                // Delete text from the first page
                if ( !pSh->SttNxtPg(true) )
                    pSh->EndPg(true);
                pSh->DelRight();
                // Delete frame of the first page
                if ( pSh->GotoFly(sSendMark) )
                {
                    pSh->EnterSelFrameMode();
                    pSh->DelRight();
                }
                if ( pSh->GotoFly(sAddrMark) )
                {
                    pSh->EnterSelFrameMode();
                    pSh->DelRight();
                }
                pSh->SttEndDoc(true);
            }
            else
                // Followup template: page 1
                pFollow = &pSh->GetPageDesc(pSh->GetCurPageDesc());

            // Insert page break
            if ( pSh->IsCursorInTable() )
            {
                pSh->SplitNode();
                pSh->Right( SwCursorSkipMode::Chars, false, 1, false );
                SfxItemSetFixed<RES_PAGEDESC, RES_PAGEDESC> aBreakSet( pSh->GetAttrPool() );
                aBreakSet.Put( SwFormatPageDesc( pFollow ) );
                pSh->SetTableAttr( aBreakSet );
            }
            else
            {
                UIName sFollowName(pFollow->GetName());
                pSh->InsertPageBreak(&sFollowName, std::nullopt);
            }
            pSh->SttEndDoc(true);
        }
        else
        {
            pFollow = &pSh->GetPageDesc(pSh->GetCurPageDesc());
            // Let's go (print)
            pSh->StartAllAction();
            pSh->DoUndo(false);

            // Again, copy the new collections "Sender" and "Receiver" to
            // a new document
            if ( pOldSh )
            {
                ::lcl_CopyCollAttr(pOldSh, pSh, RES_POOLCOLL_ENVELOPE_ADDRESS);
                ::lcl_CopyCollAttr(pOldSh, pSh, RES_POOLCOLL_SEND_ADDRESS);
            }
        }

        CurrShell aCurr(pSh);
        pSh->SetNewDoc();   // Avoid performance problems

        // Remember Flys of this site
        std::vector<SwFrameFormat*> aFlyArr;
        if( ENV_NEWDOC != nMode && !bEnvChange )
            pSh->GetPageObjs( aFlyArr );

        // Get page description
        SwPageDesc* pDesc = pSh->GetPageDescFromPool(RES_POOLPAGE_ENVELOPE);
        SwFrameFormat&   rFormat  = pDesc->GetMaster();

        Printer *pPrt = pSh->getIDocumentDeviceAccess().getPrinter( true );

    // Borders (are put together by Shift-Offset and alignment)
        Size aPaperSize = pPrt->PixelToLogic( pPrt->GetPaperSizePixel(),
                                              MapMode(MapUnit::MapTwip));
        if ( !aPaperSize.Width() && !aPaperSize.Height() )
                    aPaperSize = SvxPaperInfo::GetPaperSize(PAPER_A4);
        if ( aPaperSize.Width() > aPaperSize.Height() )
            Swap( aPaperSize );

        tools::Long lLeft  = rItem.m_nShiftRight,
             lUpper = rItem.m_nShiftDown;

        sal_uInt16 nPageW = o3tl::narrowing<sal_uInt16>(std::max(rItem.m_nWidth, rItem.m_nHeight)),
               nPageH = o3tl::narrowing<sal_uInt16>(std::min(rItem.m_nWidth, rItem.m_nHeight));

        switch (rItem.m_eAlign)
        {
            case ENV_HOR_LEFT: break;
            case ENV_HOR_CNTR: lLeft  += std::max(tools::Long(0), aPaperSize.Width() - nPageW) / 2;
                               break;
            case ENV_HOR_RGHT: lLeft  += std::max(tools::Long(0), aPaperSize.Width() - nPageW);
                               break;
            case ENV_VER_LEFT: lUpper += std::max(tools::Long(0), aPaperSize.Width() - nPageH);
                               break;
            case ENV_VER_CNTR: lUpper += std::max(tools::Long(0), aPaperSize.Width() - nPageH) / 2;
                               break;
            case ENV_VER_RGHT: break;
        }
        SvxLRSpaceItem aLRMargin( RES_LR_SPACE );
        SvxULSpaceItem aULMargin( RES_UL_SPACE );
        aLRMargin.SetLeft(SvxIndentValue::twips(o3tl::narrowing<sal_uInt16>(lLeft)));
        aULMargin.SetUpper(o3tl::narrowing<sal_uInt16>(lUpper));
        aLRMargin.SetRight(SvxIndentValue::zero());
        aULMargin.SetLower(0);
        rFormat.SetFormatAttr(aLRMargin);
        rFormat.SetFormatAttr(aULMargin);

        // Header and footer
        rFormat.SetFormatAttr(SwFormatHeader(false));
        pDesc->ChgHeaderShare(false);
        rFormat.SetFormatAttr(SwFormatFooter(false));
        pDesc->ChgFooterShare(false);

        // Page numbering
        pDesc->SetUseOn(UseOnPage::All);

        // Page size
        rFormat.SetFormatAttr(SwFormatFrameSize(SwFrameSize::Fixed,
                                            nPageW + lLeft, nPageH + lUpper));

        // Set type of page numbering
        SvxNumberType aType;
        aType.SetNumberingType(SVX_NUM_NUMBER_NONE);
        pDesc->SetNumType(aType);

        // Followup template
        if (pFollow)
            pDesc->SetFollow(pFollow);

        // Landscape
        pDesc->SetLandscape( rItem.m_eAlign >= ENV_VER_LEFT &&
                             rItem.m_eAlign <= ENV_VER_RGHT);

        // Apply page description

        size_t nPos;
        pSh->FindPageDescByName( pDesc->GetName(),
                                    false,
                                    &nPos );

        pSh->ChgPageDesc( nPos, *pDesc);
        pSh->ChgCurPageDesc(*pDesc);

        // Insert Frame
        SwFlyFrameAttrMgr aMgr(false, pSh, Frmmgr_Type::ENVELP, nullptr);
        SwFieldMgr aFieldMgr;
        aMgr.SetHeightSizeType(SwFrameSize::Variable);

        // Overwrite defaults!
        aMgr.GetAttrSet().Put( SvxBoxItem(RES_BOX) );
        aMgr.SetULSpace( 0, 0 );
        aMgr.SetLRSpace( 0, 0 );

        // Sender
        if (rItem.m_bSend)
        {
            pSh->SttEndDoc(true);
            aMgr.InsertFlyFrame(RndStdIds::FLY_AT_PAGE,
                Point(rItem.m_nSendFromLeft + lLeft, rItem.m_nSendFromTop  + lUpper),
                Size (rItem.m_nAddrFromLeft - rItem.m_nSendFromLeft, 0));

            pSh->EnterSelFrameMode();
            pSh->SetFlyName(sSendMark);
            pSh->UnSelectFrame();
            pSh->LeaveSelFrameMode();
            pSh->SetTextFormatColl( pSend );
            InsertLabEnvText( *pSh, aFieldMgr, rItem.m_aSendText );
            aMgr.UpdateAttrMgr();
        }

        // Addressee
        pSh->SttEndDoc(true);

        aMgr.InsertFlyFrame(RndStdIds::FLY_AT_PAGE,
            Point(rItem.m_nAddrFromLeft + lLeft, rItem.m_nAddrFromTop  + lUpper),
            Size (nPageW - rItem.m_nAddrFromLeft - 566, 0));
        pSh->EnterSelFrameMode();
        pSh->SetFlyName(sAddrMark);
        pSh->UnSelectFrame();
        pSh->LeaveSelFrameMode();
        pSh->SetTextFormatColl( pAddr );
        InsertLabEnvText(*pSh, aFieldMgr, rItem.m_aAddrText);

        // Move Flys to the "old" pages
        if (!aFlyArr.empty())
            pSh->SetPageObjsNewPage(aFlyArr);

        // Finished
        pSh->SttEndDoc(true);

        pSh->EndAllAction();

        if (nMode == ENV_NEWDOC)
            pSh->DoUndo();
        else
            pSh->EndUndo(SwUndoId::UI_INSERT_ENVELOPE);

        if (nMode == ENV_NEWDOC)
        {
            pFrame->GetFrame().Appear();

            if ( rItem.m_aAddrText.indexOf('<') >= 0 )
            {
                static sal_uInt16 const aInva[] =
                                    {
                                        SID_SBA_BRW_UPDATE,
                                        SID_SBA_BRW_INSERT,
                                        SID_SBA_BRW_MERGE,
                                        0
                                    };
                pFrame->GetBindings().Invalidate( aInva );

                // Open database beamer
                ShowDBObj(*pNewView, pSh->GetDBData());
            }
        }

        if ( !pItem )
        {
            rReq.AppendItem( rItem );
            if ( nMode == ENV_NEWDOC )
                rReq.AppendItem( SfxBoolItem( FN_PARAM_1, true ) );
        }

        rReq.Done();
    }
    else    // Abort
    {
        rReq.Ignore();

        xDocSh->DoClose();
        --nTitleNo;

        // Set pointer to top view
        if (pOldSh)
            SetView(&pOldSh->GetView());
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
