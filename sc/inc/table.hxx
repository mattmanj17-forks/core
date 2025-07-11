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

#pragma once

#include <tools/gen.hxx>
#include <tools/color.hxx>
#include "attarray.hxx"
#include "column.hxx"
#include "colcontainer.hxx"
#include <sal/types.h>
#include "sortparam.hxx"
#include "types.hxx"
#include <formula/types.hxx>
#include "calcmacros.hxx"
#include <formula/errorcodes.hxx>
#include "document.hxx"
#include "drwlayer.hxx"
#include "SparklineList.hxx"
#include "SolverSettings.hxx"
#include "markdata.hxx"
#include "FilterData.hxx"
#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <set>
#include <vector>

template <typename A, typename D> class ScBitMaskCompressedArray;
template <typename A, typename D> class ScCompressedArray;

namespace utl {
    class TextSearch;
}

namespace com::sun::star {
    namespace sheet {
        struct TablePageBreakData;
    }
}

namespace formula { struct VectorRefArray; }
namespace sc {

struct BroadcasterState;
class StartListeningContext;
class EndListeningContext;
class CopyFromClipContext;
class CopyToClipContext;
class CopyToDocContext;
class MixDocContext;
class ColumnSpanSet;
class RangeColumnSpanSet;
class ColumnSet;
struct ColumnBlockPosition;
struct RefUpdateContext;
struct RefUpdateInsertTabContext;
struct RefUpdateDeleteTabContext;
struct RefUpdateMoveTabContext;
struct NoteEntry;
class DocumentStreamAccess;
class CellValues;
class TableValues;
class RowHeightContext;
class CompileFormulaContext;
struct SetFormulaDirtyContext;
class ColumnIterator;
}

class SfxItemSet;
class SfxStyleSheetBase;
class SvxBoxInfoItem;
class SvxBoxItem;
class SvxSearchItem;

class ScAutoFormatData;
class ScEditDataArray;
class ScFormulaCell;
class ScOutlineTable;
class ScPrintSaverTab;
class ScProgress;
class ScRangeList;
class ScSheetEvents;
class ScConditionalFormat;
class ScConditionalFormatList;
class ScStyleSheet;
class ScTableProtection;
class ScUserListData;
struct RowInfo;
class ScFunctionData;
class CollatorWrapper;
class ScFlatUInt16RowSegments;
class ScFlatBoolRowSegments;
class ScFlatBoolColSegments;
struct ScSetStringParam;
struct ScColWidthParam;
class ScRangeName;
class ScDBData;
class ScPostIt;
struct ScInterpreterContext;


class ScColumnsRange final
{
 public:
    class Iterator final
    {
        SCCOL mCol;
    public:
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef SCCOL value_type;
        typedef SCCOL difference_type;
        typedef const SCCOL* pointer;
        typedef SCCOL reference;

        explicit Iterator(SCCOL nCol) : mCol(nCol) {}

        Iterator& operator++() { ++mCol; return *this;}
        Iterator& operator--() { --mCol; return *this;}

        // Comparing iterators from different containers is undefined, so comparing mCol is enough.
        bool operator==(const Iterator & rOther) const {return mCol == rOther.mCol;}
        bool operator!=(const Iterator & rOther) const {return !(*this == rOther);}
        SCCOL operator*() const {return mCol;}
    };

    ScColumnsRange(SCCOL nBegin, SCCOL nEnd) : maBegin(nBegin), maEnd(nEnd) {}
    const Iterator & begin() { return maBegin; }
    const Iterator & end() { return maEnd; }
    std::reverse_iterator<Iterator> rbegin() { return std::reverse_iterator<Iterator>(maEnd); }
    std::reverse_iterator<Iterator> rend() { return std::reverse_iterator<Iterator>(maBegin); }
private:
    const Iterator maBegin;
    const Iterator maEnd;
};

template <typename T>
concept ColumnDataApply = std::is_invocable_v<T, ScColumnData&, SCROW, SCROW>;

class ScTable
{
private:
    typedef ::std::vector< ScRange > ScRangeVec;

    SCTAB           nTab;
    ScDocument&     rDocument;

    ScColContainer aCol;

    OUString aName;
    OUString aCodeName;
    OUString aComment;

    OUString        aLinkDoc;
    OUString        aLinkFlt;
    OUString        aLinkOpt;
    OUString        aLinkTab;
    sal_Int32       nLinkRefreshDelay;
    ScLinkMode      nLinkMode;

    // page style template
    OUString        aPageStyle;
    Size            aPageSizeTwips;                 // size of the print-page
    SCCOL           nRepeatStartX;                  // repeating rows/columns
    SCCOL           nRepeatEndX;                    // REPEAT_NONE, if not used
    SCROW           nRepeatStartY;
    SCROW           nRepeatEndY;

    // Standard row height for this sheet - benefits XLSX because default height defined per sheet
    sal_uInt16 mnOptimalMinRowHeight; // in Twips

    std::unique_ptr<ScTableProtection> pTabProtection;

    std::unique_ptr<ScCompressedArray<SCCOL, sal_uInt16>> mpColWidth;
    std::unique_ptr<ScFlatUInt16RowSegments> mpRowHeights;

    std::unique_ptr<ScBitMaskCompressedArray<SCCOL, CRFlags>> mpColFlags;
    std::unique_ptr<ScBitMaskCompressedArray< SCROW, CRFlags>> pRowFlags;

    FilterData maFilterData;

    ::std::set<SCROW>                      maRowPageBreaks;
    ::std::set<SCROW>                      maRowManualBreaks;
    ::std::set<SCCOL>                      maColPageBreaks;
    ::std::set<SCCOL>                      maColManualBreaks;

    std::unique_ptr<ScOutlineTable> pOutlineTable;

    std::unique_ptr<ScSheetEvents>  pSheetEvents;

    mutable SCCOL nTableAreaX;
    mutable SCROW nTableAreaY;
    mutable SCCOL nTableAreaVisibleX;
    mutable SCROW nTableAreaVisibleY;

    std::unique_ptr<utl::TextSearch> pSearchText;

    mutable OUString aUpperName;             // #i62977# filled only on demand, reset in SetName

    // sort parameter to minimize stack size of quicksort
    ScSortParam     aSortParam;
    CollatorWrapper*    pSortCollator;

    ScRangeVec      aPrintRanges;

    std::optional<ScRange> moRepeatColRange;
    std::optional<ScRange> moRepeatRowRange;

    sal_uInt16          nLockCount;

    std::unique_ptr<ScRangeList> pScenarioRanges;
    Color           aScenarioColor;
    Color           aTabBgColor;
    ScScenarioFlags nScenarioFlags;
    std::unique_ptr<ScDBData> pDBDataNoName;
    mutable std::unique_ptr<ScRangeName> mpRangeName;

    std::unique_ptr<ScConditionalFormatList> mpCondFormatList;
    sc::SparklineList maSparklineList;

    ScAddress       maLOKFreezeCell;

    bool            bScenario:1;
    bool            bLayoutRTL:1;
    bool            bLoadingRTL:1;
    bool            bPageSizeValid:1;
    mutable bool    bTableAreaValid:1;
    mutable bool    bTableAreaVisibleValid:1;
    bool            bVisible:1;
    bool            bPendingRowHeights:1;
    bool            bCalcNotification:1;
    bool            bGlobalKeepQuery:1;
    bool            bPrintEntireSheet:1;
    bool            bActiveScenario:1;
    bool            mbPageBreaksValid:1;
    bool            mbForceBreaks:1;
    bool            mbTotalsRowBelow:1;
    /** this is touched from formula group threading context */
    std::atomic<bool> bStreamValid;

    // Solver settings in current tab
    std::shared_ptr<sc::SolverSettings> m_pSolverSettings;

    // Default attributes for the unallocated columns.
    ScColumnData    aDefaultColData;

friend class ScDocument;                    // for FillInfo
friend class ScColumn;
friend class ScValueIterator;
friend class ScHorizontalValueIterator;
friend class ScDBQueryDataIterator;
friend class ScFormulaGroupIterator;
friend class ScCellIterator;
template< ScQueryCellIteratorAccess accessType, ScQueryCellIteratorType queryType >
friend class ScQueryCellIteratorBase;
template< ScQueryCellIteratorAccess accessType >
friend class ScQueryCellIteratorAccessSpecific;
friend class ScHorizontalCellIterator;
friend class ScColumnTextWidthIterator;
friend class ScDocumentImport;
friend class sc::DocumentStreamAccess;
friend class sc::ColumnSpanSet;
friend class sc::RangeColumnSpanSet;
friend class sc::EditTextIterator;
friend class sc::FormulaGroupAreaListener;

public:
                ScTable( ScDocument& rDoc, SCTAB nNewTab, const OUString& rNewName,
                         bool bColInfo = true, bool bRowInfo = true );
                ~ScTable() COVERITY_NOEXCEPT_FALSE;
                ScTable(const ScTable&) = delete;
    ScTable&    operator=(const ScTable&) = delete;

    ScDocument& GetDoc() { return rDocument;}
    const ScDocument& GetDoc() const { return rDocument;}
    SCTAB GetTab() const { return nTab; }

    ScOutlineTable* GetOutlineTable()               { return pOutlineTable.get(); }

    ScColumn& CreateColumnIfNotExists( const SCCOL nScCol )
    {
        if ( nScCol >= aCol.size() )
            CreateColumnIfNotExistsImpl(nScCol);
        return aCol[nScCol];
    }
    // out-of-line the cold part of the function
    void CreateColumnIfNotExistsImpl( const SCCOL nScCol );

    const ScColumnData& GetColumnData( SCCOL nCol ) const { return nCol < aCol.size() ? aCol[ nCol ] : aDefaultColData; }
    ScColumnData& GetColumnData( SCCOL nCol ) { return nCol < aCol.size() ? aCol[ nCol ] : aDefaultColData; }

    sal_uInt64      GetCellCount() const;
    sal_uInt64      GetWeightedCount() const;
    sal_uInt64      GetWeightedCount(SCROW nStartRow, SCROW nEndRow) const;
    sal_uInt64      GetCodeCount() const;       // RPN code in formula

    sal_uInt16 GetTextWidth(SCCOL nCol, SCROW nRow) const;

    bool        SetOutlineTable( const ScOutlineTable* pNewOutline );
    void        StartOutlineTable();

    void        DoAutoOutline( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow );

    bool        TestRemoveSubTotals( const ScSubTotalParam& rParam );
    void        RemoveSubTotals( ScSubTotalParam& rParam );
    bool        DoSubTotals( ScSubTotalParam& rParam );

    const ScSheetEvents* GetSheetEvents() const              { return pSheetEvents.get(); }
    void        SetSheetEvents( std::unique_ptr<ScSheetEvents> pNew );

    bool        IsVisible() const                            { return bVisible; }
    void        SetVisible( bool bVis );

    bool        IsStreamValid() const                        { return bStreamValid; }
    void        SetStreamValid( bool bSet, bool bIgnoreLock = false );

    [[nodiscard]] bool IsColValid( const SCCOL nScCol ) const
    {
        return nScCol >= static_cast< SCCOL >( 0 ) && nScCol < aCol.size();
    }
    [[nodiscard]] bool IsColRowValid( const SCCOL nScCol, const SCROW nScRow ) const
    {
        return IsColValid( nScCol ) && GetDoc().ValidRow( nScRow );
    }
    [[nodiscard]] bool IsColRowTabValid( const SCCOL nScCol, const SCROW nScRow, const SCTAB nScTab ) const
    {
        return IsColValid( nScCol ) && GetDoc().ValidRow( nScRow ) && ValidTab( nScTab );
    }
    [[nodiscard]] bool ValidCol(SCCOL nCol) const { return GetDoc().ValidCol(nCol); }
    [[nodiscard]] bool ValidRow(SCROW nRow) const { return GetDoc().ValidRow(nRow); }
    [[nodiscard]] bool ValidColRow(SCCOL nCol, SCROW nRow) const { return GetDoc().ValidColRow(nCol, nRow); }

    bool        IsPendingRowHeights() const                  { return bPendingRowHeights; }
    void        SetPendingRowHeights( bool bSet );

    bool        GetCalcNotification() const                  { return bCalcNotification; }
    void        SetCalcNotification( bool bSet );

    bool        IsLayoutRTL() const                          { return bLayoutRTL; }
    bool        IsLoadingRTL() const                         { return bLoadingRTL; }
    void        SetLayoutRTL( bool bSet );
    void        SetLoadingRTL( bool bSet );

    bool        IsScenario() const                           { return bScenario; }
    void        SetScenario( bool bFlag );
    void        GetScenarioComment( OUString& rComment) const  { rComment = aComment; }
    void        SetScenarioComment( const OUString& rComment ) { aComment = rComment; }
    const Color& GetScenarioColor() const                    { return aScenarioColor; }
    void         SetScenarioColor(const Color& rNew)         { aScenarioColor = rNew; }
    const Color& GetTabBgColor() const                       { return aTabBgColor; }
    void         SetTabBgColor(const Color& rColor);
    ScScenarioFlags GetScenarioFlags() const                 { return nScenarioFlags; }
    void        SetScenarioFlags(ScScenarioFlags nNew)       { nScenarioFlags = nNew; }
    void        SetActiveScenario(bool bSet)                 { bActiveScenario = bSet; }
    bool        IsActiveScenario() const                     { return bActiveScenario; }

    ScLinkMode  GetLinkMode() const                          { return nLinkMode; }
    bool        IsLinked() const                             { return nLinkMode != ScLinkMode::NONE; }
    const OUString& GetLinkDoc() const                       { return aLinkDoc; }
    const OUString& GetLinkFlt() const                       { return aLinkFlt; }
    const OUString& GetLinkOpt() const                       { return aLinkOpt; }
    const OUString& GetLinkTab() const                       { return aLinkTab; }
    sal_Int32  GetLinkRefreshDelay() const                  { return nLinkRefreshDelay; }

    void        SetLink( ScLinkMode nMode, const OUString& rDoc, const OUString& rFlt,
                        const OUString& rOpt, const OUString& rTab, sal_Int32 nRefreshDelay );

    sal_Int64   GetHashCode () const;

    const OUString& GetName() const { return aName; }
    void        SetName( const OUString& rNewName );

    void        SetAnonymousDBData(std::unique_ptr<ScDBData> pDBData);
    ScDBData*   GetAnonymousDBData() { return pDBDataNoName.get();}

    const OUString& GetCodeName() const { return aCodeName; }
    void        SetCodeName( const OUString& rNewName ) { aCodeName = rNewName; }

    bool        GetTotalsRowBelow() const { return mbTotalsRowBelow; }
    void        SetTotalsRowBelow( bool bNewVal ) { mbTotalsRowBelow = bNewVal; }

    const OUString& GetUpperName() const;

    const OUString&   GetPageStyle() const                    { return aPageStyle; }
    void            SetPageStyle( const OUString& rName );
    void            PageStyleModified( const OUString& rNewName );

    bool            IsProtected() const;
    void            SetProtection(const ScTableProtection* pProtect);
    const ScTableProtection* GetProtection() const;
    void            GetUnprotectedCells( ScRangeList& rRangeList ) const;

    bool            IsEditActionAllowed( sc::EditAction eAction, SCCOL nStartCol, SCROW nStartRow,
                                         SCCOL nEndCol, SCROW nEndRow ) const;

    Size            GetPageSize() const;
    void            SetPageSize( const Size& rSize );
    void            SetRepeatArea( SCCOL nStartCol, SCCOL nEndCol, SCROW nStartRow, SCROW nEndRow );

    void        LockTable();
    void        UnlockTable();

    bool        IsBlockEditable( SCCOL nCol1, SCROW nRow1, SCCOL nCol2,
                        SCROW nRow2, bool* pOnlyNotBecauseOfMatrix = nullptr,
                        bool bNoMatrixAtAll = false ) const;
    bool        IsSelectionEditable( const ScMarkData& rMark,
                        bool* pOnlyNotBecauseOfMatrix = nullptr ) const;

    bool        HasBlockMatrixFragment( const SCCOL nCol1, SCROW nRow1, const SCCOL nCol2, SCROW nRow2,
                                        bool bNoMatrixAtAll = false ) const;
    bool        HasSelectionMatrixFragment( const ScMarkData& rMark ) const;

    // This also includes e.g. notes. Use IsEmptyData() for cell data only.
    bool        IsBlockEmpty( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 ) const;
    bool        IsNotesBlockEmpty( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 ) const;

    bool        SetString( SCCOL nCol, SCROW nRow, SCTAB nTab, const OUString& rString,
                           const ScSetStringParam * pParam = nullptr );

    bool SetEditText( SCCOL nCol, SCROW nRow, std::unique_ptr<EditTextObject> pEditText );
    void SetEditText( SCCOL nCol, SCROW nRow, const EditTextObject& rEditText, const SfxItemPool* pEditPool );
    SCROW GetFirstEditTextRow( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 ) const;

    void SetEmptyCell( SCCOL nCol, SCROW nRow );
    void SetFormula(
        SCCOL nCol, SCROW nRow, const ScTokenArray& rArray, formula::FormulaGrammar::Grammar eGram );
    void SetFormula(
        SCCOL nCol, SCROW nRow, const OUString& rFormula, formula::FormulaGrammar::Grammar eGram );

    SC_DLLPUBLIC const std::shared_ptr<sc::SolverSettings> & GetSolverSettings();

    // tdf#156815 Sets the solver settings object to nullptr to force reloading Solver settings the
    // next time the dialog is opened. This is required when sheets are renamed
    void ResetSolverSettings() { m_pSolverSettings.reset(); }

    /**
     * Takes ownership of pCell
     *
     * @return pCell if it was successfully inserted, NULL otherwise. pCell
     *         is deleted automatically on failure to insert.
     */
    ScFormulaCell* SetFormulaCell( SCCOL nCol, SCROW nRow, ScFormulaCell* pCell );

    bool SetFormulaCells( SCCOL nCol, SCROW nRow, std::vector<ScFormulaCell*>& rCells );

    bool HasFormulaCell( const SCCOL nCol1, SCROW nRow1, const SCCOL nCol2, SCROW nRow2 ) const;

    svl::SharedString GetSharedString( SCCOL nCol, SCROW nRow ) const;

    void        SetValue( SCCOL nCol, SCROW nRow, const double& rVal );
    void        SetValues( const SCCOL nCol, const SCROW nRow, const std::vector<double>& rVals );
    void        SetError( SCCOL nCol, SCROW nRow, FormulaError nError);
    SCSIZE      GetPatternCount( SCCOL nCol ) const;
    SCSIZE      GetPatternCount( SCCOL nCol, SCROW nRow1, SCROW nRow2 ) const;
    bool        ReservePatternCount( SCCOL nCol, SCSIZE nReserve );

    void        SetRawString( SCCOL nCol, SCROW nRow, const svl::SharedString& rStr );
    OUString    GetString( SCCOL nCol, SCROW nRow, ScInterpreterContext* pContext = nullptr ) const;
    double*     GetValueCell( SCCOL nCol, SCROW nRow );
    // Note that if pShared is set and a value is returned that way, the returned OUString is empty.
    OUString    GetInputString( SCCOL nCol, SCROW nRow, bool bForceSystemLocale = false ) const;
    double      GetValue( SCCOL nCol, SCROW nRow ) const;
    const EditTextObject* GetEditText( SCCOL nCol, SCROW nRow ) const;
    void RemoveEditTextCharAttribs( SCCOL nCol, SCROW nRow, const ScPatternAttr& rAttr );
    OUString GetFormula( SCCOL nCol, SCROW nRow ) const;
    const ScFormulaCell* GetFormulaCell( SCCOL nCol, SCROW nRow ) const;
    ScFormulaCell* GetFormulaCell( SCCOL nCol, SCROW nRow );

    CellType    GetCellType( SCCOL nCol, SCROW nRow ) const;
    ScRefCellValue GetCellValue( SCCOL nCol, sc::ColumnBlockPosition& rBlockPos, SCROW nRow );
    ScRefCellValue GetCellValue( SCCOL nCol, SCROW nRow ) const;

    void        GetFirstDataPos(SCCOL& rCol, SCROW& rRow) const;
    void        GetLastDataPos(SCCOL& rCol, SCROW& rRow) const;

    // Sparklines

    std::shared_ptr<sc::Sparkline> GetSparkline(SCCOL nCol, SCROW nRow);
    sc::Sparkline* CreateSparkline(SCCOL nCol, SCROW nRow, std::shared_ptr<sc::SparklineGroup> const& pSparklineGroup);
    bool DeleteSparkline(SCCOL nCol, SCROW nRow);

    sc::SparklineList& GetSparklineList();
    void CopySparklinesToTable(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, ScTable* pDestTab);
    void FillSparkline(bool bVertical, SCCOLROW nFixed, SCCOLROW nIteratingStart, SCCOLROW nIteratingEnd, SCCOLROW nFillStart, SCCOLROW nFillEnd);

    // Notes / Comments
    std::unique_ptr<ScPostIt> ReleaseNote( SCCOL nCol, SCROW nRow );
    ScPostIt*                 GetNote( SCCOL nCol, SCROW nRow );
    void                      SetNote( SCCOL nCol, SCROW nRow, std::unique_ptr<ScPostIt> pNote );

    size_t GetNoteCount( SCCOL nCol ) const;
    SCROW GetNotePosition( SCCOL nCol, size_t nIndex ) const;
    void CreateAllNoteCaptions();
    void ForgetNoteCaptions( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, bool bPreserveData );
    void CommentNotifyAddressChange(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2);

    void GetAllNoteEntries( std::vector<sc::NoteEntry>& rNotes ) const;
    void GetNotesInRange( const ScRange& rRange, std::vector<sc::NoteEntry>& rNotes ) const;
    CommentCaptionState GetAllNoteCaptionsState( const ScRange& rRange, std::vector<sc::NoteEntry>& rNotes );
    bool ContainsNotesInRange( const ScRange& rRange ) const;

    bool TestInsertRow( SCCOL nStartCol, SCCOL nEndCol, SCROW nStartRow, SCSIZE nSize ) const;
    void        InsertRow( SCCOL nStartCol, SCCOL nEndCol, SCROW nStartRow, SCSIZE nSize );
    void DeleteRow(
        const sc::ColumnSet& rRegroupCols, SCCOL nStartCol, SCCOL nEndCol, SCROW nStartRow, SCSIZE nSize,
        bool* pUndoOutline, std::vector<ScAddress>* pGroupPos );

    bool        TestInsertCol( SCROW nStartRow, SCROW nEndRow, SCSIZE nSize ) const;
    void InsertCol(
        const sc::ColumnSet& rRegroupCols, SCCOL nStartCol, SCROW nStartRow, SCROW nEndRow, SCSIZE nSize );
    void DeleteCol(
        const sc::ColumnSet& rRegroupCols, SCCOL nStartCol, SCROW nStartRow, SCROW nEndRow, SCSIZE nSize, bool* pUndoOutline );

    void DeleteArea(
        SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, InsertDeleteFlags nDelFlag,
        bool bBroadcast = true, sc::ColumnSpanSet* pBroadcastSpans = nullptr );

    void CopyToClip( sc::CopyToClipContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, ScTable* pTable );
    void CopyToClip( sc::CopyToClipContext& rCxt, const ScRangeList& rRanges, ScTable* pTable );

    void CopyStaticToDocument(
        SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, const SvNumberFormatterMergeMap& rMap,
        ScTable* pDestTab );

    void CopyCellToDocument( SCCOL nSrcCol, SCROW nSrcRow, SCCOL nDestCol, SCROW nDestRow, ScTable& rDestTab );

    bool InitColumnBlockPosition( sc::ColumnBlockPosition& rBlockPos, SCCOL nCol );

    void DeleteBeforeCopyFromClip(
        sc::CopyFromClipContext& rCxt, const ScTable& rClipTab, sc::ColumnSpanSet& rBroadcastSpans );

    void CopyOneCellFromClip(
        sc::CopyFromClipContext& rCxt, const SCCOL nCol1, const SCROW nRow1,
        const SCCOL nCol2, const SCROW nRow2,
        const SCROW nSrcRow, const ScTable* pSrcTab );

    void CopyFromClip(
        sc::CopyFromClipContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
        SCCOL nDx, SCROW nDy, ScTable* pTable );

    void StartListeningFormulaCells(
        sc::StartListeningContext& rStartCxt, sc::EndListeningContext& rEndCxt,
        SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );

    void SetDirtyFromClip(
        SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, sc::ColumnSpanSet& rBroadcastSpans );

    void CopyToTable(
        sc::CopyToDocContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
        InsertDeleteFlags nFlags, bool bMarked, ScTable* pDestTab,
        const ScMarkData* pMarkData, bool bAsLink, bool bColRowFlags,
        bool bGlobalNamesToLocal, bool bCopyCaptions );

    void CopyCaptionsToTable( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, ScTable* pDestTab, bool bCloneCaption );

    void UndoToTable(
        sc::CopyToDocContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
        InsertDeleteFlags nFlags, bool bMarked, ScTable* pDestTab );

    void        CopyConditionalFormat( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                            SCCOL nDx, SCROW nDy, const ScTable* pTable);
    /**
     * @param nCombinedStartRow start row of the combined range;
     * used for transposed multi range selection with row direction;
     * for other cases than multi range row selection this it equal to nRow1
     * @param nRowDestOffset adjustment of destination row position;
     * used for transposed multi range selection with row direction, otherwise 0
     */
    void TransposeClip(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, SCROW nCombinedStartRow,
                       SCROW nRowDestOffset, ScTable* pTransClip, InsertDeleteFlags nFlags,
                       bool bAsLink, bool bIncludeFiltered);

    // mark of this document
    void MixMarked(
        sc::MixDocContext& rCxt, const ScMarkData& rMark, ScPasteFunc nFunction,
        bool bSkipEmpty, const ScTable* pSrcTab );

    void MixData(
        sc::MixDocContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
        ScPasteFunc nFunction, bool bSkipEmpty, const ScTable* pSrcTab );

    void        CopyData( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow,
                            SCCOL nDestCol, SCROW nDestRow, SCTAB nDestTab );

    void        CopyScenarioFrom( const ScTable* pSrcTab );
    void        CopyScenarioTo( ScTable* pDestTab ) const;
    bool        TestCopyScenarioTo( const ScTable* pDestTab ) const;
    void        MarkScenarioIn(ScMarkData& rMark, ScScenarioFlags nNeededBits) const;
    bool        HasScenarioRange( const ScRange& rRange ) const;
    void        InvalidateScenarioRanges();
    const ScRangeList* GetScenarioRanges() const;

    void        CopyUpdated( const ScTable* pPosTab, ScTable* pDestTab ) const;

    void        InvalidateTableArea();
    void        InvalidatePageBreaks();

    bool        GetCellArea( SCCOL& rEndCol, SCROW& rEndRow ) const;            // FALSE = empty
    bool        GetTableArea( SCCOL& rEndCol, SCROW& rEndRow, bool bCalcHiddens = false) const;
    bool        GetPrintArea( SCCOL& rEndCol, SCROW& rEndRow, bool bNotes, bool bCalcHiddens = false) const;
    bool        GetPrintAreaHor( SCROW nStartRow, SCROW nEndRow,
                                SCCOL& rEndCol ) const;
    bool        GetPrintAreaVer( SCCOL nStartCol, SCCOL nEndCol,
                                SCROW& rEndRow, bool bNotes ) const;

    bool        GetDataStart( SCCOL& rStartCol, SCROW& rStartRow ) const;

    void        ExtendPrintArea( OutputDevice* pDev,
                        SCCOL nStartCol, SCROW nStartRow, SCCOL& rEndCol, SCROW nEndRow );

    void        GetDataArea( SCCOL& rStartCol, SCROW& rStartRow, SCCOL& rEndCol, SCROW& rEndRow,
                             bool bIncludeOld, bool bOnlyDown ) const;

    void        GetBackColorArea( SCCOL& rStartCol, SCROW& rStartRow,
                                  SCCOL& rEndCol, SCROW& rEndRow ) const;

    bool        GetDataAreaSubrange( ScRange& rRange ) const;

    bool        ShrinkToUsedDataArea( bool& o_bShrunk, SCCOL& rStartCol, SCROW& rStartRow,
                                      SCCOL& rEndCol, SCROW& rEndRow, bool bColumnsOnly,
                                      bool bStickyTopRow, bool bStickyLeftCol,
                                      ScDataAreaExtras* pDataAreaExtras ) const;

    SCROW       GetLastDataRow( SCCOL nCol1, SCCOL nCol2, SCROW nLastRow,
                                ScDataAreaExtras* pDataAreaExtras = nullptr ) const;

    bool        IsEmptyData(SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow) const;
    SCSIZE      GetEmptyLinesInBlock( SCCOL nStartCol, SCROW nStartRow,
                                        SCCOL nEndCol, SCROW nEndRow, ScDirection eDir ) const;

    void        FindAreaPos( SCCOL& rCol, SCROW& rRow, ScMoveDirection eDirection ) const;
    void        GetNextPos( SCCOL& rCol, SCROW& rRow, SCCOL nMovX, SCROW nMovY,
                            bool bMarked, bool bUnprotected, const ScMarkData& rMark, SCCOL nTabStartCol ) const;

    bool        SkipRow( const SCCOL rCol, SCROW& rRow, const SCROW nMovY, const ScMarkData& rMark,
                         const bool bUp, const SCROW nUsedY, const bool bMarked, const bool bSheetProtected ) const;
    void        LimitChartArea( SCCOL& rStartCol, SCROW& rStartRow, SCCOL& rEndCol, SCROW& rEndRow ) const;

    bool        HasData( SCCOL nCol, SCROW nRow ) const;
    bool        HasStringData( SCCOL nCol, SCROW nRow ) const;
    bool        HasValueData( SCCOL nCol, SCROW nRow ) const;
    bool        HasStringCells( SCCOL nStartCol, SCROW nStartRow,
                                SCCOL nEndCol, SCROW nEndRow ) const;

    sc::MultiDataCellState HasMultipleDataCells( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 ) const;

    FormulaError    GetErrCode( const ScAddress& rPos ) const
                    {
                        return IsColRowValid(rPos.Col(),rPos.Row()) ?
                            aCol[rPos.Col()].GetErrCode( rPos.Row() ) :
                            FormulaError::NONE;
                    }

    void        ResetChanged( const ScRange& rRange );

    void CheckVectorizationState();
    void SetAllFormulasDirty( const sc::SetFormulaDirtyContext& rCxt );
    void        SetDirty( const ScRange&, ScColumn::BroadcastMode );
    void        SetDirtyAfterLoad();
    void        SetDirtyVar();
    void        SetTableOpDirty( const ScRange& );
    void        CalcAll();
    void CalcAfterLoad( sc::CompileFormulaContext& rCxt, bool bStartListening );
    void CompileAll( sc::CompileFormulaContext& rCxt );
    void CompileXML( sc::CompileFormulaContext& rCxt, ScProgress& rProgress );

    /** Broadcast single broadcasters in range, without explicitly setting
        anything dirty, not doing area broadcasts.
        @param rHint address is modified to adapt to the actual broadcasted
                position on each iteration and upon return points to the last
                position broadcasted. */
    bool BroadcastBroadcasters( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, SfxHintId nHint );

    bool CompileErrorCells( sc::CompileFormulaContext& rCxt, FormulaError nErrCode );

    void UpdateReference(
        sc::RefUpdateContext& rCxt, ScDocument* pUndoDoc = nullptr,
        bool bIncludeDraw = true, bool bUpdateNoteCaptionPos = true );

    void        UpdateDrawRef( UpdateRefMode eUpdateRefMode, SCCOL nCol1, SCROW nRow1, SCTAB nTab1,
                                    SCCOL nCol2, SCROW nRow2, SCTAB nTab2,
                                    SCCOL nDx, SCROW nDy, SCTAB nDz, bool bUpdateNoteCaptionPos = true );

    void        UpdateTranspose( const ScRange& rSource, const ScAddress& rDest,
                                    ScDocument* pUndoDoc );

    void        UpdateGrow( const ScRange& rArea, SCCOL nGrowX, SCROW nGrowY );

    void UpdateInsertTab( sc::RefUpdateInsertTabContext& rCxt );
    void UpdateDeleteTab( sc::RefUpdateDeleteTabContext& rCxt );
    void UpdateMoveTab( sc::RefUpdateMoveTabContext& rCxt, SCTAB nTabNo, ScProgress* pProgress );
    void        UpdateCompile( bool bForceIfNameInUse = false );
    void        SetTabNo(SCTAB nNewTab);
    void        FindRangeNamesInUse(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                                 sc::UpdatedRangeNames& rIndexes) const;
    void        Fill( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                        sal_uInt64 nFillCount, FillDir eFillDir, FillCmd eFillCmd, FillDateCmd eFillDateCmd,
                        double nStepValue, const tools::Duration& rDurationStep,
                        double nMaxValue, ScProgress* pProgress);
    OUString    GetAutoFillPreview( const ScRange& rSource, SCCOL nEndX, SCROW nEndY );

    void UpdateSelectionFunction( ScFunctionData& rData, const ScMarkData& rMark );

    void        AutoFormat( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow,
                                    sal_uInt16 nFormatNo, ScProgress* pProgress );
    void        GetAutoFormatData(SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow, ScAutoFormatData& rData);
    bool        SearchAndReplace(
        const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow, const ScMarkData& rMark,
        ScRangeList& rMatchedRanges, OUString& rUndoStr, ScDocument* pUndoDoc, bool& bMatchedRangesWereClamped);

    void        FindMaxRotCol( RowInfo* pRowInfo, SCSIZE nArrCount, SCCOL nX1, SCCOL nX2 );

    bool        HasAttrib( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, HasAttrFlags nMask ) const;
    bool        HasAttribSelection( const ScMarkData& rMark, HasAttrFlags nMask ) const;
    bool        HasAttrib( SCCOL nCol, SCROW nRow, HasAttrFlags nMask,
                           SCROW* nStartRow = nullptr, SCROW* nEndRow = nullptr ) const;
    bool IsMerged( SCCOL nCol, SCROW nRow ) const;
    bool        ExtendMerge( SCCOL nStartCol, SCROW nStartRow,
                                SCCOL& rEndCol, SCROW& rEndRow,
                                bool bRefresh );
    void SetMergedCells( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );

    const SfxPoolItem*      GetAttr( SCCOL nCol, SCROW nRow, sal_uInt16 nWhich ) const;
    template<class T> const T* GetAttr( SCCOL nCol, SCROW nRow, TypedWhichId<T> nWhich ) const
    {
        return static_cast<const T*>(GetAttr(nCol, nRow, sal_uInt16(nWhich)));
    }
    const SfxPoolItem*      GetAttr( SCCOL nCol, SCROW nRow, sal_uInt16 nWhich, SCROW& nStartRow, SCROW& nEndRow ) const;
    template<class T> const T* GetAttr( SCCOL nCol, SCROW nRow, TypedWhichId<T> nWhich, SCROW& nStartRow, SCROW& nEndRow ) const
    {
        return static_cast<const T*>(GetAttr(nCol, nRow, sal_uInt16(nWhich), nStartRow, nEndRow));
    }
    const ScPatternAttr*    GetPattern( SCCOL nCol, SCROW nRow ) const;
    const ScPatternAttr*    GetMostUsedPattern( SCCOL nCol, SCROW nStartRow, SCROW nEndRow ) const;

    sal_uInt32 GetNumberFormat( const ScInterpreterContext& rContext, const ScAddress& rPos ) const;
    sal_uInt32 GetNumberFormat( SCCOL nCol, SCROW nRow ) const;
    sal_uInt32 GetNumberFormat( SCCOL nCol, SCROW nStartRow, SCROW nEndRow ) const;

    void SetNumberFormat( SCCOL nCol, SCROW nRow, sal_uInt32 nNumberFormat );

    void                    MergeSelectionPattern( ScMergePatternState& rState,
                                                const ScMarkData& rMark, bool bDeep ) const;
    void                    MergePatternArea( ScMergePatternState& rState, SCCOL nCol1, SCROW nRow1,
                                                SCCOL nCol2, SCROW nRow2, bool bDeep ) const;
    void                    MergeBlockFrame( SvxBoxItem* pLineOuter, SvxBoxInfoItem* pLineInner,
                                            ScLineFlags& rFlags,
                                            SCCOL nStartCol, SCROW nStartRow,
                                            SCCOL nEndCol, SCROW nEndRow ) const;
    void                    ApplyBlockFrame(const SvxBoxItem& rLineOuter,
                                            const SvxBoxInfoItem* pLineInner,
                                            SCCOL nStartCol, SCROW nStartRow,
                                            SCCOL nEndCol, SCROW nEndRow );

    void        ApplyAttr( SCCOL nCol, SCROW nRow, const SfxPoolItem& rAttr );
    void        ApplyPattern( SCCOL nCol, SCROW nRow, const ScPatternAttr& rAttr );
    void        ApplyPatternArea( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow,
                                  const ScPatternAttr& rAttr, ScEditDataArray* pDataArray = nullptr,
                                  bool* const pIsChanged = nullptr );
    void        SetAttrEntries( SCCOL nStartCol, SCCOL nEndCol, std::vector<ScAttrEntry> && vNewData);

    void        SetPattern( const ScAddress& rPos, const ScPatternAttr& rAttr );
    void        SetPattern( SCCOL nCol, SCROW nRow, const CellAttributeHolder& rHolder );
    void        SetPattern( SCCOL nCol, SCROW nRow, const ScPatternAttr& rAttr );
    void        ApplyPatternIfNumberformatIncompatible( const ScRange& rRange,
                            const ScPatternAttr& rPattern, SvNumFormatType nNewType );
    void        AddCondFormatData( const ScRangeList& rRange, sal_uInt32 nIndex );
    void        RemoveCondFormatData( const ScRangeList& rRange, sal_uInt32 nIndex );
    void        SetPatternAreaCondFormat( SCCOL nCol, SCROW nStartRow, SCROW nEndRow,
                    const ScPatternAttr& rAttr, const ScCondFormatIndexes& rCondFormatIndexes );

    void        ApplyStyle( SCCOL nCol, SCROW nRow, const ScStyleSheet* rStyle );
    void        ApplyStyleArea( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow, const ScStyleSheet& rStyle );
    void        ApplySelectionStyle(const ScStyleSheet& rStyle, const ScMarkData& rMark);
    void        ApplySelectionLineStyle( const ScMarkData& rMark,
                                    const ::editeng::SvxBorderLine* pLine, bool bColorOnly );

    const ScStyleSheet* GetStyle( SCCOL nCol, SCROW nRow ) const;
    const ScStyleSheet* GetSelectionStyle( const ScMarkData& rMark, bool& rFound ) const;
    const ScStyleSheet* GetAreaStyle( bool& rFound, SCCOL nCol1, SCROW nRow1,
                                                    SCCOL nCol2, SCROW nRow2 ) const;

    void        StyleSheetChanged( const SfxStyleSheetBase* pStyleSheet, bool bRemoved,
                                    OutputDevice* pDev,
                                    double nPPTX, double nPPTY,
                                    const Fraction& rZoomX, const Fraction& rZoomY );

    bool        IsStyleSheetUsed( const ScStyleSheet& rStyle ) const;

    bool        ApplyFlags( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow, ScMF nFlags );
    bool        RemoveFlags( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow, ScMF nFlags );

    void        ApplySelectionCache( ScItemPoolCache& rCache, const ScMarkData& rMark, ScEditDataArray* pDataArray = nullptr, bool* const pIsChanged = nullptr );
    void        DeleteSelection( InsertDeleteFlags nDelFlag, const ScMarkData& rMark, bool bBroadcast = true );

    void        ClearSelectionItems( const sal_uInt16* pWhich, const ScMarkData& rMark );
    void        ChangeSelectionIndent( bool bIncrement, const ScMarkData& rMark );

    const std::optional<ScRange>& GetRepeatColRange() const { return moRepeatColRange; }
    const std::optional<ScRange>& GetRepeatRowRange() const { return moRepeatRowRange; }
    void            SetRepeatColRange( std::optional<ScRange> oNew );
    void            SetRepeatRowRange( std::optional<ScRange> oNew );

    sal_uInt16          GetPrintRangeCount() const          { return static_cast< sal_uInt16 >( aPrintRanges.size() ); }
    const ScRange*  GetPrintRange(sal_uInt16 nPos) const;
    /** Returns true, if the sheet is always printed. */
    bool            IsPrintEntireSheet() const          { return bPrintEntireSheet; }

    /** Removes all print ranges. */
    void            ClearPrintRanges();
    /** Adds a new print ranges. */
    void            AddPrintRange( const ScRange& rNew );

    // Removes all named ranges used for print ranges
    void            ClearPrintNamedRanges();

    /** Marks the specified sheet to be printed completely. Deletes old print ranges! */
    void            SetPrintEntireSheet();

    void            FillPrintSaver( ScPrintSaverTab& rSaveTab ) const;
    void            RestorePrintRanges( const ScPrintSaverTab& rSaveTab );

    sal_uInt16      GetOptimalColWidth( SCCOL nCol, OutputDevice* pDev,
                                    double nPPTX, double nPPTY,
                                    const Fraction& rZoomX, const Fraction& rZoomY,
                                    bool bFormula, const ScMarkData* pMarkData,
                                    const ScColWidthParam* pParam );
    bool SetOptimalHeight(
        sc::RowHeightContext& rCxt, SCROW nStartRow, SCROW nEndRow, bool bApi,
        ScProgress* pOuterProgress = nullptr, sal_uInt64 nProgressStart = 0 );

    void SetOptimalHeightOnly(
        sc::RowHeightContext& rCxt, SCROW nStartRow, SCROW nEndRow,
        ScProgress* pOuterProgress = nullptr, sal_uInt64 nProgressStart = 0 );

    tools::Long        GetNeededSize( SCCOL nCol, SCROW nRow,
                                    OutputDevice* pDev,
                                    double nPPTX, double nPPTY,
                                    const Fraction& rZoomX, const Fraction& rZoomY,
                                    bool bWidth, bool bTotalSize,
                                    bool bInPrintTwips = false);
    void        SetColWidth( SCCOL nCol, sal_uInt16 nNewWidth );
    void        SetColWidthOnly( SCCOL nCol, sal_uInt16 nNewWidth );
    void        SetRowHeight( SCROW nRow, sal_uInt16 nNewHeight );
    bool        SetRowHeightRange( SCROW nStartRow, SCROW nEndRow, sal_uInt16 nNewHeight,
                                   double nPPTY, bool bApi );

    /**
     * Set specified row height to specified ranges.  Don't check for drawing
     * objects etc.  Just set the row height.  Nothing else.
     *
     * Note that setting a new row height via this function will not
     * invalidate page breaks.
     */
    void        SetRowHeightOnly( SCROW nStartRow, SCROW nEndRow, sal_uInt16 nNewHeight );

                        // nPPT to test for modification
    void        SetManualHeight( SCROW nStartRow, SCROW nEndRow, bool bManual );

    sal_uInt16 GetOptimalMinRowHeight() const
    {
        if (!mnOptimalMinRowHeight)
            return ScGlobal::nStdRowHeight;
        return mnOptimalMinRowHeight;
    };
    void SetOptimalMinRowHeight(sal_uInt16 nSet) { mnOptimalMinRowHeight = nSet; }

    sal_uInt16      GetColWidth( SCCOL nCol, bool bHiddenAsZero = true ) const;
    tools::Long     GetColWidth( SCCOL nStartCol, SCCOL nEndCol ) const;
    sal_uInt16 GetRowHeight( SCROW nRow, SCROW* pStartRow, SCROW* pEndRow, bool bHiddenAsZero = true ) const;
    tools::Long     GetRowHeight( SCROW nStartRow, SCROW nEndRow, bool bHiddenAsZero = true ) const;
    tools::Long     GetScaledRowHeight( SCROW nStartRow, SCROW nEndRow, double fScale ) const;
    tools::Long     GetColOffset( SCCOL nCol, bool bHiddenAsZero = true ) const;
    tools::Long     GetRowOffset( SCROW nRow, bool bHiddenAsZero = true ) const;

    /**
     * Get the last row such that the height of row 0 to the end row is as
     * high as possible without exceeding the specified height value.
     *
     * @param nHeight maximum desired height
     *
     * @return SCROW last row of the range within specified height.
     */
    SCROW       GetRowForHeight(tools::Long nHeight) const;
    /**
     * Given the height i.e. total vertical distance from the top of the sheet
     * grid, return the first visible row whose top position is below the
     * specified height.
     * Note that this variant uses pixels, not twips.
     * @param nStartRow the row to start searching at.
     * @param rStartRowHeightPx this is both the height at nStartRow, and returns the height of the first row
     *        which has height > nHeight
     */
    SCROW       GetRowForHeightPixels( SCROW nStartRow, tools::Long& rStartRowHeightPx, tools::Long nHeightPx, double fPPTY ) const;

    sal_uInt16      GetOriginalWidth( SCCOL nCol ) const;
    sal_uInt16      GetOriginalHeight( SCROW nRow ) const;

    sal_uInt16      GetCommonWidth( SCCOL nEndCol ) const;

    SCROW       GetHiddenRowCount( SCROW nRow ) const;

    void        ShowCol(SCCOL nCol, bool bShow);
    void        ShowRow(SCROW nRow, bool bShow);
    void        DBShowRow(SCROW nRow, bool bShow);

    void        ShowRows(SCROW nRow1, SCROW nRow2, bool bShow);
    void        DBShowRows(SCROW nRow1, SCROW nRow2, bool bShow);

    void        SetRowFlags( SCROW nRow, CRFlags nNewFlags );
    void        SetRowFlags( SCROW nStartRow, SCROW nEndRow, CRFlags nNewFlags );

                /// @return  the index of the last row with any set flags (auto-pagebreak is ignored).
    SCROW      GetLastFlaggedRow() const;

    /// @return  the index of the last changed column (flags and column width, auto pagebreak is ignored).
    SCCOL      GetLastChangedColFlagsWidth() const;
    /// @return  the index of the last changed row (flags and row height, auto pagebreak is ignored).
    SCROW      GetLastChangedRowFlagsWidth() const;

    bool       IsDataFiltered(SCCOL nColStart, SCROW nRowStart, SCCOL nColEnd, SCROW nRowEnd) const;
    bool       IsDataFiltered(const ScRange& rRange) const;
    CRFlags    GetColFlags( SCCOL nCol ) const;
    CRFlags    GetRowFlags( SCROW nRow ) const;

    const ScBitMaskCompressedArray< SCROW, CRFlags> * GetRowFlagsArray() const
                    { return pRowFlags.get(); }

    bool        UpdateOutlineCol( SCCOL nStartCol, SCCOL nEndCol, bool bShow );
    bool        UpdateOutlineRow( SCROW nStartRow, SCROW nEndRow, bool bShow );

    void        UpdatePageBreaks( const ScRange* pUserArea );
    void        RemoveManualBreaks();
    bool        HasManualBreaks() const;
    void        SetRowManualBreaks( ::std::set<SCROW>&& rBreaks );
    void        SetColManualBreaks( ::std::set<SCCOL>&& rBreaks );

    void        GetAllRowBreaks(::std::set<SCROW>& rBreaks, bool bPage, bool bManual) const;
    void        GetAllColBreaks(::std::set<SCCOL>& rBreaks, bool bPage, bool bManual) const;
    bool        HasRowPageBreak(SCROW nRow) const;
    bool        HasColPageBreak(SCCOL nCol) const;
    bool        HasRowManualBreak(SCROW nRow) const;
    bool        HasColManualBreak(SCCOL nCol) const;

    /**
     * Get the row position of the next manual break that occurs at or below
     * specified row.  When no more manual breaks are present at or below
     * the specified row, -1 is returned.
     *
     * @param nRow row at which the search begins.
     *
     * @return SCROW next row position with manual page break, or -1 if no
     *         more manual breaks are present.
     */
    SCROW       GetNextManualBreak(SCROW nRow) const;

    void        RemoveRowPageBreaks(SCROW nStartRow, SCROW nEndRow);
    void        RemoveRowBreak(SCROW nRow, bool bPage, bool bManual);
    void        RemoveColBreak(SCCOL nCol, bool bPage, bool bManual);
    void        SetRowBreak(SCROW nRow, bool bPage, bool bManual);
    void        SetColBreak(SCCOL nCol, bool bPage, bool bManual);
    css::uno::Sequence<
        css::sheet::TablePageBreakData> GetRowBreakData() const;

    void updateObjectsForColsChanged(SCCOL nStartCol, SCCOL nEndCol, bool bHidden, bool bChanged);
    void updateObjectsForRowsChanged(SCROW nStartRow, SCROW nEndRow, bool bHidden, bool bChanged);

    bool        RowHidden(SCROW nRow, SCROW* pFirstRow = nullptr, SCROW* pLastRow = nullptr) const;
    bool        RowHiddenLeaf(SCROW nRow, SCROW* pFirstRow = nullptr, SCROW* pLastRow = nullptr) const;
    bool        HasHiddenRows(SCROW nStartRow, SCROW nEndRow) const;
    bool        ColHidden(SCCOL nCol, SCCOL* pFirstCol = nullptr, SCCOL* pLastCol = nullptr) const;
    bool        SetRowHidden(SCROW nStartRow, SCROW nEndRow, bool bHidden);
    void        SetColHidden(SCCOL nStartCol, SCCOL nEndCol, bool bHidden);
    void        CopyColHidden(const ScTable& rTable, SCCOL nStartCol, SCCOL nEndCol);
    void        CopyRowHidden(const ScTable& rTable, SCROW nStartRow, SCROW nEndRow);
    void        CopyRowHeight(const ScTable& rSrcTable, SCROW nStartRow, SCROW nEndRow, SCROW nSrcOffset);
    SCROW       FirstVisibleRow(SCROW nStartRow, SCROW nEndRow) const;
    SCROW       LastVisibleRow(SCROW nStartRow, SCROW nEndRow) const;
    SCROW       CountVisibleRows(SCROW nStartRow, SCROW nEndRow) const;
    tools::Long GetTotalRowHeight(SCROW nStartRow, SCROW nEndRow, bool bHiddenAsZero = true) const;

    SCCOL       CountVisibleCols(SCCOL nStartCol, SCCOL nEndCol) const;

    SCCOLROW    LastHiddenColRow(SCCOLROW nPos, bool bCol) const;

    FilterData& getFilterData() { return maFilterData; }
    FilterData const& getFilterData() const { return maFilterData; }

    Color GetCellBackgroundColor(ScAddress aPos) const;
    Color GetCellTextColor(ScAddress aPos) const;

    bool IsManualRowHeight(SCROW nRow) const;

    bool HasUniformRowHeight( SCROW nRow1, SCROW nRow2 ) const;

    void        SyncColRowFlags();

    void        StripHidden( SCCOL& rX1, SCROW& rY1, SCCOL& rX2, SCROW& rY2 );
    void        ExtendHidden( SCCOL& rX1, SCROW& rY1, SCCOL& rX2, SCROW& rY2 );

    /** Sort a range of data. */
    void Sort(
        const ScSortParam& rSortParam, bool bKeepQuery, bool bUpdateRefs,
        ScProgress* pProgress, sc::ReorderParam* pUndo );

    void Reorder( const sc::ReorderParam& rParam );

    // For ValidQuery() see ScQueryEvalutor class.
    void        TopTenQuery( ScQueryParam& );
    void        PrepareQuery( ScQueryParam& rQueryParam );
    SCSIZE      Query(const ScQueryParam& rQueryParam, bool bKeepSub);
    bool        CreateQueryParam(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, ScQueryParam& rQueryParam);

    void GetFilterEntries(SCCOL nCol, SCROW nRow1, SCROW nRow2, ScFilterEntries& rFilterEntries, bool bFiltering = false);
    void GetFilteredFilterEntries(SCCOL nCol, SCROW nRow1, SCROW nRow2, const ScQueryParam& rParam, ScFilterEntries& rFilterEntries, bool bFiltering );
    [[nodiscard]]
    bool GetDataEntries(SCCOL nCol, SCROW nRow, std::set<ScTypedStrData>& rStrings);

    bool        HasColHeader( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow ) const;
    bool        HasRowHeader( SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow ) const;

    sal_Int32   GetMaxStringLen( SCCOL nCol,
                                    SCROW nRowStart, SCROW nRowEnd, rtl_TextEncoding eCharSet ) const;
    sal_Int32  GetMaxNumberStringLen( sal_uInt16& nPrecision,
                                       SCCOL nCol,
                                       SCROW nRowStart, SCROW nRowEnd ) const;

    bool        IsSortCollatorGlobal() const;
    void        InitSortCollator( const ScSortParam& rPar );
    void        DestroySortCollator();
    void        SetDrawPageSize( bool bResetStreamValid = true, bool bUpdateNoteCaptionPos = true,
                                 const ScObjectHandling eObjectHandling = ScObjectHandling::RecalcPosMode);

    void SetRangeName(std::unique_ptr<ScRangeName> pNew);
    SC_DLLPUBLIC ScRangeName* GetRangeName() const;

    void PreprocessRangeNameUpdate(
        sc::EndListeningContext& rEndListenCxt, sc::CompileFormulaContext& rCompileCxt );

    void CompileHybridFormula(
        sc::StartListeningContext& rStartListenCxt, sc::CompileFormulaContext& rCompileCxt );

    void PreprocessDBDataUpdate(
        sc::EndListeningContext& rEndListenCxt, sc::CompileFormulaContext& rCompileCxt );

    ScConditionalFormatList* GetCondFormList();
    const ScConditionalFormatList* GetCondFormList() const;
    void SetCondFormList( ScConditionalFormatList* pList );

    void DeleteConditionalFormat(sal_uLong nOldIndex);

    sal_uInt32          AddCondFormat( std::unique_ptr<ScConditionalFormat> pNew );

    SvtScriptType GetScriptType( SCCOL nCol, SCROW nRow ) const;
    void SetScriptType( SCCOL nCol, SCROW nRow, SvtScriptType nType );
    void UpdateScriptTypes( const SCCOL nCol1, SCROW nRow1, const SCCOL nCol2, SCROW nRow2 );

    SvtScriptType GetRangeScriptType( sc::ColumnBlockPosition& rBlockPos, SCCOL nCol, SCROW nRow1, SCROW nRow2 );

    formula::FormulaTokenRef ResolveStaticReference( SCCOL nCol, SCROW nRow );
    formula::FormulaTokenRef ResolveStaticReference( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );
    formula::VectorRefArray FetchVectorRefArray( SCCOL nCol, SCROW nRow1, SCROW nRow2 );
    bool HandleRefArrayForParallelism( SCCOL nCol, SCROW nRow1, SCROW nRow2,
                                       const ScFormulaCellGroupRef& mxGroup, ScAddress* pDirtiedAddress );
#ifdef DBG_UTIL
    void AssertNoInterpretNeeded( SCCOL nCol, SCROW nRow1, SCROW nRow2 );
#endif

    void SplitFormulaGroups( SCCOL nCol, std::vector<SCROW>& rRows );
    void UnshareFormulaCells( SCCOL nCol, std::vector<SCROW>& rRows );
    void RegroupFormulaCells( SCCOL nCol );

    ScRefCellValue GetRefCellValue( SCCOL nCol, SCROW nRow );
    ScRefCellValue GetRefCellValue( SCCOL nCol, SCROW nRow, sc::ColumnBlockPosition& rBlockPos );

    SvtBroadcaster* GetBroadcaster( SCCOL nCol, SCROW nRow );
    const SvtBroadcaster* GetBroadcaster( SCCOL nCol, SCROW nRow ) const;
    void DeleteBroadcasters( sc::ColumnBlockPosition& rBlockPos, SCCOL nCol, SCROW nRow1, SCROW nRow2 );
    void DeleteEmptyBroadcasters();

    void FillMatrix( ScMatrix& rMat, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, svl::SharedStringPool* pPool ) const;

    void InterpretDirtyCells( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );
    bool InterpretCellsIfNeeded( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );

    void SetFormulaResults( SCCOL nCol, SCROW nRow, const double* pResults, size_t nLen );

    void CalculateInColumnInThread( ScInterpreterContext& rContext, SCCOL nColStart, SCCOL nColEnd,
                                    SCROW nRowStart, SCROW nRowEnd, unsigned nThisThread, unsigned nThreadsTotal);
    void HandleStuffAfterParallelCalculation( SCCOL nColStart, SCCOL nColEnd, SCROW nRow, size_t nLen, ScInterpreter* pInterpreter);

    /**
     * Either start all formula cells as listeners unconditionally, or start
     * those that are marked "needs listening".
     *
     * @param rCxt context object.
     * @param bAll when true, start all formula cells as listeners. When
     *             false, only start those that are marked "needs listening".
     */
    void StartListeners( sc::StartListeningContext& rCxt, bool bAll );

    /**
     * Mark formula cells dirty that have the mbPostponedDirty flag set or
     * contain named ranges with relative references.
     */
    void SetDirtyIfPostponed();

    /**
     * Broadcast dirty formula cells that contain functions such as CELL(),
     * COLUMN() or ROW() which may change its value on move.
     */
    void BroadcastRecalcOnRefMove();

    void TransferCellValuesTo( const SCCOL nCol, SCROW nRow, size_t nLen, sc::CellValues& rDest );
    void CopyCellValuesFrom( const SCCOL nCol, SCROW nRow, const sc::CellValues& rSrc );

    std::optional<sc::ColumnIterator> GetColumnIterator( SCCOL nCol, SCROW nRow1, SCROW nRow2 ) const;

    bool EnsureFormulaCellResults( const SCCOL nCol1, SCROW nRow1, const SCCOL nCol2, SCROW nRow2, bool bSkipRunning = false );

    void ConvertFormulaToValue(
        sc::EndListeningContext& rCxt,
        const SCCOL nCol1, const SCROW nRow1, const SCCOL nCol2, const SCROW nRow2,
        sc::TableValues* pUndo );

    void SwapNonEmpty(
        sc::TableValues& rValues, sc::StartListeningContext& rStartCxt, sc::EndListeningContext& rEndCxt );

    void finalizeOutlineImport();

    void StoreToCache(SvStream& rStrm) const;

    void RestoreFromCache(SvStream& rStrm);

#if DUMP_COLUMN_STORAGE
    void DumpColumnStorage( SCCOL nCol ) const;
#endif

    /** Replace behaves differently to the Search; adjust the rCol and rRow accordingly.

        'Replace' replaces at the 'current' position, but in order to achieve
        that, we have to 'shift' the rCol / rRow to the 'previous' position -
        what it is depends on various settings in rSearchItem.
    */
    static void UpdateSearchItemAddressForReplace( const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow );

    ScColumnsRange GetWritableColumnsRange(SCCOL begin, SCCOL end);
    ScColumnsRange GetAllocatedColumnsRange(SCCOL begin, SCCOL end) const;
    ScColumnsRange GetColumnsRange(SCCOL begin, SCCOL end) const;
    SCCOL ClampToAllocatedColumns(SCCOL nCol) const { return std::min(nCol, static_cast<SCCOL>(aCol.size() - 1)); }
    SCCOL GetAllocatedColumnsCount() const { return aCol.size(); }

    /**
     * Serializes the sheet's geometry data.
     *
     * @param bColumns - if true it dumps the data for columns, else it does for rows.
     * @param eGeomType indicates the type of data to be dumped for rows/columns.
     * @return the serialization of the sheet's geometry data as an OString.
     */
    OString dumpSheetGeomData(bool bColumns, SheetGeomType eGeomType);

    std::set<SCCOL> QueryColumnsWithFormulaCells() const;

    void CheckIntegrity() const;

    void CollectBroadcasterState(sc::BroadcasterState& rState) const;

private:

    void FillFormulaVertical(
        const ScFormulaCell& rSrcCell,
        SCCOLROW& rInner, SCCOL nCol, SCROW nRow1, SCROW nRow2,
        ScProgress* pProgress, sal_uInt64& rProgress );

    void FillSeriesSimple(
        const ScCellValue& rSrcCell, SCCOLROW& rInner, SCCOLROW nIMin, SCCOLROW nIMax,
        const SCCOLROW& rCol, const SCCOLROW& rRow, bool bVertical, ScProgress* pProgress, sal_uInt64& rProgress );

    void FillAutoSimple(
        SCCOLROW nISrcStart, SCCOLROW nISrcEnd, SCCOLROW nIStart, SCCOLROW nIEnd,
        SCCOLROW& rInner, const SCCOLROW& rCol, const SCCOLROW& rRow,
        sal_uInt64 nActFormCnt, sal_uInt64 nMaxFormCnt,
        bool bHasFiltered, bool bVertical, bool bPositive,
        ScProgress* pProgress, sal_uInt64& rProgress );

    void        FillSeries( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                                sal_uInt64 nFillCount, FillDir eFillDir, FillCmd eFillCmd,
                                FillDateCmd eFillDateCmd,
                                double nStepValue, const tools::Duration& rDurationStep,
                                double nMaxValue, sal_uInt16 nMinDigits,
                                bool bAttribs, ScProgress* pProgress,
                                bool bSkipOverlappedCells = false,
                                std::vector<sal_Int32>* pNonOverlappedCellIdx = nullptr);
    void        FillAnalyse( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                                FillCmd& rCmd, FillDateCmd& rDateCmd,
                                double& rInc, tools::Duration& rDuration, sal_uInt16& rMinDigits,
                                ScUserListData*& rListData, sal_uInt16& rListIndex,
                                bool bHasFiltered, bool& rSkipOverlappedCells,
                                std::vector<sal_Int32>& rNonOverlappedCellIdx );
    void        FillAuto( SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2,
                        sal_uInt64 nFillCount, FillDir eFillDir, ScProgress* pProgress );

    bool        ValidNextPos( SCCOL nCol, SCROW nRow, const ScMarkData& rMark,
                                bool bMarked, bool bUnprotected ) const;

    void        AutoFormatArea(SCCOL nStartCol, SCROW nStartRow, SCCOL nEndCol, SCROW nEndRow,
                                const ScPatternAttr& rAttr, sal_uInt16 nFormatNo);
    void        GetAutoFormatAttr(SCCOL nCol, SCROW nRow, sal_uInt16 nIndex, ScAutoFormatData& rData);
    void        GetAutoFormatFrame(SCCOL nCol, SCROW nRow, sal_uInt16 nFlags, sal_uInt16 nIndex, ScAutoFormatData& rData);
    bool        SearchCell(const SvxSearchItem& rSearchItem, SCCOL nCol, sc::ColumnBlockConstPosition& rBlockPos, SCROW nRow,
                           const ScMarkData& rMark, OUString& rUndoStr, ScDocument* pUndoDoc);
    bool        Search(const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow,
                       const ScMarkData& rMark, OUString& rUndoStr, ScDocument* pUndoDoc);
    bool        Search(const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow,
                       SCCOL nLastCol, SCROW nLastRow,
                       const ScMarkData& rMark, OUString& rUndoStr, ScDocument* pUndoDoc,
                       std::vector< sc::ColumnBlockConstPosition >& blockPos);
    bool        SearchAll(const SvxSearchItem& rSearchItem, const ScMarkData& rMark,
                          ScRangeList& rMatchedRanges, OUString& rUndoStr, ScDocument* pUndoDoc);
    bool        Replace(const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow,
                        const ScMarkData& rMark, OUString& rUndoStr, ScDocument* pUndoDoc);
    bool        ReplaceAll(
        const SvxSearchItem& rSearchItem, const ScMarkData& rMark, ScRangeList& rMatchedRanges,
        OUString& rUndoStr, ScDocument* pUndoDoc, bool& bMatchedRangesWereClamped);

    bool        SearchStyle(const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow,
                            const ScMarkData& rMark);
    bool        ReplaceStyle(const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow,
                             const ScMarkData& rMark, bool bIsUndo);
    bool        SearchAllStyle(
        const SvxSearchItem& rSearchItem, const ScMarkData& rMark, ScRangeList& rMatchedRanges);
    bool        ReplaceAllStyle(
        const SvxSearchItem& rSearchItem, const ScMarkData& rMark, ScRangeList& rMatchedRanges,
        ScDocument* pUndoDoc);
    bool        SearchAndReplaceEmptyCells(
                    const SvxSearchItem& rSearchItem,
                    SCCOL& rCol, SCROW& rRow, const ScMarkData& rMark, ScRangeList& rMatchedRanges,
                    OUString& rUndoStr, ScDocument* pUndoDoc);
    bool        SearchRangeForEmptyCell(const ScRange& rRange,
                    const SvxSearchItem& rSearchItem, SCCOL& rCol, SCROW& rRow,
                    OUString& rUndoStr);
    bool        SearchRangeForAllEmptyCells(
        const ScRange& rRange, const SvxSearchItem& rSearchItem,
        ScRangeList& rMatchedRanges, OUString& rUndoStr, ScDocument* pUndoDoc);

                                // use the global sort parameter:
    bool        IsSorted(SCCOLROW nStart, SCCOLROW nEnd) const;
    static void DecoladeRow( ScSortInfoArray*, SCROW nRow1, SCROW nRow2 );
    short CompareCell(
        sal_uInt16 nSort,
        ScRefCellValue& rCell1, SCCOL nCell1Col, SCROW nCell1Row,
        ScRefCellValue& rCell2, SCCOL nCell2Col, SCROW nCell2Row ) const;
    short       Compare(SCCOLROW nIndex1, SCCOLROW nIndex2) const;
    short       Compare( ScSortInfoArray*, SCCOLROW nIndex1, SCCOLROW nIndex2) const;
    std::unique_ptr<ScSortInfoArray> CreateSortInfoArray( const sc::ReorderParam& rParam );
    std::unique_ptr<ScSortInfoArray> CreateSortInfoArray(
        const ScSortParam& rSortParam, SCCOLROW nInd1, SCCOLROW nInd2,
        bool bKeepQuery, bool bUpdateRefs );
    void        QuickSort( ScSortInfoArray*, SCCOLROW nLo, SCCOLROW nHi);
    void        SortReorderByColumn( const ScSortInfoArray* pArray, SCROW nRow1, SCROW nRow2,
                                     bool bPattern, ScProgress* pProgress );
    void        SortReorderAreaExtrasByColumn( const ScSortInfoArray* pArray, SCROW nDataRow1, SCROW nDataRow2,
                                               const ScDataAreaExtras& rDataAreaExtras, ScProgress* pProgress );

    void        SortReorderByRow( ScSortInfoArray* pArray, SCCOL nCol1, SCCOL nCol2,
                                  ScProgress* pProgress, bool bOnlyDataAreaExtras );
    void        SortReorderByRowRefUpdate( ScSortInfoArray* pArray, SCCOL nCol1, SCCOL nCol2,
                                           ScProgress* pProgress );
    void        SortReorderAreaExtrasByRow( ScSortInfoArray* pArray, SCCOL nDataCol1, SCCOL nDataCol2,
                                            const ScDataAreaExtras& rDataAreaExtras, ScProgress* pProgress );

    bool        CreateExcelQuery(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, ScQueryParam& rQueryParam);
    bool        CreateStarQuery(SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2, ScQueryParam& rQueryParam);
    OUString    GetUpperCellString(SCCOL nCol, SCROW nRow);

    bool        RefVisible(const ScFormulaCell* pCell);

    bool        IsEmptyLine(SCROW nRow, SCCOL nStartCol, SCCOL nEndCol) const;

    void        IncDate(double& rVal, sal_uInt16& nDayOfMonth, double nStep, FillDateCmd eCmd);
    void FillFormula(
        const ScFormulaCell* pSrcCell, SCCOL nDestCol, SCROW nDestRow, bool bLast );
    void        UpdateInsertTabAbs(SCTAB nNewPos);
    bool        GetNextSpellingCell(SCCOL& rCol, SCROW& rRow, bool bInSel,
                                    const ScMarkData& rMark) const;
    bool        GetNextMarkedCell( SCCOL& rCol, SCROW& rRow, const ScMarkData& rMark ) const;
    void        TestTabRefAbs(SCTAB nTable) const;
    void CompileDBFormula( sc::CompileFormulaContext& rCxt );
    void CompileColRowNameFormula( sc::CompileFormulaContext& rCxt );

    void        StartListening( const ScAddress& rAddress, SvtListener* pListener );
    void        EndListening( const ScAddress& rAddress, SvtListener* pListener );
    void StartListening( sc::StartListeningContext& rCxt, const ScAddress& rAddress, SvtListener& rListener );
    void EndListening( sc::EndListeningContext& rCxt, const ScAddress& rAddress, SvtListener& rListener );

    void AttachFormulaCells( sc::StartListeningContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );
    void DetachFormulaCells( sc::EndListeningContext& rCxt, SCCOL nCol1, SCROW nRow1, SCCOL nCol2, SCROW nRow2 );

    void        SetLoadingMedium(bool bLoading);

    struct FillMaxRotCacheMapHash
    {
        size_t operator()(const std::pair<const ScPatternAttr*, const SfxItemSet*>& rPair) const noexcept
        {
            return std::hash<const ScPatternAttr*>{}(rPair.first) ^ (std::hash<const SfxItemSet*>{}(rPair.second) << 1);
        }
    };
    typedef std::unordered_map<std::pair<const ScPatternAttr*, const SfxItemSet*>, ScRotateDir, FillMaxRotCacheMapHash> FillMaxRotCacheMap;

    SCSIZE      FillMaxRot( RowInfo* pRowInfo, SCSIZE nArrCount, SCCOL nX1, SCCOL nX2,
                            SCCOL nCol, SCROW nAttrRow1, SCROW nAttrRow2, SCSIZE nArrY,
                            const ScPatternAttr* pPattern, const SfxItemSet* pCondSet,
                            FillMaxRotCacheMap* pCache);

    // idle calculation of OutputDevice text width for cell
    // also invalidates script type, broadcasts for "calc as shown"
    void        InvalidateTextWidth( const ScAddress* pAdrFrom, const ScAddress* pAdrTo,
                                     bool bNumFormatChanged, bool bBroadcast );

    void        SkipFilteredRows(SCROW& rRow, SCROW& rLastNonFilteredRow, bool bForward);

    /**
     * In case the cell text goes beyond the column width, move the max column
     * position to the right.  This is called from ExtendPrintArea.
     */
    void        MaybeAddExtraColumn(SCCOL& rCol, SCROW nRow, OutputDevice* pDev, double nPPTX, double nPPTY);

    void        CopyPrintRange(const ScTable& rTable);

    SCCOL       FindNextVisibleColWithContent(SCCOL nCol, bool bRight, SCROW nRow) const;

    SCCOL       FindNextVisibleCol(SCCOL nCol, bool bRight) const;

    /**
     * Transpose clipboard patterns
     * @param nCombinedStartRow start row of the combined range;
     * used for transposed multi range selection with row direction;
     * for other cases than multi range row selection this it equal to nRow1
     * @param nRowDestOffset adjustment of destination row position;
     * used for transposed multi range row selections, otherwise 0
     */
    void TransposeColPatterns(ScTable* pTransClip, SCCOL nCol1, SCCOL nCol, SCROW nRow1,
                              SCROW nRow2, SCROW nCombinedStartRow, bool bIncludeFiltered,
                              const std::vector<SCROW>& rFilteredRows, SCROW nRowDestOffset);

    /**
     * Transpose clipboard notes
     * @param nCombinedStartRow start row of the combined range;
     * used for transposed multi range selection with row direction;
     * for other cases than multi range row selection this it equal to nRow1
     * @param nRowDestOffset adjustment of destination row position;
     * used for transposed multi range row selections, otherwise 0
     */
    void TransposeColNotes(ScTable* pTransClip, SCCOL nCol1, SCCOL nCol, SCROW nRow1, SCROW nRow2,
                           SCROW nCombinedStartRow, bool bIncludeFiltered, SCROW nRowDestOffset);

    ScColumn* FetchColumn( SCCOL nCol );
    const ScColumn* FetchColumn( SCCOL nCol ) const;

    void EndListeningIntersectedGroup(
        sc::EndListeningContext& rCxt, SCCOL nCol, SCROW nRow, std::vector<ScAddress>* pGroupPos );

    void EndListeningIntersectedGroups(
        sc::EndListeningContext& rCxt, const SCCOL nCol1, SCROW nRow1, const SCCOL nCol2, SCROW nRow2,
        std::vector<ScAddress>* pGroupPos );

    void EndListeningGroup( sc::EndListeningContext& rCxt, const SCCOL nCol, SCROW nRow );
    void SetNeedsListeningGroup( SCCOL nCol, SCROW nRow );

    /// Returns list-of-spans representation of the column-widths/row-heights in twips encoded as an OString.
    OString dumpColumnRowSizes(bool bColumns);
    /// Returns list-of-spans representation of hidden/filtered states of columns/rows encoded as an OString.
    OString dumpHiddenFiltered(bool bColumns, bool bHidden);
    /// Returns list-of-spans representation of the column/row groupings encoded as an OString.
    OString dumpColumnRowGroups(bool bColumns) const;

    SCCOL GetLOKFreezeCol() const;
    SCROW GetLOKFreezeRow() const;
    bool  SetLOKFreezeCol(SCCOL nFreezeCol);
    bool  SetLOKFreezeRow(SCROW nFreezeRow);

    /**
     * Use this to iterate through non-empty visible cells in a single column.
     */
    class VisibleDataCellIterator
    {
        static constexpr SCROW ROW_NOT_FOUND = -1;

    public:
        explicit VisibleDataCellIterator(const ScDocument& rDoc, ScFlatBoolRowSegments& rRowSegs, ScColumn& rColumn);

        /**
         * Set the start row position.  In case there is not visible data cell
         * at the specified row position, it will move to the position of the
         * first visible data cell below that point.
         *
         * @return First visible data cell if found, or NULL otherwise.
         */
        ScRefCellValue reset(SCROW nRow);

        /**
         * Find the next visible data cell position.
         *
         * @return Next visible data cell if found, or NULL otherwise.
         */
        ScRefCellValue next();

        /**
         * Get the current row position.
         *
         * @return Current row position, or ROW_NOT_FOUND if the iterator
         *         doesn't point to a valid data cell position.
         */
        SCROW getRow() const { return mnCurRow;}

    private:
        const ScDocument& mrDocument;
        ScFlatBoolRowSegments& mrRowSegs;
        ScColumn& mrColumn;
        ScRefCellValue maCell;
        SCROW mnCurRow;
        SCROW mnUBound;
    };

    // Applies a function to the selected ranges; makes sure to only allocate
    // as few columns as needed, and applies the rest to default column data.
    // The function looks like
    //     ApplyDataFunc(ScColumnData& applyTo, SCROW nTop, SCROW nBottom)
    template <ColumnDataApply ApplyDataFunc>
    void ApplyWithAllocation(const ScMarkData&, ApplyDataFunc);

    // Applies a function to the selected ranges in a given column.
    template <ColumnDataApply ApplyDataFunc> void Apply(const ScMarkData&, SCCOL, ApplyDataFunc);
};

template <ColumnDataApply ApplyDataFunc>
void ScTable::ApplyWithAllocation(const ScMarkData& rMark, ApplyDataFunc apply)
{
    if (!rMark.GetTableSelect(nTab) || !(rMark.IsMultiMarked() || rMark.IsMarked()))
        return;
    SCCOL lastChangeCol;
    if (rMark.GetArea().aEnd.Col() == GetDoc().MaxCol())
    {
        // For the same unallocated columns until the end we can change just the default.
        lastChangeCol = rMark.GetStartOfEqualColumns(GetDoc().MaxCol(), aCol.size()) - 1;
        // Allocate needed different columns before changing the default.
        if (lastChangeCol >= 0)
            CreateColumnIfNotExists(lastChangeCol);

        Apply(rMark, GetDoc().MaxCol(), apply);
    }
    else // need to allocate all columns affected
    {
        lastChangeCol = rMark.GetArea().aEnd.Col();
        CreateColumnIfNotExists(lastChangeCol);
    }

    // The loop should go not to lastChangeCol, but over all columns, to apply to already allocated
    // in the "StartOfEqualColumns" range
    for (SCCOL i = 0; i < aCol.size(); i++)
        Apply(rMark, i, apply);
}

template <ColumnDataApply ApplyDataFunc>
void ScTable::Apply(const ScMarkData& rMark, SCCOL nCol, ApplyDataFunc apply)
{
    if (rMark.IsMultiMarked())
    {
        ScColumnData& rCol = GetColumnData(nCol);
        ScMultiSelIter aMultiIter(rMark.GetMultiSelData(), nCol);
        SCROW nTop, nBottom;
        while (aMultiIter.Next(nTop, nBottom))
            apply(rCol, nTop, nBottom);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
