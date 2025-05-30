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

#include <sfx2/app.hxx>
#include <svx/svdundo.hxx>
#include <unotools/charclass.hxx>
#include <osl/diagnose.h>

#include <undodat.hxx>
#include <undoutil.hxx>
#include <undoolk.hxx>
#include <document.hxx>
#include <docsh.hxx>
#include <tabvwsh.hxx>
#include <olinetab.hxx>
#include <dbdata.hxx>
#include <rangenam.hxx>
#include <globstr.hrc>
#include <scresid.hxx>
#include <global.hxx>
#include <globalnames.hxx>
#include <target.hxx>
#include <dbdocfun.hxx>
#include <olinefun.hxx>
#include <dpobject.hxx>
#include <attrib.hxx>
#include <hints.hxx>
#include <chgtrack.hxx>
#include <refundo.hxx>
#include <markdata.hxx>
#include <utility>

// Show or hide outline groups

ScUndoDoOutline::ScUndoDoOutline( ScDocShell& rNewDocShell,
                            SCCOLROW nNewStart, SCCOLROW nNewEnd, SCTAB nNewTab,
                            ScDocumentUniquePtr pNewUndoDoc, bool bNewColumns,
                            sal_uInt16 nNewLevel, sal_uInt16 nNewEntry, bool bNewShow ) :
    ScSimpleUndo( rNewDocShell ),
    nStart( nNewStart ),
    nEnd( nNewEnd ),
    nTab( nNewTab ),
    pUndoDoc( std::move(pNewUndoDoc) ),
    bColumns( bNewColumns ),
    nLevel( nNewLevel ),
    nEntry( nNewEntry ),
    bShow( bNewShow )
{
}

ScUndoDoOutline::~ScUndoDoOutline()
{
}

OUString ScUndoDoOutline::GetComment() const
{   // Show outline" "Hide outline"
    return bShow ?
        ScResId( STR_UNDO_DOOUTLINE ) :
        ScResId( STR_UNDO_REDOOUTLINE );
}

void ScUndoDoOutline::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    // sheet has to be switched over (#46952#)!

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    // perform the inverse function

    if (bShow)
        pViewShell->HideOutline( bColumns, nLevel, nEntry, false, false );
    else
        pViewShell->ShowOutline( bColumns, nLevel, nEntry, false, false );

    //  Original column/row status
    if (bColumns)
        pUndoDoc->CopyToDocument(static_cast<SCCOL>(nStart), 0, nTab,
                                 static_cast<SCCOL>(nEnd), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false, rDoc);
    else
        pUndoDoc->CopyToDocument(0, nStart, nTab, rDoc.MaxCol(), nEnd, nTab, InsertDeleteFlags::NONE, false, rDoc);

    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(pViewShell, bColumns, !bColumns,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            true /* bGroups */, nTab);
    pViewShell->UpdateScrollBars();

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top);

    EndUndo();
}

void ScUndoDoOutline::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    // sheet has to be switched over (#46952#)!

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    if (bShow)
        pViewShell->ShowOutline( bColumns, nLevel, nEntry, false );
    else
        pViewShell->HideOutline( bColumns, nLevel, nEntry, false );

    EndRedo();
}

void ScUndoDoOutline::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoDoOutline::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;                       // is not possible
}

/** Make or delete outline groups */
ScUndoMakeOutline::ScUndoMakeOutline( ScDocShell& rNewDocShell,
                            SCCOL nStartX, SCROW nStartY, SCTAB nStartZ,
                            SCCOL nEndX, SCROW nEndY, SCTAB nEndZ,
                            std::unique_ptr<ScOutlineTable> pNewUndoTab, bool bNewColumns, bool bNewMake ) :
    ScSimpleUndo( rNewDocShell ),
    aBlockStart( nStartX, nStartY, nStartZ ),
    aBlockEnd( nEndX, nEndY, nEndZ ),
    pUndoTable( std::move(pNewUndoTab) ),
    bColumns( bNewColumns ),
    bMake( bNewMake )
{
}

ScUndoMakeOutline::~ScUndoMakeOutline()
{
}

OUString ScUndoMakeOutline::GetComment() const
{   // "Grouping" "Undo grouping"
    return bMake ?
        ScResId( STR_UNDO_MAKEOUTLINE ) :
        ScResId( STR_UNDO_REMAKEOUTLINE );
}

void ScUndoMakeOutline::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();
    SCTAB nTab = aBlockStart.Tab();

    ScUndoUtil::MarkSimpleBlock( rDocShell, aBlockStart, aBlockEnd );

    rDoc.SetOutlineTable( nTab, pUndoTable.get() );

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top|PaintPartFlags::Size);

    ScTabViewShell::notifyAllViewsHeaderInvalidation( pViewShell, bColumns ? COLUMN_HEADER : ROW_HEADER, nTab );
    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(
            pViewShell,
            bColumns /* bColumns */, !bColumns /* bRows */,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            true /* bGroups */, nTab);

    EndUndo();
}

void ScUndoMakeOutline::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    ScDocument& rDoc = rDocShell.GetDocument();

    ScUndoUtil::MarkSimpleBlock( rDocShell, aBlockStart, aBlockEnd );

    if (bMake)
        pViewShell->MakeOutline( bColumns, false );
    else
        pViewShell->RemoveOutline( bColumns, false );

    rDocShell.PostPaint(0,0,aBlockStart.Tab(),rDoc.MaxCol(),rDoc.MaxRow(),aBlockEnd.Tab(),PaintPartFlags::Grid);

    EndRedo();
}

void ScUndoMakeOutline::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
    {
        ScTabViewShell& rViewShell = pViewTarget->GetViewShell();

        if (bMake)
            rViewShell.MakeOutline( bColumns );
        else
            rViewShell.RemoveOutline( bColumns );
    }
}

bool ScUndoMakeOutline::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
}

ScUndoOutlineLevel::ScUndoOutlineLevel( ScDocShell& rNewDocShell,
                        SCCOLROW nNewStart, SCCOLROW nNewEnd, SCTAB nNewTab,
                        ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScOutlineTable> pNewUndoTab,
                        bool bNewColumns, sal_uInt16 nNewLevel )
    : ScSimpleUndo(rNewDocShell)
    , nStart(nNewStart)
    , nEnd(nNewEnd)
    , nTab(nNewTab)
    , xUndoDoc(std::move(pNewUndoDoc))
    , xUndoTable(std::move(pNewUndoTab))
    , bColumns(bNewColumns)
    , nLevel(nNewLevel)
{
}

OUString ScUndoOutlineLevel::GetComment() const
{   // "Select outline level"
    return ScResId( STR_UNDO_OUTLINELEVEL );
}

void ScUndoOutlineLevel::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    //  Original Outline table

    rDoc.SetOutlineTable(nTab, xUndoTable.get());

    //  Original column/row status

    if (bColumns)
        xUndoDoc->CopyToDocument(static_cast<SCCOL>(nStart), 0, nTab,
                                 static_cast<SCCOL>(nEnd), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false, rDoc);
    else
        xUndoDoc->CopyToDocument(0, nStart, nTab, rDoc.MaxCol(), nEnd, nTab, InsertDeleteFlags::NONE, false, rDoc);

    rDoc.UpdatePageBreaks( nTab );

    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(pViewShell, bColumns, !bColumns,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            true /* bGroups */, nTab);
    pViewShell->UpdateScrollBars();

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top);

    EndUndo();
}

void ScUndoOutlineLevel::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    // sheet has to be switched on or off before this (#46952#) !!!

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    pViewShell->SelectLevel( bColumns, nLevel, false );

    EndRedo();
}

void ScUndoOutlineLevel::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
        pViewTarget->GetViewShell().SelectLevel( bColumns, nLevel );
}

bool ScUndoOutlineLevel::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
}

/** show/hide outline over block marks */
ScUndoOutlineBlock::ScUndoOutlineBlock( ScDocShell& rNewDocShell,
                        SCCOL nStartX, SCROW nStartY, SCTAB nStartZ,
                        SCCOL nEndX, SCROW nEndY, SCTAB nEndZ,
                        ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScOutlineTable> pNewUndoTab, bool bNewShow ) :
    ScSimpleUndo( rNewDocShell ),
    aBlockStart( nStartX, nStartY, nStartZ ),
    aBlockEnd( nEndX, nEndY, nEndZ ),
    xUndoDoc(std::move(pNewUndoDoc)),
    xUndoTable(std::move(pNewUndoTab)),
    bShow( bNewShow )
{
}

OUString ScUndoOutlineBlock::GetComment() const
{   // "Show outline" "Hide outline"
    return bShow ?
        ScResId( STR_UNDO_DOOUTLINEBLK ) :
        ScResId( STR_UNDO_REDOOUTLINEBLK );
}

void ScUndoOutlineBlock::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();
    SCTAB nTab = aBlockStart.Tab();

    //  Original Outline table
    rDoc.SetOutlineTable(nTab, xUndoTable.get());

    //  Original column/row status
    SCCOLROW    nStartCol = aBlockStart.Col();
    SCCOLROW    nEndCol = aBlockEnd.Col();
    SCCOLROW    nStartRow = aBlockStart.Row();
    SCCOLROW    nEndRow = aBlockEnd.Row();

    if (!bShow)
    {                               // Size of the hidden blocks
        size_t nLevel;
        xUndoTable->GetColArray().FindTouchedLevel(nStartCol, nEndCol, nLevel);
        xUndoTable->GetColArray().ExtendBlock(nLevel, nStartCol, nEndCol);
        xUndoTable->GetRowArray().FindTouchedLevel(nStartRow, nEndRow, nLevel);
        xUndoTable->GetRowArray().ExtendBlock(nLevel, nStartRow, nEndRow);
    }

    xUndoDoc->CopyToDocument(static_cast<SCCOL>(nStartCol), 0, nTab,
                             static_cast<SCCOL>(nEndCol), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false, rDoc);
    xUndoDoc->CopyToDocument(0, nStartRow, nTab, rDoc.MaxCol(), nEndRow, nTab, InsertDeleteFlags::NONE, false, rDoc);

    rDoc.UpdatePageBreaks( nTab );

    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(pViewShell, true /* bColumns */, true /* bRows */,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            true /* bGroups */, nTab);
    pViewShell->UpdateScrollBars();

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top);


    pViewShell->OnLOKShowHideColRow(/*columns: */ true, nStartCol - 1);
    pViewShell->OnLOKShowHideColRow(/*columns: */ false, nStartRow - 1);

    EndUndo();
}

void ScUndoOutlineBlock::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    ScUndoUtil::MarkSimpleBlock( rDocShell, aBlockStart, aBlockEnd );
    if (bShow)
        pViewShell->ShowMarkedOutlines( false );
    else
        pViewShell->HideMarkedOutlines( false );

    EndRedo();
}

void ScUndoOutlineBlock::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
    {
        ScTabViewShell& rViewShell = pViewTarget->GetViewShell();

        if (bShow)
            rViewShell.ShowMarkedOutlines();
        else
            rViewShell.HideMarkedOutlines();
    }
}

bool ScUndoOutlineBlock::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
}

ScUndoRemoveAllOutlines::ScUndoRemoveAllOutlines(ScDocShell& rNewDocShell,
                                    SCCOL nStartX, SCROW nStartY, SCTAB nStartZ,
                                    SCCOL nEndX, SCROW nEndY, SCTAB nEndZ,
                                    ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScOutlineTable> pNewUndoTab)
    : ScSimpleUndo(rNewDocShell)
    , aBlockStart(nStartX, nStartY, nStartZ)
    , aBlockEnd(nEndX, nEndY, nEndZ)
    , xUndoDoc(std::move(pNewUndoDoc))
    , xUndoTable(std::move(pNewUndoTab))
{
}

OUString ScUndoRemoveAllOutlines::GetComment() const
{   // "Remove outlines"
    return ScResId( STR_UNDO_REMOVEALLOTLNS );
}

void ScUndoRemoveAllOutlines::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();
    SCTAB nTab = aBlockStart.Tab();

    //  Original Outline table
    rDoc.SetOutlineTable(nTab, xUndoTable.get());

    //  Original column/row status
    SCCOL   nStartCol = aBlockStart.Col();
    SCCOL   nEndCol = aBlockEnd.Col();
    SCROW   nStartRow = aBlockStart.Row();
    SCROW   nEndRow = aBlockEnd.Row();

    xUndoDoc->CopyToDocument(nStartCol, 0, nTab, nEndCol, rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false, rDoc);
    xUndoDoc->CopyToDocument(0, nStartRow, nTab, rDoc.MaxCol(), nEndRow, nTab, InsertDeleteFlags::NONE, false, rDoc);

    rDoc.UpdatePageBreaks( nTab );

    pViewShell->UpdateScrollBars();

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top|PaintPartFlags::Size);

    ScTabViewShell::notifyAllViewsHeaderInvalidation(pViewShell, BOTH_HEADERS, nTab);
    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(
            pViewShell,
            true /* bColumns */, true /* bRows */,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            true /* bGroups */, nTab);

    EndUndo();
}

void ScUndoRemoveAllOutlines::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    // sheet has to be switched over (#46952#)!

    SCTAB nTab = aBlockStart.Tab();
    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    pViewShell->RemoveAllOutlines( false );

    EndRedo();
}

void ScUndoRemoveAllOutlines::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
        pViewTarget->GetViewShell().RemoveAllOutlines();
}

bool ScUndoRemoveAllOutlines::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
}

ScUndoAutoOutline::ScUndoAutoOutline(ScDocShell& rNewDocShell,
                                     SCCOL nStartX, SCROW nStartY, SCTAB nStartZ,
                                     SCCOL nEndX, SCROW nEndY, SCTAB nEndZ,
                                     ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScOutlineTable> pNewUndoTab)
    : ScSimpleUndo(rNewDocShell)
    , aBlockStart(nStartX, nStartY, nStartZ)
    , aBlockEnd(nEndX, nEndY, nEndZ)
    , xUndoDoc(std::move(pNewUndoDoc))
    , xUndoTable(std::move(pNewUndoTab))
{
}

OUString ScUndoAutoOutline::GetComment() const
{
    return ScResId( STR_UNDO_AUTOOUTLINE );
}

void ScUndoAutoOutline::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();
    SCTAB nTab = aBlockStart.Tab();

    // Original outline table
    rDoc.SetOutlineTable(nTab, xUndoTable.get());

    // Original column/row status
    if (xUndoDoc && xUndoTable)
    {
        SCCOLROW nStartCol;
        SCCOLROW nStartRow;
        SCCOLROW nEndCol;
        SCCOLROW nEndRow;
        xUndoTable->GetColArray().GetRange(nStartCol, nEndCol);
        xUndoTable->GetRowArray().GetRange(nStartRow, nEndRow);

        xUndoDoc->CopyToDocument(static_cast<SCCOL>(nStartCol), 0, nTab,
                                 static_cast<SCCOL>(nEndCol), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false,
                                 rDoc);
        xUndoDoc->CopyToDocument(0, nStartRow, nTab, rDoc.MaxCol(), nEndRow, nTab, InsertDeleteFlags::NONE, false, rDoc);

        pViewShell->UpdateScrollBars();
    }

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top|PaintPartFlags::Size);

    EndUndo();
}

void ScUndoAutoOutline::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    SCTAB nTab = aBlockStart.Tab();
    // sheet has to be switched on or off before this (#46952#) !!!

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    ScRange aRange( aBlockStart.Col(), aBlockStart.Row(), nTab,
                    aBlockEnd.Col(),   aBlockEnd.Row(),   nTab );
    ScOutlineDocFunc aFunc( rDocShell );
    aFunc.AutoOutline( aRange, false );

    //  Select in View
    //  If it was called with a multi selection,
    //  then this is now the enclosing range...

    pViewShell->MarkRange( aRange );

    EndRedo();
}

void ScUndoAutoOutline::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
        pViewTarget->GetViewShell().AutoOutline();
}

bool ScUndoAutoOutline::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
}

ScUndoSubTotals::ScUndoSubTotals(ScDocShell& rNewDocShell, SCTAB nNewTab,
                                 const ScSubTotalParam& rNewParam, SCROW nNewEndY,
                                 ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScOutlineTable> pNewUndoTab,
                                 std::unique_ptr<ScRangeName> pNewUndoRange, std::unique_ptr<ScDBCollection> pNewUndoDB)
    : ScDBFuncUndo(rNewDocShell, ScRange(rNewParam.nCol1, rNewParam.nRow1, nNewTab,
                                         rNewParam.nCol2, rNewParam.nRow2, nNewTab))
    , nTab(nNewTab)
    , aParam(rNewParam)
    , nNewEndRow(nNewEndY)
    , xUndoDoc(std::move(pNewUndoDoc))
    , xUndoTable(std::move(pNewUndoTab))
    , xUndoRange(std::move(pNewUndoRange))
    , xUndoDB(std::move(pNewUndoDB))
{
}

OUString ScUndoSubTotals::GetComment() const
{   // "Subtotals"
    return ScResId( STR_UNDO_SUBTOTALS );
}

void ScUndoSubTotals::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    if (nNewEndRow > aParam.nRow2)
    {
        rDoc.DeleteRow( 0,nTab, rDoc.MaxCol(),nTab, aParam.nRow2+1, static_cast<SCSIZE>(nNewEndRow-aParam.nRow2) );
    }
    else if (nNewEndRow < aParam.nRow2)
    {
        rDoc.InsertRow( 0,nTab, rDoc.MaxCol(),nTab, nNewEndRow+1, static_cast<SCSIZE>(aParam.nRow2-nNewEndRow) );
    }

    // Original Outline table
    rDoc.SetOutlineTable(nTab, xUndoTable.get());

    // Original column/row status
    if (xUndoTable)
    {
        SCCOLROW nStartCol;
        SCCOLROW nStartRow;
        SCCOLROW nEndCol;
        SCCOLROW nEndRow;
        xUndoTable->GetColArray().GetRange(nStartCol, nEndCol);
        xUndoTable->GetRowArray().GetRange(nStartRow, nEndRow);

        xUndoDoc->CopyToDocument(static_cast<SCCOL>(nStartCol), 0, nTab,
                                 static_cast<SCCOL>(nEndCol), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false,
                                 rDoc);
        xUndoDoc->CopyToDocument(0, nStartRow, nTab, rDoc.MaxCol(), nEndRow, nTab, InsertDeleteFlags::NONE, false, rDoc);

        pViewShell->UpdateScrollBars();
    }

    //  Original data and references

    ScUndoUtil::MarkSimpleBlock( rDocShell, 0, aParam.nRow1+1, nTab,
                                            rDoc.MaxCol(), aParam.nRow2, nTab );

    rDoc.DeleteAreaTab( 0,aParam.nRow1+1, rDoc.MaxCol(),aParam.nRow2, nTab, InsertDeleteFlags::ALL );

    xUndoDoc->CopyToDocument(0, aParam.nRow1+1, nTab, rDoc.MaxCol(), aParam.nRow2, nTab,
                                                            InsertDeleteFlags::NONE, false, rDoc);    // Flags
    xUndoDoc->UndoToDocument(0, aParam.nRow1+1, nTab, rDoc.MaxCol(), aParam.nRow2, nTab,
                                                            InsertDeleteFlags::ALL, false, rDoc);

    ScUndoUtil::MarkSimpleBlock( rDocShell, aParam.nCol1,aParam.nRow1,nTab,
                                            aParam.nCol2,aParam.nRow2,nTab );

    if (xUndoRange)
        rDoc.SetRangeName(std::unique_ptr<ScRangeName>(new ScRangeName(*xUndoRange)));
    if (xUndoDB)
        rDoc.SetDBCollection(std::unique_ptr<ScDBCollection>(new ScDBCollection(*xUndoDB)), true);

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top|PaintPartFlags::Size);
    rDocShell.PostDataChanged();

    EndUndo();
}

void ScUndoSubTotals::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    ScUndoUtil::MarkSimpleBlock( rDocShell, aParam.nCol1,aParam.nRow1,nTab,
                                            aParam.nCol2,aParam.nRow2,nTab );
    pViewShell->DoSubTotals( aParam, false );

    EndRedo();
}

void ScUndoSubTotals::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoSubTotals::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;     // is not possible due to column numbers
}

ScUndoQuery::ScUndoQuery( ScDocShell& rNewDocShell, SCTAB nNewTab, const ScQueryParam& rParam,
                            ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScDBCollection> pNewUndoDB,
                            const ScRange* pOld, bool bSize, const ScRange* pAdvSrc ) :
    ScDBFuncUndo( rNewDocShell, ScRange( rParam.nCol1, rParam.nRow1, nNewTab,
                                         rParam.nCol2, rParam.nRow2, nNewTab ) ),
    nTab( nNewTab ),
    aQueryParam( rParam ),
    xUndoDoc( std::move(pNewUndoDoc) ),
    xUndoDB( std::move(pNewUndoDB) ),
    bIsAdvanced( false ),
    bDestArea( false ),
    bDoSize( bSize )
{
    if ( pOld )
    {
        bDestArea = true;
        aOldDest = *pOld;
    }
    if ( pAdvSrc )
    {
        bIsAdvanced = true;
        aAdvSource = *pAdvSrc;
    }

    pDrawUndo = GetSdrUndoAction( &rDocShell.GetDocument() );
}

ScUndoQuery::~ScUndoQuery()
{
    pDrawUndo.reset();
}

OUString ScUndoQuery::GetComment() const
{   // "Filter";
    return ScResId( STR_UNDO_QUERY );
}

void ScUndoQuery::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    if (ScTabViewShell::isAnyEditViewInRange(pViewShell, /*bColumns*/ false, aQueryParam.nRow1, aQueryParam.nRow2))
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    bool bCopy = !aQueryParam.bInplace;
    SCCOL nDestEndCol = 0;
    SCROW nDestEndRow = 0;
    if (bCopy)
    {
        nDestEndCol = aQueryParam.nDestCol + ( aQueryParam.nCol2-aQueryParam.nCol1 );
        nDestEndRow = aQueryParam.nDestRow + ( aQueryParam.nRow2-aQueryParam.nRow1 );

        ScDBData* pData = rDoc.GetDBAtCursor( aQueryParam.nDestCol, aQueryParam.nDestRow,
                                                aQueryParam.nDestTab, ScDBDataPortion::TOP_LEFT );
        if (pData)
        {
            ScRange aNewDest;
            pData->GetArea( aNewDest );
            nDestEndCol = aNewDest.aEnd.Col();
            nDestEndRow = aNewDest.aEnd.Row();
        }

        if ( bDoSize && bDestArea )
        {
            //  aDestRange is the old range
            rDoc.FitBlock( ScRange(
                                aQueryParam.nDestCol, aQueryParam.nDestRow, aQueryParam.nDestTab,
                                nDestEndCol, nDestEndRow, aQueryParam.nDestTab ),
                            aOldDest );
        }

        ScUndoUtil::MarkSimpleBlock( rDocShell,
                                    aQueryParam.nDestCol, aQueryParam.nDestRow, aQueryParam.nDestTab,
                                    nDestEndCol, nDestEndRow, aQueryParam.nDestTab );
        rDoc.DeleteAreaTab( aQueryParam.nDestCol, aQueryParam.nDestRow,
                            nDestEndCol, nDestEndRow, aQueryParam.nDestTab, InsertDeleteFlags::ALL );

        pViewShell->DoneBlockMode();

        xUndoDoc->CopyToDocument(aQueryParam.nDestCol, aQueryParam.nDestRow, aQueryParam.nDestTab,
                                 nDestEndCol, nDestEndRow, aQueryParam.nDestTab,
                                 InsertDeleteFlags::ALL, false, rDoc);
        //  Attributes are always copied (#49287#)

        // rest of the old range
        if ( bDestArea && !bDoSize )
        {
            rDoc.DeleteAreaTab( aOldDest, InsertDeleteFlags::ALL );
            xUndoDoc->CopyToDocument(aOldDest, InsertDeleteFlags::ALL, false, rDoc);
        }
    }
    else
        xUndoDoc->CopyToDocument(0, aQueryParam.nRow1, nTab, rDoc.MaxCol(), aQueryParam.nRow2, nTab,
                                 InsertDeleteFlags::NONE, false, rDoc);

    if (xUndoDB)
        rDoc.SetDBCollection(std::unique_ptr<ScDBCollection>(new ScDBCollection(*xUndoDB )), true);

    if (!bCopy)
    {
        rDoc.InvalidatePageBreaks(nTab);
        rDoc.UpdatePageBreaks( nTab );
    }

    ScRange aDirtyRange( 0 , aQueryParam.nRow1, nTab,
        rDoc.MaxCol(), aQueryParam.nRow2, nTab );
    rDoc.SetDirty( aDirtyRange, true );

    DoSdrUndoAction( pDrawUndo.get(), &rDoc );

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );


    // invalidate cache positions and update cursor and selection
    pViewShell->OnLOKShowHideColRow(/*bColumns*/ false, aQueryParam.nRow1 - 1);
    ScTabViewShell::notifyAllViewsHeaderInvalidation(pViewShell, ROW_HEADER, nTab);
    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(
            pViewShell,
            false /* bColumns */, true /* bRows */,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            false /* bGroups */, nTab);

    //  Paint

    if (bCopy)
    {
        SCCOL nEndX = nDestEndCol;
        SCROW nEndY = nDestEndRow;
        if (bDestArea)
        {
            if ( aOldDest.aEnd.Col() > nEndX )
                nEndX = aOldDest.aEnd.Col();
            if ( aOldDest.aEnd.Row() > nEndY )
                nEndY = aOldDest.aEnd.Row();
        }
        if (bDoSize)
            nEndY = rDoc.MaxRow();
        rDocShell.PostPaint( aQueryParam.nDestCol, aQueryParam.nDestRow, aQueryParam.nDestTab,
                                    nEndX, nEndY, aQueryParam.nDestTab, PaintPartFlags::Grid );
    }
    else
        rDocShell.PostPaint( 0, aQueryParam.nRow1, nTab, rDoc.MaxCol(), rDoc.MaxRow(), nTab,
                                                    PaintPartFlags::Grid | PaintPartFlags::Left );
    rDocShell.PostDataChanged();

    EndUndo();
}

void ScUndoQuery::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    if ( bIsAdvanced )
        pViewShell->Query( aQueryParam, &aAdvSource, false );
    else
        pViewShell->Query( aQueryParam, nullptr, false );

    EndRedo();
}

void ScUndoQuery::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoQuery::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;    // does not work due to column numbers
}

//      Show or hide AutoFilter buttons (doesn't include filter settings)

ScUndoAutoFilter::ScUndoAutoFilter( ScDocShell& rNewDocShell, const ScRange& rRange,
                                    OUString aName, bool bSet ) :
    ScDBFuncUndo( rNewDocShell, rRange ),
    aDBName(std::move( aName )),
    bFilterSet( bSet )
{
}

ScUndoAutoFilter::~ScUndoAutoFilter()
{
}

OUString ScUndoAutoFilter::GetComment() const
{
    return ScResId( STR_UNDO_QUERY );    // same as ScUndoQuery
}

void ScUndoAutoFilter::DoChange( bool bUndo )
{
    bool bNewFilter = bUndo ? !bFilterSet : bFilterSet;

    ScDocument& rDoc = rDocShell.GetDocument();
    ScDBData* pDBData=nullptr;
    if (aDBName == STR_DB_LOCAL_NONAME)
    {
        SCTAB nTab = aOriginalRange.aStart.Tab();
        pDBData = rDoc.GetAnonymousDBData(nTab);
    }
    else
    {
        ScDBCollection* pColl = rDoc.GetDBCollection();
        pDBData = pColl->getNamedDBs().findByUpperName(ScGlobal::getCharClass().uppercase(aDBName));
    }

    if ( !pDBData )
        return;

    pDBData->SetAutoFilter( bNewFilter );

    SCCOL nRangeX1;
    SCROW nRangeY1;
    SCCOL nRangeX2;
    SCROW nRangeY2;
    SCTAB nRangeTab;
    pDBData->GetArea( nRangeTab, nRangeX1, nRangeY1, nRangeX2, nRangeY2 );

    if ( bNewFilter )
        rDoc.ApplyFlagsTab( nRangeX1, nRangeY1, nRangeX2, nRangeY1, nRangeTab, ScMF::Auto );
    else
        rDoc.RemoveFlagsTab( nRangeX1, nRangeY1, nRangeX2, nRangeY1, nRangeTab, ScMF::Auto );

    rDocShell.PostPaint( nRangeX1, nRangeY1, nRangeTab, nRangeX2, nRangeY1, nRangeTab, PaintPartFlags::Grid );
}

void ScUndoAutoFilter::Undo()
{
    BeginUndo();
    DoChange( true );
    EndUndo();
}

void ScUndoAutoFilter::Redo()
{
    BeginRedo();
    DoChange( false );
    EndRedo();
}

void ScUndoAutoFilter::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoAutoFilter::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;
}

// change database sections (dialog)
ScUndoDBData::ScUndoDBData( ScDocShell& rNewDocShell,
                            std::unique_ptr<ScDBCollection> pNewUndoColl,
                            std::unique_ptr<ScDBCollection> pNewRedoColl ) :
    ScSimpleUndo( rNewDocShell ),
    pUndoColl( std::move(pNewUndoColl) ),
    pRedoColl( std::move(pNewRedoColl) )
{
}

ScUndoDBData::~ScUndoDBData()
{
}

OUString ScUndoDBData::GetComment() const
{   // "Change database range";
    return ScResId( STR_UNDO_DBDATA );
}

void ScUndoDBData::Undo()
{
    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    bool bOldAutoCalc = rDoc.GetAutoCalc();
    rDoc.SetAutoCalc( false );         // Avoid unnecessary calculations
    rDoc.PreprocessDBDataUpdate();
    rDoc.SetDBCollection( std::unique_ptr<ScDBCollection>(new ScDBCollection(*pUndoColl)), true );
    rDoc.CompileHybridFormula();
    rDoc.SetAutoCalc( bOldAutoCalc );

    SfxGetpApp()->Broadcast( SfxHint( SfxHintId::ScDbAreasChanged ) );

    EndUndo();
}

void ScUndoDBData::Redo()
{
    BeginRedo();

    ScDocument& rDoc = rDocShell.GetDocument();

    bool bOldAutoCalc = rDoc.GetAutoCalc();
    rDoc.SetAutoCalc( false );         // Avoid unnecessary calculations
    rDoc.PreprocessDBDataUpdate();
    rDoc.SetDBCollection( std::unique_ptr<ScDBCollection>(new ScDBCollection(*pRedoColl)), true );
    rDoc.CompileHybridFormula();
    rDoc.SetAutoCalc( bOldAutoCalc );

    SfxGetpApp()->Broadcast( SfxHint( SfxHintId::ScDbAreasChanged ) );

    EndRedo();
}

void ScUndoDBData::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoDBData::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;    // is not possible
}

ScUndoImportData::ScUndoImportData( ScDocShell& rNewDocShell, SCTAB nNewTab,
                                const ScImportParam& rParam, SCCOL nNewEndX, SCROW nNewEndY,
                                SCCOL nNewFormula,
                                ScDocumentUniquePtr pNewUndoDoc, ScDocumentUniquePtr pNewRedoDoc,
                                std::unique_ptr<ScDBData> pNewUndoData, std::unique_ptr<ScDBData> pNewRedoData ) :
    ScSimpleUndo( rNewDocShell ),
    nTab( nNewTab ),
    aImportParam( rParam ),
    nEndCol( nNewEndX ),
    nEndRow( nNewEndY ),
    xUndoDoc(std::move(pNewUndoDoc)),
    xRedoDoc(std::move(pNewRedoDoc)),
    xUndoDBData(std::move(pNewUndoData)),
    xRedoDBData(std::move(pNewRedoData)),
    nFormulaCols( nNewFormula ),
    bRedoFilled( false )
{
    // redo doc doesn't contain imported data (but everything else)
}

OUString ScUndoImportData::GetComment() const
{
    return ScResId( STR_UNDO_IMPORTDATA );
}

void ScUndoImportData::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    ScUndoUtil::MarkSimpleBlock( rDocShell, aImportParam.nCol1,aImportParam.nRow1,nTab,
                                                        nEndCol,nEndRow,nTab );

    SCTAB nTable;
    SCCOL nCol1, nCol2;
    SCROW nRow1, nRow2;
    ScDBData* pCurrentData = nullptr;
    if (xUndoDBData && xRedoDBData)
    {
        xRedoDBData->GetArea( nTable, nCol1, nRow1, nCol2, nRow2 );
        pCurrentData = ScUndoUtil::GetOldDBData(xRedoDBData.get(), &rDoc, nTab,
                                                nCol1, nRow1, nCol2, nRow2);

        if ( !bRedoFilled )
        {
            //  read redo data from document at first undo
            //  imported data is deleted later anyway,
            //  so now delete each column after copying to save memory (#41216#)

            bool bOldAutoCalc = rDoc.GetAutoCalc();
            rDoc.SetAutoCalc( false );             // outside of the loop
            for (SCCOL nCopyCol = nCol1; nCopyCol <= nCol2; nCopyCol++)
            {
                rDoc.CopyToDocument(nCopyCol,nRow1,nTab, nCopyCol,nRow2,nTab,
                                    InsertDeleteFlags::CONTENTS & ~InsertDeleteFlags::NOTE, false, *xRedoDoc);
                rDoc.DeleteAreaTab(nCopyCol, nRow1, nCopyCol, nRow2, nTab, InsertDeleteFlags::CONTENTS & ~InsertDeleteFlags::NOTE);
            }
            rDoc.SetAutoCalc( bOldAutoCalc );
            bRedoFilled = true;
        }
    }
    bool bMoveCells = xUndoDBData && xRedoDBData &&
                        xRedoDBData->IsDoSize();        // the same in old and new
    if (bMoveCells)
    {
        //  Undo: first delete the new data, then FitBlock backwards

        ScRange aOld, aNew;
        xUndoDBData->GetArea(aOld);
        xRedoDBData->GetArea(aNew);

        rDoc.DeleteAreaTab( aNew.aStart.Col(), aNew.aStart.Row(),
                                aNew.aEnd.Col(), aNew.aEnd.Row(), nTab, InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE );

        aOld.aEnd.SetCol( aOld.aEnd.Col() + nFormulaCols );     // FitBlock also for formulas
        aNew.aEnd.SetCol( aNew.aEnd.Col() + nFormulaCols );
        rDoc.FitBlock( aNew, aOld, false );                    // backwards
    }
    else
        rDoc.DeleteAreaTab( aImportParam.nCol1,aImportParam.nRow1,
                                nEndCol,nEndRow, nTab, InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE );

    xUndoDoc->CopyToDocument(aImportParam.nCol1,aImportParam.nRow1,nTab,
                             nEndCol+nFormulaCols,nEndRow,nTab,
                             InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE, false, rDoc);

    if (pCurrentData)
    {
        *pCurrentData = *xUndoDBData;

        xUndoDBData->GetArea(nTable, nCol1, nRow1, nCol2, nRow2);
        ScUndoUtil::MarkSimpleBlock( rDocShell, nCol1, nRow1, nTable, nCol2, nRow2, nTable );
    }

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    if (bMoveCells)
        rDocShell.PostPaint( 0,0,nTab, rDoc.MaxCol(),rDoc.MaxRow(),nTab, PaintPartFlags::Grid );
    else
        rDocShell.PostPaint( aImportParam.nCol1,aImportParam.nRow1,nTab,
                                nEndCol,nEndRow,nTab, PaintPartFlags::Grid );
    rDocShell.PostDataChanged();

    EndUndo();
}

void ScUndoImportData::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    ScDocument& rDoc = rDocShell.GetDocument();

    ScUndoUtil::MarkSimpleBlock( rDocShell, aImportParam.nCol1,aImportParam.nRow1,nTab,
                                                        nEndCol,nEndRow,nTab );

    SCTAB nTable;
    SCCOL nCol1, nCol2;
    SCROW nRow1, nRow2;
    ScDBData* pCurrentData = nullptr;
    if (xUndoDBData && xRedoDBData)
    {
        xUndoDBData->GetArea( nTable, nCol1, nRow1, nCol2, nRow2 );
        pCurrentData = ScUndoUtil::GetOldDBData(xUndoDBData.get(), &rDoc, nTab,
                                                nCol1, nRow1, nCol2, nRow2);
    }
    bool bMoveCells = xUndoDBData && xRedoDBData &&
                        xRedoDBData->IsDoSize();        // the same in old and new
    if (bMoveCells)
    {
        //  Redo: FitBlock, then delete data (needed for CopyToDocument)

        ScRange aOld, aNew;
        xUndoDBData->GetArea(aOld);
        xRedoDBData->GetArea(aNew);

        aOld.aEnd.SetCol( aOld.aEnd.Col() + nFormulaCols );     // FitBlock also for formulas
        aNew.aEnd.SetCol( aNew.aEnd.Col() + nFormulaCols );
        rDoc.FitBlock( aOld, aNew );

        rDoc.DeleteAreaTab( aNew.aStart.Col(), aNew.aStart.Row(),
                                aNew.aEnd.Col(), aNew.aEnd.Row(), nTab, InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE );

        xRedoDoc->CopyToDocument(aNew, InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE, false, rDoc);        // including formulas
    }
    else
    {
        rDoc.DeleteAreaTab( aImportParam.nCol1,aImportParam.nRow1,
                                nEndCol,nEndRow, nTab, InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE );
        xRedoDoc->CopyToDocument(aImportParam.nCol1,aImportParam.nRow1,nTab,
                                 nEndCol,nEndRow,nTab, InsertDeleteFlags::ALL & ~InsertDeleteFlags::NOTE, false, rDoc);
    }

    if (pCurrentData)
    {
        *pCurrentData = *xRedoDBData;

        xRedoDBData->GetArea(nTable, nCol1, nRow1, nCol2, nRow2);
        ScUndoUtil::MarkSimpleBlock( rDocShell, nCol1, nRow1, nTable, nCol2, nRow2, nTable );
    }

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    if (bMoveCells)
        rDocShell.PostPaint( 0,0,nTab, rDoc.MaxCol(),rDoc.MaxRow(),nTab, PaintPartFlags::Grid );
    else
        rDocShell.PostPaint( aImportParam.nCol1,aImportParam.nRow1,nTab,
                                nEndCol,nEndRow,nTab, PaintPartFlags::Grid );
    rDocShell.PostDataChanged();

    EndRedo();
}

void ScUndoImportData::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
    {
        ScTabViewShell& rViewShell = pViewTarget->GetViewShell();

        SCTAB nDummy;
        ScImportParam aNewParam(aImportParam);
        ScDBData* pDBData = rViewShell.GetDBData();
        pDBData->GetArea( nDummy, aNewParam.nCol1,aNewParam.nRow1, aNewParam.nCol2,aNewParam.nRow2 );

        rViewShell.ImportData( aNewParam );
    }
}

bool ScUndoImportData::CanRepeat(SfxRepeatTarget& rTarget) const
{
    //  Repeat only for import using a database range, then xUndoDBData is set

    if (xUndoDBData)
        return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
    else
        return false;       // Address book
}

ScUndoRepeatDB::ScUndoRepeatDB( ScDocShell& rNewDocShell, SCTAB nNewTab,
                                SCCOL nStartX, SCROW nStartY, SCCOL nEndX, SCROW nEndY,
                                SCROW nResultEndRow, SCCOL nCurX, SCROW nCurY,
                                ScDocumentUniquePtr pNewUndoDoc, std::unique_ptr<ScOutlineTable> pNewUndoTab,
                                std::unique_ptr<ScRangeName> pNewUndoRange, std::unique_ptr<ScDBCollection> pNewUndoDB,
                                const ScRange* pOldQ, const ScRange* pNewQ ) :
    ScSimpleUndo( rNewDocShell ),
    aBlockStart( nStartX,nStartY,nNewTab ),
    aBlockEnd( nEndX,nEndY,nNewTab ),
    nNewEndRow( nResultEndRow ),
    aCursorPos( nCurX,nCurY,nNewTab ),
    xUndoDoc(std::move(pNewUndoDoc)),
    xUndoTable(std::move(pNewUndoTab)),
    xUndoRange(std::move(pNewUndoRange)),
    xUndoDB(std::move(pNewUndoDB)),
    bQuerySize( false )
{
    if ( pOldQ && pNewQ )
    {
        aOldQuery = *pOldQ;
        aNewQuery = *pNewQ;
        bQuerySize = true;
    }
}

OUString ScUndoRepeatDB::GetComment() const
{
    return ScResId( STR_UNDO_REPEATDB );
}

void ScUndoRepeatDB::Undo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();
    SCTAB nTab = aBlockStart.Tab();

    if (bQuerySize)
    {
        rDoc.FitBlock( aNewQuery, aOldQuery, false );

        if ( aNewQuery.aEnd.Col() == aOldQuery.aEnd.Col() )
        {
            SCCOL nFormulaCols = 0;
            SCCOL nCol = aOldQuery.aEnd.Col() + 1;
            SCROW nRow = aOldQuery.aStart.Row() + 1;        // test the header
            while ( nCol <= rDoc.MaxCol() &&
                    rDoc.GetCellType(ScAddress( nCol, nRow, nTab )) == CELLTYPE_FORMULA )
            {
                ++nCol;
                ++nFormulaCols;
            }

            if ( nFormulaCols > 0 )
            {
                ScRange aOldForm = aOldQuery;
                aOldForm.aStart.SetCol( aOldQuery.aEnd.Col() + 1 );
                aOldForm.aEnd.SetCol( aOldQuery.aEnd.Col() + nFormulaCols );
                ScRange aNewForm = aOldForm;
                aNewForm.aEnd.SetRow( aNewQuery.aEnd.Row() );
                rDoc.FitBlock( aNewForm, aOldForm, false );
            }
        }
    }

    // TODO Data from Filter in other range are still missing!

    if (nNewEndRow > aBlockEnd.Row())
    {
        rDoc.DeleteRow( 0,nTab, rDoc.MaxCol(),nTab, aBlockEnd.Row()+1, static_cast<SCSIZE>(nNewEndRow-aBlockEnd.Row()) );
    }
    else if (nNewEndRow < aBlockEnd.Row())
    {
        rDoc.InsertRow( 0,nTab, rDoc.MaxCol(),nTab, nNewEndRow+1, static_cast<SCSIZE>(nNewEndRow-aBlockEnd.Row()) );
    }

    // Original Outline table
    rDoc.SetOutlineTable(nTab, xUndoTable.get());

    // Original column/row status
    if (xUndoTable)
    {
        SCCOLROW nStartCol;
        SCCOLROW nStartRow;
        SCCOLROW nEndCol;
        SCCOLROW nEndRow;
        xUndoTable->GetColArray().GetRange(nStartCol, nEndCol);
        xUndoTable->GetRowArray().GetRange(nStartRow, nEndRow);

        xUndoDoc->CopyToDocument(static_cast<SCCOL>(nStartCol), 0, nTab,
                                 static_cast<SCCOL>(nEndCol), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false,
                                 rDoc);
        xUndoDoc->CopyToDocument(0, nStartRow, nTab, rDoc.MaxCol(), nEndRow, nTab, InsertDeleteFlags::NONE, false, rDoc);

        pViewShell->UpdateScrollBars();
    }

    //  Original data and references
    ScUndoUtil::MarkSimpleBlock( rDocShell, 0, aBlockStart.Row(), nTab,
                                            rDoc.MaxCol(), aBlockEnd.Row(), nTab );
    rDoc.DeleteAreaTab( 0, aBlockStart.Row(),
                            rDoc.MaxCol(), aBlockEnd.Row(), nTab, InsertDeleteFlags::ALL );

    xUndoDoc->CopyToDocument(0, aBlockStart.Row(), nTab, rDoc.MaxCol(), aBlockEnd.Row(), nTab,
                             InsertDeleteFlags::NONE, false, rDoc);            // Flags
    xUndoDoc->UndoToDocument(0, aBlockStart.Row(), nTab, rDoc.MaxCol(), aBlockEnd.Row(), nTab,
                             InsertDeleteFlags::ALL, false, rDoc);

    ScUndoUtil::MarkSimpleBlock( rDocShell, aBlockStart.Col(),aBlockStart.Row(),nTab,
                                            aBlockEnd.Col(),aBlockEnd.Row(),nTab );

    if (xUndoRange)
        rDoc.SetRangeName(std::unique_ptr<ScRangeName>(new ScRangeName(*xUndoRange)));
    if (xUndoDB)
        rDoc.SetDBCollection(std::unique_ptr<ScDBCollection>(new ScDBCollection(*xUndoDB)), true);

    ScTabViewShell::notifyAllViewsSheetGeomInvalidation(pViewShell, false /* bColumns */, true /* bRows */,
            false /* bSizes*/, true /* bHidden */, true /* bFiltered */,
            false /* bGroups */, nTab);

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    rDocShell.PostPaint(0,0,nTab,rDoc.MaxCol(),rDoc.MaxRow(),nTab,PaintPartFlags::Grid|PaintPartFlags::Left|PaintPartFlags::Top|PaintPartFlags::Size);
    rDocShell.PostDataChanged();

    EndUndo();
}

void ScUndoRepeatDB::Redo()
{
    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (!pViewShell)
        return;

    BeginRedo();

    SCTAB nTab = aBlockStart.Tab();

    SCTAB nVisTab = pViewShell->GetViewData().GetTabNo();
    if ( nVisTab != nTab )
        pViewShell->SetTabNo( nTab );

    ScUndoUtil::MarkSimpleBlock( rDocShell, aBlockStart.Col(),aBlockStart.Row(),nTab,
                                            aBlockEnd.Col(),aBlockEnd.Row(),nTab );
    pViewShell->SetCursor( aCursorPos.Col(), aCursorPos.Row() );

    pViewShell->RepeatDB( false );

    EndRedo();
}

void ScUndoRepeatDB::Repeat(SfxRepeatTarget& rTarget)
{
    if (auto pViewTarget = dynamic_cast<ScTabViewTarget*>( &rTarget))
        pViewTarget->GetViewShell().RepeatDB();
}

bool ScUndoRepeatDB::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr;
}

ScUndoDataPilot::ScUndoDataPilot( ScDocShell& rNewDocShell,
                            ScDocumentUniquePtr pOldDoc, ScDocumentUniquePtr pNewDoc,
                            const ScDPObject* pOldObj, const ScDPObject* pNewObj, bool bMove )
    : ScSimpleUndo(rNewDocShell)
    , xOldUndoDoc(std::move(pOldDoc))
    , xNewUndoDoc(std::move(pNewDoc))
    , bAllowMove( bMove)
{
    if (pOldObj)
        xOldDPObject.reset(new ScDPObject(*pOldObj));
    if (pNewObj)
        xNewDPObject.reset(new ScDPObject(*pNewObj));
}

OUString ScUndoDataPilot::GetComment() const
{
    TranslateId pResId;
    if (xOldUndoDoc && xNewUndoDoc)
        pResId = STR_UNDO_PIVOT_MODIFY;
    else if (xNewUndoDoc)
        pResId = STR_UNDO_PIVOT_NEW;
    else
        pResId = STR_UNDO_PIVOT_DELETE;

    return ScResId(pResId);
}

void ScUndoDataPilot::Undo()
{
    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();

    ScRange aOldRange;
    ScRange aNewRange;

    if (xNewDPObject && xNewUndoDoc)
    {
        aNewRange = xNewDPObject->GetOutRange();
        rDoc.DeleteAreaTab( aNewRange, InsertDeleteFlags::ALL );
        xNewUndoDoc->CopyToDocument(aNewRange, InsertDeleteFlags::ALL, false, rDoc);
    }
    if (xOldDPObject && xOldUndoDoc)
    {
        aOldRange = xOldDPObject->GetOutRange();
        rDoc.DeleteAreaTab(aOldRange, InsertDeleteFlags::ALL);
        xOldUndoDoc->CopyToDocument(aOldRange, InsertDeleteFlags::ALL, false, rDoc);
    }

    //  update objects in collection
    if (xNewDPObject)
    {
        //  find updated object
        //! find by name!

        ScDPObject* pDocObj = rDoc.GetDPAtCursor(
                            aNewRange.aStart.Col(), aNewRange.aStart.Row(), aNewRange.aStart.Tab() );
        OSL_ENSURE(pDocObj, "DPObject not found");
        if (pDocObj)
        {
            if (xOldDPObject)
            {
                //  restore old settings
                xOldDPObject->WriteSourceDataTo( *pDocObj );
                ScDPSaveData* pData = xOldDPObject->GetSaveData();
                if (pData)
                    pDocObj->SetSaveData(*pData);
                pDocObj->SetOutRange(xOldDPObject->GetOutRange());
                xOldDPObject->WriteTempDataTo( *pDocObj );
            }
            else
            {
                //  delete inserted object
                rDoc.GetDPCollection()->FreeTable(pDocObj);
            }
        }
    }
    else if (xOldDPObject)
    {
        //  re-insert deleted object
        rDoc.GetDPCollection()->InsertNewTable(std::make_unique<ScDPObject>(*xOldDPObject));
    }

    if (xNewUndoDoc)
        rDocShell.PostPaint(aNewRange, PaintPartFlags::Grid, SC_PF_LINES);
    if (xOldUndoDoc)
        rDocShell.PostPaint(aOldRange, PaintPartFlags::Grid, SC_PF_LINES);
    rDocShell.PostDataChanged();

    if (xNewDPObject)
    {
        // notify API objects
        rDoc.BroadcastUno(ScDataPilotModifiedHint(xNewDPObject->GetName()));
    }

    EndUndo();
}

void ScUndoDataPilot::Redo()
{
    BeginRedo();

    //! copy output data instead of repeating the change,
    //! in case external data have changed!

    ScDocument& rDoc = rDocShell.GetDocument();

    ScDPObject* pSourceObj = nullptr;
    if (xOldDPObject)
    {
        //  find object to modify
        //! find by name!

        ScRange aOldRange = xOldDPObject->GetOutRange();
        pSourceObj = rDoc.GetDPAtCursor(
                        aOldRange.aStart.Col(), aOldRange.aStart.Row(), aOldRange.aStart.Tab() );
        OSL_ENSURE(pSourceObj, "DPObject not found");
    }

    ScDBDocFunc aFunc( rDocShell );
    aFunc.DataPilotUpdate(pSourceObj, xNewDPObject.get(), false, false, bAllowMove);    // no new undo action

    EndRedo();
}

void ScUndoDataPilot::Repeat(SfxRepeatTarget& /* rTarget */)
{
    //! allow deletion
}

bool ScUndoDataPilot::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    //! allow deletion
    return false;
}

ScUndoConsolidate::ScUndoConsolidate( ScDocShell& rNewDocShell, const ScArea& rArea,
                    const ScConsolidateParam& rPar, ScDocumentUniquePtr pNewUndoDoc,
                    bool bReference, SCROW nInsCount, std::unique_ptr<ScOutlineTable> pTab,
                    std::unique_ptr<ScDBData> pData )
    : ScSimpleUndo(rNewDocShell)
    , aDestArea(rArea)
    , xUndoDoc(std::move(pNewUndoDoc))
    , aParam(rPar)
    , bInsRef(bReference)
    , nInsertCount(nInsCount)
    , xUndoTab(std::move(pTab))
    , xUndoData(std::move(pData))
{
}

OUString ScUndoConsolidate::GetComment() const
{
    return ScResId( STR_UNDO_CONSOLIDATE );
}

void ScUndoConsolidate::Undo()
{
    BeginUndo();

    ScDocument& rDoc = rDocShell.GetDocument();
    SCTAB nTab = aDestArea.nTab;

    ScRange aOldRange;
    if (xUndoData)
        xUndoData->GetArea(aOldRange);

    if (bInsRef)
    {
        rDoc.DeleteRow( 0,nTab, rDoc.MaxCol(),nTab, aDestArea.nRowStart, nInsertCount );
        rDoc.SetOutlineTable(nTab, xUndoTab.get());

        // Row status
        xUndoDoc->CopyToDocument(0, 0, nTab, rDoc.MaxCol(), rDoc.MaxRow(), nTab, InsertDeleteFlags::NONE, false, rDoc);

        // Data and references
        rDoc.DeleteAreaTab( 0,aDestArea.nRowStart, rDoc.MaxCol(),aDestArea.nRowEnd, nTab, InsertDeleteFlags::ALL );
        xUndoDoc->UndoToDocument(0, aDestArea.nRowStart, nTab,
                                 rDoc.MaxCol(), aDestArea.nRowEnd, nTab,
                                 InsertDeleteFlags::ALL, false, rDoc);

        // Original range
        if (xUndoData)
        {
            rDoc.DeleteAreaTab(aOldRange, InsertDeleteFlags::ALL);
            xUndoDoc->CopyToDocument(aOldRange, InsertDeleteFlags::ALL, false, rDoc);
        }

        rDocShell.PostPaint( 0,aDestArea.nRowStart,nTab, rDoc.MaxCol(),rDoc.MaxRow(),nTab,
                                PaintPartFlags::Grid | PaintPartFlags::Left | PaintPartFlags::Size );
    }
    else
    {
        rDoc.DeleteAreaTab( aDestArea.nColStart,aDestArea.nRowStart,
                                aDestArea.nColEnd,aDestArea.nRowEnd, nTab, InsertDeleteFlags::ALL );
        xUndoDoc->CopyToDocument(aDestArea.nColStart, aDestArea.nRowStart, nTab,
                                 aDestArea.nColEnd, aDestArea.nRowEnd, nTab,
                                 InsertDeleteFlags::ALL, false, rDoc);

        //  Original range
        if (xUndoData)
        {
            rDoc.DeleteAreaTab(aOldRange, InsertDeleteFlags::ALL);
            xUndoDoc->CopyToDocument(aOldRange, InsertDeleteFlags::ALL, false, rDoc);
        }

        SCCOL nEndX = aDestArea.nColEnd;
        SCROW nEndY = aDestArea.nRowEnd;
        if (xUndoData)
        {
            if ( aOldRange.aEnd.Col() > nEndX )
                nEndX = aOldRange.aEnd.Col();
            if ( aOldRange.aEnd.Row() > nEndY )
                nEndY = aOldRange.aEnd.Row();
        }
        rDocShell.PostPaint( aDestArea.nColStart, aDestArea.nRowStart, nTab,
                                    nEndX, nEndY, nTab, PaintPartFlags::Grid );
    }

    // Adjust Database range again
    if (xUndoData)
    {
        ScDBCollection* pColl = rDoc.GetDBCollection();
        if (pColl)
        {
            ScDBData* pDocData = pColl->getNamedDBs().findByUpperName(xUndoData->GetUpperName());
            if (pDocData)
                *pDocData = *xUndoData;
        }
    }

    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (pViewShell)
    {
        SCTAB nViewTab = pViewShell->GetViewData().GetTabNo();
        if ( nViewTab != nTab )
            pViewShell->SetTabNo( nTab );
    }

    EndUndo();
}

void ScUndoConsolidate::Redo()
{
    BeginRedo();

    rDocShell.DoConsolidate( aParam, false );

    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();
    if (pViewShell)
    {
        SCTAB nViewTab = pViewShell->GetViewData().GetTabNo();
        if ( nViewTab != aParam.nTab )
            pViewShell->SetTabNo( aParam.nTab );
    }

    EndRedo();
}

void ScUndoConsolidate::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoConsolidate::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;
}

// Change source data of Chart
void ScUndoChartData::Init()
{
    ScDocument& rDoc = rDocShell.GetDocument();
    aOldRangeListRef = new ScRangeList;
    rDoc.GetOldChartParameters( aChartName, *aOldRangeListRef, bOldColHeaders, bOldRowHeaders );
}

ScUndoChartData::ScUndoChartData( ScDocShell& rNewDocShell, OUString aName,
                                    const ScRange& rNew, bool bColHdr, bool bRowHdr,
                                    bool bAdd ) :
    ScSimpleUndo( rNewDocShell ),
    aChartName(std::move( aName )),
    bOldColHeaders(false),
    bOldRowHeaders(false),
    bNewColHeaders( bColHdr ),
    bNewRowHeaders( bRowHdr ),
    bAddRange( bAdd )
{
    aNewRangeListRef = new ScRangeList;
    aNewRangeListRef->push_back( rNew );

    Init();
}

ScUndoChartData::ScUndoChartData( ScDocShell& rNewDocShell, OUString aName,
                                    ScRangeListRef xNew, bool bColHdr, bool bRowHdr,
                                    bool bAdd ) :
    ScSimpleUndo( rNewDocShell ),
    aChartName(std::move( aName )),
    bOldColHeaders(false),
    bOldRowHeaders(false),
    aNewRangeListRef(std::move( xNew )),
    bNewColHeaders( bColHdr ),
    bNewRowHeaders( bRowHdr ),
    bAddRange( bAdd )
{
    Init();
}

ScUndoChartData::~ScUndoChartData()
{
}

OUString ScUndoChartData::GetComment() const
{
    return ScResId( STR_UNDO_CHARTDATA );
}

void ScUndoChartData::Undo()
{
    BeginUndo();

    rDocShell.GetDocument().UpdateChartArea( aChartName, aOldRangeListRef,
                                bOldColHeaders, bOldRowHeaders, false );

    EndUndo();
}

void ScUndoChartData::Redo()
{
    BeginRedo();

    rDocShell.GetDocument().UpdateChartArea( aChartName, aNewRangeListRef,
                                bNewColHeaders, bNewRowHeaders, bAddRange );

    EndRedo();
}

void ScUndoChartData::Repeat(SfxRepeatTarget& /* rTarget */)
{
}

bool ScUndoChartData::CanRepeat(SfxRepeatTarget& /* rTarget */) const
{
    return false;
}

ScUndoDataForm::ScUndoDataForm( ScDocShell& rNewDocShell,
                                SCCOL nStartX, SCROW nStartY, SCTAB nStartZ,
                                SCCOL nEndX, SCROW nEndY, SCTAB nEndZ,
                                const ScMarkData& rMark,
                                ScDocumentUniquePtr pNewUndoDoc, ScDocumentUniquePtr pNewRedoDoc,
                                std::unique_ptr<ScRefUndoData> pRefData )
    : ScBlockUndo(rNewDocShell, ScRange( nStartX, nStartY, nStartZ, nEndX, nEndY, nEndZ ), SC_UNDO_SIMPLE)
    , mxMarkData(new ScMarkData(rMark))
    , xUndoDoc(std::move(pNewUndoDoc))
    , xRedoDoc(std::move(pNewRedoDoc))
    , xRefUndoData(std::move(pRefData))
    , bRedoFilled(false)
{
    //      pFill1,pFill2,pFill3 are there so the ctor calls for simple paste (without cutting)
    //      don't have to be changed and branched for 641.
    //      They can be removed later.

    if (!mxMarkData->IsMarked())                            // no cell marked:
        mxMarkData->SetMarkArea(aBlockRange);   //  mark paste block

    if (xRefUndoData)
        xRefUndoData->DeleteUnchanged(rDocShell.GetDocument());
}

OUString ScUndoDataForm::GetComment() const
{
    return ScResId( STR_UNDO_PASTE );
}

void ScUndoDataForm::Undo()
{
    BeginUndo();
    DoChange( true );
    ShowTable( aBlockRange );
    EndUndo();
    SfxGetpApp()->Broadcast( SfxHint( SfxHintId::ScAreaLinksChanged ) );
}

void ScUndoDataForm::Redo()
{
    BeginRedo();
    ScDocument& rDoc = rDocShell.GetDocument();
    EnableDrawAdjust( &rDoc, false );                                //! include in ScBlockUndo?
    DoChange( false );
    EnableDrawAdjust( &rDoc, true );                                 //! include in ScBlockUndo?
    EndRedo();
    SfxGetpApp()->Broadcast( SfxHint( SfxHintId::ScAreaLinksChanged ) );
}

void ScUndoDataForm::Repeat(SfxRepeatTarget& /*rTarget*/)
{
}

bool ScUndoDataForm::CanRepeat(SfxRepeatTarget& rTarget) const
{
    return (dynamic_cast<const ScTabViewTarget*>( &rTarget) !=  nullptr);
}

void ScUndoDataForm::DoChange( const bool bUndo )
{
    ScDocument& rDoc = rDocShell.GetDocument();

    //      RefUndoData for redo is created before first undo
    //      (with DeleteUnchanged after the DoUndo call)
    bool bCreateRedoData = (bUndo && xRefUndoData && !xRefRedoData);
    if (bCreateRedoData)
        xRefRedoData.reset(new ScRefUndoData(rDoc));

    ScRefUndoData* pWorkRefData = bUndo ? xRefUndoData.get() : xRefRedoData.get();

    bool bPaintAll = false;

    ScTabViewShell* pViewShell = ScTabViewShell::GetActiveViewShell();

    SCTAB nTabCount = rDoc.GetTableCount();
    if ( bUndo && !bRedoFilled )
    {
        if (!xRedoDoc)
        {
            bool bColInfo = ( aBlockRange.aStart.Row()==0 && aBlockRange.aEnd.Row()==rDoc.MaxRow() );
            bool bRowInfo = ( aBlockRange.aStart.Col()==0 && aBlockRange.aEnd.Col()==rDoc.MaxCol() );

            xRedoDoc.reset(new ScDocument(SCDOCMODE_UNDO));
            xRedoDoc->InitUndoSelected(rDoc, *mxMarkData, bColInfo, bRowInfo);
        }
        //  read "redo" data from the document in the first undo
            //  all sheets - CopyToDocument skips those that don't exist in pRedoDoc
        ScRange aCopyRange = aBlockRange;
        aCopyRange.aStart.SetTab(0);
        aCopyRange.aEnd.SetTab(nTabCount-1);
        rDoc.CopyToDocument(aCopyRange, InsertDeleteFlags::VALUE, false, *xRedoDoc);
        bRedoFilled = true;
    }

    sal_uInt16 nExtFlags = 0;
    rDocShell.UpdatePaintExt( nExtFlags, aBlockRange );

    for ( sal_uInt16 i=0; i <= ( aBlockRange.aEnd.Col() - aBlockRange.aStart.Col() ); i++ )
    {
        OUString aOldString = xUndoDoc->GetString(
            aBlockRange.aStart.Col()+i, aBlockRange.aStart.Row(), aBlockRange.aStart.Tab());
        rDoc.SetString( aBlockRange.aStart.Col()+i , aBlockRange.aStart.Row() , aBlockRange.aStart.Tab() , aOldString );
    }

    if (pWorkRefData)
    {
        pWorkRefData->DoUndo( rDoc, true );             // TRUE = bSetChartRangeLists for SetChartListenerCollection
        if ( rDoc.RefreshAutoFilter( 0,0, rDoc.MaxCol(),rDoc.MaxRow(), aBlockRange.aStart.Tab() ) )
            bPaintAll = true;
    }

    if (bCreateRedoData && xRefRedoData)
        xRefRedoData->DeleteUnchanged(rDoc);

    if ( bUndo )
    {
        ScChangeTrack* pChangeTrack = rDoc.GetChangeTrack();
        if ( pChangeTrack )
            pChangeTrack->Undo( 0, 0 );
    }

    ScRange aDrawRange( aBlockRange );
    rDoc.ExtendMerge( aDrawRange, true );      // only needed for single sheet (text/rtf etc.)
    PaintPartFlags nPaint = PaintPartFlags::Grid;
    if (bPaintAll)
    {
        aDrawRange.aStart.SetCol(0);
        aDrawRange.aStart.SetRow(0);
        aDrawRange.aEnd.SetCol(rDoc.MaxCol());
        aDrawRange.aEnd.SetRow(rDoc.MaxRow());
        nPaint |= PaintPartFlags::Top | PaintPartFlags::Left;
        if (pViewShell)
            pViewShell->AdjustBlockHeight(false);
    }
    else
    {
        if ( aBlockRange.aStart.Row() == 0 && aBlockRange.aEnd.Row() == rDoc.MaxRow() )        // whole column
        {
            nPaint |= PaintPartFlags::Top;
            aDrawRange.aEnd.SetCol(rDoc.MaxCol());
        }
        if ( aBlockRange.aStart.Col() == 0 && aBlockRange.aEnd.Col() == rDoc.MaxCol() )        // whole row
        {
            nPaint |= PaintPartFlags::Left;
            aDrawRange.aEnd.SetRow(rDoc.MaxRow());
        }
        if (pViewShell && pViewShell->AdjustBlockHeight(false))
        {
            aDrawRange.aStart.SetCol(0);
            aDrawRange.aStart.SetRow(0);
            aDrawRange.aEnd.SetCol(rDoc.MaxCol());
            aDrawRange.aEnd.SetRow(rDoc.MaxRow());
            nPaint |= PaintPartFlags::Left;
        }
        rDocShell.UpdatePaintExt( nExtFlags, aDrawRange );
    }

    if ( !bUndo )                               //      draw redo after updating row heights
        RedoSdrUndoAction( pDrawUndo.get() );   //!     include in ScBlockUndo?

    rDocShell.PostPaint( aDrawRange, nPaint, nExtFlags );

    rDocShell.PostDataChanged();
    if (pViewShell)
        pViewShell->CellContentChanged();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
