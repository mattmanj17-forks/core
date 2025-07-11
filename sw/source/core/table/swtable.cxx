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

#include <libxml/xmlwriter.h>

#include <hintids.hxx>
#include <hints.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/shaditem.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/colritem.hxx>
#include <osl/diagnose.h>
#include <sfx2/linkmgr.hxx>
#include <fmtfsize.hxx>
#include <fmtornt.hxx>
#include <fmtpdsc.hxx>
#include <fldbas.hxx>
#include <fmtfld.hxx>
#include <frmatr.hxx>
#include <doc.hxx>
#include <IDocumentLinksAdministration.hxx>
#include <IDocumentRedlineAccess.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <docary.hxx>
#include <frame.hxx>
#include <swtable.hxx>
#include <ndtxt.hxx>
#include <tabcol.hxx>
#include <tabfrm.hxx>
#include <cellfrm.hxx>
#include <rowfrm.hxx>
#include <swserv.hxx>
#include <expfld.hxx>
#include <mdiexp.hxx>
#include <cellatr.hxx>
#include <txatbase.hxx>
#include <htmltbl.hxx>
#include <swtblfmt.hxx>
#include <ndindex.hxx>
#include <tblrwcl.hxx>
#include <shellres.hxx>
#include <viewsh.hxx>
#include <redline.hxx>
#include <vector>
#include <calbck.hxx>
#include <o3tl/string_view.hxx>
#include <svl/numformat.hxx>
#include <txtfld.hxx>
#include <rolbck.hxx>

#ifdef DBG_UTIL
#define CHECK_TABLE(t) (t).CheckConsistency();
#else
#define CHECK_TABLE(t)
#endif

using namespace com::sun::star;


#define COLFUZZY 20

static void ChgTextToNum( SwTableBox& rBox, const OUString& rText, const Color* pCol,
                    bool bChgAlign, SwNodeOffset nNdPos );

void SwTableBox::setRowSpan( sal_Int32 nNewRowSpan )
{
    mnRowSpan = nNewRowSpan;
}

bool SwTableBox::getDummyFlag() const
{
    return mbDummyFlag;
}

void SwTableBox::setDummyFlag( bool bDummy )
{
    mbDummyFlag = bDummy;
}

//JP 15.09.98: Bug 55741 - Keep tabs (front and rear)
static void lcl_TabToBlankAtSttEnd( OUString& rText )
{
    sal_Unicode c;
    sal_Int32 n;

    for( n = 0; n < rText.getLength() && ' ' >= ( c = rText[n] ); ++n )
        if( '\x9' == c )
            rText = rText.replaceAt( n, 1, u" " );
    for( n = rText.getLength(); n && ' ' >= ( c = rText[--n] ); )
        if( '\x9' == c )
            rText = rText.replaceAt( n, 1, u" " );
}

static void lcl_DelTabsAtSttEnd( OUString& rText )
{
    sal_Unicode c;
    sal_Int32 n;
    OUStringBuffer sBuff(rText);

    for( n = 0; n < sBuff.getLength() && ' ' >= ( c = sBuff[ n ]); ++n )
    {
        if( '\x9' == c )
            sBuff.remove( n--, 1 );
    }
    for( n = sBuff.getLength(); n && ' ' >= ( c = sBuff[ --n ]); )
    {
        if( '\x9' == c )
            sBuff.remove( n, 1 );
    }
    rText = sBuff.makeStringAndClear();
}

void InsTableBox( SwDoc& rDoc, SwTableNode* pTableNd,
                        SwTableLine* pLine, SwTableBoxFormat* pBoxFrameFormat,
                        SwTableBox* pBox,
                        sal_uInt16 nInsPos, sal_uInt16 nCnt )
{
    OSL_ENSURE( pBox->GetSttNd(), "Box with no start node" );
    SwNodeIndex aIdx( *pBox->GetSttNd(), +1 );
    SwContentNode* pCNd = aIdx.GetNode().GetContentNode();
    if( !pCNd )
        pCNd = SwNodes::GoNext(&aIdx);
    assert(pCNd && "Box with no content node");

    if( pCNd->IsTextNode() )
    {
        if( pCNd->GetpSwAttrSet() )
        {
            SwAttrSet aAttrSet( *pCNd->GetpSwAttrSet() );
            if(pCNd->GetSwAttrSet().HasItem(RES_PARATR_LIST_AUTOFMT))
            {
                SwFormatAutoFormat format = aAttrSet.Get(RES_PARATR_LIST_AUTOFMT);
                const std::shared_ptr<SfxItemSet>& handle = format.GetStyleHandle();
                aAttrSet.Put(*handle);
            }
            if( pBox->GetSaveNumFormatColor() )
            {
                if( pBox->GetSaveUserColor() )
                    aAttrSet.Put( SvxColorItem( *pBox->GetSaveUserColor(), RES_CHRATR_COLOR ));
                else
                    aAttrSet.ClearItem( RES_CHRATR_COLOR );
            }
            rDoc.GetNodes().InsBoxen( pTableNd, pLine, pBoxFrameFormat,
                                    static_cast<SwTextNode*>(pCNd)->GetTextColl(),
                                    &aAttrSet, nInsPos, nCnt );
        }
        else
            rDoc.GetNodes().InsBoxen( pTableNd, pLine, pBoxFrameFormat,
                                    static_cast<SwTextNode*>(pCNd)->GetTextColl(),
                                    pCNd->GetpSwAttrSet(), nInsPos, nCnt );
    }
    else
        rDoc.GetNodes().InsBoxen( pTableNd, pLine, pBoxFrameFormat,
                rDoc.GetDfltTextFormatColl(), nullptr,
                nInsPos, nCnt );

    sal_Int32 nRowSpan = pBox->getRowSpan();
    if( nRowSpan != 1 )
    {
        SwTableBoxes& rTableBoxes = pLine->GetTabBoxes();
        for( sal_uInt16 i = 0; i < nCnt; ++i )
        {
            pBox = rTableBoxes[ i + nInsPos ];
            pBox->setRowSpan( nRowSpan );
        }
    }
}

SwTable::SwTable()
    : SwClient( nullptr ),
    m_pTableNode( nullptr ),
    m_nGraphicsThatResize( 0 ),
    m_nRowsToRepeat( 1 ),
    m_bModifyLocked( false ),
    m_bNewModel( true )
{
    // default value set in the options
    m_eTableChgMode = GetTableChgDefaultMode();
}

SwTable::SwTable( const SwTable& rTable )
    : SwClient( rTable.GetFrameFormat() ),
    m_pTableNode( nullptr ),
    m_eTableChgMode( rTable.m_eTableChgMode ),
    m_nGraphicsThatResize( 0 ),
    m_nRowsToRepeat( rTable.GetRowsToRepeat() ),
    maTableStyleName(rTable.maTableStyleName),
    m_bModifyLocked( false ),
    m_bNewModel( rTable.m_bNewModel )
{
}

void DelBoxNode( SwTableSortBoxes const & rSortCntBoxes )
{
    for (size_t n = 0; n < rSortCntBoxes.size(); ++n)
    {
        rSortCntBoxes[ n ]->m_pStartNode = nullptr;
    }
}

SwTable::~SwTable()
{
    if( m_xRefObj.is() )
    {
        SwDoc& rDoc = GetFrameFormat()->GetDoc();
        if( !rDoc.IsInDtor() )         // then remove from the list
            rDoc.getIDocumentLinksAdministration().GetLinkManager().RemoveServer( m_xRefObj.get() );

        m_xRefObj->Closed();
    }

    // the table can be deleted if it's the last client of the FrameFormat
    SwTableFormat* pFormat = GetFrameFormat();
    pFormat->Remove(*this);               // remove

    if( !pFormat->HasWriterListeners() )
        pFormat->GetDoc().DelTableFrameFormat( pFormat );   // and delete

    // Delete the pointers from the SortArray of the boxes. The objects
    // are preserved and are deleted by the lines/boxes arrays dtor.
    // Note: unfortunately not enough, pointers to the StartNode of the
    // section need deletion.
    DelBoxNode(m_TabSortContentBoxes);
    m_TabSortContentBoxes.clear();
}

namespace
{

template<class T>
T lcl_MulDiv64(sal_uInt64 nA, sal_uInt64 nM, sal_uInt64 nD)
{
    assert(nD != 0);
    return nD == 0 ? static_cast<T>(nA*nM) : static_cast<T>((nA*nM)/nD);
}

}

static void FormatInArr( std::vector<SwFormat*>& rFormatArr, SwFormat* pBoxFormat )
{
    std::vector<SwFormat*>::const_iterator it = std::find( rFormatArr.begin(), rFormatArr.end(), pBoxFormat );
    if ( it == rFormatArr.end() )
        rFormatArr.push_back( pBoxFormat );
}

static void lcl_ModifyBoxes( SwTableBoxes &rBoxes, const tools::Long nOld,
                         const tools::Long nNew, std::vector<SwFormat*>& rFormatArr );

static void lcl_ModifyLines( SwTableLines &rLines, const tools::Long nOld,
                         const tools::Long nNew, std::vector<SwFormat*>& rFormatArr, const bool bCheckSum )
{
    for ( auto &rLine : rLines)
        ::lcl_ModifyBoxes( rLine->GetTabBoxes(), nOld, nNew, rFormatArr );
    if( bCheckSum )
    {
        for(SwFormat* pFormat : rFormatArr)
        {
            const SwTwips nBox = lcl_MulDiv64<SwTwips>(pFormat->GetFrameSize().GetWidth(), nNew, nOld);
            SwFormatFrameSize aNewBox( SwFrameSize::Variable, nBox, 0 );
            pFormat->LockModify();
            pFormat->SetFormatAttr( aNewBox );
            pFormat->UnlockModify();
        }
    }
}

static void lcl_ModifyBoxes( SwTableBoxes &rBoxes, const tools::Long nOld,
                         const tools::Long nNew, std::vector<SwFormat*>& rFormatArr )
{
    sal_uInt64 nSum = 0; // To avoid rounding errors we summarize all box widths
    sal_uInt64 nOriginalSum = 0; // Sum of original widths
    for ( size_t i = 0; i < rBoxes.size(); ++i )
    {
        SwTableBox &rBox = *rBoxes[i];
        if ( !rBox.GetTabLines().empty() )
        {
            // For SubTables the rounding problem will not be solved :-(
            ::lcl_ModifyLines( rBox.GetTabLines(), nOld, nNew, rFormatArr, false );
        }
        // Adjust the box
        SwFrameFormat *pFormat = rBox.GetFrameFormat();
        sal_uInt64 nBox = pFormat->GetFrameSize().GetWidth();
        nOriginalSum += nBox;
        nBox = lcl_MulDiv64<sal_uInt64>(nBox, nNew, nOld);
        const sal_uInt64 nWishedSum = lcl_MulDiv64<sal_uInt64>(nOriginalSum, nNew, nOld) - nSum;
        if( nWishedSum > 0 )
        {
            if( nBox == nWishedSum )
                FormatInArr( rFormatArr, pFormat );
            else
            {
                nBox = nWishedSum;
                pFormat = rBox.ClaimFrameFormat();
                SwFormatFrameSize aNewBox( SwFrameSize::Variable, static_cast< SwTwips >(nBox), 0 );
                pFormat->LockModify();
                pFormat->SetFormatAttr( aNewBox );
                pFormat->UnlockModify();
            }
        }
        else {
            OSL_FAIL( "Rounding error" );
        }
        nSum += nBox;
    }
}

void SwTable::SwClientNotify(const SwModify&, const SfxHint& rHint)
{
    if(rHint.GetId() == SfxHintId::SwAutoFormatUsedHint)
    {
        auto& rAutoFormatUsedHint = static_cast<const sw::AutoFormatUsedHint&>(rHint);
        rAutoFormatUsedHint.CheckNode(GetTableNode());
    }
    else if (rHint.GetId() == SfxHintId::SwAttrSetChange)
    {
        auto pChangeHint = static_cast<const sw::AttrSetChangeHint*>(&rHint);
        // catch SSize changes, to adjust the lines/boxes
        const SwFormatFrameSize* pNewSize = nullptr, *pOldSize = nullptr;
        if (pChangeHint->m_pOld && pChangeHint->m_pNew
                && (pNewSize = pChangeHint->m_pNew->GetChgSet()->GetItemIfSet(
                        RES_FRM_SIZE,
                        false)))
        {
            pOldSize = &pChangeHint->m_pOld->GetChgSet()->GetFrameSize();
        }
        if (pOldSize && pNewSize && !m_bModifyLocked)
            AdjustWidths(pOldSize->GetWidth(), pNewSize->GetWidth());
    }
    else if (rHint.GetId() == SfxHintId::SwObjectDying)
    {
        auto pDyingHint = static_cast<const sw::ObjectDyingHint*>(&rHint);
        CheckRegistration( *pDyingHint );
    }
    else if (rHint.GetId() == SfxHintId::SwLegacyModify)
    {
        auto pLegacy = static_cast<const sw::LegacyModifyHint*>(&rHint);
        // catch SSize changes, to adjust the lines/boxes
        const sal_uInt16 nWhich = pLegacy->GetWhich();
        if (nWhich == RES_FRM_SIZE)
        {
            const SwFormatFrameSize* pNewSize = nullptr, *pOldSize = nullptr;
            pOldSize = static_cast<const SwFormatFrameSize*>(pLegacy->m_pOld);
            pNewSize = static_cast<const SwFormatFrameSize*>(pLegacy->m_pNew);
            if (pOldSize && pNewSize && !m_bModifyLocked)
                AdjustWidths(pOldSize->GetWidth(), pNewSize->GetWidth());
        }
    }
}

void SwTable::AdjustWidths( const tools::Long nOld, const tools::Long nNew )
{
    std::vector<SwFormat*> aFormatArr;
    aFormatArr.reserve( m_aLines[0]->GetTabBoxes().size() );
    ::lcl_ModifyLines( m_aLines, nOld, nNew, aFormatArr, true );
}

static void lcl_RefreshHidden( SwTabCols &rToFill, size_t nPos )
{
    for ( size_t i = 0; i < rToFill.Count(); ++i )
    {
        if ( std::abs(static_cast<tools::Long>(nPos) - rToFill[i]) <= COLFUZZY )
        {
            rToFill.SetHidden( i, false );
            break;
        }
    }
}

static void lcl_SortedTabColInsert( SwTabCols &rToFill, const SwTableBox *pBox,
                   const SwFrameFormat *pTabFormat, const bool bHidden,
                   const bool bRefreshHidden )
{
    const tools::Long nWish = pTabFormat->GetFrameSize().GetWidth();
    OSL_ENSURE(nWish, "weird <= 0 width frmfrm");

    // The value for the left edge of the box is calculated from the
    // widths of the previous boxes.
    tools::Long nPos = 0;
    tools::Long nLeftMin = 0;
    tools::Long nRightMax = 0;
    if (nWish != 0) //fdo#33012 0 width frmfmt
    {
        SwTwips nSum = 0;
        const SwTableBox  *pCur  = pBox;
        const SwTableLine *pLine = pBox->GetUpper();
        const tools::Long nAct  = rToFill.GetRight() - rToFill.GetLeft();  // +1 why?

        while ( pLine )
        {
            const SwTableBoxes &rBoxes = pLine->GetTabBoxes();
            for ( size_t i = 0; i < rBoxes.size(); ++i )
            {
                const SwTwips nWidth = rBoxes[i]->GetFrameFormat()->GetFrameSize().GetWidth();
                nSum += nWidth;
                const tools::Long nTmp = lcl_MulDiv64<tools::Long>(nSum, nAct, nWish);

                if (rBoxes[i] != pCur)
                {
                    if ( pLine == pBox->GetUpper() || 0 == nLeftMin )
                        nLeftMin = nTmp - nPos;
                    nPos = nTmp;
                }
                else
                {
                    nSum -= nWidth;
                    if ( 0 == nRightMax )
                        nRightMax = nTmp - nPos;
                    break;
                }
            }
            pCur  = pLine->GetUpper();
            pLine = pCur ? pCur->GetUpper() : nullptr;
        }
    }

    bool bInsert = !bRefreshHidden;
    for ( size_t j = 0; bInsert && (j < rToFill.Count()); ++j )
    {
        tools::Long nCmp = rToFill[j];
        if ( (nPos >= ((nCmp >= COLFUZZY) ? nCmp - COLFUZZY : nCmp)) &&
             (nPos <= (nCmp + COLFUZZY)) )
        {
            bInsert = false;        // Already has it.
        }
        else if ( nPos < nCmp )
        {
            bInsert = false;
            rToFill.Insert( nPos, bHidden, j );
        }
    }
    if ( bInsert )
        rToFill.Insert( nPos, bHidden, rToFill.Count() );
    else if ( bRefreshHidden )
        ::lcl_RefreshHidden( rToFill, nPos );

    if ( !bHidden || bRefreshHidden )
        return;

    // calculate minimum/maximum values for the existing entries:
    nLeftMin = nPos - nLeftMin;
    nRightMax = nPos + nRightMax;

    // check if nPos is entry:
    bool bFoundPos = false;
    bool bFoundMax = false;
    for ( size_t j = 0; !(bFoundPos && bFoundMax ) && j < rToFill.Count(); ++j )
    {
        SwTabColsEntry& rEntry = rToFill.GetEntry( j );
        tools::Long nCmp = rToFill[j];

        if ( (nPos >= ((nCmp >= COLFUZZY) ? nCmp - COLFUZZY : nCmp)) &&
             (nPos <= (nCmp + COLFUZZY)) )
        {
            // check if nLeftMin is > old minimum for entry nPos:
            const tools::Long nOldMin = rEntry.nMin;
            if ( nLeftMin > nOldMin )
                rEntry.nMin = nLeftMin;
            // check if nRightMin is < old maximum for entry nPos:
            const tools::Long nOldMax = rEntry.nMax;
            if ( nRightMax < nOldMax )
                rEntry.nMax = nRightMax;

            bFoundPos = true;
        }
        else if ( (nRightMax >= ((nCmp >= COLFUZZY) ? nCmp - COLFUZZY : nCmp)) &&
                  (nRightMax <= (nCmp + COLFUZZY)) )
        {
            // check if nPos is > old minimum for entry nRightMax:
            const tools::Long nOldMin = rEntry.nMin;
            if ( nPos > nOldMin )
                rEntry.nMin = nPos;

            bFoundMax = true;
        }
    }
}

static void lcl_ProcessBoxGet( const SwTableBox *pBox, SwTabCols &rToFill,
                        const SwFrameFormat *pTabFormat, bool bRefreshHidden )
{
    if ( !pBox->GetTabLines().empty() )
    {
        const SwTableLines &rLines = pBox->GetTabLines();
        for ( size_t i = 0; i < rLines.size(); ++i )
        {
            const SwTableBoxes &rBoxes = rLines[i]->GetTabBoxes();
            for ( size_t j = 0; j < rBoxes.size(); ++j )
                ::lcl_ProcessBoxGet( rBoxes[j], rToFill, pTabFormat, bRefreshHidden);
        }
    }
    else
        ::lcl_SortedTabColInsert( rToFill, pBox, pTabFormat, false, bRefreshHidden );
}

static void lcl_ProcessLineGet( const SwTableLine *pLine, SwTabCols &rToFill,
                         const SwFrameFormat *pTabFormat )
{
    for ( size_t i = 0; i < pLine->GetTabBoxes().size(); ++i )
    {
        const SwTableBox *pBox = pLine->GetTabBoxes()[i];
        if ( pBox->GetSttNd() )
            ::lcl_SortedTabColInsert( rToFill, pBox, pTabFormat, true, false );
        else
            for ( size_t j = 0; j < pBox->GetTabLines().size(); ++j )
                ::lcl_ProcessLineGet( pBox->GetTabLines()[j], rToFill, pTabFormat );
    }
}

void SwTable::GetTabCols( SwTabCols &rToFill, const SwTableBox *pStart,
              bool bRefreshHidden, bool bCurRowOnly ) const
{
    // Optimization: if bHidden is set, we only update the Hidden Array.
    if ( bRefreshHidden )
    {
        // remove corrections
        for ( size_t i = 0; i < rToFill.Count(); ++i )
        {
            SwTabColsEntry& rEntry = rToFill.GetEntry( i );
            rEntry.nPos -= rToFill.GetLeft();
            rEntry.nMin -= rToFill.GetLeft();
            rEntry.nMax -= rToFill.GetLeft();
        }

        // All are hidden, so add the visible ones.
        for ( size_t i = 0; i < rToFill.Count(); ++i )
            rToFill.SetHidden( i, true );
    }
    else
    {
        rToFill.Remove( 0, rToFill.Count() );
    }

    // Insertion cases:
    // 1. All boxes which are inferior to Line which is superior to the Start,
    //    as well as their inferior boxes if present.
    // 2. Starting from the Line, the superior box plus its neighbours; but no inferiors.
    // 3. Apply 2. to the Line superior to the chain of boxes,
    //    until the Line's superior is not a box but the table.
    // Only those boxes are inserted that don't contain further rows. The insertion
    // function takes care to avoid duplicates. In order to achieve this, we work
    // with some degree of fuzzyness (to avoid rounding errors).
    // Only the left edge of the boxes are inserted.
    // Finally, the first entry is removed again, because it's already
    // covered by the border.
    // 4. Scan the table again and insert _all_ boxes, this time as hidden.

    const SwFrameFormat *pTabFormat = GetFrameFormat();

    // 1.
    const SwTableBoxes &rBoxes = pStart->GetUpper()->GetTabBoxes();

    for ( size_t i = 0; i < rBoxes.size(); ++i )
        ::lcl_ProcessBoxGet( rBoxes[i], rToFill, pTabFormat, bRefreshHidden );

    // 2. and 3.
    const SwTableLine *pLine = pStart->GetUpper()->GetUpper() ?
                                pStart->GetUpper()->GetUpper()->GetUpper() : nullptr;
    while ( pLine )
    {
        const SwTableBoxes &rBoxes2 = pLine->GetTabBoxes();
        for ( size_t k = 0; k < rBoxes2.size(); ++k )
            ::lcl_SortedTabColInsert( rToFill, rBoxes2[k],
                                      pTabFormat, false, bRefreshHidden );
        pLine = pLine->GetUpper() ? pLine->GetUpper()->GetUpper() : nullptr;
    }

    if ( !bRefreshHidden )
    {
        // 4.
        if ( !bCurRowOnly )
        {
            for ( size_t i = 0; i < m_aLines.size(); ++i )
                ::lcl_ProcessLineGet( m_aLines[i], rToFill, pTabFormat );
        }

        rToFill.Remove( 0 );
    }

    // Now the coordinates are relative to the left table border - i.e.
    // relative to SwTabCols.nLeft. However, they are expected
    // relative to the left document border, i.e. SwTabCols.nLeftMin.
    // So all values need to be extended by nLeft.
    for ( size_t i = 0; i < rToFill.Count(); ++i )
    {
        SwTabColsEntry& rEntry = rToFill.GetEntry( i );
        rEntry.nPos += rToFill.GetLeft();
        rEntry.nMin += rToFill.GetLeft();
        rEntry.nMax += rToFill.GetLeft();
    }
}

// Structure for parameter passing
struct Parm
{
    const SwTabCols &rNew;
    const SwTabCols &rOld;
    tools::Long nNewWish,
         nOldWish;
    std::deque<SwTableBox*> aBoxArr;
    SwShareBoxFormats aShareFormats;

    Parm( const SwTabCols &rN, const SwTabCols &rO )
        : rNew( rN ), rOld( rO ), nNewWish(0), nOldWish(0)
    {}
};

static void lcl_ProcessBoxSet( SwTableBox *pBox, Parm &rParm );

static void lcl_ProcessLine( SwTableLine *pLine, Parm &rParm )
{
    SwTableBoxes &rBoxes = pLine->GetTabBoxes();
    for ( size_t i = rBoxes.size(); i > 0; )
    {
        --i;
        ::lcl_ProcessBoxSet( rBoxes[i], rParm );
    }
}

static void lcl_ProcessBoxSet( SwTableBox *pBox, Parm &rParm )
{
    if ( !pBox->GetTabLines().empty() )
    {
        SwTableLines &rLines = pBox->GetTabLines();
        for ( size_t i = rLines.size(); i > 0; )
        {
            --i;
            lcl_ProcessLine( rLines[i], rParm );
        }
    }
    else
    {
        // Search the old TabCols for the current position (calculate from
        // left and right edge). Adjust the box if the values differ from
        // the new TabCols. If the adjusted edge has no neighbour we also
        // adjust all superior boxes.

        const tools::Long nOldAct = rParm.rOld.GetRight() -
                             rParm.rOld.GetLeft(); // +1 why?

        // The value for the left edge of the box is calculated from the
        // widths of the previous boxes plus the left edge.
        tools::Long nLeft = rParm.rOld.GetLeft();
        const  SwTableBox  *pCur  = pBox;
        const  SwTableLine *pLine = pBox->GetUpper();

        while ( pLine )
        {
            const SwTableBoxes &rBoxes = pLine->GetTabBoxes();
            for ( size_t i = 0; (i < rBoxes.size()) && (rBoxes[i] != pCur); ++i)
            {
                nLeft += lcl_MulDiv64<tools::Long>(
                    rBoxes[i]->GetFrameFormat()->GetFrameSize().GetWidth(),
                    nOldAct, rParm.nOldWish);
            }
            pCur  = pLine->GetUpper();
            pLine = pCur ? pCur->GetUpper() : nullptr;
        }
        tools::Long nLeftDiff = 0;
        tools::Long nRightDiff = 0;
        if ( nLeft != rParm.rOld.GetLeft() ) // There are still boxes before this.
        {
            // Right edge is left edge plus width.
            const tools::Long nWidth = lcl_MulDiv64<tools::Long>(
                pBox->GetFrameFormat()->GetFrameSize().GetWidth(),
                nOldAct, rParm.nOldWish);
            const tools::Long nRight = nLeft + nWidth;
            size_t nLeftPos  = 0;
            size_t nRightPos = 0;
            bool bFoundLeftPos = false;
            bool bFoundRightPos = false;
            for ( size_t i = 0; i < rParm.rOld.Count(); ++i )
            {
                if ( nLeft >= (rParm.rOld[i] - COLFUZZY) &&
                     nLeft <= (rParm.rOld[i] + COLFUZZY) )
                {
                    nLeftPos = i;
                    bFoundLeftPos = true;
                }
                else if ( nRight >= (rParm.rOld[i] - COLFUZZY) &&
                          nRight <= (rParm.rOld[i] + COLFUZZY) )
                {
                    nRightPos = i;
                    bFoundRightPos = true;
                }
            }
            nLeftDiff = bFoundLeftPos ?
                rParm.rOld[nLeftPos] - rParm.rNew[nLeftPos] : 0;
            nRightDiff= bFoundRightPos ?
                rParm.rNew[nRightPos] - rParm.rOld[nRightPos] : 0;
        }
        else    // The first box.
        {
            nLeftDiff = rParm.rOld.GetLeft() - rParm.rNew.GetLeft();
            if ( rParm.rOld.Count() )
            {
                // Calculate the difference to the edge touching the first box.
                const tools::Long nWidth = lcl_MulDiv64<tools::Long>(
                    pBox->GetFrameFormat()->GetFrameSize().GetWidth(),
                    nOldAct, rParm.nOldWish);
                const tools::Long nTmp = nWidth + rParm.rOld.GetLeft();
                for ( size_t i = 0; i < rParm.rOld.Count(); ++i )
                {
                    if ( nTmp >= (rParm.rOld[i] - COLFUZZY) &&
                         nTmp <= (rParm.rOld[i] + COLFUZZY) )
                    {
                        nRightDiff = rParm.rNew[i] - rParm.rOld[i];
                        break;
                    }
                }
            }
        }

        if( pBox->getRowSpan() == 1 )
        {
            const sal_uInt16 nPos = pBox->GetUpper()->GetBoxPos( pBox );
            SwTableBoxes& rTableBoxes = pBox->GetUpper()->GetTabBoxes();
            if( nPos && rTableBoxes[ nPos - 1 ]->getRowSpan() != 1 )
                nLeftDiff = 0;
            if( nPos + 1 < o3tl::narrowing<sal_uInt16>(rTableBoxes.size()) &&
                rTableBoxes[ nPos + 1 ]->getRowSpan() != 1 )
                nRightDiff = 0;
        }
        else
            nLeftDiff = nRightDiff = 0;

        if ( nLeftDiff || nRightDiff )
        {
            // The difference is the actual difference amount. For stretched
            // tables, it does not make sense to adjust the attributes of the
            // boxes by this amount. The difference amount needs to be converted
            // accordingly.
            tools::Long nTmp = rParm.rNew.GetRight() - rParm.rNew.GetLeft(); // +1 why?
            nLeftDiff *= rParm.nNewWish;
            nLeftDiff /= nTmp;
            nRightDiff *= rParm.nNewWish;
            nRightDiff /= nTmp;
            tools::Long nDiff = nLeftDiff + nRightDiff;

            // Adjust the box and all superiors by the difference amount.
            while ( pBox )
            {
                SwFormatFrameSize aFormatFrameSize( pBox->GetFrameFormat()->GetFrameSize() );
                aFormatFrameSize.SetWidth( aFormatFrameSize.GetWidth() + nDiff );
                if ( aFormatFrameSize.GetWidth() < 0 )
                    aFormatFrameSize.SetWidth( -aFormatFrameSize.GetWidth() );
                rParm.aShareFormats.SetSize( *pBox, aFormatFrameSize );

                // The outer cells of the last row are responsible to adjust a surrounding cell.
                // Last line check:
                if ( pBox->GetUpper()->GetUpper() &&
                     pBox->GetUpper() != pBox->GetUpper()->GetUpper()->GetTabLines().back())
                {
                    pBox = nullptr;
                }
                else
                {
                    // Middle cell check:
                    if ( pBox != pBox->GetUpper()->GetTabBoxes().front() )
                        nDiff = nRightDiff;

                    if ( pBox != pBox->GetUpper()->GetTabBoxes().back() )
                        nDiff -= nRightDiff;

                    pBox = nDiff ? pBox->GetUpper()->GetUpper() : nullptr;
                }
            }
        }
    }
}

static void lcl_ProcessBoxPtr( SwTableBox *pBox, std::deque<SwTableBox*> &rBoxArr,
                           bool bBefore )
{
    if ( !pBox->GetTabLines().empty() )
    {
        const SwTableLines &rLines = pBox->GetTabLines();
        for ( size_t i = 0; i < rLines.size(); ++i )
        {
            const SwTableBoxes &rBoxes = rLines[i]->GetTabBoxes();
            for ( size_t j = 0; j < rBoxes.size(); ++j )
                ::lcl_ProcessBoxPtr( rBoxes[j], rBoxArr, bBefore );
        }
    }
    else if ( bBefore )
        rBoxArr.push_front( pBox );
    else
        rBoxArr.push_back( pBox );
}

static void lcl_AdjustBox( SwTableBox *pBox, const tools::Long nDiff, Parm &rParm );

static void lcl_AdjustLines( SwTableLines &rLines, const tools::Long nDiff, Parm &rParm )
{
    for ( size_t i = 0; i < rLines.size(); ++i )
    {
        SwTableBox *pBox = rLines[i]->GetTabBoxes()
                                [rLines[i]->GetTabBoxes().size()-1];
        lcl_AdjustBox( pBox, nDiff, rParm );
    }
}

static void lcl_AdjustBox( SwTableBox *pBox, const tools::Long nDiff, Parm &rParm )
{
    if ( !pBox->GetTabLines().empty() )
        ::lcl_AdjustLines( pBox->GetTabLines(), nDiff, rParm );

    // Adjust the size of the box.
    SwFormatFrameSize aFormatFrameSize( pBox->GetFrameFormat()->GetFrameSize() );
    aFormatFrameSize.SetWidth( aFormatFrameSize.GetWidth() + nDiff );

    rParm.aShareFormats.SetSize( *pBox, aFormatFrameSize );
}

void SwTable::SetTabCols( const SwTabCols &rNew, const SwTabCols &rOld,
                          const SwTableBox *pStart, bool bCurRowOnly )
{
    CHECK_TABLE( *this )

    SetHTMLTableLayout(std::shared_ptr<SwHTMLTableLayout>());    // delete HTML-Layout

    // FME: Made rOld const. The caller is responsible for passing correct
    // values of rOld. Therefore we do not have to call GetTabCols anymore:
    //GetTabCols( rOld, pStart );

    Parm aParm( rNew, rOld );

    OSL_ENSURE( rOld.Count() == rNew.Count(), "Number of columns changed.");

    // Convert the edges. We need to adjust the size of the table and some boxes.
    // For the size adjustment, we must not make use of the Modify, since that'd
    // adjust all boxes, which we really don't want.
    SwFrameFormat *pFormat = GetFrameFormat();
    aParm.nOldWish = aParm.nNewWish = pFormat->GetFrameSize().GetWidth();
    if ( (rOld.GetLeft() != rNew.GetLeft()) ||
         (rOld.GetRight()!= rNew.GetRight()) )
    {
        LockModify();
        {
            SvxLRSpaceItem aLR( pFormat->GetLRSpace() );
            SvxShadowItem aSh( pFormat->GetShadow() );

            SwTwips nShRight = aSh.CalcShadowSpace( SvxShadowItemSide::RIGHT );
            SwTwips nShLeft = aSh.CalcShadowSpace( SvxShadowItemSide::LEFT );

            aLR.SetLeft(SvxIndentValue::twips(rNew.GetLeft() - nShLeft));
            aLR.SetRight(SvxIndentValue::twips(rNew.GetRightMax() - rNew.GetRight() - nShRight));
            pFormat->SetFormatAttr( aLR );

            // The alignment of the table needs to be adjusted accordingly.
            // This is done by preserving the exact positions that have been
            // set by the user.
            SwFormatHoriOrient aOri( pFormat->GetHoriOrient() );
            if( text::HoriOrientation::NONE != aOri.GetHoriOrient() &&
                text::HoriOrientation::CENTER != aOri.GetHoriOrient() )
            {
                const bool bLeftDist = rNew.GetLeft() != nShLeft;
                const bool bRightDist = rNew.GetRight() + nShRight != rNew.GetRightMax();
                if(!bLeftDist && !bRightDist)
                    aOri.SetHoriOrient( text::HoriOrientation::FULL );
                else if(!bRightDist && rNew.GetLeft() > nShLeft )
                    aOri.SetHoriOrient( text::HoriOrientation::RIGHT );
                else if(!bLeftDist && rNew.GetRight() + nShRight < rNew.GetRightMax())
                    aOri.SetHoriOrient( text::HoriOrientation::LEFT );
                else
                {
                    // if an automatic table hasn't (really) changed size, then leave it as auto.
                    const tools::Long nOldWidth = rOld.GetRight() - rOld.GetLeft();
                    const tools::Long nNewWidth = rNew.GetRight() - rNew.GetLeft();
                    if (aOri.GetHoriOrient() != text::HoriOrientation::FULL
                        || std::abs(nOldWidth - nNewWidth) > COLFUZZY)
                    {
                        aOri.SetHoriOrient(text::HoriOrientation::LEFT_AND_WIDTH);
                    }
                }
            }
            pFormat->SetFormatAttr( aOri );
        }
        const tools::Long nAct = rOld.GetRight() - rOld.GetLeft(); // +1 why?
        tools::Long nTabDiff = 0;

        if ( rOld.GetLeft() != rNew.GetLeft() )
        {
            nTabDiff = rOld.GetLeft() - rNew.GetLeft();
            nTabDiff *= aParm.nOldWish;
            nTabDiff /= nAct;
        }
        if ( rOld.GetRight() != rNew.GetRight() )
        {
            tools::Long nDiff = rNew.GetRight() - rOld.GetRight();
            nDiff *= aParm.nOldWish;
            nDiff /= nAct;
            nTabDiff += nDiff;
            if( !IsNewModel() )
                ::lcl_AdjustLines( GetTabLines(), nDiff, aParm );
        }

        // Adjust the size of the table, watch out for stretched tables.
        if ( nTabDiff )
        {
            aParm.nNewWish += nTabDiff;
            if ( aParm.nNewWish < 0 )
                aParm.nNewWish = USHRT_MAX; // Oops! Have to roll back.
            SwFormatFrameSize aSz( pFormat->GetFrameSize() );
            if ( aSz.GetWidth() != aParm.nNewWish )
            {
                aSz.SetWidth( aParm.nNewWish );
                aSz.SetWidthPercent( 0 );
                pFormat->SetFormatAttr( aSz );
            }
        }
        UnlockModify();
    }

    if( IsNewModel() )
        NewSetTabCols( aParm, rNew, rOld, pStart, bCurRowOnly );
    else
    {
        if ( bCurRowOnly )
        {
            // To adjust the current row, we need to process all its boxes,
            // similar to the filling of the TabCols (see GetTabCols()).
            // Unfortunately we again have to take care to adjust the boxes
            // from back to front, respectively from outer to inner.
            // The best way to achieve this is probably to track the boxes
            // in a PtrArray.
            const SwTableBoxes &rBoxes = pStart->GetUpper()->GetTabBoxes();
            for ( size_t i = 0; i < rBoxes.size(); ++i )
                ::lcl_ProcessBoxPtr( rBoxes[i], aParm.aBoxArr, false );

            const SwTableLine *pLine = pStart->GetUpper()->GetUpper() ?
                                    pStart->GetUpper()->GetUpper()->GetUpper() : nullptr;
            const SwTableBox  *pExcl = pStart->GetUpper()->GetUpper();
            while ( pLine )
            {
                const SwTableBoxes &rBoxes2 = pLine->GetTabBoxes();
                bool bBefore = true;
                for ( size_t i = 0; i < rBoxes2.size(); ++i )
                {
                    if ( rBoxes2[i] != pExcl )
                        ::lcl_ProcessBoxPtr( rBoxes2[i], aParm.aBoxArr, bBefore );
                    else
                        bBefore = false;
                }
                pExcl = pLine->GetUpper();
                pLine = pLine->GetUpper() ? pLine->GetUpper()->GetUpper() : nullptr;
            }
            // After we've inserted a bunch of boxes (hopefully all and in
            // correct order), we just need to process them in reverse order.
            for ( int j = aParm.aBoxArr.size()-1; j >= 0; --j )
            {
                SwTableBox *pBox = aParm.aBoxArr[j];
                ::lcl_ProcessBoxSet( pBox, aParm );
            }
        }
        else
        {
            // Adjusting the entire table is 'easy'. All boxes without lines are
            // adjusted, as are their superiors. Of course we need to process
            // in reverse order to prevent fooling ourselves!
            SwTableLines &rLines = GetTabLines();
            for ( size_t i = rLines.size(); i > 0; )
            {
                --i;
                ::lcl_ProcessLine( rLines[i], aParm );
            }
        }
    }

#ifdef DBG_UTIL
    CheckBoxWidth(GetTabLines(), *GetFrameFormat());
#endif
}

typedef std::pair<sal_uInt16, sal_uInt16> ColChange;
typedef std::list< ColChange > ChangeList;

static void lcl_AdjustWidthsInLine( SwTableLine* pLine, ChangeList& rOldNew,
    Parm& rParm, sal_uInt16 nColFuzzy )
{
    ChangeList::iterator pCurr = rOldNew.begin();
    if( pCurr == rOldNew.end() )
        return;
    const size_t nCount = pLine->GetTabBoxes().size();
    SwTwips nBorder = 0;
    SwTwips nRest = 0;
    for( size_t i = 0; i < nCount; ++i )
    {
        SwTableBox* pBox = pLine->GetTabBoxes()[i];
        SwTwips nWidth = pBox->GetFrameFormat()->GetFrameSize().GetWidth();
        SwTwips nNewWidth = nWidth - nRest;
        nRest = 0;
        nBorder += nWidth;
        if( pCurr != rOldNew.end() && nBorder + nColFuzzy >= pCurr->first )
        {
            nBorder -= nColFuzzy;
            while( pCurr != rOldNew.end() && nBorder > pCurr->first )
                ++pCurr;
            if( pCurr != rOldNew.end() )
            {
                nBorder += nColFuzzy;
                if( nBorder + nColFuzzy >= pCurr->first )
                {
                    if( pCurr->second == pCurr->first )
                        nRest = 0;
                    else
                        nRest = pCurr->second - nBorder;
                    nNewWidth += nRest;
                    ++pCurr;
                }
            }
        }
        if( nNewWidth != nWidth )
        {
            if( nNewWidth < 0 )
            {
                nRest += 1 - nNewWidth;
                nNewWidth = 1;
            }
            SwFormatFrameSize aFormatFrameSize( pBox->GetFrameFormat()->GetFrameSize() );
            aFormatFrameSize.SetWidth( nNewWidth );
            rParm.aShareFormats.SetSize( *pBox, aFormatFrameSize );
        }
    }
}

static void lcl_CalcNewWidths( std::vector<sal_uInt16> &rSpanPos, ChangeList& rChanges,
    SwTableLine* pLine, tools::Long nWish, tools::Long nWidth, bool bTop )
{
    if( rChanges.empty() )
    {
        rSpanPos.clear();
        return;
    }
    if( rSpanPos.empty() )
    {
        rChanges.clear();
        return;
    }
    std::vector<sal_uInt16> aNewSpanPos;
    ChangeList::iterator pCurr = rChanges.begin();
    ChangeList aNewChanges { *pCurr }; // Nullposition
    std::vector<sal_uInt16>::iterator pSpan = rSpanPos.begin();
    sal_uInt16 nCurr = 0;
    SwTwips nOrgSum = 0;
    bool bRowSpan = false;
    sal_uInt16 nRowSpanCount = 0;
    const size_t nCount = pLine->GetTabBoxes().size();
    for( size_t nCurrBox = 0; nCurrBox < nCount; ++nCurrBox )
    {
        SwTableBox* pBox = pLine->GetTabBoxes()[nCurrBox];
        SwTwips nCurrWidth = pBox->GetFrameFormat()->GetFrameSize().GetWidth();
        const sal_Int32 nRowSpan = pBox->getRowSpan();
        const bool bCurrRowSpan = bTop ? nRowSpan < 0 :
            ( nRowSpan > 1 || nRowSpan < -1 );
        if( bRowSpan || bCurrRowSpan )
            aNewSpanPos.push_back( nRowSpanCount );
        bRowSpan = bCurrRowSpan;
        nOrgSum += nCurrWidth;
        const sal_uInt16 nPos = lcl_MulDiv64<sal_uInt16>(
            lcl_MulDiv64<sal_uInt64>(nOrgSum, nWidth, nWish),
            nWish, nWidth);
        while( pCurr != rChanges.end() && pCurr->first < nPos )
        {
            ++nCurr;
            ++pCurr;
        }
        bool bNew = true;
        if( pCurr != rChanges.end() && pCurr->first <= nPos &&
            pCurr->first != pCurr->second )
        {
            pSpan = std::find_if(pSpan, rSpanPos.end(),
                [nCurr](const sal_uInt16 nSpan) { return nSpan >= nCurr; });
            if( pSpan != rSpanPos.end() && *pSpan == nCurr )
            {
                aNewChanges.push_back( *pCurr );
                ++nRowSpanCount;
                bNew = false;
            }
        }
        if( bNew )
        {
            ColChange aTmp( nPos, nPos );
            aNewChanges.push_back( aTmp );
            ++nRowSpanCount;
        }
    }

    pCurr = aNewChanges.begin();
    ChangeList::iterator pLast = pCurr;
    ChangeList::iterator pLeftMove = pCurr;
    while( pCurr != aNewChanges.end() )
    {
        if( pLeftMove == pCurr )
        {
            while( ++pLeftMove != aNewChanges.end() && pLeftMove->first <= pLeftMove->second )
                ;
        }
        if( pCurr->second == pCurr->first )
        {
            if( pLeftMove != aNewChanges.end() && pCurr->second > pLeftMove->second )
            {
                if( pLeftMove->first == pLast->first )
                    pCurr->second = pLeftMove->second;
                else
                {
                    pCurr->second = lcl_MulDiv64<sal_uInt16>(
                        pCurr->first - pLast->first,
                        pLeftMove->second - pLast->second,
                        pLeftMove->first - pLast->first) + pLast->second;
                }
            }
            pLast = pCurr;
            ++pCurr;
        }
        else if( pCurr->second > pCurr->first )
        {
            pLast = pCurr;
            ++pCurr;
            ChangeList::iterator pNext = pCurr;
            while( pNext != pLeftMove && pNext->second == pNext->first &&
                pNext->second < pLast->second )
                ++pNext;
            while( pCurr != pNext )
            {
                if( pNext == aNewChanges.end() || pNext->first == pLast->first )
                    pCurr->second = pLast->second;
                else
                {
                    pCurr->second = lcl_MulDiv64<sal_uInt16>(
                        pCurr->first - pLast->first,
                        pNext->second - pLast->second,
                        pNext->first - pLast->first) + pLast->second;
                }
                ++pCurr;
            }
            pLast = pCurr;
        }
        else
        {
            pLast = pCurr;
            ++pCurr;
        }
    }

    rChanges.swap(aNewChanges);
    rSpanPos.swap(aNewSpanPos);
}

void SwTable::NewSetTabCols( Parm &rParm, const SwTabCols &rNew,
    const SwTabCols &rOld, const SwTableBox *pStart, bool bCurRowOnly )
{
#if OSL_DEBUG_LEVEL > 1
    static int nCallCount = 0;
    ++nCallCount;
#endif
    // First step: evaluate which lines have been moved/which widths changed
    ChangeList aOldNew;
    const tools::Long nNewWidth = rParm.rNew.GetRight() - rParm.rNew.GetLeft();
    const tools::Long nOldWidth = rParm.rOld.GetRight() - rParm.rOld.GetLeft();
    if( nNewWidth < 1 || nOldWidth < 1 )
        return;
    for( size_t i = 0; i <= rOld.Count(); ++i )
    {
        tools::Long nNewPos;
        tools::Long nOldPos;
        if( i == rOld.Count() )
        {
            nOldPos = rParm.rOld.GetRight() - rParm.rOld.GetLeft();
            nNewPos = rParm.rNew.GetRight() - rParm.rNew.GetLeft();
        }
        else
        {
            nOldPos = rOld[i] - rParm.rOld.GetLeft();
            nNewPos = rNew[i] - rParm.rNew.GetLeft();
        }
        nNewPos = lcl_MulDiv64<tools::Long>(nNewPos, rParm.nNewWish, nNewWidth);
        nOldPos = lcl_MulDiv64<tools::Long>(nOldPos, rParm.nOldWish, nOldWidth);
        if( nOldPos != nNewPos && nNewPos > 0 && nOldPos > 0 )
        {
            ColChange aChg( o3tl::narrowing<sal_uInt16>(nOldPos), o3tl::narrowing<sal_uInt16>(nNewPos) );
            aOldNew.push_back( aChg );
        }
    }
    // Finished first step
    int nCount = aOldNew.size();
    if( !nCount )
        return; // no change, nothing to do
    SwTableLines &rLines = GetTabLines();
    if( bCurRowOnly )
    {
        const SwTableLine* pCurrLine = pStart->GetUpper();
        sal_uInt16 nCurr = rLines.GetPos( pCurrLine );
        if( nCurr >= USHRT_MAX )
            return;

        ColChange aChg( 0, 0 );
        aOldNew.push_front( aChg );
        std::vector<sal_uInt16> aRowSpanPos;
        if (nCurr > 0)
        {
            ChangeList aCopy;
            sal_uInt16 nPos = 0;
            for( const auto& rCop : aOldNew )
            {
                aCopy.push_back( rCop );
                aRowSpanPos.push_back( nPos++ );
            }
            lcl_CalcNewWidths( aRowSpanPos, aCopy, rLines[nCurr],
                rParm.nOldWish, nOldWidth, true );
            sal_uInt16 j = nCurr;
            while (!aRowSpanPos.empty() && j > 0)
            {
                j = o3tl::sanitizing_dec(j);
                lcl_CalcNewWidths( aRowSpanPos, aCopy, rLines[j],
                    rParm.nOldWish, nOldWidth, true );
                lcl_AdjustWidthsInLine( rLines[j], aCopy, rParm, 0 );
            }
            aRowSpanPos.clear();
        }
        if( nCurr+1 < o3tl::narrowing<sal_uInt16>(rLines.size()) )
        {
            ChangeList aCopy;
            sal_uInt16 nPos = 0;
            for( const auto& rCop : aOldNew )
            {
                aCopy.push_back( rCop );
                aRowSpanPos.push_back( nPos++ );
            }
            lcl_CalcNewWidths( aRowSpanPos, aCopy, rLines[nCurr],
                rParm.nOldWish, nOldWidth, false );
            bool bGoOn = !aRowSpanPos.empty();
            sal_uInt16 j = nCurr;
            while( bGoOn )
            {
                lcl_CalcNewWidths( aRowSpanPos, aCopy, rLines[++j],
                    rParm.nOldWish, nOldWidth, false );
                lcl_AdjustWidthsInLine( rLines[j], aCopy, rParm, 0 );
                bGoOn = !aRowSpanPos.empty() && j+1 < o3tl::narrowing<sal_uInt16>(rLines.size());
            }
        }
        ::lcl_AdjustWidthsInLine( rLines[nCurr], aOldNew, rParm, COLFUZZY );
    }
    else
    {
        for( size_t i = 0; i < rLines.size(); ++i )
            ::lcl_AdjustWidthsInLine( rLines[i], aOldNew, rParm, COLFUZZY );
    }
    CHECK_TABLE( *this )
}

// return the pointer of the box specified.
static bool lcl_IsValidRowName( std::u16string_view rStr )
{
    bool bIsValid = true;
    size_t nLen = rStr.size();
    for( size_t i = 0;  i < nLen && bIsValid; ++i )
    {
        const sal_Unicode cChar = rStr[i];
        if (cChar < '0' || cChar > '9')
            bIsValid = false;
    }
    return bIsValid;
}

// #i80314#
// add 3rd parameter and its handling
sal_uInt16 SwTable::GetBoxNum( OUString& rStr, bool bFirstPart,
                            const bool bPerformValidCheck )
{
    sal_uInt16 nRet = 0;
    if( bFirstPart )   // true == column; false == row
    {
        sal_Int32 nPos = 0;
        // the first one uses letters for addressing!
        bool bFirst = true;
        sal_uInt32 num = 0;
        bool overflow = false;
        while (nPos<rStr.getLength())
        {
            sal_Unicode cChar = rStr[nPos];
            if ((cChar<'A' || cChar>'Z') && (cChar<'a' || cChar>'z'))
                break;
            cChar -= 'A';
            if( cChar >= 26 )
                cChar -= 'a' - '[';
            if( bFirst )
                bFirst = false;
            else
                ++num;
            num = num * 52 + cChar;
            if (num > SAL_MAX_UINT16) {
                overflow = true;
            }
            ++nPos;
        }
        nRet = overflow ? SAL_MAX_UINT16 : num;
        rStr = rStr.copy( nPos );      // Remove char from String
    }
    else
    {
        const sal_Int32 nPos = rStr.indexOf( "." );
        if ( nPos<0 )
        {
            nRet = 0;
            if ( !bPerformValidCheck || lcl_IsValidRowName( rStr ) )
            {
                nRet = o3tl::narrowing<sal_uInt16>(rStr.toInt32());
            }
            rStr.clear();
        }
        else
        {
            nRet = 0;
            const std::u16string_view aText( rStr.subView( 0, nPos ) );
            if ( !bPerformValidCheck || lcl_IsValidRowName( aText ) )
            {
                nRet = o3tl::narrowing<sal_uInt16>(o3tl::toInt32(aText));
            }
            rStr = rStr.copy( nPos+1 );
        }
    }
    return nRet;
}

// #i80314#
// add 2nd parameter and its handling
const SwTableBox* SwTable::GetTableBox( const OUString& rName,
                                      const bool bPerformValidCheck ) const
{
    const SwTableBox* pBox = nullptr;
    const SwTableLine* pLine;
    const SwTableLines* pLines;

    sal_uInt16 nLine, nBox;
    OUString aNm( rName );
    while( !aNm.isEmpty() )
    {
        nBox = SwTable::GetBoxNum( aNm, nullptr == pBox, bPerformValidCheck );
        // first box ?
        if( !pBox )
            pLines = &GetTabLines();
        else
        {
            pLines = &pBox->GetTabLines();
            if( nBox )
                --nBox;
        }

        nLine = SwTable::GetBoxNum( aNm, false, bPerformValidCheck );

        // determine line
        if( !nLine || nLine > pLines->size() )
            return nullptr;
        pLine = (*pLines)[ nLine-1 ];

        // determine box
        const SwTableBoxes* pBoxes = &pLine->GetTabBoxes();
        if( nBox >= pBoxes->size() )
            return nullptr;
        pBox = (*pBoxes)[ nBox ];
    }

    // check if the box found has any contents
    if( pBox && !pBox->GetSttNd() )
    {
        OSL_FAIL( "Box without content, looking for the next one!" );
        // "drop this" until the first box
        while( !pBox->GetTabLines().empty() )
            pBox = pBox->GetTabLines().front()->GetTabBoxes().front();
    }
    return pBox;
}

SwTableBox* SwTable::GetTableBox( SwNodeOffset nSttIdx )
{
    // For optimizations, don't always process the entire SortArray.
    // Converting text to table, tries certain conditions
    // to ask for a table box of a table that is not yet having a format
    if(!GetFrameFormat())
        return nullptr;
    SwTableBox* pRet = nullptr;
    SwNodes& rNds = GetFrameFormat()->GetDoc().GetNodes();
    SwNodeOffset nIndex = nSttIdx + 1;
    SwContentNode* pCNd = nullptr;
    SwTableNode* pTableNd = nullptr;

    while ( nIndex < rNds.Count() )
    {
        pTableNd = rNds[ nIndex ]->GetTableNode();
        if ( pTableNd )
            break;

        pCNd = rNds[ nIndex ]->GetContentNode();
        if ( pCNd )
            break;

        ++nIndex;
    }

    if ( pCNd || pTableNd )
    {
        sw::BroadcastingModify* pModify = pCNd;
        // #144862# Better handling of table in table
        if ( pTableNd && pTableNd->GetTable().GetFrameFormat() )
            pModify = pTableNd->GetTable().GetFrameFormat();

        SwFrame* pFrame = pModify ? SwIterator<SwFrame,sw::BroadcastingModify>(*pModify).First() : nullptr;
        while ( pFrame && !pFrame->IsCellFrame() )
            pFrame = pFrame->GetUpper();
        if ( pFrame )
            pRet = const_cast<SwTableBox*>(static_cast<SwCellFrame*>(pFrame)->GetTabBox());
    }

    // In case the layout doesn't exist yet or anything else goes wrong.
    if ( !pRet )
    {
        for (size_t n = m_TabSortContentBoxes.size(); n; )
        {
            if (m_TabSortContentBoxes[ --n ]->GetSttIdx() == nSttIdx)
            {
                return m_TabSortContentBoxes[ n ];
            }
        }
    }
    return pRet;
}

bool SwTable::IsTableComplex() const
{
    // Returns true for complex tables, i.e. tables that contain nestings,
    // like containing boxes not part of the first line, e.g. results of
    // splits/merges which lead to more complex structures.
    for (size_t n = 0; n < m_TabSortContentBoxes.size(); ++n)
    {
        if (m_TabSortContentBoxes[ n ]->GetUpper()->GetUpper())
        {
            return true;
        }
    }
    return false;
}

SwTableLine::SwTableLine( SwTableLineFormat *pFormat, sal_uInt16 nBoxes,
                            SwTableBox *pUp )
    : SwClient( pFormat )
    , m_pUpper( pUp )
    , m_eRedlineType( RedlineType::None )
{
    m_aBoxes.reserve( nBoxes );
}

SwTableLine::~SwTableLine()
{
    for (size_t i = 0; i < m_aBoxes.size(); ++i)
    {
        delete m_aBoxes[i];
    }
    // the TabelleLine can be deleted if it's the last client of the FrameFormat
    sw::BroadcastingModify* pMod = GetFrameFormat();
    pMod->Remove(*this);               // remove,
    if( !pMod->HasWriterListeners() )
        delete pMod;    // and delete
}

SwTableLineFormat* SwTableLine::ClaimFrameFormat()
{
    // This method makes sure that this object is an exclusive SwTableLine client
    // of an SwTableLineFormat object
    // If other SwTableLine objects currently listen to the same SwTableLineFormat as
    // this one, something needs to be done
    SwTableLineFormat *pRet = GetFrameFormat();
    SwIterator<SwTableLine,SwFormat> aIter( *pRet );
    for( SwTableLine* pLast = aIter.First(); pLast; pLast = aIter.Next() )
    {
        if ( pLast != this )
        {
            // found another SwTableLine that is a client of the current Format
            // create a new Format as a copy and use it for this object
            SwTableLineFormat *pNewFormat = pRet->GetDoc().MakeTableLineFormat();
            *pNewFormat = *pRet;

            // register SwRowFrames that know me as clients at the new Format
            SwIterator<SwRowFrame,SwFormat> aFrameIter( *pRet );
            for( SwRowFrame* pFrame = aFrameIter.First(); pFrame; pFrame = aFrameIter.Next() )
                if( pFrame->GetTabLine() == this )
                    pFrame->RegisterToFormat( *pNewFormat );

            // register myself
            pNewFormat->Add(*this);
            pRet = pNewFormat;
            break;
        }
    }

    return pRet;
}

void SwTableLine::ChgFrameFormat(SwTableLineFormat* pNewFormat)
{
    auto pOld = GetFrameFormat();
    pOld->CallSwClientNotify(sw::TableLineFormatChanged(*pNewFormat, *this));
    // Now, re-register self.
    pNewFormat->Add(*this);
    if(!pOld->HasWriterListeners())
        delete pOld;
}

SwTwips SwTableLine::GetTableLineHeight( bool& bLayoutAvailable ) const
{
    SwTwips nRet = 0;
    bLayoutAvailable = false;
    SwIterator<SwRowFrame,SwFormat> aIter( *GetFrameFormat() );
    // A row could appear several times in headers/footers so only one chain of master/follow tables
    // will be accepted...
    const SwTabFrame* pChain = nullptr; // My chain
    for( SwRowFrame* pLast = aIter.First(); pLast; pLast = aIter.Next() )
    {
        if (pLast->GetTabLine() != this)
            continue;

        const SwTabFrame* pTab = pLast->FindTabFrame();
        if (!pTab)
            continue;

        bLayoutAvailable = ( pTab->IsVertical() ) ?
                           ( 0 < pTab->getFrameArea().Height() ) :
                           ( 0 < pTab->getFrameArea().Width() );

        // The first one defines the chain, if a chain is defined, only members of the chain
        // will be added.
        if (!pChain || pChain->IsAnFollow( pTab ) || pTab->IsAnFollow(pChain))
        {
            pChain = pTab; // defines my chain (even it is already)
            if( pTab->IsVertical() )
                nRet += pLast->getFrameArea().Width();
            else
                nRet += pLast->getFrameArea().Height();
            // Optimization, if there are no master/follows in my chain, nothing more to add
            if( !pTab->HasFollow() && !pTab->IsFollow() )
                break;
            // This is not an optimization, this is necessary to avoid double additions of
            // repeating rows
            if( pTab->IsInHeadline(*pLast) )
                break;
        }
    }
    return nRet;
}

bool SwTableLine::IsEmpty() const
{
    for (size_t i = 0; i < m_aBoxes.size(); ++i)
    {
        if ( !m_aBoxes[i]->IsEmpty() )
            return false;
    }
    return true;
}

bool SwTable::IsEmpty() const
{
    for (size_t i = 0; i < m_aLines.size(); ++i)
    {
        if ( !m_aLines[i]->IsEmpty() )
            return false;
    }
    return true;
}

bool SwTable::HasDeletedRowOrCell() const
{
    const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
    if ( aRedlineTable.empty() )
        return false;

    SwRedlineTable::size_type nRedlinePos = 0;
    for (size_t i = 0; i < m_aLines.size(); ++i)
    {
        // has a deleted row
        if ( m_aLines[i]->IsDeleted(nRedlinePos) )
            return true;

        // has a deleted cell in the not deleted row
        SwTableBoxes& rBoxes = m_aLines[i]->GetTabBoxes();
        for( size_t j = 0; j < rBoxes.size(); ++j )
        {
            if ( RedlineType::Delete == rBoxes[j]->GetRedlineType() )
                return true;
        }
    }
    return false;
}

bool SwTable::IsDeleted() const
{
    const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
    if ( aRedlineTable.empty() )
        return false;

    SwRedlineTable::size_type nRedlinePos = 0;
    for (size_t i = 0; i < m_aLines.size(); ++i)
    {
        if ( !m_aLines[i]->IsDeleted(nRedlinePos) )
            return false;
    }
    return true;
}

void SwTable::GatherFormulas(std::vector<SwTableBoxFormula*>& rvFormulas)
{
    GatherFormulas(GetFrameFormat()->GetDoc(), rvFormulas);
}

void SwTable::GatherFormulas(SwDoc& rDoc, std::vector<SwTableBoxFormula*>& rvFormulas)
{
    rvFormulas.clear();
    sw::TableFrameFormats* pTableFrameFormats = rDoc.GetTableFrameFormats();
    for(SwTableFormat* pFormat : *pTableFrameFormats)
    {
        SwTable* pTable = FindTable(pFormat);
        if (!pTable)
            continue;
        SwTableLines& rTableLines = pTable->GetTabLines();
        for (SwTableLine* pTableLine : rTableLines)
        {
            SwTableBoxes& rTableBoxes = pTableLine->GetTabBoxes();
            for (SwTableBox* pTableBox : rTableBoxes)
            {
                SwTableBoxFormat* pTableBoxFormat = pTableBox->GetFrameFormat();
                if (const SwTableBoxFormula* pBoxFormula = pTableBoxFormat->GetItemIfSet( RES_BOXATR_FORMULA, false ))
                {
                    const SwNode* pNd = pBoxFormula->GetNodeOfFormula();
                    if(!pNd || &pNd->GetNodes() != &pNd->GetDoc().GetNodes()) // is this ever valid or should we assert here?
                        continue;
                    rvFormulas.push_back(const_cast<SwTableBoxFormula*>(pBoxFormula));
                }
            }
        }
    }
}

void SwTable::Split(const UIName& sNewTableName, sal_uInt16 nSplitLine, SwHistory* pHistory)
{
    SwTableFormulaUpdate aHint(this);
    aHint.m_eFlags = TBL_SPLITTBL;
    aHint.m_aData.pNewTableNm = &sNewTableName;
    aHint.m_nSplitLine = nSplitLine;

    std::vector<SwTableBoxFormula*> vFormulas;
    GatherFormulas(vFormulas);
    for(auto pBoxFormula: vFormulas)
    {
        const SwNode* pNd = pBoxFormula->GetNodeOfFormula();
        const SwTableNode* pTableNd = pNd->FindTableNode();
        if(pTableNd == nullptr)
            continue;
        if(&pTableNd->GetTable() == this)
        {
            sal_uInt16 nLnPos = SwTableFormula::GetLnPosInTable(*this, pBoxFormula->GetTableBox());
            aHint.m_bBehindSplitLine = USHRT_MAX != nLnPos && aHint.m_nSplitLine <= nLnPos;
        }
        else
            aHint.m_bBehindSplitLine = false;
        pBoxFormula->ToSplitMergeBoxNmWithHistory(aHint, pHistory);
    }
}

void SwTable::Merge(const SwTable& rTable, SwHistory* pHistory)
{
    SwTableFormulaUpdate aHint(this);
    aHint.m_eFlags = TBL_MERGETBL;
    aHint.m_aData.pDelTable = &rTable;
    std::vector<SwTableBoxFormula*> vFormulas;
    GatherFormulas(vFormulas);
    for(auto pBoxFormula: vFormulas)
        pBoxFormula->ToSplitMergeBoxNmWithHistory(aHint, pHistory);
}

void SwTable::UpdateFields(TableFormulaUpdateFlags eFlags)
{
    SwDoc& rDoc = GetFrameFormat()->GetDoc();
    auto pFieldType = rDoc.getIDocumentFieldsAccess().GetFieldType(SwFieldIds::Table, OUString(), false);
    if(!pFieldType)
        return;
    std::vector<SwFormatField*> vFields;
    pFieldType->GatherFields(vFields);
    for(auto pFormatField : vFields)
    {
        SwTableField* pField = static_cast<SwTableField*>(pFormatField->GetField());
        // table where this field is located
        const SwTableNode* pTableNd;
        const SwTextNode& rTextNd = pFormatField->GetTextField()->GetTextNode();
        pTableNd = rTextNd.FindTableNode();
        if(pTableNd == nullptr || &pTableNd->GetTable() != this)
            continue;

        switch(eFlags)
        {
            case TBL_BOXNAME:
                // to the external representation
                pField->PtrToBoxNm(this);
                break;
            case TBL_RELBOXNAME:
                // to the relative representation
                pField->ToRelBoxNm(this);
                break;
            case TBL_BOXPTR:
                // to the internal representation
                // JP 17.06.96: internal representation on all formulas
                //              (reference to other table!!!)
                pField->BoxNmToPtr( &pTableNd->GetTable() );
                break;
            default:
                assert(false); // Only TBL_BOXNAME, TBL_RELBOXNAME and TBL_BOXPTR are supported
                break;
        }
    }

    // process all table box formulas
    SwTableLines& rTableLines = GetTabLines();
    for (SwTableLine* pTableLine : rTableLines)
    {
        SwTableBoxes& rTableBoxes = pTableLine->GetTabBoxes();
        for (SwTableBox* pTableBox : rTableBoxes)
        {
            SwTableBoxFormat* pTableBoxFormat = pTableBox->GetFrameFormat();
            if (const SwTableBoxFormula* pItem = pTableBoxFormat->GetItemIfSet( RES_BOXATR_FORMULA, false ))
            {
                // SwTableBoxFormula is non-shareable, so const_cast is somewhat OK
                auto & rBoxFormula = const_cast<SwTableBoxFormula&>(*pItem);
                if(eFlags == TBL_BOXPTR)
                    rBoxFormula.TryBoxNmToPtr();
                else if(eFlags == TBL_RELBOXNAME)
                    rBoxFormula.TryRelBoxNm();
                else
                    rBoxFormula.ChangeState();
            }
        }
    }
}

void SwTable::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("SwTable"));
    (void)xmlTextWriterWriteFormatAttribute(pWriter, BAD_CAST("ptr"), "%p", this);
    (void)xmlTextWriterWriteFormatAttribute(pWriter, BAD_CAST("table-format"), "%p", GetFrameFormat());
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("lines"));
    for (const auto& pLine : m_aLines)
    {
        pLine->dumpAsXml(pWriter);
    }
    (void)xmlTextWriterEndElement(pWriter);
    (void)xmlTextWriterEndElement(pWriter);
}

// TODO Set HasTextChangesOnly=true, if needed based on the redlines in the cells.
// At tracked row deletion, return with the newest deletion of the row or
// at tracked row insertion, return with the oldest insertion in the row, which
// contain the change data of the row change.
// If the return value is SwRedlineTable::npos, there is no tracked row change.
SwRedlineTable::size_type SwTableLine::UpdateTextChangesOnly(
    SwRedlineTable::size_type& rRedlinePos, bool bUpdateProperty ) const
{
    SwRedlineTable::size_type nRet = SwRedlineTable::npos;
    const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();

    // check table row property "HasTextChangesOnly", if it's defined and its
    // value is false, and all text content is in delete redlines, the row is deleted
    const SvxPrintItem *pHasTextChangesOnlyProp =
            GetFrameFormat()->GetAttrSet().GetItem<SvxPrintItem>(RES_PRINT);
    if ( pHasTextChangesOnlyProp && !pHasTextChangesOnlyProp->GetValue() )
    {
        const SwTableBoxes & rBoxes = GetTabBoxes();
        size_t nBoxes = rBoxes.size();
        bool bInsertion = false;
        bool bPlainTextInLine = false;
        SwRedlineTable::size_type nOldestRedline = SwRedlineTable::npos;
        SwRedlineTable::size_type nNewestRedline = SwRedlineTable::npos;
        for (size_t nBoxIndex = 0; nBoxIndex < nBoxes && rRedlinePos < aRedlineTable.size(); ++nBoxIndex)
        {
            auto pBox = rBoxes[nBoxIndex];
            if ( pBox->IsEmpty( /*bWithRemainingNestedTable =*/ false ) )
            {
               // no text content, check the next cells
               continue;
            }

            bool bHasRedlineInBox = false;
            SwPosition aCellStart( *pBox->GetSttNd(), SwNodeOffset(0) );
            SwPosition aCellEnd( *pBox->GetSttNd()->EndOfSectionNode(), SwNodeOffset(-1) );
            SwNodeIndex pEndNodeIndex(aCellEnd.GetNode());
            SwRangeRedline* pPreviousDeleteRedline = nullptr;
            for( ; rRedlinePos < aRedlineTable.size(); ++rRedlinePos )
            {
                const SwRangeRedline* pRedline = aRedlineTable[ rRedlinePos ];

                if ( pRedline->Start()->GetNodeIndex() > pEndNodeIndex.GetIndex() )
                {
                    // no more redlines in the actual cell,
                    // check the next ones
                    break;
                }

                // redline in the cell
                if ( aCellStart <= *pRedline->Start() )
                {
                    if ( !bHasRedlineInBox )
                    {
                        bHasRedlineInBox = true;
                        // plain text before the first redline in the text
                        if ( pRedline->Start()->GetContentIndex() > 0 )
                            bPlainTextInLine = true;
                    }

                    RedlineType nType = pRedline->GetType();

                    // first insert redline
                    if ( !bInsertion )
                    {
                        if ( RedlineType::Insert == nType )
                        {
                            bInsertion = true;
                        }
                        else
                        {
                            // plain text between the delete redlines
                            if ( pPreviousDeleteRedline &&
                                *pPreviousDeleteRedline->End() < *pRedline->Start() &&
                                // in the same section, i.e. not in a nested table
                                pPreviousDeleteRedline->End()->nNode.GetNode().StartOfSectionNode() ==
                                    pRedline->Start()->nNode.GetNode().StartOfSectionNode() )
                            {
                                bPlainTextInLine = true;
                            }
                            pPreviousDeleteRedline = const_cast<SwRangeRedline*>(pRedline);
                        }
                    }

                    // search newest and oldest redlines
                    if ( nNewestRedline == SwRedlineTable::npos ||
                             aRedlineTable[nNewestRedline]->GetRedlineData().GetTimeStamp() <
                                 pRedline->GetRedlineData().GetTimeStamp() )
                    {
                        nNewestRedline = rRedlinePos;
                    }
                    if ( nOldestRedline == SwRedlineTable::npos ||
                             aRedlineTable[nOldestRedline]->GetRedlineData().GetTimeStamp() >
                                 pRedline->GetRedlineData().GetTimeStamp() )
                    {
                        nOldestRedline = rRedlinePos;
                    }
                }
            }

            // there is text content outside of redlines: not a deletion
            if ( !bInsertion && ( !bHasRedlineInBox || ( pPreviousDeleteRedline &&
                 // in the same cell, i.e. not in a nested table
                 pPreviousDeleteRedline->End()->nNode.GetNode().StartOfSectionNode() ==
                      aCellEnd.GetNode().StartOfSectionNode()  &&
                 ( pPreviousDeleteRedline->End()->GetNode() < aCellEnd.GetNode() ||
                   pPreviousDeleteRedline->End()->GetContentIndex() <
                           aCellEnd.GetNode().GetContentNode()->Len() ) ) ) )
            {
                bPlainTextInLine = true;
                // not deleted cell content: the row is not empty
                // maybe insertion of a row, try to search it
                bInsertion = true;
            }
        }

        // choose return redline, if it exists or remove changed row attribute
        if ( bInsertion && SwRedlineTable::npos != nOldestRedline &&
                RedlineType::Insert == aRedlineTable[ nOldestRedline ]->GetType() )
        {
            // there is an insert redline, which is the oldest redline in the row
            nRet = nOldestRedline;
        }
        else if ( !bInsertion && !bPlainTextInLine && SwRedlineTable::npos != nNewestRedline &&
                RedlineType::Delete == aRedlineTable[ nNewestRedline ]->GetType() )
        {
            // there is a delete redline, which is the newest redline in the row,
            // and no text outside of redlines, and no insert redline in the row,
            // i.e. whole text content is deleted
            nRet = nNewestRedline;
        }
        else
        {
            // no longer tracked row insertion or deletion
            nRet = SwRedlineTable::npos;
            // set TextChangesOnly = true to remove the tracked deletion
            // FIXME Undo is not supported here (this is only a fallback,
            // because using SetRowNotTracked() is not recommended here)
            if ( bUpdateProperty )
            {
                SvxPrintItem aUnsetTracking(RES_PRINT, true);
                SwFrameFormat *pFormat = const_cast<SwTableLine*>(this)->ClaimFrameFormat();
                pFormat->LockModify();
                pFormat->SetFormatAttr( aUnsetTracking );
                pFormat->UnlockModify();
            }
        }
    }

    // cache the result
    const_cast<SwTableLine*>(this)->SetRedlineType( SwRedlineTable::npos == nRet
        ? RedlineType::None
        : aRedlineTable[ nRet ]->GetType());

    return nRet;
}

SwRedlineTable::size_type SwTableLine::GetTableRedline() const
{
    const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
    const SwStartNode* pFirstBox = GetTabBoxes().front()->GetSttNd();
    const SwStartNode* pLastBox = GetTabBoxes().back()->GetSttNd();

    // Box with no start node
    if ( !pFirstBox || !pLastBox )
        return SwRedlineTable::npos;

    const SwPosition aLineStart(*pFirstBox);
    const SwPosition aLineEnd(*pLastBox);
    SwRedlineTable::size_type n = 0;

    const SwRangeRedline* pFnd = aRedlineTable.FindAtPosition( aLineStart, n, /*next=*/false );
    if( pFnd && *pFnd->Start() < aLineStart && *pFnd->End() > aLineEnd )
        return n;

    return SwRedlineTable::npos;
}

bool SwTableLine::IsTracked(SwRedlineTable::size_type& rRedlinePos, bool bOnlyDeleted) const
{
   SwRedlineTable::size_type nPos = UpdateTextChangesOnly(rRedlinePos);
   if ( nPos != SwRedlineTable::npos )
   {
       const SwRedlineTable& aRedlineTable =
           GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
       if ( RedlineType::Delete == aRedlineTable[nPos]->GetType() ||
            ( !bOnlyDeleted && RedlineType::Insert == aRedlineTable[nPos]->GetType() ) )
           return true;
   }
   return false;
}

bool SwTableLine::IsDeleted(SwRedlineTable::size_type& rRedlinePos) const
{
   // if not a deleted row, check the deleted columns
   if ( !IsTracked(rRedlinePos, /*bOnlyDeleted=*/true) )
   {
       const SwTableBoxes& rBoxes = GetTabBoxes();
       for( size_t i = 0; i < rBoxes.size(); ++i )
       {
           // there is a not deleted column
           if ( rBoxes[i]->GetRedlineType() != RedlineType::Delete )
               return false;
       }
   }

   return true;
}

RedlineType SwTableLine::GetRedlineType() const
{
    const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
    if ( aRedlineTable.empty() )
        return RedlineType::None;

    // check table row property "HasTextChangesOnly", if it's defined and its value is
    // false, return with the cached redline type, if it exists, otherwise calculate it
    const SvxPrintItem *pHasTextChangesOnlyProp =
            GetFrameFormat()->GetAttrSet().GetItem<SvxPrintItem>(RES_PRINT);
    if ( pHasTextChangesOnlyProp && !pHasTextChangesOnlyProp->GetValue() )
    {
        if ( RedlineType::None != m_eRedlineType )
            return m_eRedlineType;

        SwRedlineTable::size_type nPos = 0;
        nPos = UpdateTextChangesOnly(nPos);
        if ( nPos != SwRedlineTable::npos )
            return aRedlineTable[nPos]->GetType();
    }
    else if ( RedlineType::None != m_eRedlineType )
        // empty the cache
        const_cast<SwTableLine*>(this)->SetRedlineType( RedlineType::None );

    // is the whole table part of a changed text
    SwRedlineTable::size_type nTableRedline = GetTableRedline();
    if ( nTableRedline != SwRedlineTable::npos )
        return aRedlineTable[nTableRedline]->GetType();

    return RedlineType::None;
}

void SwTableLine::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("SwTableLine"));
    (void)xmlTextWriterWriteFormatAttribute(pWriter, BAD_CAST("ptr"), "%p", this);

    GetFrameFormat()->dumpAsXml(pWriter);

    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("boxes"));
    for (const auto& pBox : m_aBoxes)
    {
        pBox->dumpAsXml(pWriter);
    }
    (void)xmlTextWriterEndElement(pWriter);

    (void)xmlTextWriterEndElement(pWriter);
}

SwTableBox::SwTableBox( SwTableBoxFormat* pFormat, sal_uInt16 nLines, SwTableLine *pUp )
    : SwClient(nullptr)
    , m_aLines()
    , m_pStartNode(nullptr)
    , m_pUpper(pUp)
    , mnRowSpan(1)
    , mbDummyFlag(false)
    , mbDirectFormatting(false)
{
    m_aLines.reserve( nLines );
    CheckBoxFormat( pFormat )->Add(*this);
}

SwTableBox::SwTableBox( SwTableBoxFormat* pFormat, const SwNodeIndex &rIdx,
                        SwTableLine *pUp )
    : SwClient(nullptr)
    , m_aLines()
    , m_pUpper(pUp)
    , mnRowSpan(1)
    , mbDummyFlag(false)
    , mbDirectFormatting(false)
{
    CheckBoxFormat( pFormat )->Add(*this);

    m_pStartNode = rIdx.GetNode().GetStartNode();

    // insert into the table
    const SwTableNode* pTableNd = m_pStartNode->FindTableNode();
    assert(pTableNd && "In which table is that box?");
    SwTableSortBoxes& rSrtArr = const_cast<SwTableSortBoxes&>(pTableNd->GetTable().
                                GetTabSortBoxes());
    SwTableBox* p = this;   // error: &this
    rSrtArr.insert( p );        // insert
}

SwTableBox::SwTableBox( SwTableBoxFormat* pFormat, const SwStartNode& rSttNd, SwTableLine *pUp )
    : SwClient(nullptr)
    , m_aLines()
    , m_pStartNode(&rSttNd)
    , m_pUpper(pUp)
    , mnRowSpan(1)
    , mbDummyFlag(false)
    , mbDirectFormatting(false)
{
    CheckBoxFormat( pFormat )->Add(*this);

    // insert into the table
    const SwTableNode* pTableNd = m_pStartNode->FindTableNode();
    assert(pTableNd && "In which table is the box?");
    SwTableSortBoxes& rSrtArr = const_cast<SwTableSortBoxes&>(pTableNd->GetTable().
                                GetTabSortBoxes());
    SwTableBox* p = this;   // error: &this
    rSrtArr.insert( p );        // insert
}

void SwTableBox::RemoveFromTable()
{
    if (m_pStartNode) // box containing contents?
    {
        // remove from table
        const SwTableNode* pTableNd = m_pStartNode->FindTableNode();
        assert(pTableNd && "In which table is that box?");
        SwTableSortBoxes& rSrtArr = const_cast<SwTableSortBoxes&>(pTableNd->GetTable().
                                    GetTabSortBoxes());
        SwTableBox *p = this;   // error: &this
        rSrtArr.erase( p );        // remove
        m_pStartNode = nullptr; // clear it so this is only run once
    }
}

SwTableBox::~SwTableBox()
{
    if (!GetFrameFormat()->GetDoc().IsInDtor())
    {
        RemoveFromTable();
    }

    // the TabelleBox can be deleted if it's the last client of the FrameFormat
    sw::BroadcastingModify* pMod = GetFrameFormat();
    pMod->Remove(*this);               // remove,
    if( !pMod->HasWriterListeners() )
        delete pMod;    // and delete
}

SwTableBoxFormat* SwTableBox::CheckBoxFormat( SwTableBoxFormat* pFormat )
{
    // We might need to create a new format here, because the box must be
    // added to the format solely if pFormat has a value or form.
    if( SfxItemState::SET == pFormat->GetItemState( RES_BOXATR_VALUE, false ) ||
        SfxItemState::SET == pFormat->GetItemState( RES_BOXATR_FORMULA, false ) )
    {
        SwTableBox* pOther = SwIterator<SwTableBox,SwFormat>( *pFormat ).First();
        if( pOther )
        {
            SwTableBoxFormat* pNewFormat = pFormat->GetDoc().MakeTableBoxFormat();
            pNewFormat->LockModify();
            *pNewFormat = *pFormat;

            // Remove values and formulas
            pNewFormat->ResetFormatAttr( RES_BOXATR_FORMULA, RES_BOXATR_VALUE );
            pNewFormat->UnlockModify();

            pFormat = pNewFormat;
        }
    }
    return pFormat;
}

SwTableBoxFormat* SwTableBox::ClaimFrameFormat()
{
    // This method makes sure that this object is an exclusive SwTableBox client
    // of an SwTableBoxFormat object
    // If other SwTableBox objects currently listen to the same SwTableBoxFormat as
    // this one, something needs to be done
    SwTableBoxFormat *pRet = GetFrameFormat();
    SwIterator<SwTableBox,SwFormat> aIter( *pRet );
    for( SwTableBox* pLast = aIter.First(); pLast; pLast = aIter.Next() )
    {
        if ( pLast != this )
        {
            // Found another SwTableBox object
            // create a new Format as a copy and assign me to it
            // don't copy values and formulas
            SwTableBoxFormat* pNewFormat = pRet->GetDoc().MakeTableBoxFormat();
            pNewFormat->LockModify();
            *pNewFormat = *pRet;
            pNewFormat->ResetFormatAttr( RES_BOXATR_FORMULA, RES_BOXATR_VALUE );
            pNewFormat->UnlockModify();

            // re-register SwCellFrame objects that know me
            SwIterator<SwCellFrame,SwFormat> aFrameIter( *pRet );
            for( SwCellFrame* pCell = aFrameIter.First(); pCell; pCell = aFrameIter.Next() )
                if( pCell->GetTabBox() == this )
                    pCell->RegisterToFormat( *pNewFormat );

            // re-register myself
            pNewFormat->Add(*this);
            pRet = pNewFormat;
            break;
        }
    }
    return pRet;
}

void SwTableBox::ChgFrameFormat(SwTableBoxFormat* pNewFormat, bool bNeedToReregister)
{
    SwFrameFormat* pOld = GetFrameFormat();
    // tdf#84635 We set bNeedToReregister=false to avoid a quadratic slowdown on loading large tables,
    // and since we are creating the table for the first time, no re-registration is necessary.
    // First, re-register the Frames.
    if(bNeedToReregister)
        pOld->CallSwClientNotify(sw::TableBoxFormatChanged(*pNewFormat, *this));
    // Now, re-register self.
    pNewFormat->Add(*this);
    if(!pOld->HasWriterListeners())
        delete pOld;
}

// Return the name of this box. This is determined dynamically
// resulting from the position in the lines/boxes/tables.
void sw_GetTableBoxColStr( sal_uInt16 nCol, OUString& rNm )
{
    const sal_uInt16 coDiff = 52;   // 'A'-'Z' 'a' - 'z'

    do {
        const sal_uInt16 nCalc = nCol % coDiff;
        if( nCalc >= 26 )
            rNm = OUStringChar( sal_Unicode('a' - 26 + nCalc) ) + rNm;
        else
            rNm = OUStringChar( sal_Unicode('A' + nCalc) ) + rNm;

        nCol = nCol - nCalc;
        if( 0 == nCol )
            break;
        nCol /= coDiff;
        --nCol;
    } while( true );
}

Point SwTableBox::GetCoordinates() const
{
    if( !m_pStartNode )       // box without content?
    {
        // search for the next first box?
        return Point( 0, 0 );
    }

    const SwTable& rTable = m_pStartNode->FindTableNode()->GetTable();
    sal_uInt16 nX, nY;
    const SwTableBox* pBox = this;
    do {
        const SwTableLine* pLine = pBox->GetUpper();
        // at the first level?
        const SwTableLines* pLines = pLine->GetUpper()
                ? &pLine->GetUpper()->GetTabLines() : &rTable.GetTabLines();

        nY = pLines->GetPos( pLine ) + 1 ;
        nX = pBox->GetUpper()->GetBoxPos( pBox ) + 1;
        pBox = pLine->GetUpper();
    } while( pBox );
    return Point( nX, nY );
}

OUString SwTableBox::GetName() const
{
    if( !m_pStartNode )       // box without content?
    {
        // search for the next first box?
        return OUString();
    }

    const SwTable& rTable = m_pStartNode->FindTableNode()->GetTable();
    sal_uInt16 nPos;
    OUString sNm, sTmp;
    const SwTableBox* pBox = this;
    do {
        const SwTableLine* pLine = pBox->GetUpper();
        // at the first level?
        const SwTableLines* pLines = pLine->GetUpper()
                ? &pLine->GetUpper()->GetTabLines() : &rTable.GetTabLines();

        nPos = pLines->GetPos( pLine ) + 1;
        sTmp = OUString::number( nPos );
        if( !sNm.isEmpty() )
            sNm = sTmp + "." + sNm;
        else
            sNm = sTmp;

        nPos = pBox->GetUpper()->GetBoxPos( pBox );
        sTmp = OUString::number(nPos + 1);
        pBox = pLine->GetUpper();
        if( nullptr != pBox )
            sNm = sTmp + "." + sNm;
        else
            sw_GetTableBoxColStr( nPos, sNm );

    } while( pBox );
    return sNm;
}

bool SwTableBox::IsInHeadline( const SwTable* pTable ) const
{
    if( !GetUpper() )           // should only happen upon merge.
        return false;

    if( !pTable )
        pTable = &m_pStartNode->FindTableNode()->GetTable();

    const SwTableLine* pLine = GetUpper();
    while( pLine->GetUpper() )
        pLine = pLine->GetUpper()->GetUpper();

    // Headerline?
    return pTable->GetTabLines()[ 0 ] == pLine;
}

SwNodeOffset SwTableBox::GetSttIdx() const
{
    return m_pStartNode ? m_pStartNode->GetIndex() : SwNodeOffset(0);
}

bool SwTableBox::IsEmpty( bool bWithRemainingNestedTable ) const
{
    const SwStartNode *pSttNd = GetSttNd();

    if ( !pSttNd )
        return false;

    const SwNode * pFirstNode = pSttNd->GetNodes()[pSttNd->GetIndex() + 1];

    if ( pSttNd->GetIndex() + 2 == pSttNd->EndOfSectionIndex() )
    {
        // single empty node in the box
        const SwContentNode *pCNd = pFirstNode->GetContentNode();
        if ( pCNd && !pCNd->Len() )
            return true;

        // tdf#157011 OOXML w:std cell content is imported with terminating 0x01 characters,
        // i.e. an empty box can contain double 0x01: handle it to avoid losing change tracking
        // FIXME regression since commit b5c616d10bff3213840d4893d13b4493de71fa56
        if ( pCNd && pCNd->Len() == 2 && pCNd->GetTextNode() )
        {
            const OUString &rText = pCNd->GetTextNode()->GetText();
            if ( rText[0] == 0x01 && rText[1] == 0x01 )
                return true;
        }
    }
    else if ( bWithRemainingNestedTable )
    {
        if ( const SwTableNode * pTableNode = pFirstNode->GetTableNode() )
        {
            // empty nested table in the box and
            // no text content after it
            if ( pTableNode->EndOfSectionIndex() + 2 == pSttNd->EndOfSectionIndex() )
                return pTableNode->GetTable().IsEmpty();
        }
    }

    return false;
}

    // retrieve information from the client
bool SwTable::GetInfo( SwFindNearestNode& rInfo ) const
{
    if( GetFrameFormat() &&
        GetFrameFormat()->GetFormatAttr( RES_PAGEDESC ).GetPageDesc() &&
        !m_TabSortContentBoxes.empty() &&
        m_TabSortContentBoxes[0]->GetSttNd()->GetNodes().IsDocNodes() )
        rInfo.CheckNode( *m_TabSortContentBoxes[0]->GetSttNd()->FindTableNode() );
    return true;
}

SwTable * SwTable::FindTable( SwFrameFormat const*const pFormat )
{
    return pFormat
        ? SwIterator<SwTable,SwFormat>(*pFormat).First()
        : nullptr;
}

SwTableNode* SwTable::GetTableNode() const
{
    return !GetTabSortBoxes().empty() ?
           const_cast<SwTableNode*>(GetTabSortBoxes()[ 0 ]->GetSttNd()->FindTableNode()) :
           m_pTableNode;
}

void SwTable::SetRefObject( SwServerObject* pObj )
{
    if( m_xRefObj.is() )
        m_xRefObj->Closed();

    m_xRefObj = pObj;
}

void SwTable::SetHTMLTableLayout(std::shared_ptr<SwHTMLTableLayout> const& r)
{
    m_xHTMLLayout = r;
}

static void ChgTextToNum( SwTableBox& rBox, const OUString& rText, const Color* pCol,
                    bool bChgAlign )
{
    SwNodeOffset nNdPos = rBox.IsValidNumTextNd();
    ChgTextToNum( rBox,rText,pCol,bChgAlign,nNdPos);
}
void ChgTextToNum( SwTableBox& rBox, const OUString& rText, const Color* pCol,
                    bool bChgAlign, SwNodeOffset nNdPos )
{

    if( NODE_OFFSET_MAX == nNdPos )
        return;

    SwDoc& rDoc = rBox.GetFrameFormat()->GetDoc();
    SwTextNode* pTNd = rDoc.GetNodes()[ nNdPos ]->GetTextNode();

    // assign adjustment
    if( bChgAlign )
    {
        const SfxPoolItem* pItem;
        pItem = &pTNd->SwContentNode::GetAttr( RES_PARATR_ADJUST );
        SvxAdjust eAdjust = static_cast<const SvxAdjustItem*>(pItem)->GetAdjust();
        if( SvxAdjust::Left == eAdjust || SvxAdjust::Block == eAdjust )
        {
            SvxAdjustItem aAdjust( *static_cast<const SvxAdjustItem*>(pItem) );
            aAdjust.SetAdjust( SvxAdjust::Right );
            pTNd->SetAttr( aAdjust );
        }
    }

    // assign color or save "user color"
    const SvxColorItem* pColorItem = nullptr;
    if( pTNd->GetpSwAttrSet() )
        pColorItem = pTNd->GetpSwAttrSet()->GetItemIfSet( RES_CHRATR_COLOR, false );

    const std::optional<Color>& pOldNumFormatColor = rBox.GetSaveNumFormatColor();
    std::optional<Color> pNewUserColor;
    if (pColorItem)
        pNewUserColor = pColorItem->GetValue();

    if( ( pNewUserColor && pOldNumFormatColor &&
            *pNewUserColor == *pOldNumFormatColor ) ||
        ( !pNewUserColor && !pOldNumFormatColor ))
    {
        // Keep the user color, set updated values, delete old NumFormatColor if needed
        if( pCol )
            // if needed, set the color
            pTNd->SetAttr( SvxColorItem( *pCol, RES_CHRATR_COLOR ));
        else if( pColorItem )
        {
            pNewUserColor = rBox.GetSaveUserColor();
            if( pNewUserColor )
                pTNd->SetAttr( SvxColorItem( *pNewUserColor, RES_CHRATR_COLOR ));
            else
                pTNd->ResetAttr( RES_CHRATR_COLOR );
        }
    }
    else
    {
        // Save user color, set NumFormat color if needed, but never reset the color
        rBox.SetSaveUserColor( pNewUserColor ? *pNewUserColor : std::optional<Color>() );

        if( pCol )
            // if needed, set the color
            pTNd->SetAttr( SvxColorItem( *pCol, RES_CHRATR_COLOR ));

    }
    rBox.SetSaveNumFormatColor( pCol ? *pCol : std::optional<Color>() );

    if( pTNd->GetText() != rText )
    {
        // Exchange text. Bugfix to keep Tabs (front and back!) and annotations (inword comment anchors)
        const OUString& rOrig = pTNd->GetText();
        sal_Int32 n;

        for( n = 0; n < rOrig.getLength() && ('\x9' == rOrig[n] || CH_TXTATR_INWORD == rOrig[n]); ++n )
            ;
        for( ; n < rOrig.getLength() && '\x01' == rOrig[n]; ++n )
            ;
        SwContentIndex aIdx( pTNd, n );
        for( n = rOrig.getLength(); n && ('\x9' == rOrig[--n] || CH_TXTATR_INWORD == rOrig[n]); )
            ;
        sal_Int32 nEndPos = n;
        n -= aIdx.GetIndex() - 1;

        // Reset DontExpand-Flags before exchange, to retrigger expansion
        {
            SwContentIndex aResetIdx( aIdx, n );
            pTNd->DontExpandFormat( aResetIdx.GetIndex(), false, false );
        }

        if( !rDoc.getIDocumentRedlineAccess().IsIgnoreRedline() && !rDoc.getIDocumentRedlineAccess().GetRedlineTable().empty() )
        {
            SwPaM aTemp(*pTNd, 0, *pTNd, rOrig.getLength());
            rDoc.getIDocumentRedlineAccess().DeleteRedline(aTemp, true, RedlineType::Any);
        }

        // preserve comments inside of the number by deleting number portions starting from the back
        sal_Int32 nCommentPos = pTNd->GetText().lastIndexOf( CH_TXTATR_INWORD, nEndPos );
        while( nCommentPos > aIdx.GetIndex() )
        {
            pTNd->EraseText( SwContentIndex(pTNd, nCommentPos+1), nEndPos - nCommentPos, SwInsertFlags::EMPTYEXPAND );
            // find the next non-sequential comment anchor
            do
            {
                nEndPos = nCommentPos;
                n = nEndPos - aIdx.GetIndex();
                nCommentPos = pTNd->GetText().lastIndexOf( CH_TXTATR_INWORD, nEndPos );
                --nEndPos;
            }
            while( nCommentPos > aIdx.GetIndex() && nCommentPos == nEndPos );
        }

        pTNd->EraseText( aIdx, n, SwInsertFlags::EMPTYEXPAND );
        pTNd->InsertText( rText, aIdx, SwInsertFlags::EMPTYEXPAND );

        if( rDoc.getIDocumentRedlineAccess().IsRedlineOn() )
        {
            SwPaM aTemp(*pTNd, 0, *pTNd, rText.getLength());
            rDoc.getIDocumentRedlineAccess().AppendRedline(new SwRangeRedline(RedlineType::Insert, aTemp), true);
        }
    }

    // assign vertical orientation
    const SwFormatVertOrient* pVertOrientItem;
    if( bChgAlign &&
        ( !(pVertOrientItem = rBox.GetFrameFormat()->GetItemIfSet( RES_VERT_ORIENT )) ||
            text::VertOrientation::TOP == pVertOrientItem->GetVertOrient() ))
    {
        rBox.GetFrameFormat()->SetFormatAttr( SwFormatVertOrient( 0, text::VertOrientation::BOTTOM ));
    }

}

static void ChgNumToText( SwTableBox& rBox, sal_uLong nFormat )
{
    SwNodeOffset nNdPos = rBox.IsValidNumTextNd( false );
    if( NODE_OFFSET_MAX == nNdPos )
        return;

    SwDoc& rDoc = rBox.GetFrameFormat()->GetDoc();
    SwTextNode* pTNd = rDoc.GetNodes()[ nNdPos ]->GetTextNode();
    bool bChgAlign = rDoc.IsInsTableAlignNum();

    const Color * pCol = nullptr;
    if( getSwDefaultTextFormat() != nFormat )
    {
        // special text format:
        OUString sTmp;
        const OUString sText( pTNd->GetText() );
        rDoc.GetNumberFormatter()->GetOutputString( sText, nFormat, sTmp, &pCol );
        if( sText != sTmp )
        {
            // exchange text
            // Reset DontExpand-Flags before exchange, to retrigger expansion
            pTNd->DontExpandFormat( sText.getLength(), false, false );
            SwContentIndex aIdx( pTNd, 0 );
            pTNd->EraseText( aIdx, SAL_MAX_INT32, SwInsertFlags::EMPTYEXPAND );
            pTNd->InsertText( sTmp, aIdx, SwInsertFlags::EMPTYEXPAND );
        }
    }

    const SfxItemSet* pAttrSet = pTNd->GetpSwAttrSet();

    // assign adjustment
    const SvxAdjustItem* pAdjustItem;
    if( bChgAlign && pAttrSet &&
        (pAdjustItem = pAttrSet->GetItemIfSet( RES_PARATR_ADJUST, false )) &&
        SvxAdjust::Right == pAdjustItem->GetAdjust() )
    {
        pTNd->SetAttr( SvxAdjustItem( SvxAdjust::Left, RES_PARATR_ADJUST ) );
    }

    // assign color or save "user color"
    const SvxColorItem* pColorItem = nullptr;
    if( pAttrSet )
        pColorItem = pAttrSet->GetItemIfSet( RES_CHRATR_COLOR, false );

    const std::optional<Color>& pOldNumFormatColor = rBox.GetSaveNumFormatColor();
    std::optional<Color> pNewUserColor;
    if (pColorItem)
        pNewUserColor = pColorItem->GetValue();

    if( ( pNewUserColor && pOldNumFormatColor &&
            *pNewUserColor == *pOldNumFormatColor ) ||
        ( !pNewUserColor && !pOldNumFormatColor ))
    {
        // Keep the user color, set updated values, delete old NumFormatColor if needed
        if( pCol )
            // if needed, set the color
            pTNd->SetAttr( SvxColorItem( *pCol, RES_CHRATR_COLOR ));
        else if( pColorItem )
        {
            pNewUserColor = rBox.GetSaveUserColor();
            if( pNewUserColor )
                pTNd->SetAttr( SvxColorItem( *pNewUserColor, RES_CHRATR_COLOR ));
            else
                pTNd->ResetAttr( RES_CHRATR_COLOR );
        }
    }
    else
    {
        // Save user color, set NumFormat color if needed, but never reset the color
        rBox.SetSaveUserColor( pNewUserColor );

        if( pCol )
            // if needed, set the color
            pTNd->SetAttr( SvxColorItem( *pCol, RES_CHRATR_COLOR ));

    }
    rBox.SetSaveNumFormatColor( pCol ? *pCol : std::optional<Color>() );

    // assign vertical orientation
    const SwFormatVertOrient* pVertOrientItem;
    if( bChgAlign &&
        (pVertOrientItem = rBox.GetFrameFormat()->GetItemIfSet( RES_VERT_ORIENT, false )) &&
        text::VertOrientation::BOTTOM == pVertOrientItem->GetVertOrient() )
    {
        rBox.GetFrameFormat()->SetFormatAttr( SwFormatVertOrient( 0, text::VertOrientation::TOP ));
    }

}
void SwTableBoxFormat::BoxAttributeChanged(SwTableBox& rBox, const SwTableBoxNumFormat* pNewFormat, const SwTableBoxFormula* pNewFormula, const SwTableBoxValue* pNewValue, sal_uLong nOldFormat)
{
    sal_uLong nNewFormat;
    if(pNewFormat)
    {
        nNewFormat = pNewFormat->GetValue();
        // new formatting
        // is it newer or has the current been removed?
        if( SfxItemState::SET != GetItemState(RES_BOXATR_VALUE, false))
            pNewFormat = nullptr;
    }
    else
    {
        // fetch the current Item
        pNewFormat = GetItemIfSet(RES_BOXATR_FORMAT, false);
        nOldFormat = GetTableBoxNumFormat().GetValue();
        nNewFormat = pNewFormat ? pNewFormat->GetValue() : nOldFormat;
    }

    // is it newer or has the current been removed?
    if(pNewValue)
    {
        if(GetDoc().GetNumberFormatter()->IsTextFormat(nNewFormat))
            nOldFormat = 0;
        else
        {
            if(SfxItemState::SET == GetItemState(RES_BOXATR_VALUE, false))
                nOldFormat = getSwDefaultTextFormat();
            else
                nNewFormat = getSwDefaultTextFormat();
        }
    }

    // Logic:
    // Value change: -> "simulate" a format change!
    // Format change:
    // Text -> !Text or format change:
    //          - align right for horizontal alignment, if LEFT or JUSTIFIED
    //          - align bottom for vertical alignment, if TOP is set, or default
    //          - replace text (color? negative numbers RED?)
    // !Text -> Text:
    //          - align left for horizontal alignment, if RIGHT
    //          - align top for vertical alignment, if BOTTOM is set
    SvNumberFormatter* pNumFormatr = GetDoc().GetNumberFormatter();
    bool bNewIsTextFormat = pNumFormatr->IsTextFormat(nNewFormat);

    if((!bNewIsTextFormat && nOldFormat != nNewFormat) || pNewFormula)
    {
        bool bIsNumFormat = false;
        OUString aOrigText;
        bool bChgText = true;
        double fVal = 0;
        if(!pNewValue)
            pNewValue = GetItemIfSet(RES_BOXATR_VALUE, false);
        if(!pNewValue)
        {
            // so far, no value has been set, so try to evaluate the content
            SwNodeOffset nNdPos = rBox.IsValidNumTextNd();
            if(NODE_OFFSET_MAX != nNdPos)
            {
                sal_uInt32 nTmpFormatIdx = nNewFormat;
                OUString aText(GetDoc().GetNodes()[nNdPos] ->GetTextNode()->GetRedlineText());
                aOrigText = aText;
                if(aText.isEmpty())
                    bChgText = false;
                else
                {
                    // Keep Tabs
                    lcl_TabToBlankAtSttEnd(aText);

                    // JP 22.04.98: Bug 49659 -
                    //  Special casing for percent
                    if(SvNumFormatType::PERCENT == pNumFormatr->GetType(nNewFormat))
                    {
                        sal_uInt32 nTmpFormat = 0;
                        if(GetDoc().IsNumberFormat(aText, nTmpFormat, fVal))
                        {
                            if(SvNumFormatType::NUMBER == pNumFormatr->GetType( nTmpFormat))
                                aText += "%";

                            bIsNumFormat = GetDoc().IsNumberFormat(aText, nTmpFormatIdx, fVal);
                        }
                    }
                    else
                        bIsNumFormat = GetDoc().IsNumberFormat(aText, nTmpFormatIdx, fVal);

                    if(bIsNumFormat)
                    {
                        // directly assign value - without Modify
                        bool bIsLockMod = IsModifyLocked();
                        LockModify();
                        SetFormatAttr(SwTableBoxValue(fVal));
                        if(!bIsLockMod)
                            UnlockModify();
                    }
                }
            }
        }
        else
        {
            fVal = pNewValue->GetValue();
            bIsNumFormat = true;
        }

        // format contents with the new value assigned and write to paragraph
        const Color* pCol = nullptr;
        OUString sNewText;
        bool bChangeFormat = true;
        if(DBL_MAX == fVal)
        {
            sNewText = SwViewShell::GetShellRes()->aCalc_Error;
        }
        else
        {
            if(bIsNumFormat)
                pNumFormatr->GetOutputString(fVal, nNewFormat, sNewText, &pCol);
            else
            {
                // Original text could not be parsed as
                // number/date/time/..., so keep the text.
#if 0
                // Actually the text should be formatted
                // according to the format, which may include
                // additional text from the format, for example
                // in {0;-0;"BAD: "@}. But other places when
                // entering a new value or changing text or
                // changing to a different format of type Text
                // don't do this (yet?).
                pNumFormatr->GetOutputString(aOrigText, nNewFormat, sNewText, &pCol);
#else
                sNewText = aOrigText;
#endif
                // Remove the newly assigned numbering format as well if text actually exists.
                // Exception: assume user-defined formats are always intentional.
                if (bChgText && pNumFormatr->IsTextFormat(nOldFormat)
                    && !pNumFormatr->IsUserDefined(nNewFormat))
                {
                    rBox.GetFrameFormat()->ResetFormatAttr(RES_BOXATR_FORMAT);
                    bChangeFormat = false;
                }
            }

            if(!bChgText)
                sNewText.clear();
        }

        // across all boxes
        if (bChangeFormat)
            ChgTextToNum(rBox, sNewText, pCol, GetDoc().IsInsTableAlignNum());

    }
    else if(bNewIsTextFormat && nOldFormat != nNewFormat)
        ChgNumToText(rBox, nNewFormat);
}

SwTableBox* SwTableBoxFormat::SwTableBoxFormat::GetTableBox()
{
    SwIterator<SwTableBox,SwFormat> aIter(*this);
    auto pBox = aIter.First();
    SAL_INFO_IF(!pBox, "sw.core", "no box found at format");
    SAL_WARN_IF(pBox && aIter.Next(), "sw.core", "more than one box found at format");
    return pBox;
}

// for detection of modifications (mainly TableBoxAttribute)
void SwTableBoxFormat::SwClientNotify(const SwModify& rMod, const SfxHint& rHint)
{
    if(rHint.GetId() == SfxHintId::SwFormatChange
        || rHint.GetId() == SfxHintId::SwObjectDying
        || rHint.GetId() == SfxHintId::SwUpdateAttr)
    {
        SwFrameFormat::SwClientNotify(rMod, rHint);
        return;
    }
    if(rHint.GetId() != SfxHintId::SwLegacyModify && rHint.GetId() != SfxHintId::SwAttrSetChange)
        return;
    if(IsModifyLocked() || GetDoc().IsInDtor())
    {
        SwFrameFormat::SwClientNotify(rMod, rHint);
        return;
    }
    const SwTableBoxNumFormat* pNewFormat = nullptr;
    const SwTableBoxFormula* pNewFormula = nullptr;
    const SwTableBoxValue* pNewVal = nullptr;
    sal_uLong nOldFormat = getSwDefaultTextFormat();

    if(rHint.GetId() == SfxHintId::SwLegacyModify)
    {
        auto pLegacy = static_cast<const sw::LegacyModifyHint*>(&rHint);
        switch(pLegacy->m_pNew ? pLegacy->m_pNew->Which() : 0)
        {
            case RES_BOXATR_FORMAT:
                pNewFormat = static_cast<const SwTableBoxNumFormat*>(pLegacy->m_pNew);
                nOldFormat = static_cast<const SwTableBoxNumFormat*>(pLegacy->m_pOld)->GetValue();
                break;
            case RES_BOXATR_FORMULA:
                pNewFormula = static_cast<const SwTableBoxFormula*>(pLegacy->m_pNew);
                break;
            case RES_BOXATR_VALUE:
                pNewVal = static_cast<const SwTableBoxValue*>(pLegacy->m_pNew);
                break;
        }
    }
    else // rHint.GetId() == SfxHintId::SwAttrSetChange
    {
        auto pChangeHint = static_cast<const sw::AttrSetChangeHint*>(&rHint);
        const SfxItemSet& rSet = *pChangeHint->m_pNew->GetChgSet();
        pNewFormat = rSet.GetItemIfSet( RES_BOXATR_FORMAT, false);
        if(pNewFormat)
            nOldFormat = pChangeHint->m_pOld->GetChgSet()->Get(RES_BOXATR_FORMAT).GetValue();
        pNewFormula = rSet.GetItemIfSet(RES_BOXATR_FORMULA, false);
        pNewVal = rSet.GetItemIfSet(RES_BOXATR_VALUE, false);
    }

    // something changed and some BoxAttribute remained in the set!
    if( pNewFormat || pNewFormula || pNewVal )
    {
        GetDoc().getIDocumentFieldsAccess().SetFieldsDirty(true, nullptr, SwNodeOffset(0));

        if(SfxItemState::SET == GetItemState(RES_BOXATR_FORMAT, false) ||
           SfxItemState::SET == GetItemState(RES_BOXATR_VALUE, false) ||
           SfxItemState::SET == GetItemState(RES_BOXATR_FORMULA, false) )
        {
            if(auto pBox = GetTableBox())
                BoxAttributeChanged(*pBox, pNewFormat, pNewFormula, pNewVal, nOldFormat);
        }
    }
    // call base class
    SwFrameFormat::SwClientNotify(rMod, rHint);
}

bool SwTableBoxFormat::supportsFullDrawingLayerFillAttributeSet() const
{
    return false;
}

bool SwTableFormat::supportsFullDrawingLayerFillAttributeSet() const
{
    return false;
}

bool SwTableLineFormat::supportsFullDrawingLayerFillAttributeSet() const
{
    return false;
}

bool SwTableBox::HasNumContent( double& rNum, sal_uInt32& rFormatIndex,
                            bool& rIsEmptyTextNd ) const
{
    bool bRet = false;
    SwNodeOffset nNdPos = IsValidNumTextNd();
    if( NODE_OFFSET_MAX != nNdPos )
    {
        OUString aText( m_pStartNode->GetNodes()[ nNdPos ]->GetTextNode()->GetRedlineText() );
        // Keep Tabs
        lcl_TabToBlankAtSttEnd( aText );
        rIsEmptyTextNd = aText.isEmpty();
        SvNumberFormatter* pNumFormatr = GetFrameFormat()->GetDoc().GetNumberFormatter();

        if( const SwTableBoxNumFormat* pItem = GetFrameFormat()->GetItemIfSet( RES_BOXATR_FORMAT, false) )
        {
            rFormatIndex = pItem->GetValue();
            // Special casing for percent
            if( !rIsEmptyTextNd && SvNumFormatType::PERCENT == pNumFormatr->GetType( rFormatIndex ))
            {
                sal_uInt32 nTmpFormat = 0;
                if( GetFrameFormat()->GetDoc().IsNumberFormat( aText, nTmpFormat, rNum ) &&
                    SvNumFormatType::NUMBER == pNumFormatr->GetType( nTmpFormat ))
                    aText += "%";
            }
        }
        else
            rFormatIndex = 0;

        bRet = GetFrameFormat()->GetDoc().IsNumberFormat( aText, rFormatIndex, rNum );
    }
    else
        rIsEmptyTextNd = false;
    return bRet;
}

bool SwTableBox::IsNumberChanged() const
{
    bool bRet = true;

    if( SfxItemState::SET == GetFrameFormat()->GetItemState( RES_BOXATR_FORMULA, false ))
    {
        const SwTableBoxNumFormat *pNumFormat = GetFrameFormat()->GetItemIfSet( RES_BOXATR_FORMAT, false );
        const SwTableBoxValue *pValue = GetFrameFormat()->GetItemIfSet( RES_BOXATR_VALUE, false );

        SwNodeOffset nNdPos;
        if( pNumFormat && pValue && NODE_OFFSET_MAX != ( nNdPos = IsValidNumTextNd() ) )
        {
            OUString sNewText, sOldText( m_pStartNode->GetNodes()[ nNdPos ]->
                                    GetTextNode()->GetRedlineText() );
            lcl_DelTabsAtSttEnd( sOldText );

            const Color* pCol = nullptr;
            GetFrameFormat()->GetDoc().GetNumberFormatter()->GetOutputString(
                pValue->GetValue(), pNumFormat->GetValue(), sNewText, &pCol );

            bRet = sNewText != sOldText ||
                    !( ( !pCol && !GetSaveNumFormatColor() ) ||
                       ( pCol && GetSaveNumFormatColor() &&
                        *pCol == *GetSaveNumFormatColor() ));
        }
    }
    return bRet;
}

SwNodeOffset SwTableBox::IsValidNumTextNd( bool bCheckAttr ) const
{
    SwNodeOffset nPos = NODE_OFFSET_MAX;
    if( m_pStartNode )
    {
        SwNodeIndex aIdx( *m_pStartNode );
        SwNodeOffset nIndex = aIdx.GetIndex();
        const SwNodeOffset nIndexEnd = m_pStartNode->GetNodes()[ nIndex ]->EndOfSectionIndex();
        const SwTextNode *pTextNode = nullptr;
        while( ++nIndex < nIndexEnd )
        {
            const SwNode* pNode = m_pStartNode->GetNodes()[nIndex];
            if( pNode->IsTableNode() )
            {
                pTextNode = nullptr;
                break;
            }
            if( pNode->IsTextNode() )
            {
                if( pTextNode )
                {
                    pTextNode = nullptr;
                    break;
                }
                else
                {
                    pTextNode = pNode->GetTextNode();
                    nPos = nIndex;
                }
            }
        }
        if( pTextNode )
        {
            if( bCheckAttr )
            {
                const SwpHints* pHts = pTextNode->GetpSwpHints();
                // do some tests if there's only text in the node!
                // Flys/fields/...
                if( pHts )
                {
                    sal_Int32 nNextSetField = 0;
                    for( size_t n = 0; n < pHts->Count(); ++n )
                    {
                        const SwTextAttr* pAttr = pHts->Get(n);
                        if( RES_TXTATR_NOEND_BEGIN <= pAttr->Which() )
                        {
                            if ( (pAttr->GetStart() == nNextSetField)
                                 && (pAttr->Which() == RES_TXTATR_FIELD))
                            {
                                // #i104949# hideous hack for report builder:
                                // it inserts hidden variable-set fields at
                                // the beginning of para in cell, but they
                                // should not turn cell into text cell
                                const SwField* pField = pAttr->GetFormatField().GetField();
                                if (pField &&
                                    (pField->GetTypeId() == SwFieldTypesEnum::Set) &&
                                    (static_cast<SwSetExpField const*>
                                           (pField)->GetSubType() &
                                        SwGetSetExpType::Invisible))
                                {
                                    nNextSetField = pAttr->GetStart() + 1;
                                    continue;
                                }
                            }
                            else if( RES_TXTATR_ANNOTATION == pAttr->Which() ||
                                     RES_TXTATR_FTN == pAttr->Which() )
                            {
                                continue;
                            }
                            nPos = NODE_OFFSET_MAX;
                            break;
                        }
                    }
                }
            }
        }
        else
            nPos = NODE_OFFSET_MAX;
    }
    return nPos;
}

// is this a Formula box or one with numeric content (AutoSum)
sal_uInt16 SwTableBox::IsFormulaOrValueBox() const
{
    sal_uInt16 nWhich = 0;
    const SwTextNode* pTNd;
    SwFrameFormat* pFormat = GetFrameFormat();
    if( SfxItemState::SET == pFormat->GetItemState( RES_BOXATR_FORMULA, false ))
        nWhich = RES_BOXATR_FORMULA;
    else if( SfxItemState::SET == pFormat->GetItemState( RES_BOXATR_VALUE, false ) &&
            !pFormat->GetDoc().GetNumberFormatter()->IsTextFormat(
                pFormat->GetTableBoxNumFormat().GetValue() ))
        nWhich = RES_BOXATR_VALUE;
    else if( m_pStartNode && m_pStartNode->GetIndex() + 2 == m_pStartNode->EndOfSectionIndex()
            && nullptr != ( pTNd = m_pStartNode->GetNodes()[ m_pStartNode->GetIndex() + 1 ]
            ->GetTextNode() ) && pTNd->GetText().isEmpty())
        nWhich = USHRT_MAX;

    return nWhich;
}

void SwTableBox::ActualiseValueBox()
{
    SwFrameFormat* pFormat = GetFrameFormat();
    const SwTableBoxNumFormat *pFormatItem = pFormat->GetItemIfSet( RES_BOXATR_FORMAT, true );
    if (!pFormatItem)
        return;
    const SwTableBoxValue *pValItem = pFormat->GetItemIfSet( RES_BOXATR_VALUE );
    if (!pValItem)
        return;

    const sal_uLong nFormatId = pFormatItem->GetValue();
    SwNodeOffset nNdPos = NODE_OFFSET_MAX;
    SvNumberFormatter* pNumFormatr = pFormat->GetDoc().GetNumberFormatter();

    if( !pNumFormatr->IsTextFormat( nFormatId ) &&
        NODE_OFFSET_MAX != (nNdPos = IsValidNumTextNd()) )
    {
        double fVal = pValItem->GetValue();
        const Color* pCol = nullptr;
        OUString sNewText;
        pNumFormatr->GetOutputString( fVal, nFormatId, sNewText, &pCol );

        const OUString& rText = m_pStartNode->GetNodes()[ nNdPos ]->GetTextNode()->GetText();
        if( rText != sNewText )
            ChgTextToNum( *this, sNewText, pCol, false ,nNdPos);
    }
}

SwRedlineTable::size_type SwTableBox::GetRedline() const
{
    const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
    const SwStartNode *pSttNd = GetSttNd();

    if ( aRedlineTable.empty() || !pSttNd )
        return SwRedlineTable::npos;

    // check table row property "HasTextChangesOnly", if it's defined and its value is
    // false, return with the first redline of the cell
    const SvxPrintItem *pHasTextChangesOnlyProp =
            GetFrameFormat()->GetAttrSet().GetItem<SvxPrintItem>(RES_PRINT);
    if ( !pHasTextChangesOnlyProp || pHasTextChangesOnlyProp->GetValue() )
        return SwRedlineTable::npos;

    SwPosition aCellStart( *GetSttNd(), SwNodeOffset(0) );
    SwPosition aCellEnd( *GetSttNd()->EndOfSectionNode(), SwNodeOffset(-1) );
    SwNodeIndex pEndNodeIndex(aCellEnd.GetNode());
    SwRedlineTable::size_type nRedlinePos = 0;
    for( ; nRedlinePos < aRedlineTable.size(); ++nRedlinePos )
    {
        const SwRangeRedline* pRedline = aRedlineTable[ nRedlinePos ];

        if ( pRedline->Start()->GetNodeIndex() > pEndNodeIndex.GetIndex() )
        {
            // no more redlines in the actual cell,
            // check the next ones
            break;
        }

        // redline in the cell
        if ( aCellStart <= *pRedline->Start() )
            return nRedlinePos;
    }

    return SwRedlineTable::npos;
}

RedlineType SwTableBox::GetRedlineType() const
{
    SwRedlineTable::size_type nPos = GetRedline();
    if ( nPos != SwRedlineTable::npos )
    {
        const SwRedlineTable& aRedlineTable = GetFrameFormat()->GetDoc().getIDocumentRedlineAccess().GetRedlineTable();
        const SwRangeRedline* pRedline = aRedlineTable[ nPos ];
        if ( RedlineType::Delete == pRedline->GetType() ||
             RedlineType::Insert == pRedline->GetType() )
        {
            return pRedline->GetType();
        }
    }
    return RedlineType::None;
}

void SwTableBox::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("SwTableBox"));
    (void)xmlTextWriterWriteFormatAttribute(pWriter, BAD_CAST("ptr"), "%p", this);
    (void)xmlTextWriterWriteAttribute(pWriter, BAD_CAST("start-node"), BAD_CAST(OString::number(static_cast<sal_Int32>(m_pStartNode->GetIndex())).getStr()));
    (void)xmlTextWriterWriteAttribute(pWriter, BAD_CAST("rowspan"), BAD_CAST(OString::number(mnRowSpan).getStr()));
    GetFrameFormat()->dumpAsXml(pWriter);
    (void)xmlTextWriterEndElement(pWriter);
}

struct SwTableCellInfo::Impl
{
    const SwTable * m_pTable;
    const SwCellFrame * m_pCellFrame;
    const SwTabFrame * m_pTabFrame;
    typedef o3tl::sorted_vector<const SwTableBox *> TableBoxes_t;
    TableBoxes_t m_HandledTableBoxes;

public:
    Impl()
        : m_pTable(nullptr), m_pCellFrame(nullptr), m_pTabFrame(nullptr)
    {
    }

    void setTable(const SwTable * pTable)
    {
        m_pTable = pTable;
        SwFrameFormat * pFrameFormat = m_pTable->GetFrameFormat();
        m_pTabFrame = SwIterator<SwTabFrame,SwFormat>(*pFrameFormat).First();
        if (m_pTabFrame && m_pTabFrame->IsFollow())
            m_pTabFrame = m_pTabFrame->FindMaster(true);
    }

    const SwCellFrame * getCellFrame() const { return m_pCellFrame; }

    const SwFrame * getNextFrameInTable(const SwFrame * pFrame);
    const SwCellFrame * getNextCellFrame(const SwFrame * pFrame);
    const SwCellFrame * getNextTableBoxsCellFrame(const SwFrame * pFrame);
    bool getNext();
};

const SwFrame * SwTableCellInfo::Impl::getNextFrameInTable(const SwFrame * pFrame)
{
    const SwFrame * pResult = nullptr;

    if (((! pFrame->IsTabFrame()) || pFrame == m_pTabFrame) && pFrame->GetLower())
        pResult = pFrame->GetLower();
    else if (pFrame->GetNext())
        pResult = pFrame->GetNext();
    else
    {
        while (pFrame->GetUpper() != nullptr)
        {
            pFrame = pFrame->GetUpper();

            if (pFrame->IsTabFrame())
            {
                m_pTabFrame = static_cast<const SwTabFrame *>(pFrame)->GetFollow();
                pResult = m_pTabFrame;
                break;
            }
            else if (pFrame->GetNext())
            {
                pResult = pFrame->GetNext();
                break;
            }
        }
    }

    return pResult;
}

const SwCellFrame * SwTableCellInfo::Impl::getNextCellFrame(const SwFrame * pFrame)
{
    const SwCellFrame * pResult = nullptr;

    while ((pFrame = getNextFrameInTable(pFrame)) != nullptr)
    {
        if (pFrame->IsCellFrame())
        {
            pResult = static_cast<const SwCellFrame *>(pFrame);
            break;
        }
    }

    return pResult;
}

const SwCellFrame * SwTableCellInfo::Impl::getNextTableBoxsCellFrame(const SwFrame * pFrame)
{
    const SwCellFrame * pResult = nullptr;

    while ((pFrame = getNextCellFrame(pFrame)) != nullptr)
    {
        const SwCellFrame * pCellFrame = static_cast<const SwCellFrame *>(pFrame);
        const SwTableBox * pTabBox = pCellFrame->GetTabBox();
        auto aIt = m_HandledTableBoxes.insert(pTabBox);
        if (aIt.second)
        {
            pResult = pCellFrame;
            break;
        }
    }

    return pResult;
}

const SwCellFrame * SwTableCellInfo::getCellFrame() const
{
    return m_pImpl->getCellFrame();
}

bool SwTableCellInfo::Impl::getNext()
{
    if (m_pCellFrame == nullptr)
    {
        if (m_pTabFrame != nullptr)
            m_pCellFrame = Impl::getNextTableBoxsCellFrame(m_pTabFrame);
    }
    else
        m_pCellFrame = Impl::getNextTableBoxsCellFrame(m_pCellFrame);

    return m_pCellFrame != nullptr;
}

SwTableCellInfo::SwTableCellInfo(const SwTable * pTable)
    : m_pImpl(std::make_unique<Impl>())
{
    m_pImpl->setTable(pTable);
}

SwTableCellInfo::~SwTableCellInfo()
{
}

bool SwTableCellInfo::getNext()
{
    return m_pImpl->getNext();
}

SwRect SwTableCellInfo::getRect() const
{
    SwRect aRet;

    if (getCellFrame() != nullptr)
        aRet = getCellFrame()->getFrameArea();

    return aRet;
}

const SwTableBox * SwTableCellInfo::getTableBox() const
{
    const SwTableBox * pRet = nullptr;

    if (getCellFrame() != nullptr)
        pRet = getCellFrame()->GetTabBox();

    return pRet;
}

void SwTable::RegisterToFormat( SwFormat& rFormat )
{
    rFormat.Add(*this);
}

bool SwTable::HasLayout() const
{
    const SwFrameFormat* pFrameFormat = GetFrameFormat();
    //a table in a clipboard document doesn't have any layout information
    return pFrameFormat && SwIterator<SwTabFrame,SwFormat>(*pFrameFormat).First();
}

void SwTableBox::RegisterToFormat( SwFormat& rFormat )
{
    rFormat.Add(*this);
}

// free's any remaining child objects
SwTableLines::~SwTableLines()
{
    for ( const_iterator it = begin(); it != end(); ++it )
        delete *it;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
