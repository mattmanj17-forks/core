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

#include <formula/token.hxx>
#include <svx/svdocapt.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/lokhelper.hxx>
#include <svl/stritem.hxx>
#include <svl/numformat.hxx>
#include <svl/zforlist.hxx>
#include <svl/zformat.hxx>
#include <vcl/uitest/logger.hxx>
#include <vcl/uitest/eventdescription.hxx>
#include <vcl/keycod.hxx>
#include <vcl/keycodes.hxx>
#include <editeng/editview.hxx>
#include <rtl/math.hxx>
#include <sal/log.hxx>

#include <viewfunc.hxx>
#include <viewdata.hxx>
#include <drwlayer.hxx>
#include <docsh.hxx>
#include <futext.hxx>
#include <docfunc.hxx>
#include <sc.hrc>
#include <reftokenhelper.hxx>
#include <externalrefmgr.hxx>
#include <markdata.hxx>
#include <drawview.hxx>
#include <inputhdl.hxx>
#include <tabvwsh.hxx>
#include <scmod.hxx>
#include <postit.hxx>
#include <comphelper/scopeguard.hxx>

#include <vector>

namespace
{

void collectUIInformation( const OUString& aevent )
{
    EventDescription aDescription;
    aDescription.aID =  "grid_window";
    aDescription.aParameters = {{ aevent ,  ""}};
    aDescription.aAction = "COMMENT";
    aDescription.aParent = "MainWindow";
    aDescription.aKeyWord = "ScGridWinUIObject";
    UITestLogger::getInstance().logEvent(aDescription);
}

}

using ::std::vector;

void ScViewFunc::DetectiveAddPred()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveAddPred( GetViewData().GetCurPos() );
    RecalcPPT();    //! use broadcast in DocFunc instead?
}

void ScViewFunc::DetectiveDelPred()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveDelPred( GetViewData().GetCurPos() );
    RecalcPPT();
}

void ScViewFunc::DetectiveAddSucc()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveAddSucc( GetViewData().GetCurPos() );
    RecalcPPT();
}

void ScViewFunc::DetectiveDelSucc()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveDelSucc( GetViewData().GetCurPos() );
    RecalcPPT();
}

void ScViewFunc::DetectiveAddError()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveAddError( GetViewData().GetCurPos() );
    RecalcPPT();
}

void ScViewFunc::DetectiveDelAll()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveDelAll( GetViewData().GetTabNo() );
    RecalcPPT();
}

void ScViewFunc::DetectiveMarkInvalid()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveMarkInvalid( GetViewData().GetTabNo() );
    RecalcPPT();
}

void ScViewFunc::DetectiveRefresh()
{
    ScDocShell& rDocSh = GetViewData().GetDocShell();
    rDocSh.GetDocFunc().DetectiveRefresh();
    RecalcPPT();
}

static void lcl_jumpToRange(const ScRange& rRange, ScViewData* pView, const ScDocument& rDoc)
{
    OUString aAddrText(rRange.Format(rDoc, ScRefFlags::RANGE_ABS_3D));
    SfxStringItem aPosItem(SID_CURRENTCELL, aAddrText);
    SfxBoolItem aUnmarkItem(FN_PARAM_1, true);        // remove existing selection
    pView->GetDispatcher().ExecuteList(
        SID_CURRENTCELL, SfxCallMode::SYNCHRON | SfxCallMode::RECORD,
        { &aPosItem, &aUnmarkItem });
}

void ScViewFunc::MarkAndJumpToRanges(const ScRangeList& rRanges)
{
    ScViewData& rView = GetViewData();
    ScDocShell& rDocSh = rView.GetDocShell();

    ScRangeList aRangesToMark;
    ScAddress aCurPos = rView.GetCurPos();
    size_t ListSize = rRanges.size();
    for ( size_t i = 0; i < ListSize; ++i )
    {
        const ScRange & r = rRanges[i];
        // Collect only those ranges that are on the same sheet as the current
        // cursor.
        if (r.aStart.Tab() == aCurPos.Tab())
            aRangesToMark.push_back(r);
    }

    if (aRangesToMark.empty())
        return;

    // Jump to the first range of all precedent ranges.
    const ScRange & r = aRangesToMark.front();
    lcl_jumpToRange(r, &rView, rDocSh.GetDocument());

    ListSize = aRangesToMark.size();
    for ( size_t i = 0; i < ListSize; ++i )
    {
        MarkRange(aRangesToMark[i], false, true);
    }
}

void ScViewFunc::DetectiveMarkPred()
{
    ScViewData& rView = GetViewData();
    ScDocShell& rDocSh = rView.GetDocShell();
    ScDocument& rDoc = rDocSh.GetDocument();
    ScMarkData& rMarkData = rView.GetMarkData();
    ScAddress aCurPos = rView.GetCurPos();
    ScRangeList aRanges;
    if (rMarkData.IsMarked() || rMarkData.IsMultiMarked())
        rMarkData.FillRangeListWithMarks(&aRanges, false);
    else
        aRanges.push_back(ScRange(aCurPos));

    vector<ScTokenRef> aRefTokens;
    rDocSh.GetDocFunc().DetectiveCollectAllPreds(aRanges, aRefTokens);

    if (aRefTokens.empty())
        // No precedents found.  Nothing to do.
        return;

    ScTokenRef p = aRefTokens.front();
    if (ScRefTokenHelper::isExternalRef(p))
    {
        // This is external.  Open the external document if available, and
        // jump to the destination.

        sal_uInt16 nFileId = p->GetIndex();
        ScExternalRefManager* pRefMgr = rDoc.GetExternalRefManager();
        const OUString* pPath = pRefMgr->getExternalFileName(nFileId);

        ScRange aRange;
        if (pPath && ScRefTokenHelper::getRangeFromToken(&rDoc, aRange, p, aCurPos, true))
        {
            OUString aTabName = p->GetString().getString();
            OUString aRangeStr(aRange.Format(rDoc, ScRefFlags::VALID));
            OUString sUrl =
                *pPath +
                "#" +
                aTabName +
                "." +
                aRangeStr;

            ScGlobal::OpenURL(sUrl, OUString());
        }
        return;
    }
    else
    {
        ScRange aRange;
        ScRefTokenHelper::getRangeFromToken(&rDoc, aRange, p, aCurPos);
        if (aRange.aStart.Tab() != aCurPos.Tab())
        {
            // The first precedent range is on a different sheet.  Jump to it
            // immediately and forget the rest.
            lcl_jumpToRange(aRange, &rView, rDoc);
            return;
        }
    }

    ScRangeList aDestRanges;
    ScRefTokenHelper::getRangeListFromTokens(&rDoc, aDestRanges, aRefTokens, aCurPos);
    MarkAndJumpToRanges(aDestRanges);
}

void ScViewFunc::DetectiveMarkSucc()
{
    ScViewData& rView = GetViewData();
    ScDocShell& rDocSh = rView.GetDocShell();
    ScMarkData& rMarkData = rView.GetMarkData();
    ScAddress aCurPos = rView.GetCurPos();
    ScRangeList aRanges;
    if (rMarkData.IsMarked() || rMarkData.IsMultiMarked())
        rMarkData.FillRangeListWithMarks(&aRanges, false);
    else
        aRanges.push_back(ScRange(aCurPos));

    vector<ScTokenRef> aRefTokens;
    rDocSh.GetDocFunc().DetectiveCollectAllSuccs(aRanges, aRefTokens);

    if (aRefTokens.empty())
        // No dependents found.  Nothing to do.
        return;

    ScRangeList aDestRanges;
    ScRefTokenHelper::getRangeListFromTokens(&rView.GetDocument(), aDestRanges, aRefTokens, aCurPos);
    MarkAndJumpToRanges(aDestRanges);
}

/** Insert date or time into current cell.

    If cell is in input or edit mode, insert date/time at cursor position, else
    create a date or time or date+time cell as follows:

    - key date on time cell  =>  current date + time of cell  =>  date+time formatted cell
      - unless time cell was empty or 00:00 time  =>  current date  =>  date formatted cell
    - key date on date+time cell  =>  current date + 00:00 time  =>  date+time formatted cell
      - unless date was current date  =>  current date  =>  date formatted cell
    - key date on other cell  =>  current date  =>  date formatted cell
    - key time on date cell  =>  date of cell + current time  =>  date+time formatted cell
      - unless date cell was empty  =>  current time  =>  time formatted cell
    - key time on date+time cell  =>  current time  =>  time formatted cell
      - unless cell was empty  =>  current date+time  =>  date+time formatted cell
    - key time on other cell  =>  current time  =>  time formatted cell
 */
void ScViewFunc::InsertCurrentTime(SvNumFormatType nReqFmt, const OUString& rUndoStr)
{
    ScViewData& rViewData = GetViewData();

    ScInputHandler* pInputHdl = ScModule::get()->GetInputHdl(rViewData.GetViewShell());
    bool bInputMode = (pInputHdl && pInputHdl->IsInputMode());

    ScDocShell& rDocSh = rViewData.GetDocShell();
    ScDocument& rDoc = rDocSh.GetDocument();
    ScAddress aCurPos = rViewData.GetCurPos();
    const sal_uInt32 nCurNumFormat = rDoc.GetNumberFormat(ScRange(aCurPos));
    SvNumberFormatter* pFormatter = rDoc.GetFormatTable();
    const SvNumberformat* pCurNumFormatEntry = pFormatter->GetEntry(nCurNumFormat);
    const SvNumFormatType nCurNumFormatType = (pCurNumFormatEntry ?
            pCurNumFormatEntry->GetMaskedType() : SvNumFormatType::UNDEFINED);

    const int nView(comphelper::LibreOfficeKit::isActive() ? SfxLokHelper::getCurrentView() : -1);
    if (nView >= 0)
    {
        const auto [isTimezoneSet, aTimezone] = SfxLokHelper::getViewTimezone(nView);
        comphelper::LibreOfficeKit::setTimezone(isTimezoneSet, aTimezone);
    }

    comphelper::ScopeGuard aAutoUserTimezone(
        [nView]()
        {
            if (nView >= 0)
            {
                const auto [isTimezoneSet, aTimezone] = SfxLokHelper::getDefaultTimezone();
                comphelper::LibreOfficeKit::setTimezone(isTimezoneSet, aTimezone);
            }
        });

    if (bInputMode)
    {
        double fVal = 0.0;
        sal_uInt32 nFormat = 0;
        switch (nReqFmt)
        {
            case SvNumFormatType::DATE:
                {
                    Date aActDate( Date::SYSTEM );
                    fVal = aActDate - pFormatter->GetNullDate();
                    if (nCurNumFormatType == SvNumFormatType::DATE)
                        nFormat = nCurNumFormat;
                }
                break;
            case SvNumFormatType::TIME:
                {
                    tools::Time aActTime( tools::Time::SYSTEM );
                    fVal = aActTime.GetTimeInDays();
                    if (nCurNumFormatType == SvNumFormatType::TIME)
                        nFormat = nCurNumFormat;
                }
                break;
            default:
                SAL_WARN("sc.ui","unhandled current date/time request");
                nReqFmt = SvNumFormatType::DATETIME;
                [[fallthrough]];
            case SvNumFormatType::DATETIME:
                {
                    DateTime aActDateTime( DateTime::SYSTEM );
                    fVal = DateTime::Sub( aActDateTime, DateTime( pFormatter->GetNullDate()));
                    if (nCurNumFormatType == SvNumFormatType::DATETIME)
                        nFormat = nCurNumFormat;
                }
                break;
        }

        if (!nFormat)
        {
            const LanguageType nLang = (pCurNumFormatEntry ? pCurNumFormatEntry->GetLanguage() : ScGlobal::eLnge);
            nFormat = pFormatter->GetStandardFormat( nReqFmt, nLang);
            // This would return a more precise format with seconds and 100th
            // seconds for a time request.
            //nFormat = pFormatter->GetStandardFormat( fVal, nFormat, nReqFmt, nLang);
        }
        OUString aString;
        const Color* pColor;
        pFormatter->GetOutputString( fVal, nFormat, aString, &pColor);

        pInputHdl->DataChanging();
        EditView* pTopView = pInputHdl->GetTopView();
        if (pTopView)
            pTopView->InsertText( aString);
        EditView* pTableView = pInputHdl->GetTableView();
        if (pTableView)
            pTableView->InsertText( aString);
        pInputHdl->DataChanged();
    }
    else
    {
        // Clear "Enter pastes" mode.
        rViewData.SetPasteMode( ScPasteFlags::NONE );
        // Clear CopySourceOverlay in each window of a split/frozen tabview.
        rViewData.GetViewShell()->UpdateCopySourceOverlay();

        bool bForceReqFmt = false;
        const double fCell = rDoc.GetValue( aCurPos);
        // Combine requested date/time stamp with existing cell time/date, if any.
        switch (nReqFmt)
        {
            case SvNumFormatType::DATE:
                switch (nCurNumFormatType)
                {
                    case SvNumFormatType::TIME:
                        // An empty cell formatted as time (or 00:00 time) shall
                        // not result in the current date with 00:00 time, but only
                        // in current date.
                        if (fCell != 0.0)
                            nReqFmt = SvNumFormatType::DATETIME;
                        break;
                    case SvNumFormatType::DATETIME:
                        {
                            // Force to only date if the existing date+time is the
                            // current date. This way inserting current date twice
                            // on an existing date+time cell can be used to force
                            // date, which otherwise would only be possible by
                            // applying a date format.
                            double fDate = rtl::math::approxFloor( fCell);
                            if (fDate == (Date( Date::SYSTEM) - pFormatter->GetNullDate()))
                                bForceReqFmt = true;
                        }
                        break;
                    default: break;
                }
                break;
            case SvNumFormatType::TIME:
                switch (nCurNumFormatType)
                {
                    case SvNumFormatType::DATE:
                        // An empty cell formatted as date shall not result in the
                        // null date and current time, but only in current time.
                        if (fCell != 0.0)
                            nReqFmt = SvNumFormatType::DATETIME;
                        break;
                    case SvNumFormatType::DATETIME:
                        // Requesting current time on an empty date+time cell
                        // inserts both current date+time.
                        if (fCell == 0.0)
                            nReqFmt = SvNumFormatType::DATETIME;
                        else
                        {
                            // Add current time to an existing date+time where time is
                            // zero and date is current date, else force time only.
                            double fDate = rtl::math::approxFloor( fCell);
                            double fTime = fCell - fDate;
                            if (fTime == 0.0 && fDate == (Date( Date::SYSTEM) - pFormatter->GetNullDate()))
                                nReqFmt = SvNumFormatType::DATETIME;
                            else
                                bForceReqFmt = true;
                        }
                        break;
                    default: break;
                }
                break;
            default:
                SAL_WARN("sc.ui","unhandled current date/time request");
                nReqFmt = SvNumFormatType::DATETIME;
                [[fallthrough]];
            case SvNumFormatType::DATETIME:
                break;
        }
        double fVal = 0.0;
        switch (nReqFmt)
        {
            case SvNumFormatType::DATE:
                {
                    Date aActDate( Date::SYSTEM );
                    fVal = aActDate - pFormatter->GetNullDate();
                }
                break;
            case SvNumFormatType::TIME:
                {
                    tools::Time aActTime( tools::Time::SYSTEM );
                    fVal = aActTime.GetTimeInDays();
                }
                break;
            case SvNumFormatType::DATETIME:
                switch (nCurNumFormatType)
                {
                    case SvNumFormatType::DATE:
                        {
                            double fDate = rtl::math::approxFloor( fCell);
                            tools::Time aActTime( tools::Time::SYSTEM );
                            fVal = fDate + aActTime.GetTimeInDays();
                        }
                        break;
                    case SvNumFormatType::TIME:
                        {
                            double fTime = fCell - rtl::math::approxFloor( fCell);
                            Date aActDate( Date::SYSTEM );
                            fVal = (aActDate - pFormatter->GetNullDate()) + fTime;
                        }
                        break;
                    default:
                        {
                            DateTime aActDateTime( DateTime::SYSTEM );
                            fVal = DateTime::Sub( aActDateTime, DateTime( pFormatter->GetNullDate()));
                        }
                }
                break;
            default: break;

        }

        SfxUndoManager* pUndoMgr = rDocSh.GetUndoManager();
        pUndoMgr->EnterListAction(rUndoStr, rUndoStr, 0, rViewData.GetViewShell()->GetViewShellId());

        rDocSh.GetDocFunc().SetValueCell(aCurPos, fVal, true);

        // Set the new cell format only when it differs from the current cell
        // format type. Preserve a date+time format unless we force a format
        // through.
        if (bForceReqFmt || (nReqFmt != nCurNumFormatType && nCurNumFormatType != SvNumFormatType::DATETIME))
            SetNumberFormat(nReqFmt);
        else
            rViewData.UpdateInputHandler();     // update input bar with new value

        pUndoMgr->LeaveListAction();
    }
}

void ScViewFunc::ShowNote( bool bShow )
{
    if( bShow )
        HideNoteOverlay();
    const ScViewData& rViewData = GetViewData();
    ScAddress aPos( rViewData.GetCurX(), rViewData.GetCurY(), rViewData.GetTabNo() );
    // show note moved to ScDocFunc, to be able to use it in notesuno.cxx
    rViewData.GetDocShell().GetDocFunc().ShowNote( aPos, bShow );
}

void ScViewFunc::EditNote()
{
    // HACK: If another text object is selected, make sure it gets unselected
    if (FuText* pOldFuText = dynamic_cast<FuText*>(GetDrawFuncPtr()))
        pOldFuText->KeyInput(KeyEvent(0, vcl::KeyCode(KEY_ESCAPE)));

    // for editing display and activate

    ScDocShell& rDocSh = GetViewData().GetDocShell();
    ScDocument& rDoc = rDocSh.GetDocument();
    SCCOL nCol = GetViewData().GetCurX();
    SCROW nRow = GetViewData().GetCurY();
    SCTAB nTab = GetViewData().GetTabNo();
    ScAddress aPos( nCol, nRow, nTab );

    // start drawing undo to catch undo action for insertion of the caption object
    rDocSh.MakeDrawLayer();
    ScDrawLayer* pDrawLayer = rDoc.GetDrawLayer();
    pDrawLayer->BeginCalcUndo(true);
    // generated undo action is processed in FuText::StopEditMode

    // get existing note or create a new note (including caption drawing object)
    ScPostIt* pNote = rDoc.GetOrCreateNote( aPos );
    if(!pNote)
        return;

    // hide temporary note caption
    HideNoteOverlay();
    // show caption object without changing internal visibility state
    pNote->ShowCaptionTemp( aPos );

    /*  Drawing object has been created in ScDocument::GetOrCreateNote() or
        in ScPostIt::ShowCaptionTemp(), so ScPostIt::GetCaption() should
        return a caption object. */
    SdrCaptionObj* pCaption = pNote->GetCaption();
    if( !pCaption )
        return;

    if ( ScDrawView* pScDrawView = GetScDrawView() )
       pScDrawView->SyncForGrid( pCaption );

    // activate object (as in FuSelection::TestComment)
    GetViewData().GetDispatcher().Execute( SID_DRAW_NOTEEDIT, SfxCallMode::SYNCHRON | SfxCallMode::RECORD );
    // now get the created FuText and set into EditMode
    FuText* pFuText = dynamic_cast<FuText*>(GetDrawFuncPtr());
    if (pFuText)
    {
        ScrollToObject( pCaption );         // make object fully visible
        pFuText->SetInEditMode( pCaption );

        ScTabView::OnLOKNoteStateChanged( pNote );
    }
    collectUIInformation(u"OPEN"_ustr);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
