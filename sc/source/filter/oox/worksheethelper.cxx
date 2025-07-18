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

#include <memory>
#include <utility>
#include <worksheethelper.hxx>

#include <algorithm>
#include <com/sun/star/awt/Point.hpp>
#include <com/sun/star/awt/Size.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/sheet/ConditionOperator2.hpp>
#include <com/sun/star/sheet/TableValidationVisibility.hpp>
#include <com/sun/star/sheet/ValidationType.hpp>
#include <com/sun/star/sheet/ValidationAlertStyle.hpp>
#include <com/sun/star/sheet/XCellAddressable.hpp>
#include <com/sun/star/sheet/XMultiFormulaTokens.hpp>
#include <com/sun/star/sheet/XSheetCellRangeContainer.hpp>
#include <com/sun/star/sheet/XSheetCondition2.hpp>
#include <com/sun/star/sheet/XSheetOutline.hpp>
#include <com/sun/star/sheet/XSpreadsheet.hpp>
#include <com/sun/star/table/XColumnRowRange.hpp>
#include <com/sun/star/text/WritingMode2.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <osl/diagnose.h>
#include <rtl/ustrbuf.hxx>
#include <oox/core/filterbase.hxx>
#include <oox/helper/propertyset.hxx>
#include <oox/token/properties.hxx>
#include <oox/token/tokens.hxx>
#include <addressconverter.hxx>
#include <autofilterbuffer.hxx>
#include <commentsbuffer.hxx>
#include <condformatbuffer.hxx>
#include <document.hxx>
#include <drawingfragment.hxx>
#include <pagesettings.hxx>
#include <querytablebuffer.hxx>
#include <sheetdatabuffer.hxx>
#include <stylesbuffer.hxx>
#include <tokenuno.hxx>
#include <unitconverter.hxx>
#include <viewsettings.hxx>
#include <worksheetbuffer.hxx>
#include <worksheetsettings.hxx>
#include <formulabuffer.hxx>
#include <scitems.hxx>
#include <editutil.hxx>
#include <tokenarray.hxx>
#include <table.hxx>
#include <tablebuffer.hxx>
#include <documentimport.hxx>
#include <stlsheet.hxx>
#include <stlpool.hxx>
#include <cellvalue.hxx>
#include <columnspanset.hxx>
#include <dbdata.hxx>
#include <cellsuno.hxx>
#include <fmtuno.hxx>

#include <svl/stritem.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/editobj.hxx>
#include <editeng/flditem.hxx>
#include <tools/gen.hxx>

namespace oox::xls {

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::sheet;
using namespace ::com::sun::star::table;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::uno;

namespace {

void lclUpdateProgressBar( const ISegmentProgressBarRef& rxProgressBar, double fPosition )
{
    if( rxProgressBar )
        rxProgressBar->setPosition( fPosition );
}

// TODO Needed because input might be >32-bit (in 64-bit builds),
//  or a negative, already overflown value (in 32-bit builds)
sal_Int32 lclClampToNonNegativeInt32( tools::Long aVal )
{
    if ( aVal > SAL_MAX_INT32 || aVal < 0 )
    {
        SAL_WARN( "sc.filter", "Overflow detected, " << aVal << " does not fit into sal_Int32, or is negative." );
        return SAL_MAX_INT32;
    }
    return static_cast<sal_Int32>( aVal );
}

} // namespace

ColumnModel::ColumnModel() :
    maRange( -1 ),
    mfWidth( 0.0 ),
    mnXfId( -1 ),
    mnLevel( 0 ),
    mbShowPhonetic( false ),
    mbHidden( false ),
    mbCollapsed( false )
{
}

bool ColumnModel::isMergeable( const ColumnModel& rModel ) const
{
    return
        (maRange.mnFirst        <= rModel.maRange.mnFirst) &&
        (rModel.maRange.mnFirst <= maRange.mnLast + 1) &&
        (mfWidth                == rModel.mfWidth) &&
        // ignore mnXfId, cell formatting is always set directly
        (mnLevel                == rModel.mnLevel) &&
        (mbHidden               == rModel.mbHidden) &&
        (mbCollapsed            == rModel.mbCollapsed);
}

RowModel::RowModel() :
    mnRow( -1 ),
    mfHeight( 0.0 ),
    mnXfId( -1 ),
    mnLevel( 0 ),
    mbCustomHeight( false ),
    mbCustomFormat( false ),
    mbShowPhonetic( false ),
    mbHidden( false ),
    mbCollapsed( false ),
    mbThickTop( false ),
    mbThickBottom( false )
{
}

bool RowModel::isMergeable( const RowModel& rModel ) const
{
    return
        // ignore maColSpans - is handled separately in SheetDataBuffer class
        (mfHeight       == rModel.mfHeight) &&
        // ignore mnXfId, mbCustomFormat, mbShowPhonetic - cell formatting is always set directly
        (mnLevel        == rModel.mnLevel) &&
        (mbCustomHeight == rModel.mbCustomHeight) &&
        (mbHidden       == rModel.mbHidden) &&
        (mbCollapsed    == rModel.mbCollapsed);
}

PageBreakModel::PageBreakModel()
    : mnColRow(0)
    , mnMin(0)
    , mnMax(0)
    , mbManual(false)
{
}

HyperlinkModel::HyperlinkModel()
{
}

ValidationModel::ValidationModel() :
    mnType( XML_none ),
    mnOperator( XML_between ),
    mnErrorStyle( XML_stop ),
    mbShowInputMsg( false ),
    mbShowErrorMsg( false ),
    mbNoDropDown( false ),
    mbAllowBlank( false )
{
}

void ValidationModel::setBiffType( sal_uInt8 nType )
{
    static const sal_Int32 spnTypeIds[] = {
        XML_none, XML_whole, XML_decimal, XML_list, XML_date, XML_time, XML_textLength, XML_custom };
    mnType = STATIC_ARRAY_SELECT( spnTypeIds, nType, XML_none );
}

void ValidationModel::setBiffOperator( sal_uInt8 nOperator )
{
    static const sal_Int32 spnOperators[] = {
        XML_between, XML_notBetween, XML_equal, XML_notEqual,
        XML_greaterThan, XML_lessThan, XML_greaterThanOrEqual, XML_lessThanOrEqual };
    mnOperator = STATIC_ARRAY_SELECT( spnOperators, nOperator, XML_TOKEN_INVALID );
}

void ValidationModel::setBiffErrorStyle( sal_uInt8 nErrorStyle )
{
    static const sal_Int32 spnErrorStyles[] = { XML_stop, XML_warning, XML_information };
    mnErrorStyle = STATIC_ARRAY_SELECT( spnErrorStyles, nErrorStyle, XML_stop );
}

class WorksheetGlobals : public WorkbookHelper, public IWorksheetProgress
{
public:
    explicit            WorksheetGlobals(
                            const WorkbookHelper& rHelper,
                            ISegmentProgressBarRef xProgressBar,
                            WorksheetType eSheetType,
                            SCTAB nSheet );

    /** Returns true, if this helper refers to an existing Calc sheet. */
    bool         isValidSheet() const { return mxSheet.is(); }

    /** Returns the type of this sheet. */
    WorksheetType getSheetType() const { return meSheetType; }
    /** Returns the index of the current sheet. */
    SCTAB        getSheetIndex() const { return maUsedArea.aStart.Tab(); }
    /** Returns the XSpreadsheet interface of the current sheet. */
    const Reference< XSpreadsheet >& getSheet() const { return mxSheet; }

    /** Returns the XCell interface for the passed cell address. */
    Reference< XCell >  getCell( const ScAddress& rAddress ) const;
    /** Returns the XSheetCellRanges interface for the passed cell range addresses. */
    rtl::Reference<ScCellRangesObj> getCellRangeList( const ScRangeList& rRanges ) const;

    /** Returns the XCellRange interface for a column. */
    Reference< XCellRange > getColumn( sal_Int32 nCol ) const;
    /** Returns the XCellRange interface for a row. */
    Reference< XCellRange > getRow( sal_Int32 nRow ) const;

    /** Returns the XDrawPage interface of the draw page of the current sheet. */
    Reference< XDrawPage > getDrawPage() const;
    /** Returns the size of the entire drawing page in 1/100 mm. */
    const awt::Size&         getDrawPageSize() const;

    /** Returns the absolute position of the top-left corner of the cell in 1/100 mm. */
    awt::Point               getCellPosition( sal_Int32 nCol, sal_Int32 nRow ) const;

    /** Returns the address of the cell that contains the passed point in 1/100 mm. */
    ScAddress                getCellAddressFromPosition( const awt::Point& rPosition ) const;
    /** Returns the cell range address that contains the passed rectangle in 1/100 mm. */
    ScRange                  getCellRangeFromRectangle( const awt::Rectangle& rRect ) const;

    /** Returns the buffer for cell contents and cell formatting. */
    SheetDataBuffer& getSheetData() { return maSheetData; }
    /** Returns the conditional formatting in this sheet. */
    CondFormatBuffer& getCondFormats() { return maCondFormats; }
    /** Returns the buffer for all cell comments in this sheet. */
    CommentsBuffer& getComments() { return maComments; }
    /** Returns the auto filters for the sheet. */
    AutoFilterBuffer& getAutoFilters() { return maAutoFilters; }
    /** Returns the buffer for all web query tables in this sheet. */
    QueryTableBuffer& getQueryTables() { return maQueryTables; }
    /** Returns the worksheet settings object. */
    WorksheetSettings& getWorksheetSettings() { return maSheetSett; }
    /** Returns the page/print settings for this sheet. */
    PageSettings& getPageSettings() { return maPageSett; }
    /** Returns the view settings for this sheet. */
    SheetViewSettings& getSheetViewSettings() { return maSheetViewSett; }
    /** Returns the VML drawing page for this sheet (OOXML/BIFF12 only). */
    VmlDrawing&  getVmlDrawing() { return *mxVmlDrawing; }
    /** returns the ExtLst entries that need to be filled */
    ExtLst&      getExtLst() { return maExtLst; }

    /** Sets a column or row page break described in the passed struct. */
    void                setPageBreak( const PageBreakModel& rModel, bool bRowBreak );
    /** Inserts the hyperlink URL into the spreadsheet. */
    void                setHyperlink( const HyperlinkModel& rModel );
    /** Inserts the data validation settings into the spreadsheet. */
    void                setValidation( const ValidationModel& rModel );
    /** Sets the path to the DrawingML fragment of this sheet. */
    void                setDrawingPath( const OUString& rDrawingPath );
    /** Sets the path to the legacy VML drawing fragment of this sheet. */
    void                setVmlDrawingPath( const OUString& rVmlDrawingPath );

    /** Extends the used area of this sheet by the passed cell position. */
    void                extendUsedArea( const ScAddress& rAddress );

    /** Extends the used area of this sheet by the passed cell range. */
    void                extendUsedArea( const ScRange& rRange );
    /** Extends the shape bounding box by the position and size of the passed rectangle. */
    void                extendShapeBoundingBox( const awt::Rectangle& rShapeRect );

    /** Sets base width for all columns (without padding pixels). This value
        is only used, if base width has not been set with setDefaultColumnWidth(). */
    void                setBaseColumnWidth( sal_Int32 nWidth );
    /** Sets default width for all columns. This function overrides the base
        width set with the setBaseColumnWidth() function. */
    void                setDefaultColumnWidth( double fWidth );
    /** Sets column settings for a specific column range.
        @descr  Column default formatting is converted directly, other settings
        are cached and converted in the finalizeImport() call. */
    void                setColumnModel( const ColumnModel& rModel );
    /** Converts column default cell formatting. */
    void convertColumnFormat( sal_Int32 nFirstCol, sal_Int32 nLastCol, sal_Int32 nXfId );

    /** Sets default height and hidden state for all unused rows in the sheet. */
    void                setDefaultRowSettings( double fHeight, bool bCustomHeight, bool bHidden, bool bThickTop, bool bThickBottom );
    /** Sets row settings for a specific row.
        @descr  Row default formatting is converted directly, other settings
        are cached and converted in the finalizeImport() call. */
    void                setRowModel( const RowModel& rModel );

    /** Initial conversion before importing the worksheet. */
    void                initializeWorksheetImport();
    /** Final conversion after importing the worksheet. */
    void                finalizeWorksheetImport();

    void finalizeDrawingImport();

    /// Allow the threaded importer to override our progress bar impl.
    virtual ISegmentProgressBarRef getRowProgress() override
    {
        return mxRowProgress;
    }
    virtual void setCustomRowProgress( const ISegmentProgressBarRef &rxRowProgress ) override
    {
        mxRowProgress = rxRowProgress;
        mbFastRowProgress = true;
    }

private:
    typedef ::std::vector< sal_Int32 >                  OutlineLevelVec;
    typedef ::std::pair< ColumnModel, sal_Int32 >       ColumnModelRange;
    typedef ::std::map< sal_Int32, ColumnModelRange >   ColumnModelRangeMap;
    typedef ::std::pair< RowModel, sal_Int32 >          RowModelRange;
    typedef ::std::map< sal_Int32, RowModelRange >      RowModelRangeMap;

    /** Inserts all imported hyperlinks into their cell ranges. */
    void finalizeHyperlinkRanges();
    /** Generates the final URL for the passed hyperlink. */
    OUString            getHyperlinkUrl( const HyperlinkModel& rHyperlink ) const;
    /** Inserts a hyperlinks into the specified cell. */
    void insertHyperlink( const ScAddress& rAddress, const OUString& rUrl );

    /** Inserts all imported data validations into their cell ranges. */
    void                finalizeValidationRanges() const;

    /** Converts column properties for all columns in the sheet. */
    void                convertColumns();
    /** Converts column properties. */
    void                convertColumns( OutlineLevelVec& orColLevels, const ValueRange& rColRange, const ColumnModel& rModel );

    /** Converts row properties for all rows in the sheet. */
    void                convertRows(const std::vector<sc::ColRowSpan>& rSpans);
    /** Converts row properties. */
    void                convertRows(OutlineLevelVec& orRowLevels, const ValueRange& rRowRange,
                                    const RowModel& rModel,
                                    const std::vector<sc::ColRowSpan>& rSpans,
                                    double fDefHeight = -1.0);

    /** Converts outline grouping for the passed column or row. */
    void                convertOutlines( OutlineLevelVec& orLevels, sal_Int32 nColRow, sal_Int32 nLevel, bool bCollapsed, bool bRows );
    /** Groups columns or rows for the given range. */
    void                groupColumnsOrRows( sal_Int32 nFirstColRow, sal_Int32 nLastColRow, bool bCollapsed, bool bRows );

    /** Imports the drawings of the sheet (DML, VML, DFF) and updates the used area. */
    void                finalizeDrawings();

    /** Update the row import progress bar */
    void UpdateRowProgress( const ScRange& rUsedArea, SCROW nRow );

private:
    typedef ::std::unique_ptr< VmlDrawing >       VmlDrawingPtr;

    const ScAddress&    mrMaxApiPos;        /// Reference to maximum Calc cell address from address converter.
    ScRange             maUsedArea;         /// Used area of the sheet, and sheet index of the sheet.
    ColumnModel         maDefColModel;      /// Default column formatting.
    ColumnModelRangeMap maColModels;        /// Ranges of columns sorted by first column index.
    RowModel            maDefRowModel;      /// Default row formatting.
    RowModelRangeMap    maRowModels;        /// Ranges of rows sorted by first row index.
    std::vector< HyperlinkModel > maHyperlinks;       /// Cell ranges containing hyperlinks.
    std::vector< ValidationModel > maValidations;      /// Cell ranges containing data validation settings.
    SheetDataBuffer     maSheetData;        /// Buffer for cell contents and cell formatting.
    CondFormatBuffer    maCondFormats;      /// Buffer for conditional formatting.
    CommentsBuffer      maComments;         /// Buffer for all cell comments in this sheet.
    AutoFilterBuffer    maAutoFilters;      /// Sheet auto filters (not associated to a table).
    QueryTableBuffer    maQueryTables;      /// Buffer for all web query tables in this sheet.
    WorksheetSettings   maSheetSett;        /// Global settings for this sheet.
    PageSettings        maPageSett;         /// Page/print settings for this sheet.
    SheetViewSettings   maSheetViewSett;    /// View settings for this sheet.
    VmlDrawingPtr       mxVmlDrawing;       /// Collection of all VML shapes.
    ExtLst              maExtLst;           /// List of extended elements
    OUString            maDrawingPath;      /// Path to DrawingML fragment.
    OUString            maVmlDrawingPath;   /// Path to legacy VML drawing fragment.
    awt::Size                maDrawPageSize;     /// Current size of the drawing page in 1/100 mm.
    awt::Rectangle           maShapeBoundingBox; /// Bounding box for all shapes from all drawings.
    ISegmentProgressBarRef mxProgressBar;   /// Sheet progress bar.
    bool                   mbFastRowProgress; /// Do we have a progress bar thread ?
    ISegmentProgressBarRef mxRowProgress;   /// Progress bar for row/cell processing.
    ISegmentProgressBarRef mxFinalProgress; /// Progress bar for finalization.
    WorksheetType       meSheetType;        /// Type of this sheet.
    Reference< XSpreadsheet > mxSheet;      /// Reference to the current sheet.
    bool                mbHasDefWidth;      /// True = default column width is set from defaultColWidth attribute.
};

WorksheetGlobals::WorksheetGlobals( const WorkbookHelper& rHelper, ISegmentProgressBarRef xProgressBar, WorksheetType eSheetType, SCTAB nSheet ) :
    WorkbookHelper( rHelper ),
    mrMaxApiPos( rHelper.getAddressConverter().getMaxApiAddress() ),
    maUsedArea( SCCOL_MAX, SCROW_MAX, nSheet, -1, -1, nSheet ), // Set start address to largest possible value, and end address to smallest
    maSheetData( *this ),
    maCondFormats( *this ),
    maComments( *this ),
    maAutoFilters( *this ),
    maQueryTables( *this ),
    maSheetSett( *this ),
    maPageSett( *this ),
    maSheetViewSett( *this ),
    mxProgressBar(std::move( xProgressBar )),
    mbFastRowProgress( false ),
    meSheetType( eSheetType ),
    mxSheet(getSheetFromDoc( nSheet )),
    mbHasDefWidth( false )
{
    if( !mxSheet.is() )
        maUsedArea.aStart.SetTab( -1 );

    // default column settings (width and hidden state may be updated later)
    maDefColModel.mfWidth = 8.5;
    maDefColModel.mnXfId = -1;
    maDefColModel.mnLevel = 0;
    maDefColModel.mbHidden = false;
    maDefColModel.mbCollapsed = false;

    // default row settings (height and hidden state may be updated later)
    maDefRowModel.mfHeight = 0.0;
    maDefRowModel.mnXfId = -1;
    maDefRowModel.mnLevel = 0;
    maDefRowModel.mbCustomHeight = false;
    maDefRowModel.mbCustomFormat = false;
    maDefRowModel.mbShowPhonetic = false;
    maDefRowModel.mbHidden = false;
    maDefRowModel.mbCollapsed = false;

    // buffers
    mxVmlDrawing.reset( new VmlDrawing( *this ) );

    // prepare progress bars
    if( mxProgressBar )
    {
        mxRowProgress = mxProgressBar->createSegment( 0.5 );
        mxFinalProgress = mxProgressBar->createSegment( 0.5 );
    }
}

Reference< XCell > WorksheetGlobals::getCell( const ScAddress& rAddress ) const
{
    Reference< XCell > xCell;
    if( mxSheet.is() ) try
    {
        xCell = mxSheet->getCellByPosition( rAddress.Col(), rAddress.Row() );
    }
    catch( Exception& )
    {
    }
    return xCell;
}

rtl::Reference<ScCellRangesObj> WorksheetGlobals::getCellRangeList( const ScRangeList& rRanges ) const
{
    rtl::Reference<ScCellRangesObj> xRanges;
    if( mxSheet.is() && !rRanges.empty() )
    {
        ScDocShell* pDocShell = getScDocument().GetDocumentShell();
        xRanges = new ScCellRangesObj(pDocShell, ScRangeList());
        xRanges->addRangeAddresses( rRanges, false );
    }
    return xRanges;
}

Reference< XCellRange > WorksheetGlobals::getColumn( sal_Int32 nCol ) const
{
    Reference< XCellRange > xColumn;
    try
    {
        Reference< XColumnRowRange > xColRowRange( mxSheet, UNO_QUERY_THROW );
        Reference< XTableColumns > xColumns( xColRowRange->getColumns(), UNO_SET_THROW );
        xColumn.set( xColumns->getByIndex( nCol ), UNO_QUERY );
    }
    catch( Exception& )
    {
    }
    return xColumn;
}

Reference< XCellRange > WorksheetGlobals::getRow( sal_Int32 nRow ) const
{
    Reference< XCellRange > xRow;
    try
    {
        Reference< XColumnRowRange > xColRowRange( mxSheet, UNO_QUERY_THROW );
        Reference< XTableRows > xRows( xColRowRange->getRows(), UNO_SET_THROW );
        xRow.set( xRows->getByIndex( nRow ), UNO_QUERY );
    }
    catch( Exception& )
    {
    }
    return xRow;
}

Reference< XDrawPage > WorksheetGlobals::getDrawPage() const
{
    Reference< XDrawPage > xDrawPage;
    try
    {
        xDrawPage = Reference< XDrawPageSupplier >( mxSheet, UNO_QUERY_THROW )->getDrawPage();
    }
    catch( Exception& )
    {
    }
    return xDrawPage;
}

const awt::Size& WorksheetGlobals::getDrawPageSize() const
{
    OSL_ENSURE( (maDrawPageSize.Width > 0) && (maDrawPageSize.Height > 0), "WorksheetGlobals::getDrawPageSize - called too early, size invalid" );
    return maDrawPageSize;
}

awt::Point WorksheetGlobals::getCellPosition( sal_Int32 nCol, sal_Int32 nRow ) const
{
    const tools::Rectangle aMMRect( getScDocument().GetMMRect( nCol, nRow, nCol, nRow, getSheetIndex() ) );
    awt::Point aPoint( lclClampToNonNegativeInt32( aMMRect.Left() ),
                       lclClampToNonNegativeInt32( aMMRect.Top() ) );
    return aPoint;
}

namespace {

sal_Int32 lclGetMidAddr( sal_Int32 nBegAddr, sal_Int32 nEndAddr, sal_Int32 nBegPos, sal_Int32 nEndPos, sal_Int32 nSearchPos )
{
    // use sal_Int64 to prevent integer overflow
    return nBegAddr + 1 + static_cast< sal_Int32 >( static_cast< sal_Int64 >( nEndAddr - nBegAddr - 2 ) * (nSearchPos - nBegPos) / (nEndPos - nBegPos) );
}

bool lclPrepareInterval( sal_Int32 nBegAddr, sal_Int32& rnMidAddr, sal_Int32 nEndAddr,
        sal_Int32 nBegPos, sal_Int32 nEndPos, sal_Int32 nSearchPos )
{
    // searched position before nBegPos -> use nBegAddr
    if( nSearchPos <= nBegPos )
    {
        rnMidAddr = nBegAddr;
        return false;
    }

    // searched position after nEndPos, or begin next to end -> use nEndAddr
    if( (nSearchPos >= nEndPos) || (nBegAddr + 1 >= nEndAddr) )
    {
        rnMidAddr = nEndAddr;
        return false;
    }

    /*  Otherwise find mid address according to position. lclGetMidAddr() will
        return an address between nBegAddr and nEndAddr. */
    rnMidAddr = lclGetMidAddr( nBegAddr, nEndAddr, nBegPos, nEndPos, nSearchPos );
    return true;
}

bool lclUpdateInterval( sal_Int32& rnBegAddr, sal_Int32& rnMidAddr, sal_Int32& rnEndAddr,
        sal_Int32& rnBegPos, sal_Int32 nMidPos, sal_Int32& rnEndPos, sal_Int32 nSearchPos )
{
    // nSearchPos < nMidPos: use the interval [begin,mid] in the next iteration
    if( nSearchPos < nMidPos )
    {
        // if rnBegAddr is next to rnMidAddr, the latter is the column/row in question
        if( rnBegAddr + 1 >= rnMidAddr )
            return false;
        // otherwise, set interval end to mid
        rnEndPos = nMidPos;
        rnEndAddr = rnMidAddr;
        rnMidAddr = lclGetMidAddr( rnBegAddr, rnEndAddr, rnBegPos, rnEndPos, nSearchPos );
        return true;
    }

    // nSearchPos > nMidPos: use the interval [mid,end] in the next iteration
    if( nSearchPos > nMidPos )
    {
        // if rnMidAddr is next to rnEndAddr, the latter is the column/row in question
        if( rnMidAddr + 1 >= rnEndAddr )
        {
            rnMidAddr = rnEndAddr;
            return false;
        }
        // otherwise, set interval start to mid
        rnBegPos = nMidPos;
        rnBegAddr = rnMidAddr;
        rnMidAddr = lclGetMidAddr( rnBegAddr, rnEndAddr, rnBegPos, rnEndPos, nSearchPos );
        return true;
    }

    // nSearchPos == nMidPos: rnMidAddr is the column/row in question, do not loop anymore
    return false;
}

} // namespace

ScAddress WorksheetGlobals::getCellAddressFromPosition( const awt::Point& rPosition ) const
{
    // starting cell address and its position in drawing layer (top-left edge)
    sal_Int32 nBegCol = 0;
    sal_Int32 nBegRow = 0;
    awt::Point aBegPos( 0, 0 );

    // end cell address and its position in drawing layer (bottom-right edge)
    sal_Int32 nEndCol = mrMaxApiPos.Col() + 1;
    sal_Int32 nEndRow = mrMaxApiPos.Row() + 1;
    awt::Point aEndPos( maDrawPageSize.Width, maDrawPageSize.Height );

    // starting point for interval search
    sal_Int32 nMidCol, nMidRow;
    bool bLoopCols = lclPrepareInterval( nBegCol, nMidCol, nEndCol, aBegPos.X, aEndPos.X, rPosition.X );
    bool bLoopRows = lclPrepareInterval( nBegRow, nMidRow, nEndRow, aBegPos.Y, aEndPos.Y, rPosition.Y );
    awt::Point aMidPos = getCellPosition( nMidCol, nMidRow );

    /*  The loop will find the column/row index of the cell right of/below
        the cell containing the passed point, unless the point is located at
        the top or left border of the containing cell. */
    while( bLoopCols || bLoopRows )
    {
        bLoopCols = bLoopCols && lclUpdateInterval( nBegCol, nMidCol, nEndCol, aBegPos.X, aMidPos.X, aEndPos.X, rPosition.X );
        bLoopRows = bLoopRows && lclUpdateInterval( nBegRow, nMidRow, nEndRow, aBegPos.Y, aMidPos.Y, aEndPos.Y, rPosition.Y );
        aMidPos = getCellPosition( nMidCol, nMidRow );
    }

    /*  The cell left of/above the current search position contains the passed
        point, unless the point is located on the top/left border of the cell,
        or the last column/row of the sheet has been reached. */
    if( aMidPos.X > rPosition.X ) --nMidCol;
    if( aMidPos.Y > rPosition.Y ) --nMidRow;
    return ScAddress( nMidCol, nMidRow, getSheetIndex() );
}

ScRange WorksheetGlobals::getCellRangeFromRectangle( const awt::Rectangle& rRect ) const
{
    ScAddress aStartAddr = getCellAddressFromPosition( awt::Point( rRect.X, rRect.Y ) );
    awt::Point aBotRight( rRect.X + rRect.Width, rRect.Y + rRect.Height );
    ScAddress aEndAddr = getCellAddressFromPosition( aBotRight );
    bool bMultiCols = aStartAddr.Col() < aEndAddr.Col();
    bool bMultiRows = aStartAddr.Row() < aEndAddr.Row();
    if( bMultiCols || bMultiRows )
    {
        /*  Reduce end position of the cell range to previous column or row, if
            the rectangle ends exactly between two columns or rows. */
        awt::Point aEndPos = getCellPosition( aEndAddr.Col(), aEndAddr.Row() );
        if( bMultiCols && (aBotRight.X <= aEndPos.X) )
            aEndAddr.IncCol(-1);
        if( bMultiRows && (aBotRight.Y <= aEndPos.Y) )
            aEndAddr.IncRow(-1);
    }
    return ScRange( aStartAddr.Col(), aStartAddr.Row(), getSheetIndex(),
                    aEndAddr.Col(), aEndAddr.Row(), getSheetIndex() );
}

void WorksheetGlobals::setPageBreak( const PageBreakModel& rModel, bool bRowBreak )
{
    if( rModel.mbManual && (rModel.mnColRow > 0) )
    {
        PropertySet aPropSet( bRowBreak ? getRow( rModel.mnColRow ) : getColumn( rModel.mnColRow ) );
        aPropSet.setProperty( PROP_IsStartOfNewPage, true );
    }
}

void WorksheetGlobals::setHyperlink( const HyperlinkModel& rModel )
{
    maHyperlinks.push_back( rModel );
}

void WorksheetGlobals::setValidation( const ValidationModel& rModel )
{
    maValidations.push_back( rModel );
}

void WorksheetGlobals::setDrawingPath( const OUString& rDrawingPath )
{
    maDrawingPath = rDrawingPath;
}

void WorksheetGlobals::setVmlDrawingPath( const OUString& rVmlDrawingPath )
{
    maVmlDrawingPath = rVmlDrawingPath;
}

void WorksheetGlobals::extendUsedArea( const ScAddress& rAddress )
{
    maUsedArea.aStart.SetCol( ::std::min( maUsedArea.aStart.Col(), rAddress.Col() ) );
    maUsedArea.aStart.SetRow( ::std::min( maUsedArea.aStart.Row(), rAddress.Row() ) );
    maUsedArea.aEnd.SetCol( ::std::max( maUsedArea.aEnd.Col(), rAddress.Col() ) );
    maUsedArea.aEnd.SetRow( ::std::max( maUsedArea.aEnd.Row(), rAddress.Row() ) );
}

void WorksheetGlobals::extendUsedArea( const ScRange& rRange )
{
    extendUsedArea( rRange.aStart );
    extendUsedArea( rRange.aEnd );
}

void WorksheetHelper::extendUsedArea( const ScRange& rRange )
{
    extendUsedArea( rRange.aStart );
    extendUsedArea( rRange.aEnd );
}

void WorksheetGlobals::extendShapeBoundingBox( const awt::Rectangle& rShapeRect )
{
    if( (maShapeBoundingBox.Width == 0) && (maShapeBoundingBox.Height == 0) )
    {
        // width and height of maShapeBoundingBox are assumed to be zero on first cell
        maShapeBoundingBox = rShapeRect;
    }
    else
    {
        sal_Int32 nEndX = ::std::max( maShapeBoundingBox.X + maShapeBoundingBox.Width, rShapeRect.X + rShapeRect.Width );
        sal_Int32 nEndY = ::std::max( maShapeBoundingBox.Y + maShapeBoundingBox.Height, rShapeRect.Y + rShapeRect.Height );
        maShapeBoundingBox.X = ::std::min( maShapeBoundingBox.X, rShapeRect.X );
        maShapeBoundingBox.Y = ::std::min( maShapeBoundingBox.Y, rShapeRect.Y );
        maShapeBoundingBox.Width = nEndX - maShapeBoundingBox.X;
        maShapeBoundingBox.Height = nEndY - maShapeBoundingBox.Y;
    }
}

void WorksheetGlobals::setBaseColumnWidth( sal_Int32 nWidth )
{
    // do not modify width, if setDefaultColumnWidth() has been used
    if( !mbHasDefWidth && (nWidth > 0) )
    {
        // #i3006# add 5 pixels padding to the width
        maDefColModel.mfWidth = nWidth + getUnitConverter().scaleValue( 5, Unit::ScreenX, Unit::Digit );
    }
}

void WorksheetGlobals::setDefaultColumnWidth( double fWidth )
{
    // overrides a width set with setBaseColumnWidth()
    if( fWidth > 0.0 )
    {
        maDefColModel.mfWidth = fWidth;
        mbHasDefWidth = true;
    }
}

void WorksheetGlobals::setColumnModel( const ColumnModel& rModel )
{
    // convert 1-based OOXML column indexes to 0-based API column indexes
    sal_Int32 nFirstCol = rModel.maRange.mnFirst - 1;
    sal_Int32 nLastCol = rModel.maRange.mnLast - 1;
    if( !(getAddressConverter().checkCol( nFirstCol, true ) && (nFirstCol <= nLastCol)) )
        return;

    // Validate last column index.
    // If last column is equal to last possible column, Excel adds one
    // more. We do that also in XclExpColinfo::SaveXml() and for 1024 end
    // up with 1025 instead, which would lead to excess columns in
    // checkCol(). Cater for this oddity.
    if (nLastCol == mrMaxApiPos.Col() + 1)
        --nLastCol;
    // This is totally fouled up. If we saved 1025 and the file is saved
    // with Excel again, it increments the value to 1026.
    else if (nLastCol == mrMaxApiPos.Col() + 2)
        nLastCol -= 2;
    // Excel may add a column range for the remaining columns (with
    // <cols><col .../></cols>), even if not used or only used to grey out
    // columns in page break view. Don't let that trigger overflow warning,
    // so check for the last possible column. If there really is content in
    // the range that should be caught anyway.
    else if (nLastCol == getAddressConverter().getMaxXlsAddress().Col())
        nLastCol = mrMaxApiPos.Col();
    // User may have applied custom column widths to arbitrary excess
    // columns. Ignore those and don't track as overflow columns (false).
    // Effectively this does the same as the above cases, just keep them
    // for explanation.
    // Actual data present should trigger the overflow detection later.
    else if( !getAddressConverter().checkCol( nLastCol, false ) )
        nLastCol = mrMaxApiPos.Col();
    // try to find entry in column model map that is able to merge with the passed model
    bool bInsertModel = true;
    if( !maColModels.empty() )
    {
        // find first column model range following nFirstCol (nFirstCol < aIt->first), or end of map
        ColumnModelRangeMap::iterator aIt = maColModels.upper_bound( nFirstCol );
        OSL_ENSURE( aIt == maColModels.end(), "WorksheetGlobals::setColModel - columns are unsorted" );
        // if inserting before another column model, get last free column
        OSL_ENSURE( (aIt == maColModels.end()) || (nLastCol < aIt->first), "WorksheetGlobals::setColModel - multiple models of the same column" );
        if( aIt != maColModels.end() )
            nLastCol = ::std::min( nLastCol, aIt->first - 1 );
        if( aIt != maColModels.begin() )
        {
            // go to previous map element (which may be able to merge with the passed model)
            --aIt;
            // the usage of upper_bound() above ensures that aIt->first is less than or equal to nFirstCol now
            sal_Int32& rnLastMapCol = aIt->second.second;
            OSL_ENSURE( rnLastMapCol < nFirstCol, "WorksheetGlobals::setColModel - multiple models of the same column" );
            nFirstCol = ::std::max( rnLastMapCol + 1, nFirstCol );
            if( (rnLastMapCol + 1 == nFirstCol) && (nFirstCol <= nLastCol) && aIt->second.first.isMergeable( rModel ) )
            {
                // can merge with existing model, update last column index
                rnLastMapCol = nLastCol;
                bInsertModel = false;
            }
        }
    }
    if( nFirstCol <= nLastCol )
    {
        // insert the column model, if it has not been merged with another
        if( bInsertModel )
            maColModels[ nFirstCol ] = ColumnModelRange( rModel, nLastCol );
        // set column formatting directly
        convertColumnFormat( nFirstCol, nLastCol, rModel.mnXfId );
    }
}

void WorksheetGlobals::convertColumnFormat( sal_Int32 nFirstCol, sal_Int32 nLastCol, sal_Int32 nXfId )
{
    ScRange aRange( nFirstCol, 0, getSheetIndex(), nLastCol, mrMaxApiPos.Row(), getSheetIndex() );
    if( getAddressConverter().validateCellRange( aRange, true, false ) )
    {
        const StylesBuffer& rStyles = getStyles();

        // Set cell styles via direct API - the preferred approach.
        ScDocumentImport& rDoc = getDocImport();
        rStyles.writeCellXfToDoc(rDoc, aRange, nXfId);
    }
}

void WorksheetGlobals::setDefaultRowSettings( double fHeight, bool bCustomHeight, bool bHidden, bool bThickTop, bool bThickBottom )
{
    maDefRowModel.mfHeight = fHeight;
    maDefRowModel.mbCustomHeight = bCustomHeight;
    maDefRowModel.mbHidden = bHidden;
    maDefRowModel.mbThickTop = bThickTop;
    maDefRowModel.mbThickBottom = bThickBottom;
}

void WorksheetGlobals::setRowModel( const RowModel& rModel )
{
    // convert 1-based OOXML row index to 0-based API row index
    sal_Int32 nRow = rModel.mnRow - 1;
    if( getAddressConverter().checkRow( nRow, true ) )
    {
        // try to find entry in row model map that is able to merge with the passed model
        bool bInsertModel = true;
        bool bUnusedRow = true;
        if( !maRowModels.empty() )
        {
            // find first row model range following nRow (nRow < aIt->first), or end of map
            RowModelRangeMap::iterator aIt = maRowModels.upper_bound( nRow );
            OSL_ENSURE( aIt == maRowModels.end(), "WorksheetGlobals::setRowModel - rows are unsorted" );
            if( aIt != maRowModels.begin() )
            {
                // go to previous map element (which may be able to merge with the passed model)
                --aIt;
                // the usage of upper_bound() above ensures that aIt->first is less than or equal to nRow now
                sal_Int32& rnLastMapRow = aIt->second.second;
                bUnusedRow = rnLastMapRow < nRow;
                OSL_ENSURE( bUnusedRow, "WorksheetGlobals::setRowModel - multiple models of the same row" );
                if( (rnLastMapRow + 1 == nRow) && aIt->second.first.isMergeable( rModel ) )
                {
                    // can merge with existing model, update last row index
                    ++rnLastMapRow;
                    bInsertModel = false;
                }
            }
        }
        if( bUnusedRow )
        {
            // insert the row model, if it has not been merged with another
            if( bInsertModel )
                maRowModels[ nRow ] = RowModelRange( rModel, nRow );
            // set row formatting
            maSheetData.setRowFormat( nRow, rModel.mnXfId, rModel.mbCustomFormat );
        }
    }

    UpdateRowProgress( maUsedArea, nRow );
}

// This is called at a higher frequency inside the (threaded) inner loop.
void WorksheetGlobals::UpdateRowProgress( const ScRange& rUsedArea, SCROW nRow )
{
    if (!mxRowProgress || nRow < rUsedArea.aStart.Row() || rUsedArea.aEnd.Row() < nRow)
        return;

    double fNewPos = static_cast<double>(nRow - rUsedArea.aStart.Row() + 1.0) / (rUsedArea.aEnd.Row() - rUsedArea.aStart.Row() + 1.0);

    if (mbFastRowProgress)
        mxRowProgress->setPosition(fNewPos);
    else
    {
        double fCurPos = mxRowProgress->getPosition();
        if (fCurPos < fNewPos && (fNewPos - fCurPos) > 0.3)
            // Try not to re-draw progress bar too frequently.
            mxRowProgress->setPosition(fNewPos);
    }
}

void WorksheetGlobals::initializeWorksheetImport()
{
    // set default cell style for unused cells
    ScDocumentImport& rDoc = getDocImport();

    ScStyleSheet* pStyleSheet =
        static_cast<ScStyleSheet*>(rDoc.getDoc().GetStyleSheetPool()->Find(
            getStyles().getDefaultStyleName(), SfxStyleFamily::Para));

    if (pStyleSheet)
        rDoc.setCellStyleToSheet(getSheetIndex(), *pStyleSheet);

    /*  Remember the current sheet index in global data, needed by global
        objects, e.g. the chart converter. */
    setCurrentSheetIndex( getSheetIndex() );
}

void WorksheetGlobals::finalizeWorksheetImport()
{
    lclUpdateProgressBar( mxRowProgress, 1.0 );
    maSheetData.finalizeImport();

    getCondFormats().finalizeImport();
    lclUpdateProgressBar( mxFinalProgress, 0.25 );
    finalizeHyperlinkRanges();
    finalizeValidationRanges();
    maAutoFilters.finalizeImport( getSheetIndex() );
    maQueryTables.finalizeImport();
    maSheetSett.finalizeImport();
    maPageSett.finalizeImport();
    maSheetViewSett.finalizeImport();

    lclUpdateProgressBar( mxFinalProgress, 0.5 );
    convertColumns();

    // tdf#99913 rows hidden by filter need extra flag
    ScDocument& rDoc = getScDocument();
    std::vector<sc::ColRowSpan> aSpans;
    SCTAB nTab = getSheetIndex();

    ScTable* pTable = rDoc.FetchTable(nTab);
    if (pTable)
        pTable->SetOptimalMinRowHeight(maDefRowModel.mfHeight * 20); // in TWIPS

    ScDBData* pDBData = rDoc.GetAnonymousDBData(nTab);
    if (pDBData && pDBData->HasAutoFilter())
    {
        ScRange aRange;
        pDBData->GetArea(aRange);
        SCCOLROW nStartRow = static_cast<SCCOLROW>(aRange.aStart.Row());
        SCCOLROW nEndRow = static_cast<SCCOLROW>(aRange.aEnd.Row());
        aSpans.push_back(sc::ColRowSpan(nStartRow, nEndRow));
    }
    ScDBCollection* pDocColl = rDoc.GetDBCollection();
    if (!pDocColl->empty())
    {
        ScDBCollection::NamedDBs& rDBs = pDocColl->getNamedDBs();
        for (const auto& rxDB : rDBs)
        {
            if (rxDB->GetTab() == nTab && rxDB->HasAutoFilter())
            {
                ScRange aRange;
                rxDB->GetArea(aRange);
                SCCOLROW nStartRow = static_cast<SCCOLROW>(aRange.aStart.Row());
                SCCOLROW nEndRow = static_cast<SCCOLROW>(aRange.aEnd.Row());
                aSpans.push_back(sc::ColRowSpan(nStartRow, nEndRow));
            }
        }
    }
    convertRows(aSpans);
    lclUpdateProgressBar( mxFinalProgress, 1.0 );
}

void WorksheetGlobals::finalizeDrawingImport()
{
    finalizeDrawings();

    // forget current sheet index in global data
    setCurrentSheetIndex( -1 );
}

// private --------------------------------------------------------------------

void WorksheetGlobals::finalizeHyperlinkRanges()
{
    for (auto const& link : maHyperlinks)
    {
        OUString aUrl = getHyperlinkUrl(link);
        // try to insert URL into each cell of the range
        if( !aUrl.isEmpty() )
            for( ScAddress aAddress(link.maRange.aStart.Col(), link.maRange.aStart.Row(), getSheetIndex() ); aAddress.Row() <= link.maRange.aEnd.Row(); aAddress.IncRow() )
                for( aAddress.SetCol(link.maRange.aStart.Col()); aAddress.Col() <= link.maRange.aEnd.Col(); aAddress.IncCol() )
                    insertHyperlink( aAddress, aUrl );
    }
}

OUString WorksheetGlobals::getHyperlinkUrl( const HyperlinkModel& rHyperlink ) const
{
    OUStringBuffer aUrlBuffer;
    if( !rHyperlink.maTarget.isEmpty() )
        aUrlBuffer.append( getBaseFilter().getAbsoluteUrl( rHyperlink.maTarget ) );
    if( !rHyperlink.maLocation.isEmpty() )
        aUrlBuffer.append( "#" + rHyperlink.maLocation );
    OUString aUrl = aUrlBuffer.makeStringAndClear();

    if( aUrl.startsWith("#") )
    {
        sal_Int32 nSepPos = aUrl.lastIndexOf( '!' );
        if( nSepPos > 0 )
        {
            // Do not attempt to blindly convert '#SheetName!A1' to
            // '#SheetName.A1', it can be #SheetName!R1C1 as well. Hyperlink
            // handler has to handle all, but prefer '#SheetName.A1' if
            // possible.
            if (nSepPos < aUrl.getLength() - 1)
            {
                ScRange aRange;
                const ScDocumentImport& rDoc = getDocImport();
                if ((aRange.ParseAny( aUrl.copy( nSepPos + 1 ), rDoc.getDoc(),
                                formula::FormulaGrammar::CONV_XL_R1C1)
                      & ScRefFlags::VALID) == ScRefFlags::ZERO)
                    aUrl = aUrl.replaceAt( nSepPos, 1, rtl::OUStringChar( '.' ) );
            }
            // #i66592# convert sheet names that have been renamed on import
            OUString aSheetName = aUrl.copy( 1, nSepPos - 1 );
            OUString aCalcName = getWorksheets().getCalcSheetName( aSheetName );
            if( !aCalcName.isEmpty() )
                aUrl = aUrl.replaceAt( 1, nSepPos - 1, aCalcName );
        }
    }

    return aUrl;
}

void WorksheetGlobals::insertHyperlink( const ScAddress& rAddress, const OUString& rUrl )
{
    ScDocumentImport& rDoc = getDocImport();
    ScRefCellValue aCell(rDoc.getDoc(), rAddress);

    if (aCell.getType() == CELLTYPE_STRING || aCell.getType() == CELLTYPE_EDIT)
    {
        OUString aStr = aCell.getString(rDoc.getDoc());
        ScFieldEditEngine& rEE = rDoc.getDoc().GetEditEngine();
        rEE.Clear();

        SvxURLField aURLField(rUrl, aStr, SvxURLFormat::Repr);
        SvxFieldItem aURLItem(aURLField, EE_FEATURE_FIELD);
        rEE.QuickInsertField(aURLItem, ESelection());

        rDoc.setEditCell(rAddress, rEE.CreateTextObject());
    }
    else
    {
        // Handle other cell types e.g. formulas ( and ? ) that have associated
        // hyperlinks.
        // Ideally all hyperlinks should be treated  as below. For the moment,
        // given the current absence of ods support let's just handle what we
        // previously didn't handle the new way.
        // Unfortunately we won't be able to preserve such hyperlinks when
        // saving to ods. Note: when we are able to save such hyperlinks to ods
        // we should handle *all* imported hyperlinks as below ( e.g. as cell
        // attribute ) for better interoperability.

        SfxStringItem aItem(ATTR_HYPERLINK, rUrl);
        rDoc.getDoc().ApplyAttr(rAddress.Col(), rAddress.Row(), rAddress.Tab(), aItem);
    }
}

void WorksheetGlobals::finalizeValidationRanges() const
{
    for (auto const& validation : maValidations)
    {
        rtl::Reference<ScCellRangesObj> xRanges = getCellRangeList(validation.maRanges);
        if (!xRanges)
            continue;
        rtl::Reference<ScTableValidationObj> xValidation = xRanges->getValidation();

        if( xValidation.is() )
        {
            PropertySet aValProps(( Reference< XPropertySet >(xValidation) ));

            try
            {
                const OUString aToken = validation.msRef.getToken( 0, ' ' );

                rtl::Reference<ScTableSheetObj> xSheet = getSheetFromDoc( getCurrentSheetIndex() );
                rtl::Reference<ScCellRangeObj> xDBCellRange = xSheet->getScCellRangeByName( aToken );

                rtl::Reference<ScCellObj> xCell = xDBCellRange->getScCellByPosition( 0, 0 );
                CellAddress aFirstCell = xCell->getCellAddress();
                xValidation->setSourcePosition( aFirstCell );
            }
            catch(const Exception&)
            {
            }

            // convert validation type to API enum
            ValidationType eType = ValidationType_ANY;
            switch( validation.mnType )
            {
                case XML_custom:        eType = ValidationType_CUSTOM;      break;
                case XML_date:          eType = ValidationType_DATE;        break;
                case XML_decimal:       eType = ValidationType_DECIMAL;     break;
                case XML_list:          eType = ValidationType_LIST;        break;
                case XML_none:          eType = ValidationType_ANY;         break;
                case XML_textLength:    eType = ValidationType_TEXT_LEN;    break;
                case XML_time:          eType = ValidationType_TIME;        break;
                case XML_whole:         eType = ValidationType_WHOLE;       break;
                default:    OSL_FAIL( "WorksheetData::finalizeValidationRanges - unknown validation type" );
            }
            aValProps.setProperty( PROP_Type, eType );

            // convert error alert style to API enum
            ValidationAlertStyle eAlertStyle = ValidationAlertStyle_STOP;
            switch( validation.mnErrorStyle )
            {
                case XML_information:   eAlertStyle = ValidationAlertStyle_INFO;    break;
                case XML_stop:          eAlertStyle = ValidationAlertStyle_STOP;    break;
                case XML_warning:       eAlertStyle = ValidationAlertStyle_WARNING; break;
                default:    OSL_FAIL( "WorksheetData::finalizeValidationRanges - unknown error style" );
            }
            aValProps.setProperty( PROP_ErrorAlertStyle, eAlertStyle );

            // convert dropdown style to API visibility constants
            sal_Int16 nVisibility = validation.mbNoDropDown ? TableValidationVisibility::INVISIBLE : TableValidationVisibility::UNSORTED;
            aValProps.setProperty( PROP_ShowList, nVisibility );

            // messages
            aValProps.setProperty( PROP_ShowInputMessage, validation.mbShowInputMsg );
            aValProps.setProperty( PROP_InputTitle, validation.maInputTitle );
            aValProps.setProperty( PROP_InputMessage, validation.maInputMessage );
            aValProps.setProperty( PROP_ShowErrorMessage, validation.mbShowErrorMsg );
            aValProps.setProperty( PROP_ErrorTitle, validation.maErrorTitle );
            aValProps.setProperty( PROP_ErrorMessage, validation.maErrorMessage );

            // allow blank cells
            aValProps.setProperty( PROP_IgnoreBlankCells, validation.mbAllowBlank );

            // condition operator
            if( eType == ValidationType_CUSTOM )
                xValidation->setConditionOperator( ConditionOperator2::FORMULA );
            else
                xValidation->setConditionOperator( CondFormatBuffer::convertToApiOperator( validation.mnOperator ) );

            // condition formulas
            xValidation->setTokens( 0, validation.maTokens1 );
            xValidation->setTokens( 1, validation.maTokens2 );

            // write back validation settings to cell range(s)
            xRanges->setValidation( xValidation );
        }
    }
}

void WorksheetGlobals::convertColumns()
{
    sal_Int32 nNextCol = 0;
    sal_Int32 nMaxCol = mrMaxApiPos.Col();
    // stores first grouped column index for each level
    OutlineLevelVec aColLevels;

    for (auto const& colModel : maColModels)
    {
        // column indexes are stored 0-based in maColModels
        ValueRange aColRange( ::std::max( colModel.first, nNextCol ), ::std::min( colModel.second.second, nMaxCol ) );
        // process gap between two column models, use default column model
        if( nNextCol < aColRange.mnFirst )
            convertColumns( aColLevels, ValueRange( nNextCol, aColRange.mnFirst - 1 ), maDefColModel );
        // process the column model
        convertColumns( aColLevels, aColRange, colModel.second.first );
        // cache next column to be processed
        nNextCol = aColRange.mnLast + 1;
    }

    // remaining default columns to end of sheet
    convertColumns( aColLevels, ValueRange( nNextCol, nMaxCol ), maDefColModel );
    // close remaining column outlines spanning to end of sheet
    convertOutlines( aColLevels, nMaxCol + 1, 0, false, false );
}

void WorksheetGlobals::convertColumns( OutlineLevelVec& orColLevels,
        const ValueRange& rColRange, const ColumnModel& rModel )
{
    // column width: convert 'number of characters' to column width in twips
    sal_Int32 nWidth = std::round(getUnitConverter().scaleValue( rModel.mfWidth, Unit::Digit, Unit::Twip ));

    SCTAB nTab = getSheetIndex();
    ScDocument& rDoc = getScDocument();
    SCCOL nStartCol = rColRange.mnFirst;
    SCCOL nEndCol = rColRange.mnLast;

    if( nWidth > 0 )
    {
        // macro sheets have double width
        if( meSheetType == WorksheetType::Macro )
            nWidth *= 2;

        for( SCCOL nCol = nStartCol; nCol <= nEndCol; ++nCol )
        {
            rDoc.SetColWidthOnly(nCol, nTab, nWidth);
        }
    }

    // hidden columns: TODO: #108683# hide columns later?
    if( rModel.mbHidden )
    {
        rDoc.SetColHidden( nStartCol, nEndCol, nTab, true );
    }

    // outline settings for this column range
    convertOutlines( orColLevels, rColRange.mnFirst, rModel.mnLevel, rModel.mbCollapsed, false );
}

void WorksheetGlobals::convertRows(const std::vector<sc::ColRowSpan>& rSpans)
{
    sal_Int32 nNextRow = 0;
    sal_Int32 nMaxRow = mrMaxApiPos.Row();
    // stores first grouped row index for each level
    OutlineLevelVec aRowLevels;

    for (auto const& rowModel : maRowModels)
    {
        // row indexes are stored 0-based in maRowModels
        ValueRange aRowRange( ::std::max( rowModel.first, nNextRow ), ::std::min( rowModel.second.second, nMaxRow ) );
        // process gap between two row models, use default row model
        if( nNextRow < aRowRange.mnFirst )
            convertRows(aRowLevels, ValueRange(nNextRow, aRowRange.mnFirst - 1), maDefRowModel,
                        rSpans);
        // process the row model
        convertRows(aRowLevels, aRowRange, rowModel.second.first, rSpans, maDefRowModel.mfHeight);
        // cache next row to be processed
        nNextRow = aRowRange.mnLast + 1;
    }

    // remaining default rows to end of sheet
    convertRows(aRowLevels, ValueRange(nNextRow, nMaxRow), maDefRowModel, rSpans);
    // close remaining row outlines spanning to end of sheet
    convertOutlines( aRowLevels, nMaxRow + 1, 0, false, true );
}

void WorksheetGlobals::convertRows(OutlineLevelVec& orRowLevels, const ValueRange& rRowRange,
                                   const RowModel& rModel,
                                   const std::vector<sc::ColRowSpan>& rSpans, double fDefHeight)
{
    // row height: convert points to row height in twips
    double fHeight = (rModel.mfHeight >= 0.0) ? rModel.mfHeight : fDefHeight;
    sal_Int32 nHeight = std::round(o3tl::toTwips( fHeight, o3tl::Length::pt ));
    SCROW nStartRow = rRowRange.mnFirst;
    SCROW nEndRow = rRowRange.mnLast;
    SCTAB nTab = getSheetIndex();
    if( nHeight > 0 )
    {
        /* always import the row height, ensures better layout */
        ScDocument& rDoc = getScDocument();
        rDoc.SetRowHeightOnly(nStartRow, nEndRow, nTab, nHeight);
        if(rModel.mbCustomHeight)
            rDoc.SetManualHeight( nStartRow, nEndRow, nTab, true );
    }

    // hidden rows: TODO: #108683# hide rows later?
    if( rModel.mbHidden )
    {
        ScDocument& rDoc = getScDocument();
        rDoc.SetRowHidden( nStartRow, nEndRow, nTab, true );
        for (const auto& rSpan : rSpans)
        {
            // tdf#99913 rows hidden by filter need extra flag
            if (rSpan.mnStart <= nStartRow && nStartRow <= rSpan.mnEnd)
            {
                SCROW nLast = ::std::min(nEndRow, rSpan.mnEnd);
                rDoc.SetRowFiltered(nStartRow, nLast, nTab, true);
                break;
            }
        }
    }

    // outline settings for this row range
    convertOutlines( orRowLevels, rRowRange.mnFirst, rModel.mnLevel, rModel.mbCollapsed, true );
}

void WorksheetGlobals::convertOutlines( OutlineLevelVec& orLevels,
        sal_Int32 nColRow, sal_Int32 nLevel, bool bCollapsed, bool bRows )
{
    /*  It is ensured from caller functions, that this function is called
        without any gaps between the processed column or row ranges. */

    OSL_ENSURE( nLevel >= 0, "WorksheetGlobals::convertOutlines - negative outline level" );
    nLevel = ::std::max< sal_Int32 >( nLevel, 0 );

    sal_Int32 nSize = orLevels.size();
    if( nSize < nLevel )
    {
        // Outline level increased. Push the begin column position.
        orLevels.insert(orLevels.end(), nLevel - nSize, nColRow);
    }
    else if( nLevel < nSize )
    {
        // Outline level decreased. Pop them all out.
        for( sal_Int32 nIndex = nLevel; nIndex < nSize; ++nIndex )
        {
            sal_Int32 nFirstInLevel = orLevels.back();
            orLevels.pop_back();
            groupColumnsOrRows( nFirstInLevel, nColRow - 1, bCollapsed, bRows );
            bCollapsed = false; // collapse only once
        }
    }
}

void WorksheetGlobals::groupColumnsOrRows( sal_Int32 nFirstColRow, sal_Int32 nLastColRow, bool bCollapse, bool bRows )
{
    try
    {
        Reference< XSheetOutline > xOutline( mxSheet, UNO_QUERY_THROW );
        if( bRows )
        {
            CellRangeAddress aRange( getSheetIndex(), 0, nFirstColRow, 0, nLastColRow );
            xOutline->group( aRange, TableOrientation_ROWS );
            if( bCollapse )
                xOutline->hideDetail( aRange );
        }
        else
        {
            CellRangeAddress aRange( getSheetIndex(), nFirstColRow, 0, nLastColRow, 0 );
            xOutline->group( aRange, TableOrientation_COLUMNS );
            if( bCollapse )
                xOutline->hideDetail( aRange );
        }
    }
    catch( Exception& )
    {
    }
}

void WorksheetGlobals::finalizeDrawings()
{
    // calculate the current drawing page size (after rows/columns are imported)
    const Size aPageSize( getScDocument().GetMMRect( 0, 0, mrMaxApiPos.Col(), mrMaxApiPos.Row(), getSheetIndex() ).GetSize() );
    maDrawPageSize.Width = lclClampToNonNegativeInt32( aPageSize.Width() );
    maDrawPageSize.Height = lclClampToNonNegativeInt32( aPageSize.Height() );

    // import DML and VML
    if( !maDrawingPath.isEmpty() )
        importOoxFragment( new DrawingFragment( *this, maDrawingPath ) );
    if( !maVmlDrawingPath.isEmpty() )
        importOoxFragment( new VmlDrawingFragment( *this, maVmlDrawingPath ) );

    // comments (after callout shapes have been imported from VML/DFF)
    maComments.finalizeImport();

    /*  Extend used area of the sheet by cells covered with drawing objects.
        Needed if the imported document is inserted as "OLE object from file"
        and thus does not provide an OLE size property by itself. */
    if( (maShapeBoundingBox.Width > 0) || (maShapeBoundingBox.Height > 0) )
    {
        ScRange aRange(getCellRangeFromRectangle(maShapeBoundingBox));
        if (aRange.aStart.Col() < 0)
            aRange.aStart.SetCol(0);
        if (aRange.aStart.Row() < 0)
            aRange.aStart.SetRow(0);
        if (aRange.aEnd.Col() < 0)
            aRange.aEnd.SetCol(0);
        if (aRange.aEnd.Row() < 0)
            aRange.aEnd.SetRow(0);
        extendUsedArea(aRange);
    }

    // if no used area is set, default to A1
    if( maUsedArea.aStart.Col() > maUsedArea.aEnd.Col() )
    {
        maUsedArea.aStart.SetCol( 0 );
        maUsedArea.aEnd.SetCol( 0 );
    }

    if( maUsedArea.aStart.Row() > maUsedArea.aEnd.Row() )
    {
        maUsedArea.aStart.SetRow( 0 );
        maUsedArea.aEnd.SetRow( 0 );
    }

    /*  Register the used area of this sheet in global view settings. The
        global view settings will set the visible area if this document is an
        embedded OLE object. */
    getViewSettings().setSheetUsedArea( maUsedArea );

    /*  #i103686# Set right-to-left sheet layout. Must be done after all
        drawing shapes to simplify calculation of shape coordinates. */
    if( maSheetViewSett.isSheetRightToLeft() )
    {
        PropertySet aPropSet( mxSheet );
        aPropSet.setProperty( PROP_TableLayout, WritingMode2::RL_TB );
    }
}

WorksheetHelper::WorksheetHelper( WorksheetGlobals& rSheetGlob ) :
    WorkbookHelper( rSheetGlob ),
    mrSheetGlob( rSheetGlob )
{
}

/*static*/ WorksheetGlobalsRef WorksheetHelper::constructGlobals( const WorkbookHelper& rHelper,
        const ISegmentProgressBarRef& rxProgressBar, WorksheetType eSheetType, SCTAB nSheet )
{
    WorksheetGlobalsRef xSheetGlob = std::make_shared<WorksheetGlobals>( rHelper, rxProgressBar, eSheetType, nSheet );
    if( !xSheetGlob->isValidSheet() )
        xSheetGlob.reset();
    return xSheetGlob;
}

/* static */ IWorksheetProgress *WorksheetHelper::getWorksheetInterface( const WorksheetGlobalsRef &xRef )
{
    return static_cast< IWorksheetProgress *>( xRef.get() );
}

WorksheetType WorksheetHelper::getSheetType() const
{
    return mrSheetGlob.getSheetType();
}

SCTAB WorksheetHelper::getSheetIndex() const
{
    return mrSheetGlob.getSheetIndex();
}

const Reference< XSpreadsheet >& WorksheetHelper::getSheet() const
{
    return mrSheetGlob.getSheet();
}

Reference< XCell > WorksheetHelper::getCell( const ScAddress& rAddress ) const
{
    return mrSheetGlob.getCell( rAddress );
}

Reference< XDrawPage > WorksheetHelper::getDrawPage() const
{
    return mrSheetGlob.getDrawPage();
}

awt::Point WorksheetHelper::getCellPosition( sal_Int32 nCol, sal_Int32 nRow ) const
{
    return mrSheetGlob.getCellPosition( nCol, nRow );
}

const awt::Size& WorksheetHelper::getDrawPageSize() const
{
    return mrSheetGlob.getDrawPageSize();
}

SheetDataBuffer& WorksheetHelper::getSheetData() const
{
    return mrSheetGlob.getSheetData();
}

CondFormatBuffer& WorksheetHelper::getCondFormats() const
{
    return mrSheetGlob.getCondFormats();
}

CommentsBuffer& WorksheetHelper::getComments() const
{
    return mrSheetGlob.getComments();
}

AutoFilterBuffer& WorksheetHelper::getAutoFilters() const
{
    return mrSheetGlob.getAutoFilters();
}

QueryTableBuffer& WorksheetHelper::getQueryTables() const
{
    return mrSheetGlob.getQueryTables();
}

WorksheetSettings& WorksheetHelper::getWorksheetSettings() const
{
    return mrSheetGlob.getWorksheetSettings();
}

PageSettings& WorksheetHelper::getPageSettings() const
{
    return mrSheetGlob.getPageSettings();
}

SheetViewSettings& WorksheetHelper::getSheetViewSettings() const
{
    return mrSheetGlob.getSheetViewSettings();
}

VmlDrawing& WorksheetHelper::getVmlDrawing() const
{
    return mrSheetGlob.getVmlDrawing();
}

ExtLst& WorksheetHelper::getExtLst() const
{
    return mrSheetGlob.getExtLst();
}

void WorksheetHelper::setPageBreak( const PageBreakModel& rModel, bool bRowBreak )
{
    mrSheetGlob.setPageBreak( rModel, bRowBreak );
}

void WorksheetHelper::setHyperlink( const HyperlinkModel& rModel )
{
    mrSheetGlob.setHyperlink( rModel );
}

void WorksheetHelper::setValidation( const ValidationModel& rModel )
{
    mrSheetGlob.setValidation( rModel );
}

void WorksheetHelper::setDrawingPath( const OUString& rDrawingPath )
{
    mrSheetGlob.setDrawingPath( rDrawingPath );
}

void WorksheetHelper::setVmlDrawingPath( const OUString& rVmlDrawingPath )
{
    mrSheetGlob.setVmlDrawingPath( rVmlDrawingPath );
}

void WorksheetHelper::extendUsedArea( const ScAddress& rAddress )
{
    mrSheetGlob.extendUsedArea( rAddress );
}

void WorksheetHelper::extendShapeBoundingBox( const awt::Rectangle& rShapeRect )
{
    mrSheetGlob.extendShapeBoundingBox( rShapeRect );
}

void WorksheetHelper::setBaseColumnWidth( sal_Int32 nWidth )
{
    mrSheetGlob.setBaseColumnWidth( nWidth );
}

void WorksheetHelper::setDefaultColumnWidth( double fWidth )
{
    mrSheetGlob.setDefaultColumnWidth( fWidth );
}

void WorksheetHelper::setColumnModel( const ColumnModel& rModel )
{
    mrSheetGlob.setColumnModel( rModel );
}

void WorksheetHelper::setDefaultRowSettings( double fHeight, bool bCustomHeight, bool bHidden, bool bThickTop, bool bThickBottom )
{
    mrSheetGlob.setDefaultRowSettings( fHeight, bCustomHeight, bHidden, bThickTop, bThickBottom );
}

void WorksheetHelper::setRowModel( const RowModel& rModel )
{
    mrSheetGlob.setRowModel( rModel );
}

void WorksheetHelper::setCellFormulaValue(
    const ScAddress& rAddress, const OUString& rValueStr, sal_Int32 nCellType )
{
    getFormulaBuffer().setCellFormulaValue(rAddress, rValueStr, nCellType);
}

void WorksheetHelper::putRichString( const ScAddress& rAddress, RichString& rString, const oox::xls::Font* pFirstPortionFont, bool bSingleLine )
{
    ScEditEngineDefaulter& rEE = getEditEngine();

    rEE.SetSingleLine(bSingleLine);

    // The cell will own the text object instance returned from convert().
    getDocImport().setEditCell(rAddress, rString.convert(rEE, pFirstPortionFont));

    rEE.SetSingleLine(false);
}

void WorksheetHelper::putFormulaTokens( const ScAddress& rAddress, const ApiTokenSequence& rTokens )
{
    ScDocumentImport& rDoc = getDocImport();
    std::unique_ptr<ScTokenArray> pTokenArray(new ScTokenArray(rDoc.getDoc()));
    ScTokenConversion::ConvertToTokenArray(rDoc.getDoc(), *pTokenArray, rTokens);
    rDoc.setFormulaCell(rAddress, std::move(pTokenArray));
}

void WorksheetHelper::initializeWorksheetImport()
{
    mrSheetGlob.initializeWorksheetImport();
}

void WorksheetHelper::finalizeWorksheetImport()
{
    mrSheetGlob.finalizeWorksheetImport();
}

void WorksheetHelper::finalizeDrawingImport()
{
    mrSheetGlob.finalizeDrawingImport();
}

void WorksheetHelper::setCellFormula( const ScAddress& rTokenAddress, const OUString& rTokenStr )
{
    getFormulaBuffer().setCellFormula( rTokenAddress,  rTokenStr );
}

void WorksheetHelper::setCellFormula(
    const ScAddress& rAddr, sal_Int32 nSharedId,
    const OUString& rCellValue, sal_Int32 nValueType )
{
    getFormulaBuffer().setCellFormula(rAddr, nSharedId, rCellValue, nValueType);
}

void WorksheetHelper::setCellArrayFormula( const ScRange& rRangeAddress, const ScAddress& rTokenAddress, const OUString& rTokenStr )
{
    getFormulaBuffer().setCellArrayFormula( rRangeAddress,  rTokenAddress, rTokenStr );
}

void WorksheetHelper::createSharedFormulaMapEntry(
    const ScAddress& rAddress, sal_Int32 nSharedId, const OUString& rTokens )
{
    getFormulaBuffer().createSharedFormulaMapEntry(rAddress, nSharedId, rTokens);
}

} // namespace oox

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
