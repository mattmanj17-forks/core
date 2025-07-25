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

#include <sal/types.h>
#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/sequence.hxx>
#include <o3tl/safeint.hxx>
#include <officecfg/VCL.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <tools/debug.hxx>
#include <tools/urlobj.hxx>

#include <utility>
#include <vcl/metaact.hxx>
#include <vcl/print.hxx>
#include <vcl/printer/Options.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld.hxx>

#include <printdlg.hxx>
#include <salinst.hxx>
#include <salprn.hxx>
#include <strings.hrc>
#include <svdata.hxx>

#include <com/sun/star/awt/Size.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/ui/dialogs/FilePicker.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/view/DuplexMode.hpp>
#include <com/sun/star/view/PaperOrientation.hpp>

#include <unordered_map>
#include <unordered_set>

#ifdef MACOSX
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerControlAccess.hpp>
#endif

using namespace vcl;

namespace {

class ImplPageCache
{
    struct CacheEntry
    {
        GDIMetaFile                 aPage;
        PrinterController::PageSize aSize;
    };

    std::vector< CacheEntry >   maPages;
    std::vector< sal_Int32 >    maPageNumbers;
    std::vector< sal_Int32 >    maCacheRanking;

    static const sal_Int32      nCacheSize = 6;

    void updateRanking( sal_Int32 nLastHit )
    {
        if( maCacheRanking[0] != nLastHit )
        {
            for( sal_Int32 i = nCacheSize-1; i > 0; i-- )
                maCacheRanking[i] = maCacheRanking[i-1];
            maCacheRanking[0] = nLastHit;
        }
    }

public:
    ImplPageCache()
    : maPages( nCacheSize )
    , maPageNumbers( nCacheSize, -1 )
    , maCacheRanking( nCacheSize )
    {
        for( sal_Int32 i = 0; i < nCacheSize; i++ )
            maCacheRanking[i] = nCacheSize - i - 1;
    }

    // caution: does not ensure uniqueness
    void insert( sal_Int32 i_nPageNo, const GDIMetaFile& i_rPage, const PrinterController::PageSize& i_rSize )
    {
        sal_Int32 nReplacePage = maCacheRanking.back();
        maPages[ nReplacePage ].aPage = i_rPage;
        maPages[ nReplacePage ].aSize = i_rSize;
        maPageNumbers[ nReplacePage ] = i_nPageNo;
        // cache insertion means in our case, the page was just queried
        // so update the ranking
        updateRanking( nReplacePage );
    }

    // caution: bad algorithm; should there ever be reason to increase the cache size beyond 6
    // this needs to be urgently rewritten. However do NOT increase the cache size lightly,
    // whole pages can be rather memory intensive
    bool get( sal_Int32 i_nPageNo, GDIMetaFile& o_rPageFile, PrinterController::PageSize& o_rSize )
    {
        for( sal_Int32 i = 0; i < nCacheSize; ++i )
        {
            if( maPageNumbers[i] == i_nPageNo )
            {
                updateRanking( i );
                o_rPageFile = maPages[i].aPage;
                o_rSize = maPages[i].aSize;
                return true;
            }
        }
        return false;
    }

    void invalidate()
    {
        for( sal_Int32 i = 0; i < nCacheSize; ++i )
        {
            maPageNumbers[i] = -1;
            maPages[i].aPage.Clear();
            maCacheRanking[i] = nCacheSize - i - 1;
        }
    }
};

}

class vcl::ImplPrinterControllerData
{
public:
    struct ControlDependency
    {
        OUString       maDependsOnName;
        sal_Int32           mnDependsOnEntry;

        ControlDependency() : mnDependsOnEntry( -1 ) {}
    };

    typedef std::unordered_map< OUString, size_t > PropertyToIndexMap;
    typedef std::unordered_map< OUString, ControlDependency > ControlDependencyMap;
    typedef std::unordered_map< OUString, css::uno::Sequence< sal_Bool > > ChoiceDisableMap;

    VclPtr< Printer >                                           mxPrinter;
    weld::Window*                                               mpWindow;
    css::uno::Sequence< css::beans::PropertyValue >             maUIOptions;
    std::vector< css::beans::PropertyValue >                    maUIProperties;
    std::vector< bool >                                         maUIPropertyEnabled;
    PropertyToIndexMap                                          maPropertyToIndex;
    ControlDependencyMap                                        maControlDependencies;
    ChoiceDisableMap                                            maChoiceDisableMap;
    bool                                                        mbFirstPage;
    bool                                                        mbLastPage;
    bool                                                        mbReversePageOrder;
    bool                                                        mbPapersizeFromSetup;
    bool                                                        mbPapersizeFromUser;
    bool                                                        mbOrientationFromUser;
    bool                                                        mbPrinterModified;
    css::view::PrintableState                                   meJobState;

    vcl::PrinterController::MultiPageSetup                      maMultiPage;

    std::shared_ptr<vcl::PrintProgressDialog>                   mxProgress;

    ImplPageCache                                               maPageCache;

    // set by user through printer properties subdialog of printer settings dialog
    Size                                                        maDefaultPageSize;
    // set by user through print dialog
    Size                                                        maUserPageSize;
    // set by user through print dialog
    Orientation                                                 meUserOrientation;
    // set by user through printer properties subdialog of printer settings dialog
    sal_Int32                                                   mnDefaultPaperBin;
    // Set by user through printer properties subdialog of print dialog.
    // Overrides application-set tray for a page.
    sal_Int32                                                   mnFixedPaperBin;

    // N.B. Apparently we have three levels of paper tray settings
    // (latter overrides former):
    // 1. default tray
    // 2. tray set for a concrete page by an application, e.g., writer
    //    allows setting a printer tray (for the default printer) for a
    //    page style. This setting can be overridden by user by selecting
    //    "Use only paper tray from printer preferences" on the Options
    //    page in the print dialog, in which case the default tray is
    //    used for all pages.
    // 3. tray set in printer properties the printer dialog
    // I'm not quite sure why 1. and 3. are distinct, but the commit
    // history suggests this is intentional...

    ImplPrinterControllerData() :
        mpWindow( nullptr ),
        mbFirstPage( true ),
        mbLastPage( false ),
        mbReversePageOrder( false ),
        mbPapersizeFromSetup( false ),
        mbPapersizeFromUser( false ),
        mbOrientationFromUser( false ),
        mbPrinterModified( false ),
        meJobState( css::view::PrintableState_JOB_STARTED ),
        meUserOrientation( Orientation::Portrait ),
        mnDefaultPaperBin( -1 ),
        mnFixedPaperBin( -1 )
    {}

    ~ImplPrinterControllerData()
    {
        if (mxProgress)
        {
            mxProgress->response(RET_CANCEL);
            mxProgress.reset();
        }
    }

    Size getRealPaperSize( const Size& i_rPageSize, bool bNoNUP ) const
    {
        Size size;
        if ( mbPapersizeFromUser )
            size = maUserPageSize;
        else if( mbPapersizeFromSetup )
            size =  maDefaultPageSize;
        else if( maMultiPage.nRows * maMultiPage.nColumns > 1 && ! bNoNUP )
            size = maMultiPage.aPaperSize;
        else
            size = i_rPageSize;
        if(mbOrientationFromUser)
        {
            if ( (meUserOrientation == Orientation::Portrait && size.Width() > size.Height()) ||
                 (meUserOrientation == Orientation::Landscape && size.Width() < size.Height()) )
            {
                // coverity[swapped_arguments : FALSE] - this is in the correct order
                size = Size( size.Height(), size.Width() );
            }
        }
        return size;
    }
    PrinterController::PageSize modifyJobSetup( const css::uno::Sequence< css::beans::PropertyValue >& i_rProps );
    void resetPaperToLastConfigured();
};

PrinterController::PrinterController(const VclPtr<Printer>& i_xPrinter, weld::Window* i_pWindow)
    : mpImplData( new ImplPrinterControllerData )
{
    mpImplData->mxPrinter = i_xPrinter;
    mpImplData->mpWindow = i_pWindow;
}

static OUString queryFile( Printer const * pPrinter, const OUString & rJobName )
{
    OUString aResult;

    const css::uno::Reference< css::uno::XComponentContext >& xContext( ::comphelper::getProcessComponentContext() );
    css::uno::Reference< css::ui::dialogs::XFilePicker3 > xFilePicker = css::ui::dialogs::FilePicker::createWithMode(xContext, css::ui::dialogs::TemplateDescription::FILESAVE_AUTOEXTENSION);

    try
    {
#ifdef MACOSX
        // Try to mimic the save dialog behavior when using the native
        // print dialog to save to PDF.
        if( pPrinter )
        {
            // Set the suggested file name if possible
            if( !rJobName.isEmpty() )
                xFilePicker->setDefaultName( rJobName );

            // macOS normally saves only to PDF
            if( pPrinter->GetCapabilities( PrinterCapType::PDF ) )
            {
                xFilePicker->appendFilter( u"Portable Document Format"_ustr, u"*.pdf"_ustr );

                css::uno::Reference< css::ui::dialogs::XFilePickerControlAccess > xControlAccess( xFilePicker, css::uno::UNO_QUERY );
                if( xControlAccess.is() )
                    xControlAccess->setValue( css::ui::dialogs::ExtendedFilePickerElementIds::CHECKBOX_AUTOEXTENSION, 0, css::uno::Any( true ) );
            }
        }
#else
        (void)rJobName;
#ifdef UNX
        // add PostScript and PDF
        bool bPS = true, bPDF = true;
        if( pPrinter )
        {
            if( pPrinter->GetCapabilities( PrinterCapType::PDF ) )
                bPS = false;
            else
                bPDF = false;
        }
        if( bPS )
            xFilePicker->appendFilter( u"PostScript"_ustr, u"*.ps"_ustr );
        if( bPDF )
            xFilePicker->appendFilter( u"Portable Document Format"_ustr, u"*.pdf"_ustr );
#elif defined _WIN32
        (void)pPrinter;
        xFilePicker->appendFilter( "*.PRN", "*.prn" );
#endif
        // add arbitrary files
        xFilePicker->appendFilter(VclResId(SV_STDTEXT_ALLFILETYPES), u"*.*"_ustr);
#endif
    }
    catch (const css::lang::IllegalArgumentException&)
    {
        TOOLS_WARN_EXCEPTION( "vcl.gdi", "caught IllegalArgumentException when registering filter" );
    }

    if( xFilePicker->execute() == css::ui::dialogs::ExecutableDialogResults::OK )
    {
        css::uno::Sequence< OUString > aPathSeq( xFilePicker->getSelectedFiles() );
        INetURLObject aObj( aPathSeq[0] );
        aResult = aObj.PathToFileName();
    }
    return aResult;
}

namespace {

struct PrintJobAsync
{
    std::shared_ptr<PrinterController>  mxController;
    JobSetup                            maInitSetup;

    PrintJobAsync(std::shared_ptr<PrinterController>  i_xController,
                  const JobSetup& i_rInitSetup)
    : mxController(std::move( i_xController )), maInitSetup( i_rInitSetup )
    {}

    DECL_LINK( ExecJob, void*, void );
};

}

IMPL_LINK_NOARG(PrintJobAsync, ExecJob, void*, void)
{
    Printer::ImplPrintJob(mxController, maInitSetup);

    // clean up, do not access members after this
    delete this;
}

void Printer::PrintJob(const std::shared_ptr<PrinterController>& i_xController,
                       const JobSetup& i_rInitSetup)
{
    bool bSynchronous = false;
    css::beans::PropertyValue* pVal = i_xController->getValue( u"Wait"_ustr );
    if( pVal )
        pVal->Value >>= bSynchronous;

    if( bSynchronous )
        ImplPrintJob(i_xController, i_rInitSetup);
    else
    {
        PrintJobAsync* pAsync = new PrintJobAsync(i_xController, i_rInitSetup);
        Application::PostUserEvent( LINK( pAsync, PrintJobAsync, ExecJob ) );
    }
}

bool Printer::PreparePrintJob(std::shared_ptr<PrinterController> xController,
                           const JobSetup& i_rInitSetup)
{
    Printer::updatePrinters();

    // check if there is a default printer; if not, show an error box (if appropriate)
    if( GetDefaultPrinterName().isEmpty() )
    {
        if (xController->isShowDialogs())
        {
            std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(xController->getWindow(), u"vcl/ui/errornoprinterdialog.ui"_ustr));
            std::unique_ptr<weld::MessageDialog> xBox(xBuilder->weld_message_dialog(u"ErrorNoPrinterDialog"_ustr));
            xBox->run();
        }
        xController->setValue( u"IsDirect"_ustr,
                               css::uno::Any( false ) );
    }

    // setup printer

    // #i114306# changed behavior back from persistence
    // if no specific printer is already set, create the default printer
    if (!xController->getPrinter())
    {
        OUString aPrinterName( i_rInitSetup.GetPrinterName() );
        VclPtrInstance<Printer> xPrinter( aPrinterName );
        xPrinter->SetJobSetup(i_rInitSetup);
        xController->setPrinter(xPrinter);
        xController->setPapersizeFromSetup(xPrinter->GetPrinterSettingsPreferred());
    }

    // reset last page property
    xController->setLastPage(false);

    // update "PageRange" property inferring from other properties:
    // case 1: "Pages" set from UNO API ->
    //         setup "Print Selection" and insert "PageRange" attribute
    // case 2: "All pages" is selected
    //         update "Page range" attribute to have a sensible default,
    //         but leave "All" as selected

    // "Pages" attribute from API is now equivalent to "PageRange"
    // AND "PrintContent" = 1 except calc where it is "PrintRange" = 1
    // Argh ! That sure needs cleaning up
    css::beans::PropertyValue* pContentVal = xController->getValue(u"PrintRange"_ustr);
    if( ! pContentVal )
        pContentVal = xController->getValue(u"PrintContent"_ustr);

    // case 1: UNO API has set "Pages"
    css::beans::PropertyValue* pPagesVal = xController->getValue(u"Pages"_ustr);
    if( pPagesVal )
    {
        OUString aPagesVal;
        pPagesVal->Value >>= aPagesVal;
        if( !aPagesVal.isEmpty() )
        {
            // "Pages" attribute from API is now equivalent to "PageRange"
            // AND "PrintContent" = 1 except calc where it is "PrintRange" = 1
            // Argh ! That sure needs cleaning up
            if( pContentVal )
            {
                pContentVal->Value <<= sal_Int32( 1 );
                xController->setValue(u"PageRange"_ustr, pPagesVal->Value);
            }
        }
    }
    // case 2: is "All" selected ?
    else if( pContentVal )
    {
        sal_Int32 nContent = -1;
        if( pContentVal->Value >>= nContent )
        {
            if( nContent == 0 )
            {
                // do not overwrite PageRange if it is already set
                css::beans::PropertyValue* pRangeVal = xController->getValue(u"PageRange"_ustr);
                OUString aRange;
                if( pRangeVal )
                    pRangeVal->Value >>= aRange;
                if( aRange.isEmpty() )
                {
                    sal_Int32 nPages = xController->getPageCount();
                    if( nPages > 0 )
                    {
                        OUStringBuffer aBuf( 32 );
                        aBuf.append( "1" );
                        if( nPages > 1 )
                        {
                            aBuf.append( "-" + OUString::number( nPages ) );
                        }
                        xController->setValue(u"PageRange"_ustr, css::uno::Any(aBuf.makeStringAndClear()));
                    }
                }
            }
        }
    }

    css::beans::PropertyValue* pReverseVal = xController->getValue(u"PrintReverse"_ustr);
    if( pReverseVal )
    {
        bool bReverse = false;
        pReverseVal->Value >>= bReverse;
        xController->setReversePrint( bReverse );
    }

    css::beans::PropertyValue* pPapersizeFromSetupVal = xController->getValue(u"PapersizeFromSetup"_ustr);
    if( pPapersizeFromSetupVal )
    {
        bool bPapersizeFromSetup = false;
        pPapersizeFromSetupVal->Value >>= bPapersizeFromSetup;
        xController->setPapersizeFromSetup(bPapersizeFromSetup);
    }

    // setup NUp printing from properties
    sal_Int32 nRows = xController->getIntProperty(u"NUpRows"_ustr, 1);
    sal_Int32 nCols = xController->getIntProperty(u"NUpColumns"_ustr, 1);
    if( nRows > 1 || nCols > 1 )
    {
        PrinterController::MultiPageSetup aMPS;
        aMPS.nRows         = std::max<sal_Int32>(nRows, 1);
        aMPS.nColumns      = std::max<sal_Int32>(nCols, 1);
        sal_Int32 nValue = xController->getIntProperty(u"NUpPageMarginLeft"_ustr, aMPS.nLeftMargin);
        if( nValue >= 0 )
            aMPS.nLeftMargin = nValue;
        nValue = xController->getIntProperty(u"NUpPageMarginRight"_ustr, aMPS.nRightMargin);
        if( nValue >= 0 )
            aMPS.nRightMargin = nValue;
        nValue = xController->getIntProperty( u"NUpPageMarginTop"_ustr, aMPS.nTopMargin );
        if( nValue >= 0 )
            aMPS.nTopMargin = nValue;
        nValue = xController->getIntProperty( u"NUpPageMarginBottom"_ustr, aMPS.nBottomMargin );
        if( nValue >= 0 )
            aMPS.nBottomMargin = nValue;
        nValue = xController->getIntProperty( u"NUpHorizontalSpacing"_ustr, aMPS.nHorizontalSpacing );
        if( nValue >= 0 )
            aMPS.nHorizontalSpacing = nValue;
        nValue = xController->getIntProperty( u"NUpVerticalSpacing"_ustr, aMPS.nVerticalSpacing );
        if( nValue >= 0 )
            aMPS.nVerticalSpacing = nValue;
        aMPS.bDrawBorder = xController->getBoolProperty( u"NUpDrawBorder"_ustr, aMPS.bDrawBorder );
        aMPS.nOrder = static_cast<NupOrderType>(xController->getIntProperty( u"NUpSubPageOrder"_ustr, static_cast<sal_Int32>(aMPS.nOrder) ));
        aMPS.aPaperSize = xController->getPrinter()->PixelToLogic( xController->getPrinter()->GetPaperSizePixel(), MapMode( MapUnit::Map100thMM ) );
        css::beans::PropertyValue* pPgSizeVal = xController->getValue( u"NUpPaperSize"_ustr );
        css::awt::Size aSizeVal;
        if( pPgSizeVal && (pPgSizeVal->Value >>= aSizeVal) )
        {
            aMPS.aPaperSize.setWidth( aSizeVal.Width );
            aMPS.aPaperSize.setHeight( aSizeVal.Height );
        }

        xController->setMultipage( aMPS );
    }

    // in direct print case check whether there is anything to print.
    // if not, show an errorbox (if appropriate)
    if( xController->isShowDialogs() && xController->isDirectPrint() )
    {
        if( xController->getFilteredPageCount() == 0 )
        {
            std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(xController->getWindow(), u"vcl/ui/errornocontentdialog.ui"_ustr));
            std::unique_ptr<weld::MessageDialog> xBox(xBuilder->weld_message_dialog(u"ErrorNoContentDialog"_ustr));
            xBox->run();
            return false;
        }
    }

    // check if the printer brings up its own dialog
    // in that case leave the work to that dialog
    if( ! xController->getPrinter()->GetCapabilities( PrinterCapType::ExternalDialog ) &&
        ! xController->isDirectPrint() &&
        xController->isShowDialogs()
        )
    {
        try
        {
            PrintDialog aDlg(xController->getWindow(), xController);
            if (!aDlg.run())
            {
                xController->abortJob();
                return false;
            }
            if (aDlg.isPrintToFile())
            {
                OUString aJobName;
                css::beans::PropertyValue* pJobNameVal = xController->getValue( u"JobName"_ustr );
                if( pJobNameVal )
                    pJobNameVal->Value >>= aJobName;
                OUString aFile = queryFile( xController->getPrinter().get(), aJobName );
                if( aFile.isEmpty() )
                {
                    xController->abortJob();
                    return false;
                }
                xController->setValue( u"LocalFileName"_ustr,
                                       css::uno::Any( aFile ) );
            }
            else if (aDlg.isSingleJobs())
            {
                xController->getPrinter()->SetSinglePrintJobs(true);
            }
        }
        catch (const std::bad_alloc&)
        {
        }
    }

    xController->pushPropertiesToPrinter();
    return true;
}

bool Printer::ExecutePrintJob(const std::shared_ptr<PrinterController>& xController)
{
    OUString aJobName;
    css::beans::PropertyValue* pJobNameVal = xController->getValue( u"JobName"_ustr );
    if( pJobNameVal )
        pJobNameVal->Value >>= aJobName;

    return xController->getPrinter()->StartJob( aJobName, xController );
}

void Printer::FinishPrintJob(const std::shared_ptr<PrinterController>& xController)
{
    xController->resetPaperToLastConfigured();
    xController->jobFinished( xController->getJobState() );
}

void Printer::ImplPrintJob(const std::shared_ptr<PrinterController>& xController,
                           const JobSetup& i_rInitSetup)
{
    if (PreparePrintJob(xController, i_rInitSetup))
    {
        ExecutePrintJob(xController);
    }
    FinishPrintJob(xController);
}

bool Printer::StartJob( const OUString& i_rJobName, std::shared_ptr<vcl::PrinterController> const & i_xController)
{
    mnError = ERRCODE_NONE;

    if ( IsDisplayPrinter() )
        return false;

    if ( IsJobActive() || IsPrinting() )
        return false;

    sal_uInt32 nCopies = mnCopyCount;
    bool    bCollateCopy = mbCollateCopy;
    bool    bUserCopy = false;

    if ( nCopies > 1 )
    {
        const sal_uInt32 nDevCopy = GetCapabilities( bCollateCopy
            ? PrinterCapType::CollateCopies
            : PrinterCapType::Copies );

        // need to do copies by hand ?
        if ( nCopies > nDevCopy )
        {
            bUserCopy = true;
            nCopies = 1;
            bCollateCopy = false;
        }
    }
    else
        bCollateCopy = false;

    ImplSVData* pSVData = ImplGetSVData();
    mpPrinter = pSVData->mpDefInst->CreatePrinter( mpInfoPrinter );

    if (!mpPrinter)
        return false;

    bool bSinglePrintJobs = i_xController->getPrinter()->IsSinglePrintJobs();

    css::beans::PropertyValue* pFileValue = i_xController->getValue(u"LocalFileName"_ustr);
    if( pFileValue )
    {
        OUString aFile;
        pFileValue->Value >>= aFile;
        if( !aFile.isEmpty() )
        {
            mbPrintFile = true;
            maPrintFile = aFile;
            bSinglePrintJobs = false;
        }
    }

    OUString* pPrintFile = nullptr;
    if ( mbPrintFile )
        pPrintFile = &maPrintFile;
    mpPrinterOptions->ReadFromConfig( mbPrintFile );

    mbPrinting              = true;
    if( GetCapabilities( PrinterCapType::UsePullModel ) )
    {
        mbJobActive             = true;
        // SAL layer does all necessary page printing
        // and also handles showing a dialog
        // that also means it must call jobStarted when the dialog is finished
        // it also must set the JobState of the Controller
        if( mpPrinter->StartJob( pPrintFile,
                                 i_rJobName,
                                 Application::GetDisplayName(),
                                 &maJobSetup.ImplGetData(),
                                 *i_xController) )
        {
            EndJob();
        }
        else
        {
            mnError = ImplSalPrinterErrorCodeToVCL(mpPrinter->GetErrorCode());
            if ( !mnError )
                mnError = PRINTER_GENERALERROR;
            mbPrinting = false;
            mpPrinter.reset();
            mbJobActive = false;

            GDIMetaFile aDummyFile;
            i_xController->setLastPage(true);
            i_xController->getFilteredPageFile(0, aDummyFile);

            return false;
        }
    }
    else
    {
        // possibly a dialog has been shown
        // now the real job starts
        i_xController->setJobState( css::view::PrintableState_JOB_STARTED );
        i_xController->jobStarted();

        int nJobs = 1;
        int nOuterRepeatCount = 1;
        int nInnerRepeatCount = 1;
        if( bUserCopy )
        {
            if( mbCollateCopy )
                nOuterRepeatCount = mnCopyCount;
            else
                nInnerRepeatCount = mnCopyCount;
        }
        if( bSinglePrintJobs )
        {
            nJobs = mnCopyCount;
            nCopies = 1;
            nOuterRepeatCount = nInnerRepeatCount = 1;
        }

        for( int nJobIteration = 0; nJobIteration < nJobs; nJobIteration++ )
        {
            bool bError = false;
            if( mpPrinter->StartJob( pPrintFile,
                                     i_rJobName,
                                     Application::GetDisplayName(),
                                     nCopies,
                                     bCollateCopy,
                                     i_xController->isDirectPrint(),
                                     &maJobSetup.ImplGetData() ) )
            {
                bool bAborted = false;
                mbJobActive             = true;
                i_xController->createProgressDialog();
                const int nPages = i_xController->getFilteredPageCount();
                // abort job, if no pages will be printed.
                if ( nPages == 0 )
                {
                    i_xController->abortJob();
                    bAborted = true;
                }
                for( int nOuterIteration = 0; nOuterIteration < nOuterRepeatCount && ! bAborted; nOuterIteration++ )
                {
                    for( int nPage = 0; nPage < nPages && ! bAborted; nPage++ )
                    {
                        for( int nInnerIteration = 0; nInnerIteration < nInnerRepeatCount && ! bAborted; nInnerIteration++ )
                        {
                            if( nPage == nPages-1 &&
                                nOuterIteration == nOuterRepeatCount-1 &&
                                nInnerIteration == nInnerRepeatCount-1 &&
                                nJobIteration == nJobs-1 )
                            {
                                i_xController->setLastPage(true);
                            }
                            i_xController->printFilteredPage(nPage);
                            if (i_xController->isProgressCanceled())
                            {
                                i_xController->abortJob();
                            }
                            if (i_xController->getJobState() ==
                                    css::view::PrintableState_JOB_ABORTED)
                            {
                                bAborted = true;
                            }
                        }
                    }
                    // FIXME: duplex ?
                }
                EndJob();

                if( nJobIteration < nJobs-1 )
                {
                    mpPrinter = pSVData->mpDefInst->CreatePrinter( mpInfoPrinter );

                    if ( mpPrinter )
                        mbPrinting              = true;
                    else
                        bError = true;
                }
            }
            else
                bError = true;

            if( bError )
            {
                mnError = mpPrinter ? ImplSalPrinterErrorCodeToVCL(mpPrinter->GetErrorCode()) : ERRCODE_NONE;
                if ( !mnError )
                    mnError = PRINTER_GENERALERROR;
                i_xController->setJobState( mnError == PRINTER_ABORT
                                            ? css::view::PrintableState_JOB_ABORTED
                                            : css::view::PrintableState_JOB_FAILED );
                mbPrinting = false;
                mpPrinter.reset();

                return false;
            }
        }

        if (i_xController->getJobState() == css::view::PrintableState_JOB_STARTED)
            i_xController->setJobState(css::view::PrintableState_JOB_SPOOLED);
    }

    // make last used printer persistent for UI jobs
    if (i_xController->isShowDialogs() && !i_xController->isDirectPrint())
    {
        std::shared_ptr<comphelper::ConfigurationChanges> batch(comphelper::ConfigurationChanges::create());
        officecfg::VCL::VCLSettings::PrintDialog::LastPrinter::set( GetName(), batch );
        batch->commit();
    }

    return true;
}

PrinterController::~PrinterController()
{
}

css::view::PrintableState PrinterController::getJobState() const
{
    return mpImplData->meJobState;
}

void PrinterController::setJobState( css::view::PrintableState i_eState )
{
    mpImplData->meJobState = i_eState;
}

const VclPtr<Printer>& PrinterController::getPrinter() const
{
    return mpImplData->mxPrinter;
}

weld::Window* PrinterController::getWindow() const
{
    return mpImplData->mpWindow;
}

void PrinterController::dialogsParentClosing()
{
    mpImplData->mpWindow = nullptr;
    if (mpImplData->mxProgress)
    {
        // close the dialog without doing anything, just get rid of it
        mpImplData->mxProgress->response(RET_OK);
        mpImplData->mxProgress.reset();
    }
}

void PrinterController::setPrinter( const VclPtr<Printer>& i_rPrinter )
{
    VclPtr<Printer> xPrinter = mpImplData->mxPrinter;

    Size aPaperSize;          // Save current paper size
    Orientation eOrientation = Orientation::Portrait; // Save current paper orientation
    bool bSavedSizeOrientation = false;

    // #tdf 126744 Transfer paper size and orientation settings to newly selected printer
    if ( xPrinter )
    {
        aPaperSize = xPrinter->GetPaperSize();
        eOrientation = xPrinter->GetOrientation();
        bSavedSizeOrientation = true;
    }

    mpImplData->mxPrinter = i_rPrinter;
    setValue( u"Name"_ustr,
              css::uno::Any( i_rPrinter->GetName() ) );
    mpImplData->mnDefaultPaperBin = mpImplData->mxPrinter->GetPaperBin();
    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    mpImplData->maDefaultPageSize = mpImplData->mxPrinter->GetPaperSize();

    if ( bSavedSizeOrientation )
    {
          mpImplData->mxPrinter->SetPaperSizeUser(aPaperSize);
          mpImplData->mxPrinter->SetOrientation(eOrientation);
    }

    mpImplData->mbPapersizeFromUser = false;
    mpImplData->mbOrientationFromUser = false;
    mpImplData->mxPrinter->Pop();
    mpImplData->mnFixedPaperBin = -1;
}

void PrinterController::resetPrinterOptions( bool i_bFileOutput )
{
    vcl::printer::Options aOpt;
    aOpt.ReadFromConfig( i_bFileOutput );
    mpImplData->mxPrinter->SetPrinterOptions( aOpt );
}

void PrinterController::invalidatePageCache()
{
    mpImplData->maPageCache.invalidate();
}

void PrinterController::setupPrinter( weld::Window* i_pParent )
{
    bool bRet = false;

    // Important to hold printer alive while doing setup etc.
    VclPtr< Printer > xPrinter = mpImplData->mxPrinter;

    if( !xPrinter )
        return;

    xPrinter->Push();
    xPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));

    // get current data
    Size aPaperSize(xPrinter->GetPaperSize());
    Orientation eOrientation = xPrinter->GetOrientation();
    sal_uInt16 nPaperBin = xPrinter->GetPaperBin();

    // reset paper size back to last configured size, not
    // whatever happens to be the current page
    // (but only if the printer config has changed, otherwise
    // don't override printer page auto-detection - tdf#91362)
    if (getPrinterModified() || getPapersizeFromSetup())
    {
        resetPaperToLastConfigured();
    }

    // call driver setup
    bRet = xPrinter->Setup( i_pParent, PrinterSetupMode::SingleJob );
    SAL_WARN_IF(xPrinter != mpImplData->mxPrinter, "vcl.gdi",
                "Printer changed underneath us during setup");
    xPrinter = mpImplData->mxPrinter;

    Size aNewPaperSize(xPrinter->GetPaperSize());
    if (bRet)
    {
        bool bInvalidateCache = false;
        setPapersizeFromSetup(xPrinter->GetPrinterSettingsPreferred());

        // was papersize overridden ? if so we need to take action if we're
        // configured to use the driver papersize
        if (aNewPaperSize != mpImplData->maDefaultPageSize)
        {
            mpImplData->maDefaultPageSize = aNewPaperSize;
            bInvalidateCache = getPapersizeFromSetup();
        }

        // was bin overridden ? if so we need to take action
        sal_uInt16 nNewPaperBin = xPrinter->GetPaperBin();
        if (nNewPaperBin != nPaperBin)
        {
            mpImplData->mnFixedPaperBin = nNewPaperBin;
            bInvalidateCache = true;
        }

        if (bInvalidateCache)
            invalidatePageCache();
    }
    else
    {
        //restore to whatever it was before we entered this method
        xPrinter->SetOrientation( eOrientation );
        if (aPaperSize != aNewPaperSize)
            xPrinter->SetPaperSizeUser(aPaperSize);
    }
    xPrinter->Pop();
}

PrinterController::PageSize vcl::ImplPrinterControllerData::modifyJobSetup( const css::uno::Sequence< css::beans::PropertyValue >& i_rProps )
{
    PrinterController::PageSize aPageSize;
    aPageSize.aSize = mxPrinter->GetPaperSize();
    css::awt::Size aSetSize, aIsSize;
    sal_Int32 nPaperBin = mnDefaultPaperBin;
    for( const auto& rProp : i_rProps )
    {
        if ( rProp.Name == "PreferredPageSize" )
        {
            rProp.Value >>= aSetSize;
        }
        else if ( rProp.Name == "PageSize" )
        {
            rProp.Value >>= aIsSize;
        }
        else if ( rProp.Name == "PageIncludesNonprintableArea" )
        {
            bool bVal = false;
            rProp.Value >>= bVal;
            aPageSize.bFullPaper = bVal;
        }
        else if ( rProp.Name == "PrinterPaperTray" )
        {
            sal_Int32 nBin = -1;
            rProp.Value >>= nBin;
            if( nBin >= 0 && o3tl::make_unsigned(nBin) < mxPrinter->GetPaperBinCount() )
                nPaperBin = nBin;
        }
        else if ( rProp.Name == "PaperOrientation" )
        {
            css::view::PaperOrientation nOrientation = css::view::PaperOrientation::PaperOrientation_PORTRAIT;
            rProp.Value >>= nOrientation;
            mxPrinter->SetOrientation( nOrientation == css::view::PaperOrientation::PaperOrientation_LANDSCAPE ? Orientation::Landscape : Orientation::Portrait );
        }
    }

    Size aCurSize( mxPrinter->GetPaperSize() );
    if( aSetSize.Width && aSetSize.Height )
    {
        Size aSetPaperSize( aSetSize.Width, aSetSize.Height );
        Size aRealPaperSize( getRealPaperSize( aSetPaperSize, true/*bNoNUP*/ ) );
        if( aRealPaperSize != aCurSize )
            aIsSize = aSetSize;
    }

    if( aIsSize.Width && aIsSize.Height )
    {
        aPageSize.aSize.setWidth( aIsSize.Width );
        aPageSize.aSize.setHeight( aIsSize.Height );

        Size aRealPaperSize( getRealPaperSize( aPageSize.aSize, true/*bNoNUP*/ ) );
        if( aRealPaperSize != aCurSize )
            mxPrinter->SetPaperSizeUser( aRealPaperSize );
    }

    // paper bin set from properties in print dialog overrides
    // application default for a page
    if ( mnFixedPaperBin != -1 )
        nPaperBin = mnFixedPaperBin;

    if( nPaperBin != -1 && nPaperBin != mxPrinter->GetPaperBin() )
        mxPrinter->SetPaperBin( nPaperBin );

    return aPageSize;
}

//fdo#61886

//when printing is finished, set the paper size of the printer to either what
//the user explicitly set as the desired paper size, or fallback to whatever
//the printer had before printing started. That way it doesn't contain the last
//paper size of a multiple paper size using document when we are in our normal
//auto accept document paper size mode and end up overwriting the original
//paper size setting for file->printer_settings just by pressing "ok" in the
//print dialog
void vcl::ImplPrinterControllerData::resetPaperToLastConfigured()
{
    mxPrinter->Push();
    mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    Size aCurSize(mxPrinter->GetPaperSize());
    if (aCurSize != maDefaultPageSize)
        mxPrinter->SetPaperSizeUser(maDefaultPageSize);
    mxPrinter->Pop();
}

int PrinterController::getPageCountProtected() const
{
    const MapMode aMapMode( MapUnit::Map100thMM );

    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode( aMapMode );
    int nPages = getPageCount();
    mpImplData->mxPrinter->Pop();
    return nPages;
}

css::uno::Sequence< css::beans::PropertyValue > PrinterController::getPageParametersProtected( int i_nPage ) const
{
    const MapMode aMapMode( MapUnit::Map100thMM );

    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode( aMapMode );
    css::uno::Sequence< css::beans::PropertyValue > aResult( getPageParameters( i_nPage ) );
    mpImplData->mxPrinter->Pop();
    return aResult;
}

PrinterController::PageSize PrinterController::getPageFile( int i_nUnfilteredPage, GDIMetaFile& o_rMtf, bool i_bMayUseCache )
{
    // update progress if necessary
    if( mpImplData->mxProgress )
    {
        // do nothing if printing is canceled
        if( mpImplData->mxProgress->isCanceled() )
            return PrinterController::PageSize();
        mpImplData->mxProgress->tick();
        Application::Reschedule( true );
    }

    if( i_bMayUseCache )
    {
        PrinterController::PageSize aPageSize;
        if( mpImplData->maPageCache.get( i_nUnfilteredPage, o_rMtf, aPageSize ) )
        {
            return aPageSize;
        }
    }
    else
        invalidatePageCache();

    o_rMtf.Clear();

    // get page parameters
    css::uno::Sequence< css::beans::PropertyValue > aPageParm( getPageParametersProtected( i_nUnfilteredPage ) );
    const MapMode aMapMode( MapUnit::Map100thMM );

    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode( aMapMode );

    // modify job setup if necessary
    PrinterController::PageSize aPageSize = mpImplData->modifyJobSetup( aPageParm );

    o_rMtf.SetPrefSize( aPageSize.aSize );
    o_rMtf.SetPrefMapMode( aMapMode );

    mpImplData->mxPrinter->EnableOutput( false );

    o_rMtf.Record( mpImplData->mxPrinter.get() );

    printPage( i_nUnfilteredPage );

    o_rMtf.Stop();
    o_rMtf.WindStart();
    mpImplData->mxPrinter->Pop();

    if( i_bMayUseCache )
        mpImplData->maPageCache.insert( i_nUnfilteredPage, o_rMtf, aPageSize );

    // reset "FirstPage" property to false now we've gotten at least our first one
    mpImplData->mbFirstPage = false;

    return aPageSize;
}

static void appendSubPage( GDIMetaFile& o_rMtf, const tools::Rectangle& i_rClipRect, GDIMetaFile& io_rSubPage, bool i_bDrawBorder )
{
    // intersect all clipregion actions with our clip rect
    io_rSubPage.WindStart();
    io_rSubPage.Clip( i_rClipRect );

    // save gstate
    o_rMtf.AddAction( new MetaPushAction( PushFlags::ALL ) );

    // clip to page rect
    o_rMtf.AddAction( new MetaClipRegionAction( vcl::Region( i_rClipRect ), true ) );

    // append the subpage
    io_rSubPage.WindStart();
    io_rSubPage.Play( o_rMtf );

    // restore gstate
    o_rMtf.AddAction( new MetaPopAction() );

    // draw a border
    if( !i_bDrawBorder )
        return;

    // save gstate
    o_rMtf.AddAction( new MetaPushAction( PushFlags::LINECOLOR | PushFlags::FILLCOLOR | PushFlags::CLIPREGION | PushFlags::MAPMODE ) );
    o_rMtf.AddAction( new MetaMapModeAction( MapMode( MapUnit::Map100thMM ) ) );

    tools::Rectangle aBorderRect( i_rClipRect );
    o_rMtf.AddAction( new MetaLineColorAction( COL_BLACK, true ) );
    o_rMtf.AddAction( new MetaFillColorAction( COL_TRANSPARENT, false ) );
    o_rMtf.AddAction( new MetaRectAction( aBorderRect ) );

    // restore gstate
    o_rMtf.AddAction( new MetaPopAction() );
}

PrinterController::PageSize PrinterController::getFilteredPageFile( int i_nFilteredPage, GDIMetaFile& o_rMtf, bool i_bMayUseCache )
{
    const MultiPageSetup& rMPS( mpImplData->maMultiPage );
    int nSubPages = rMPS.nRows * rMPS.nColumns;
    if( nSubPages < 1 )
        nSubPages = 1;

    // reverse sheet order
    if( mpImplData->mbReversePageOrder )
    {
        int nDocPages = getFilteredPageCount();
        i_nFilteredPage = nDocPages - 1 - i_nFilteredPage;
    }

    // there is no filtering to be done (and possibly the page size of the
    // original page is to be set), when N-Up is "neutral" that is there is
    // only one subpage and the margins are 0
    if( nSubPages == 1 &&
        rMPS.nLeftMargin == 0 && rMPS.nRightMargin == 0 &&
        rMPS.nTopMargin == 0 && rMPS.nBottomMargin == 0 )
    {
        PrinterController::PageSize aPageSize = getPageFile( i_nFilteredPage, o_rMtf, i_bMayUseCache );
        if (mpImplData->meJobState != css::view::PrintableState_JOB_STARTED)
        {   // rhbz#657394: check that we are still printing...
            return PrinterController::PageSize();
        }
        Size aPaperSize = mpImplData->getRealPaperSize( aPageSize.aSize, true );
        mpImplData->mxPrinter->SetMapMode( MapMode( MapUnit::Map100thMM ) );
        mpImplData->mxPrinter->SetPaperSizeUser( aPaperSize );
        if( aPaperSize != aPageSize.aSize )
        {
            // user overridden page size, center Metafile
            o_rMtf.WindStart();
            tools::Long nDX = (aPaperSize.Width() - aPageSize.aSize.Width()) / 2;
            tools::Long nDY = (aPaperSize.Height() - aPageSize.aSize.Height()) / 2;
            o_rMtf.Move( nDX, nDY, mpImplData->mxPrinter->GetDPIX(), mpImplData->mxPrinter->GetDPIY() );
            o_rMtf.WindStart();
            o_rMtf.SetPrefSize( aPaperSize );
            aPageSize.aSize = aPaperSize;
        }
        return aPageSize;
    }

    // set last page property really only on the very last page to be rendered
    // that is on the last subpage of a NUp run
    bool bIsLastPage = mpImplData->mbLastPage;
    mpImplData->mbLastPage = false;

    Size aPaperSize( mpImplData->getRealPaperSize( mpImplData->maMultiPage.aPaperSize, false ) );

    // multi page area: page size minus margins + one time spacing right and down
    // the added spacing is so each subpage can be calculated including its spacing
    Size aMPArea( aPaperSize );
    aMPArea.AdjustWidth( -(rMPS.nLeftMargin + rMPS.nRightMargin) );
    aMPArea.AdjustWidth(rMPS.nHorizontalSpacing );
    aMPArea.AdjustHeight( -(rMPS.nTopMargin + rMPS.nBottomMargin) );
    aMPArea.AdjustHeight(rMPS.nVerticalSpacing );

    // determine offsets
    tools::Long nAdvX = aMPArea.Width() / rMPS.nColumns;
    tools::Long nAdvY = aMPArea.Height() / rMPS.nRows;

    // determine size of a "cell" subpage, leave a little space around pages
    Size aSubPageSize( nAdvX - rMPS.nHorizontalSpacing, nAdvY - rMPS.nVerticalSpacing );

    o_rMtf.Clear();
    o_rMtf.SetPrefSize( aPaperSize );
    o_rMtf.SetPrefMapMode( MapMode( MapUnit::Map100thMM ) );
    o_rMtf.AddAction( new MetaMapModeAction( MapMode( MapUnit::Map100thMM ) ) );

    int nDocPages = getPageCountProtected();
    if (mpImplData->meJobState != css::view::PrintableState_JOB_STARTED)
    {   // rhbz#657394: check that we are still printing...
        return PrinterController::PageSize();
    }
    for( int nSubPage = 0; nSubPage < nSubPages; nSubPage++ )
    {
        // map current sub page to real page
        int nPage = i_nFilteredPage * nSubPages + nSubPage;
        if( nSubPage == nSubPages-1 ||
            nPage == nDocPages-1 )
        {
            mpImplData->mbLastPage = bIsLastPage;
        }
        if( nPage >= 0 && nPage < nDocPages )
        {
            GDIMetaFile aPageFile;
            PrinterController::PageSize aPageSize = getPageFile( nPage, aPageFile, i_bMayUseCache );
            if( aPageSize.aSize.Width() && aPageSize.aSize.Height() )
            {
                tools::Long nCellX = 0, nCellY = 0;
                switch( rMPS.nOrder )
                {
                case NupOrderType::LRTB:
                    nCellX = (nSubPage % rMPS.nColumns);
                    nCellY = (nSubPage / rMPS.nColumns);
                    break;
                case NupOrderType::TBLR:
                    nCellX = (nSubPage / rMPS.nRows);
                    nCellY = (nSubPage % rMPS.nRows);
                    break;
                case NupOrderType::RLTB:
                    nCellX = rMPS.nColumns - 1 - (nSubPage % rMPS.nColumns);
                    nCellY = (nSubPage / rMPS.nColumns);
                    break;
                case NupOrderType::TBRL:
                    nCellX = rMPS.nColumns - 1 - (nSubPage / rMPS.nRows);
                    nCellY = (nSubPage % rMPS.nRows);
                    break;
                }
                // scale the metafile down to a sub page size
                double fScaleX = double(aSubPageSize.Width())/double(aPageSize.aSize.Width());
                double fScaleY = double(aSubPageSize.Height())/double(aPageSize.aSize.Height());
                double fScale  = std::min( fScaleX, fScaleY );
                aPageFile.Scale( fScale, fScale );
                aPageFile.WindStart();

                // move the subpage so it is centered in its "cell"
                tools::Long nOffX = (aSubPageSize.Width() - tools::Long(double(aPageSize.aSize.Width()) * fScale)) / 2;
                tools::Long nOffY = (aSubPageSize.Height() - tools::Long(double(aPageSize.aSize.Height()) * fScale)) / 2;
                tools::Long nX = rMPS.nLeftMargin + nOffX + nAdvX * nCellX;
                tools::Long nY = rMPS.nTopMargin + nOffY + nAdvY * nCellY;
                aPageFile.Move( nX, nY, mpImplData->mxPrinter->GetDPIX(), mpImplData->mxPrinter->GetDPIY() );
                aPageFile.WindStart();
                // calculate border rectangle
                tools::Rectangle aSubPageRect( Point( nX, nY ),
                                        Size( tools::Long(double(aPageSize.aSize.Width())*fScale),
                                              tools::Long(double(aPageSize.aSize.Height())*fScale) ) );

                // append subpage to page
                appendSubPage( o_rMtf, aSubPageRect, aPageFile, rMPS.bDrawBorder );
            }
        }
    }
    o_rMtf.WindStart();

    // subsequent getPageFile calls have changed the paper, reset it to current value
    mpImplData->mxPrinter->SetMapMode( MapMode( MapUnit::Map100thMM ) );
    mpImplData->mxPrinter->SetPaperSizeUser( aPaperSize );

    return PrinterController::PageSize( aPaperSize, true );
}

int PrinterController::getFilteredPageCount() const
{
    int nDiv = mpImplData->maMultiPage.nRows * mpImplData->maMultiPage.nColumns;
    if( nDiv < 1 )
        nDiv = 1;
    return (getPageCountProtected() + (nDiv-1)) / nDiv;
}

DrawModeFlags PrinterController::removeTransparencies( GDIMetaFile const & i_rIn, GDIMetaFile& o_rOut )
{
    DrawModeFlags nRestoreDrawMode = mpImplData->mxPrinter->GetDrawMode();
    sal_Int32 nMaxBmpDPIX = mpImplData->mxPrinter->GetDPIX();
    sal_Int32 nMaxBmpDPIY = mpImplData->mxPrinter->GetDPIY();

    const vcl::printer::Options& rPrinterOptions = mpImplData->mxPrinter->GetPrinterOptions();

    static const sal_Int32 OPTIMAL_BMP_RESOLUTION = 300;
    static const sal_Int32 NORMAL_BMP_RESOLUTION  = 200;

    if( rPrinterOptions.IsReduceBitmaps() )
    {
        // calculate maximum resolution for bitmap graphics
        if( printer::BitmapMode::Optimal == rPrinterOptions.GetReducedBitmapMode() )
        {
            nMaxBmpDPIX = std::min( sal_Int32(OPTIMAL_BMP_RESOLUTION), nMaxBmpDPIX );
            nMaxBmpDPIY = std::min( sal_Int32(OPTIMAL_BMP_RESOLUTION), nMaxBmpDPIY );
        }
        else if( printer::BitmapMode::Normal == rPrinterOptions.GetReducedBitmapMode() )
        {
            nMaxBmpDPIX = std::min( sal_Int32(NORMAL_BMP_RESOLUTION), nMaxBmpDPIX );
            nMaxBmpDPIY = std::min( sal_Int32(NORMAL_BMP_RESOLUTION), nMaxBmpDPIY );
        }
        else
        {
            nMaxBmpDPIX = std::min( sal_Int32(rPrinterOptions.GetReducedBitmapResolution()), nMaxBmpDPIX );
            nMaxBmpDPIY = std::min( sal_Int32(rPrinterOptions.GetReducedBitmapResolution()), nMaxBmpDPIY );
        }
    }

    // convert to greyscales
    if( rPrinterOptions.IsConvertToGreyscales() )
    {
        mpImplData->mxPrinter->SetDrawMode( mpImplData->mxPrinter->GetDrawMode() |
                                            ( DrawModeFlags::GrayLine | DrawModeFlags::GrayFill | DrawModeFlags::GrayText |
                                              DrawModeFlags::GrayBitmap | DrawModeFlags::GrayGradient ) );
    }

    // disable transparency output
    if( rPrinterOptions.IsReduceTransparency() && ( vcl::printer::TransparencyMode::NONE == rPrinterOptions.GetReducedTransparencyMode() ) )
    {
        mpImplData->mxPrinter->SetDrawMode( mpImplData->mxPrinter->GetDrawMode() | DrawModeFlags::NoTransparency );
    }

    Color aBg( COL_TRANSPARENT ); // default: let RemoveTransparenciesFromMetaFile do its own background logic
    if( mpImplData->maMultiPage.nRows * mpImplData->maMultiPage.nColumns > 1 )
    {
        // in N-Up printing we have no "page" background operation
        // we also have no way to determine the paper color
        // so let's go for white, which will kill 99.9% of the real cases
        aBg = COL_WHITE;
    }
    mpImplData->mxPrinter->RemoveTransparenciesFromMetaFile( i_rIn, o_rOut, nMaxBmpDPIX, nMaxBmpDPIY,
                                                             rPrinterOptions.IsReduceTransparency(),
                                                             rPrinterOptions.GetReducedTransparencyMode() == vcl::printer::TransparencyMode::Auto,
                                                             rPrinterOptions.IsReduceBitmaps() && rPrinterOptions.IsReducedBitmapIncludesTransparency(),
                                                             aBg
                                                             );
    return nRestoreDrawMode;
}

void PrinterController::printFilteredPage( int i_nPage )
{
    if( mpImplData->meJobState != css::view::PrintableState_JOB_STARTED )
        return; // rhbz#657394: check that we are still printing...

    GDIMetaFile aPageFile;
    PrinterController::PageSize aPageSize = getFilteredPageFile( i_nPage, aPageFile );

    if( mpImplData->mxProgress )
    {
        // do nothing if printing is canceled
        if( mpImplData->mxProgress->isCanceled() )
        {
            setJobState( css::view::PrintableState_JOB_ABORTED );
            return;
        }
    }

    // in N-Up printing set the correct page size
    mpImplData->mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    // aPageSize was filtered through mpImplData->getRealPaperSize already by getFilteredPageFile()
    mpImplData->mxPrinter->SetPaperSizeUser( aPageSize.aSize );
    if( mpImplData->mnFixedPaperBin != -1 &&
        mpImplData->mxPrinter->GetPaperBin() != mpImplData->mnFixedPaperBin )
    {
        mpImplData->mxPrinter->SetPaperBin( mpImplData->mnFixedPaperBin );
    }

    // if full paper is meant to be used, move the output to accommodate for pageoffset
    if( aPageSize.bFullPaper )
    {
        Point aPageOffset( mpImplData->mxPrinter->GetPageOffset() );
        aPageFile.WindStart();
        aPageFile.Move( -aPageOffset.X(), -aPageOffset.Y(), mpImplData->mxPrinter->GetDPIX(), mpImplData->mxPrinter->GetDPIY() );
    }

    GDIMetaFile aCleanedFile;
    DrawModeFlags nRestoreDrawMode = removeTransparencies( aPageFile, aCleanedFile );

    mpImplData->mxPrinter->EnableOutput();

    // actually print the page
    mpImplData->mxPrinter->ImplStartPage();

    mpImplData->mxPrinter->Push();
    aCleanedFile.WindStart();
    aCleanedFile.Play(*mpImplData->mxPrinter);
    mpImplData->mxPrinter->Pop();

    mpImplData->mxPrinter->ImplEndPage();

    mpImplData->mxPrinter->SetDrawMode( nRestoreDrawMode );
}

void PrinterController::jobStarted()
{
}

void PrinterController::jobFinished( css::view::PrintableState )
{
}

void PrinterController::abortJob()
{
    setJobState( css::view::PrintableState_JOB_ABORTED );
    // applications (well, sw) depend on a page request with "IsLastPage" = true
    // to free resources, else they (well, sw) will crash eventually
    setLastPage( true );

    if (mpImplData->mxProgress)
    {
        mpImplData->mxProgress->response(RET_CANCEL);
        mpImplData->mxProgress.reset();
    }

    GDIMetaFile aMtf;
    getPageFile( 0, aMtf );
}

void PrinterController::setLastPage( bool i_bLastPage )
{
    mpImplData->mbLastPage = i_bLastPage;
}

void PrinterController::setReversePrint( bool i_bReverse )
{
    mpImplData->mbReversePageOrder = i_bReverse;
}

void PrinterController::setPapersizeFromSetup( bool i_bPapersizeFromSetup )
{
    mpImplData->mbPapersizeFromSetup = i_bPapersizeFromSetup;
    mpImplData->mxPrinter->SetPrinterSettingsPreferred( i_bPapersizeFromSetup );
    if ( i_bPapersizeFromSetup )
    {
        mpImplData->mbPapersizeFromUser = false;
        mpImplData->mbOrientationFromUser = false;
    }
}

bool PrinterController::getPapersizeFromSetup() const
{
    return mpImplData->mbPapersizeFromSetup;
}

void PrinterController::setPaperSizeFromUser( Size i_aUserSize )
{
    mpImplData->mbPapersizeFromUser = true;
    mpImplData->mbPapersizeFromSetup = false;
    mpImplData->mxPrinter->SetPrinterSettingsPreferred( false );

    mpImplData->maUserPageSize = i_aUserSize;
}

void PrinterController::setOrientationFromUser( Orientation eOrientation, bool set )
{
    mpImplData->mbOrientationFromUser = set;
    mpImplData->meUserOrientation = eOrientation;
}

void PrinterController::setPrinterModified( bool i_bPrinterModified )
{
    mpImplData->mbPrinterModified = i_bPrinterModified;
}

bool PrinterController::getPrinterModified() const
{
    return mpImplData->mbPrinterModified;
}

css::uno::Sequence< css::beans::PropertyValue > PrinterController::getJobProperties( const css::uno::Sequence< css::beans::PropertyValue >& i_rMergeList ) const
{
    std::unordered_set< OUString > aMergeSet;
    size_t nResultLen = size_t(i_rMergeList.getLength()) + mpImplData->maUIProperties.size() + 3;
    for( const auto& rPropVal : i_rMergeList )
        aMergeSet.insert( rPropVal.Name );

    css::uno::Sequence< css::beans::PropertyValue > aResult( nResultLen );
    auto pResult = aResult.getArray();
    std::copy(i_rMergeList.begin(), i_rMergeList.end(), pResult);
    int nCur = i_rMergeList.getLength();
    for(const css::beans::PropertyValue & rPropVal : mpImplData->maUIProperties)
    {
        if( aMergeSet.find( rPropVal.Name ) == aMergeSet.end() )
            pResult[nCur++] = rPropVal;
    }
    // append IsFirstPage
    if( aMergeSet.find( u"IsFirstPage"_ustr ) == aMergeSet.end() )
    {
        css::beans::PropertyValue aVal;
        aVal.Name = "IsFirstPage";
        aVal.Value <<= mpImplData->mbFirstPage;
        pResult[nCur++] = std::move(aVal);
    }
    // append IsLastPage
    if( aMergeSet.find( u"IsLastPage"_ustr ) == aMergeSet.end() )
    {
        css::beans::PropertyValue aVal;
        aVal.Name = "IsLastPage";
        aVal.Value <<= mpImplData->mbLastPage;
        pResult[nCur++] = std::move(aVal);
    }
    // append IsPrinter
    if( aMergeSet.find( u"IsPrinter"_ustr ) == aMergeSet.end() )
    {
        css::beans::PropertyValue aVal;
        aVal.Name = "IsPrinter";
        aVal.Value <<= true;
        pResult[nCur++] = std::move(aVal);
    }
    aResult.realloc( nCur );
    return aResult;
}

const css::uno::Sequence< css::beans::PropertyValue >& PrinterController::getUIOptions() const
{
    return mpImplData->maUIOptions;
}

css::beans::PropertyValue* PrinterController::getValue( const OUString& i_rProperty )
{
    std::unordered_map< OUString, size_t >::const_iterator it =
        mpImplData->maPropertyToIndex.find( i_rProperty );
    return it != mpImplData->maPropertyToIndex.end() ? &mpImplData->maUIProperties[it->second] : nullptr;
}

const css::beans::PropertyValue* PrinterController::getValue( const OUString& i_rProperty ) const
{
    std::unordered_map< OUString, size_t >::const_iterator it =
        mpImplData->maPropertyToIndex.find( i_rProperty );
    return it != mpImplData->maPropertyToIndex.end() ? &mpImplData->maUIProperties[it->second] : nullptr;
}

void PrinterController::setValue( const OUString& i_rPropertyName, const css::uno::Any& i_rValue )
{
    css::beans::PropertyValue aVal;
    aVal.Name = i_rPropertyName;
    aVal.Value = i_rValue;

    setValue( aVal );
}

void PrinterController::setValue( const css::beans::PropertyValue& i_rPropertyValue )
{
    std::unordered_map< OUString, size_t >::const_iterator it =
        mpImplData->maPropertyToIndex.find( i_rPropertyValue.Name );
    if( it != mpImplData->maPropertyToIndex.end() )
        mpImplData->maUIProperties[ it->second ] = i_rPropertyValue;
    else
    {
        // insert correct index into property map
        mpImplData->maPropertyToIndex[ i_rPropertyValue.Name ] = mpImplData->maUIProperties.size();
        mpImplData->maUIProperties.push_back( i_rPropertyValue );
        mpImplData->maUIPropertyEnabled.push_back( true );
    }
}

void PrinterController::setUIOptions( const css::uno::Sequence< css::beans::PropertyValue >& i_rOptions )
{
    SAL_WARN_IF( mpImplData->maUIOptions.hasElements(), "vcl.gdi", "setUIOptions called twice !" );

    mpImplData->maUIOptions = i_rOptions;

    for( const auto& rOpt : i_rOptions )
    {
        css::uno::Sequence< css::beans::PropertyValue > aOptProp;
        rOpt.Value >>= aOptProp;
        bool bIsEnabled = true;
        bool bHaveProperty = false;
        OUString aPropName;
        vcl::ImplPrinterControllerData::ControlDependency aDep;
        css::uno::Sequence< sal_Bool > aChoicesDisabled;
        for (const css::beans::PropertyValue& rEntry : aOptProp)
        {
            if ( rEntry.Name == "Property" )
            {
                css::beans::PropertyValue aVal;
                rEntry.Value >>= aVal;
                DBG_ASSERT( mpImplData->maPropertyToIndex.find( aVal.Name )
                            == mpImplData->maPropertyToIndex.end(), "duplicate property entry" );
                setValue( aVal );
                aPropName = aVal.Name;
                bHaveProperty = true;
            }
            else if ( rEntry.Name == "Enabled" )
            {
                bool bValue = true;
                rEntry.Value >>= bValue;
                bIsEnabled = bValue;
            }
            else if ( rEntry.Name == "DependsOnName" )
            {
                rEntry.Value >>= aDep.maDependsOnName;
            }
            else if ( rEntry.Name == "DependsOnEntry" )
            {
                rEntry.Value >>= aDep.mnDependsOnEntry;
            }
            else if ( rEntry.Name == "ChoicesDisabled" )
            {
                rEntry.Value >>= aChoicesDisabled;
            }
        }
        if( bHaveProperty )
        {
            vcl::ImplPrinterControllerData::PropertyToIndexMap::const_iterator it =
                mpImplData->maPropertyToIndex.find( aPropName );
            // sanity check
            if( it != mpImplData->maPropertyToIndex.end() )
            {
                mpImplData->maUIPropertyEnabled[ it->second ] = bIsEnabled;
            }
            if( !aDep.maDependsOnName.isEmpty() )
                mpImplData->maControlDependencies[ aPropName ] = std::move(aDep);
            if( aChoicesDisabled.hasElements() )
                mpImplData->maChoiceDisableMap[ aPropName ] = std::move(aChoicesDisabled);
        }
    }
}

bool PrinterController::isUIOptionEnabled( const OUString& i_rProperty ) const
{
    bool bEnabled = false;
    std::unordered_map< OUString, size_t >::const_iterator prop_it =
        mpImplData->maPropertyToIndex.find( i_rProperty );
    if( prop_it != mpImplData->maPropertyToIndex.end() )
    {
        bEnabled = mpImplData->maUIPropertyEnabled[prop_it->second];

        if( bEnabled )
        {
            // check control dependencies
            vcl::ImplPrinterControllerData::ControlDependencyMap::const_iterator it =
                mpImplData->maControlDependencies.find( i_rProperty );
            if( it != mpImplData->maControlDependencies.end() )
            {
                // check if the dependency is enabled
                // if the dependency is disabled, we are too
                bEnabled = isUIOptionEnabled( it->second.maDependsOnName );

                if( bEnabled )
                {
                    // does the dependency have the correct value ?
                    const css::beans::PropertyValue* pVal = getValue( it->second.maDependsOnName );
                    OSL_ENSURE( pVal, "unknown property in dependency" );
                    if( pVal )
                    {
                        sal_Int32 nDepVal = 0;
                        bool bDepVal = false;
                        if( pVal->Value >>= nDepVal )
                        {
                            bEnabled = (nDepVal == it->second.mnDependsOnEntry) || (it->second.mnDependsOnEntry == -1);
                        }
                        else if( pVal->Value >>= bDepVal )
                        {
                            // could be a dependency on a checked boolean
                            // in this case the dependency is on a non zero for checked value
                            bEnabled = (   bDepVal && it->second.mnDependsOnEntry != 0) ||
                                       ( ! bDepVal && it->second.mnDependsOnEntry == 0);
                        }
                        else
                        {
                            // if the type does not match something is awry
                            OSL_FAIL( "strange type in control dependency" );
                            bEnabled = false;
                        }
                    }
                }
            }
        }
    }
    return bEnabled;
}

void PrinterController::setUIChoicesDisabled(const OUString& rPropName, css::uno::Sequence<sal_Bool>& rChoicesDisabled)
{
    mpImplData->maChoiceDisableMap[rPropName] = std::move(rChoicesDisabled);
}

bool PrinterController::isUIChoiceEnabled( const OUString& i_rProperty, sal_Int32 i_nValue ) const
{
    bool bEnabled = true;
    ImplPrinterControllerData::ChoiceDisableMap::const_iterator it =
        mpImplData->maChoiceDisableMap.find( i_rProperty );
    if(it != mpImplData->maChoiceDisableMap.end() )
    {
        const css::uno::Sequence< sal_Bool >& rDisabled( it->second );
        if( i_nValue >= 0 && i_nValue < rDisabled.getLength() )
            bEnabled = ! rDisabled[i_nValue];
    }
    return bEnabled;
}

OUString PrinterController::makeEnabled( const OUString& i_rProperty )
{
    OUString aDependency;

    vcl::ImplPrinterControllerData::ControlDependencyMap::const_iterator it =
        mpImplData->maControlDependencies.find( i_rProperty );
    if( it != mpImplData->maControlDependencies.end() )
    {
        if( isUIOptionEnabled( it->second.maDependsOnName ) )
        {
           aDependency = it->second.maDependsOnName;
           const css::beans::PropertyValue* pVal = getValue( aDependency );
           OSL_ENSURE( pVal, "unknown property in dependency" );
           if( pVal )
           {
               sal_Int32 nDepVal = 0;
               bool bDepVal = false;
               if( pVal->Value >>= nDepVal )
               {
                   if( it->second.mnDependsOnEntry != -1 )
                   {
                       setValue( aDependency, css::uno::Any( sal_Int32( it->second.mnDependsOnEntry ) ) );
                   }
               }
               else if( pVal->Value >>= bDepVal )
               {
                   setValue( aDependency, css::uno::Any( it->second.mnDependsOnEntry != 0 ) );
               }
               else
               {
                   // if the type does not match something is awry
                   OSL_FAIL( "strange type in control dependency" );
               }
           }
        }
    }

    return aDependency;
}

void PrinterController::createProgressDialog()
{
    if (!mpImplData->mxProgress)
    {
        bool bShow = true;
        css::beans::PropertyValue* pMonitor = getValue( u"MonitorVisible"_ustr );
        if( pMonitor )
            pMonitor->Value >>= bShow;
        else
        {
            const css::beans::PropertyValue* pVal = getValue( u"IsApi"_ustr );
            if( pVal )
            {
                bool bApi = false;
                pVal->Value >>= bApi;
                bShow = ! bApi;
            }
        }

        if( bShow && ! Application::IsHeadlessModeEnabled() )
        {
            mpImplData->mxProgress = std::make_shared<PrintProgressDialog>(getWindow(), getPageCountProtected());
            weld::DialogController::runAsync(mpImplData->mxProgress, [](sal_Int32 /*nResult*/){});
        }
    }
    else
    {
        mpImplData->mxProgress->response(RET_CANCEL);
        mpImplData->mxProgress.reset();
    }
}

bool PrinterController::isProgressCanceled() const
{
    return mpImplData->mxProgress && mpImplData->mxProgress->isCanceled();
}

void PrinterController::setMultipage( const MultiPageSetup& i_rMPS )
{
    mpImplData->maMultiPage = i_rMPS;
}

const PrinterController::MultiPageSetup& PrinterController::getMultipage() const
{
    return mpImplData->maMultiPage;
}

void PrinterController::resetPaperToLastConfigured()
{
    mpImplData->resetPaperToLastConfigured();
}

void PrinterController::pushPropertiesToPrinter()
{
    sal_Int32 nCopyCount = 1;
    // set copycount and collate
    const css::beans::PropertyValue* pVal = getValue( u"CopyCount"_ustr );
    if( pVal )
        pVal->Value >>= nCopyCount;
    bool bCollate = false;
    pVal = getValue( u"Collate"_ustr );
    if( pVal )
        pVal->Value >>= bCollate;
    mpImplData->mxPrinter->SetCopyCount( static_cast<sal_uInt16>(nCopyCount), bCollate );

    pVal = getValue(u"SinglePrintJobs"_ustr);
    bool bSinglePrintJobs = false;
    if (pVal)
        pVal->Value >>= bSinglePrintJobs;
    mpImplData->mxPrinter->SetSinglePrintJobs(bSinglePrintJobs);

    // duplex mode
    pVal = getValue( u"DuplexMode"_ustr );
    if( pVal )
    {
        sal_Int16 nDuplex = css::view::DuplexMode::UNKNOWN;
        pVal->Value >>= nDuplex;
        switch( nDuplex )
        {
            case css::view::DuplexMode::OFF: mpImplData->mxPrinter->SetDuplexMode( DuplexMode::Off ); break;
            case css::view::DuplexMode::LONGEDGE: mpImplData->mxPrinter->SetDuplexMode( DuplexMode::LongEdge ); break;
            case css::view::DuplexMode::SHORTEDGE: mpImplData->mxPrinter->SetDuplexMode( DuplexMode::ShortEdge ); break;
        }
    }
}

bool PrinterController::isShowDialogs() const
{
    bool bApi = getBoolProperty( u"IsApi"_ustr, false );
    return ! bApi && ! Application::IsHeadlessModeEnabled();
}

bool PrinterController::isDirectPrint() const
{
    bool bDirect = getBoolProperty( u"IsDirect"_ustr, false );
    return bDirect;
}

bool PrinterController::getBoolProperty( const OUString& i_rProperty, bool i_bFallback ) const
{
    bool bRet = i_bFallback;
    const css::beans::PropertyValue* pVal = getValue( i_rProperty );
    if( pVal )
        pVal->Value >>= bRet;
    return bRet;
}

sal_Int32 PrinterController::getIntProperty( const OUString& i_rProperty, sal_Int32 i_nFallback ) const
{
    sal_Int32 nRet = i_nFallback;
    const css::beans::PropertyValue* pVal = getValue( i_rProperty );
    if( pVal )
        pVal->Value >>= nRet;
    return nRet;
}

/*
 * PrinterOptionsHelper
**/
css::uno::Any PrinterOptionsHelper::getValue( const OUString& i_rPropertyName ) const
{
    css::uno::Any aRet;
    std::unordered_map< OUString, css::uno::Any >::const_iterator it =
        m_aPropertyMap.find( i_rPropertyName );
    if( it != m_aPropertyMap.end() )
        aRet = it->second;
    return aRet;
}

bool PrinterOptionsHelper::getBoolValue( const OUString& i_rPropertyName, bool i_bDefault ) const
{
    bool bRet = false;
    css::uno::Any aVal( getValue( i_rPropertyName ) );
    return (aVal >>= bRet) ? bRet : i_bDefault;
}

sal_Int64 PrinterOptionsHelper::getIntValue( const OUString& i_rPropertyName, sal_Int64 i_nDefault ) const
{
    sal_Int64 nRet = 0;
    css::uno::Any aVal( getValue( i_rPropertyName ) );
    return (aVal >>= nRet) ? nRet : i_nDefault;
}

OUString PrinterOptionsHelper::getStringValue( const OUString& i_rPropertyName ) const
{
    OUString aRet;
    css::uno::Any aVal( getValue( i_rPropertyName ) );
    return (aVal >>= aRet) ? aRet : OUString();
}

bool PrinterOptionsHelper::processProperties( const css::uno::Sequence< css::beans::PropertyValue >& i_rNewProp )
{
    bool bChanged = false;

    for( const auto& rVal : i_rNewProp )
    {
        std::unordered_map< OUString, css::uno::Any >::iterator it =
            m_aPropertyMap.find( rVal.Name );

        bool bElementChanged = (it == m_aPropertyMap.end()) || (it->second != rVal.Value);
        if( bElementChanged )
        {
            m_aPropertyMap[ rVal.Name ] = rVal.Value;
            bChanged = true;
        }
    }
    return bChanged;
}

void PrinterOptionsHelper::appendPrintUIOptions( css::uno::Sequence< css::beans::PropertyValue >& io_rProps ) const
{
    if( !m_aUIProperties.empty() )
    {
        sal_Int32 nIndex = io_rProps.getLength();
        io_rProps.realloc( nIndex+1 );
        io_rProps.getArray()[ nIndex ] = comphelper::makePropertyValue(
            u"ExtraPrintUIOptions"_ustr, comphelper::containerToSequence(m_aUIProperties));
    }
}

css::uno::Any PrinterOptionsHelper::setUIControlOpt(const css::uno::Sequence< OUString >& i_rIDs,
                                          const OUString& i_rTitle,
                                          const css::uno::Sequence< OUString >& i_rHelpIds,
                                          const OUString& i_rType,
                                          const css::beans::PropertyValue* i_pVal,
                                          const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    sal_Int32 nElements =
        2                                                             // ControlType + ID
        + (i_rTitle.isEmpty() ? 0 : 1)                                // Text
        + (i_rHelpIds.hasElements() ? 1 : 0)                          // HelpId
        + (i_pVal ? 1 : 0)                                            // Property
        + i_rControlOptions.maAddProps.size()                         // additional props
        + (i_rControlOptions.maGroupHint.isEmpty() ? 0 : 1)           // grouping
        + (i_rControlOptions.mbInternalOnly ? 1 : 0)                  // internal hint
        + (i_rControlOptions.mbEnabled ? 0 : 1)                       // enabled
        ;
    if( !i_rControlOptions.maDependsOnName.isEmpty() )
    {
        nElements += 1;
        if( i_rControlOptions.mnDependsOnEntry != -1 )
            nElements += 1;
        if( i_rControlOptions.mbAttachToDependency )
            nElements += 1;
    }

    css::uno::Sequence< css::beans::PropertyValue > aCtrl( nElements );
    auto pCtrl = aCtrl.getArray();
    sal_Int32 nUsed = 0;
    if( !i_rTitle.isEmpty() )
    {
        pCtrl[nUsed  ].Name  = "Text";
        pCtrl[nUsed++].Value <<= i_rTitle;
    }
    if( i_rHelpIds.hasElements() )
    {
        pCtrl[nUsed  ].Name = "HelpId";
        pCtrl[nUsed++].Value <<= i_rHelpIds;
    }
    pCtrl[nUsed  ].Name  = "ControlType";
    pCtrl[nUsed++].Value <<= i_rType;
    pCtrl[nUsed  ].Name  = "ID";
    pCtrl[nUsed++].Value <<= i_rIDs;
    if( i_pVal )
    {
        pCtrl[nUsed  ].Name  = "Property";
        pCtrl[nUsed++].Value <<= *i_pVal;
    }
    if( !i_rControlOptions.maDependsOnName.isEmpty() )
    {
        pCtrl[nUsed  ].Name  = "DependsOnName";
        pCtrl[nUsed++].Value <<= i_rControlOptions.maDependsOnName;
        if( i_rControlOptions.mnDependsOnEntry != -1 )
        {
            pCtrl[nUsed  ].Name  = "DependsOnEntry";
            pCtrl[nUsed++].Value <<= i_rControlOptions.mnDependsOnEntry;
        }
        if( i_rControlOptions.mbAttachToDependency )
        {
            pCtrl[nUsed  ].Name  = "AttachToDependency";
            pCtrl[nUsed++].Value <<= i_rControlOptions.mbAttachToDependency;
        }
    }
    if( !i_rControlOptions.maGroupHint.isEmpty() )
    {
        pCtrl[nUsed  ].Name    = "GroupingHint";
        pCtrl[nUsed++].Value <<= i_rControlOptions.maGroupHint;
    }
    if( i_rControlOptions.mbInternalOnly )
    {
        pCtrl[nUsed  ].Name    = "InternalUIOnly";
        pCtrl[nUsed++].Value <<= true;
    }
    if( ! i_rControlOptions.mbEnabled )
    {
        pCtrl[nUsed  ].Name    = "Enabled";
        pCtrl[nUsed++].Value <<= false;
    }

    sal_Int32 nAddProps = i_rControlOptions.maAddProps.size();
    for( sal_Int32 i = 0; i < nAddProps; i++ )
        pCtrl[ nUsed++ ] = i_rControlOptions.maAddProps[i];

    SAL_WARN_IF( nUsed != nElements, "vcl.gdi", "nUsed != nElements, probable heap corruption" );

    return css::uno::Any( aCtrl );
}

css::uno::Any PrinterOptionsHelper::setGroupControlOpt(const OUString& i_rID,
                                             const OUString& i_rTitle,
                                             const OUString& i_rHelpId)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, u"Group"_ustr);
}

css::uno::Any PrinterOptionsHelper::setSubgroupControlOpt(const OUString& i_rID,
                                                const OUString& i_rTitle,
                                                const OUString& i_rHelpId,
                                                const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, u"Subgroup"_ustr, nullptr, i_rControlOptions);
}

css::uno::Any PrinterOptionsHelper::setBoolControlOpt(const OUString& i_rID,
                                            const OUString& i_rTitle,
                                            const OUString& i_rHelpId,
                                            const OUString& i_rProperty,
                                            bool i_bValue,
                                            const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_bValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, u"Bool"_ustr, &aVal, i_rControlOptions);
}

css::uno::Any PrinterOptionsHelper::setChoiceRadiosControlOpt(const css::uno::Sequence< OUString >& i_rIDs,
                                              const OUString& i_rTitle,
                                              const css::uno::Sequence< OUString >& i_rHelpId,
                                              const OUString& i_rProperty,
                                              const css::uno::Sequence< OUString >& i_rChoices,
                                              sal_Int32 i_nValue,
                                              const css::uno::Sequence< sal_Bool >& i_rDisabledChoices,
                                              const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    UIControlOptions aOpt( i_rControlOptions );
    sal_Int32 nUsed = aOpt.maAddProps.size();
    aOpt.maAddProps.resize( nUsed + 1 + (i_rDisabledChoices.hasElements() ? 1 : 0) );
    aOpt.maAddProps[nUsed].Name = "Choices";
    aOpt.maAddProps[nUsed].Value <<= i_rChoices;
    if( i_rDisabledChoices.hasElements() )
    {
        aOpt.maAddProps[nUsed+1].Name = "ChoicesDisabled";
        aOpt.maAddProps[nUsed+1].Value <<= i_rDisabledChoices;
    }

    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_nValue;
    return setUIControlOpt(i_rIDs, i_rTitle, i_rHelpId, u"Radio"_ustr, &aVal, aOpt);
}

css::uno::Any PrinterOptionsHelper::setChoiceListControlOpt(const OUString& i_rID,
                                              const OUString& i_rTitle,
                                              const css::uno::Sequence< OUString >& i_rHelpId,
                                              const OUString& i_rProperty,
                                              const css::uno::Sequence< OUString >& i_rChoices,
                                              sal_Int32 i_nValue,
                                              const css::uno::Sequence< sal_Bool >& i_rDisabledChoices,
                                              const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    UIControlOptions aOpt( i_rControlOptions );
    sal_Int32 nUsed = aOpt.maAddProps.size();
    aOpt.maAddProps.resize( nUsed + 1 + (i_rDisabledChoices.hasElements() ? 1 : 0) );
    aOpt.maAddProps[nUsed].Name = "Choices";
    aOpt.maAddProps[nUsed].Value <<= i_rChoices;
    if( i_rDisabledChoices.hasElements() )
    {
        aOpt.maAddProps[nUsed+1].Name = "ChoicesDisabled";
        aOpt.maAddProps[nUsed+1].Value <<= i_rDisabledChoices;
    }

    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_nValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, i_rHelpId, u"List"_ustr, &aVal, aOpt);
}

css::uno::Any PrinterOptionsHelper::setRangeControlOpt(const OUString& i_rID,
                                             const OUString& i_rTitle,
                                             const OUString& i_rHelpId,
                                             const OUString& i_rProperty,
                                             sal_Int32 i_nValue,
                                             sal_Int32 i_nMinValue,
                                             sal_Int32 i_nMaxValue,
                                             const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    UIControlOptions aOpt( i_rControlOptions );
    if( i_nMaxValue >= i_nMinValue )
    {
        sal_Int32 nUsed = aOpt.maAddProps.size();
        aOpt.maAddProps.resize( nUsed + 2 );
        aOpt.maAddProps[nUsed  ].Name  = "MinValue";
        aOpt.maAddProps[nUsed++].Value <<= i_nMinValue;
        aOpt.maAddProps[nUsed  ].Name  = "MaxValue";
        aOpt.maAddProps[nUsed++].Value <<= i_nMaxValue;
    }

    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_nValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, u"Range"_ustr, &aVal, aOpt);
}

css::uno::Any PrinterOptionsHelper::setEditControlOpt(const OUString& i_rID,
                                            const OUString& i_rTitle,
                                            const OUString& i_rHelpId,
                                            const OUString& i_rProperty,
                                            const OUString& i_rValue,
                                            const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_rValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, u"Edit"_ustr, &aVal, i_rControlOptions);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
