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

#include <ReportController.hxx>
#include <ReportDefinition.hxx>
#include <CondFormat.hxx>
#include <UITools.hxx>
#include <AddField.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <DateTime.hxx>

#include <sfx2/filedlghelper.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <rptui_slotid.hrc>
#include <reportformula.hxx>

#include <comphelper/documentconstants.hxx>
#include <unotools/mediadescriptor.hxx>
#include <comphelper/propertysequence.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <comphelper/types.hxx>

#include <connectivity/dbtools.hxx>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/style/ParagraphAdjust.hpp>
#include <com/sun/star/util/NumberFormatter.hpp>
#include <com/sun/star/ui/dialogs/XFilePicker3.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerControlAccess.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include <com/sun/star/frame/FrameSearchFlag.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/report/XImageControl.hpp>
#include <com/sun/star/report/XFixedLine.hpp>
#include <com/sun/star/report/Function.hpp>
#include <com/sun/star/awt/FontDescriptor.hpp>
#include <com/sun/star/sdb/XParametersSupplier.hpp>
#include <com/sun/star/sdb/CommandType.hpp>
#include <com/sun/star/sdbcx/XTablesSupplier.hpp>
#include <com/sun/star/embed/EmbedMapUnits.hpp>
#include <com/sun/star/awt/FontWeight.hpp>
#include <com/sun/star/awt/FontUnderline.hpp>
#include <com/sun/star/awt/FontSlant.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/status/FontHeight.hpp>
#include <com/sun/star/report/ReportEngine.hpp>
#include <com/sun/star/report/XFormattedField.hpp>
#include <com/sun/star/sdb/SQLContext.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/document/XUndoManagerSupplier.hpp>

#include <vcl/svapp.hxx>
#include <vcl/unohelp.hxx>

#include <i18nutil/paper.hxx>
#include <svx/fmview.hxx>
#include <editeng/memberids.h>
#include <svx/svxids.hrc>
#include <svx/svdobj.hxx>
#include <svx/unomid.hxx>
#include <svx/dataaccessdescriptor.hxx>
#include <svx/xfillit0.hxx>
#include <svx/xflclit.hxx>
#include <svx/xflgrit.hxx>
#include <svx/xflhtit.hxx>
#include <svx/xbtmpit.hxx>
#include <svx/xflftrit.hxx>
#include <svx/xsflclit.hxx>
#include <svx/xflbckit.hxx>
#include <svx/xflbmpit.hxx>
#include <svx/xflbmsli.hxx>
#include <svx/xflbmsxy.hxx>
#include <svx/xflbmtit.hxx>
#include <svx/xflboxy.hxx>
#include <svx/xflbstit.hxx>
#include <svx/xflbtoxy.hxx>
#include <svx/xfltrit.hxx>
#include <svx/xgrscit.hxx>
#include <editeng/svxenum.hxx>
#include <svx/pageitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/sizeitem.hxx>
#include <sfx2/zoomitem.hxx>
#include <svx/zoomslideritem.hxx>
#include <editeng/brushitem.hxx>
#include <svx/flagsdef.hxx>
#include <svx/svdpagv.hxx>
#include <svx/svxdlg.hxx>

#include <core_resource.hxx>
#include <DesignView.hxx>
#include <RptObject.hxx>
#include <RptUndo.hxx>
#include <strings.hxx>
#include <RptDef.hxx>
#include <ReportSection.hxx>
#include <SectionView.hxx>
#include <UndoActions.hxx>
#include <dlgpage.hxx>
#include <strings.hrc>

#include <svl/itempool.hxx>
#include <svl/itemset.hxx>
#include <svtools/cliplistener.hxx>
#include <unotools/syslocale.hxx>
#include <unotools/viewoptions.hxx>
#include <unotools/localedatawrapper.hxx>

#include <osl/mutex.hxx>
#include <PropertyForward.hxx>
#include <SectionWindow.hxx>

#include <GroupsSorting.hxx>
#include <PageNumber.hxx>
#include <UndoEnv.hxx>

#include <memory>
#include <string_view>

#include <cppuhelper/exc_hlp.hxx>
#include <unotools/confignode.hxx>

#include <ReportControllerObserver.hxx>

#define MAX_ROWS_FOR_PREVIEW    20

#define RPTUI_ID_LRSPACE    TypedWhichId<SvxLRSpaceItem>(XATTR_FILL_FIRST - 8)
#define RPTUI_ID_ULSPACE    TypedWhichId<SvxULSpaceItem>(XATTR_FILL_FIRST - 7)
#define RPTUI_ID_PAGE       TypedWhichId<SvxPageItem>(XATTR_FILL_FIRST - 6)
#define RPTUI_ID_SIZE       TypedWhichId<SvxSizeItem>(XATTR_FILL_FIRST - 5)
#define RPTUI_ID_PAGE_MODE  TypedWhichId<SfxUInt16Item>(XATTR_FILL_FIRST - 4)
#define RPTUI_ID_START      TypedWhichId<SfxUInt16Item>(XATTR_FILL_FIRST - 3)
#define RPTUI_ID_END        TypedWhichId<SfxUInt16Item>(XATTR_FILL_FIRST - 2)
#define RPTUI_ID_BRUSH      TypedWhichId<SvxBrushItem>(XATTR_FILL_FIRST - 1)
/// Note that we deliberately overlap an existing item id, so that we can have contiguous item ids for
/// the static defaults.
#define RPTUI_ID_METRIC     TypedWhichId<SfxUInt16Item>(XATTR_FILL_LAST)

static_assert((RPTUI_ID_METRIC - RPTUI_ID_LRSPACE) == 28, "Item ids are not contiguous");

using namespace ::com::sun::star;
using namespace uno;
using namespace beans;
using namespace frame;
using namespace util;
using namespace lang;
using namespace container;
using namespace sdbcx;
using namespace sdbc;
using namespace sdb;
using namespace ui;
using namespace ui::dialogs;
using namespace ::dbtools;
using namespace ::rptui;
using namespace ::dbaui;
using namespace ::comphelper;
using namespace ::cppu;


namespace
{
    void lcl_setFontWPU_nothrow(const uno::Reference< report::XReportControlFormat>& _xReportControlFormat,const sal_Int32 _nId)
    {
        if ( !_xReportControlFormat.is() )
            return;

        try
        {
            awt::FontDescriptor aFontDescriptor = _xReportControlFormat->getFontDescriptor();
            switch(_nId)
            {
                case SID_ATTR_CHAR_WEIGHT:
                    aFontDescriptor.Weight = (awt::FontWeight::NORMAL + awt::FontWeight::BOLD) - aFontDescriptor.Weight;
                    break;
                case SID_ATTR_CHAR_POSTURE:
                    aFontDescriptor.Slant = static_cast<awt::FontSlant>(static_cast<sal_Int16>(awt::FontSlant_ITALIC) - static_cast<sal_Int16>(aFontDescriptor.Slant));
                    break;
                case SID_ATTR_CHAR_UNDERLINE:
                    aFontDescriptor.Underline = awt::FontUnderline::SINGLE - aFontDescriptor.Underline;
                    break;
                default:
                    OSL_FAIL("Illegal value in default!");
                    break;
            }

            _xReportControlFormat->setFontDescriptor(aFontDescriptor);
        }
        catch(const beans::UnknownPropertyException&)
        {
        }
    }
}


static void lcl_getReportControlFormat(const Sequence< PropertyValue >& aArgs,
                                 ODesignView* _pView,
                                 uno::Reference< awt::XWindow>& _xWindow,
                                 ::std::vector< uno::Reference< uno::XInterface > >& _rControlsFormats)
{
    uno::Reference< report::XReportControlFormat> xReportControlFormat;
    if ( aArgs.hasElements() )
    {
        SequenceAsHashMap aMap(aArgs);
        xReportControlFormat = aMap.getUnpackedValueOrDefault(REPORTCONTROLFORMAT,uno::Reference< report::XReportControlFormat>());
        _xWindow = aMap.getUnpackedValueOrDefault(CURRENT_WINDOW,uno::Reference< awt::XWindow>());
    }

    if ( !xReportControlFormat.is() )
    {
        _pView->fillControlModelSelection(_rControlsFormats);
    }
    else
    {
        _rControlsFormats.push_back(xReportControlFormat);
    }

    if ( !_xWindow.is() )
        _xWindow = VCLUnoHelper::GetInterface(_pView);
}

OUString SAL_CALL OReportController::getImplementationName()
{
    return u"com.sun.star.report.comp.ReportDesign"_ustr;
}

Sequence< OUString> SAL_CALL OReportController::getSupportedServiceNames()
{
    return { u"com.sun.star.sdb.ReportDesign"_ustr };
}

#define PROPERTY_ID_ZOOMVALUE   1


OReportController::OReportController(Reference< XComponentContext > const & xContext)
    :OReportController_BASE(xContext)
    ,OPropertyStateContainer(OGenericUnoController_Base::rBHelper)
    ,m_aSelectionListeners( getMutex() )
    ,m_sMode(u"normal"_ustr)
    ,m_nSplitPos(-1)
    ,m_nPageNum(-1)
    ,m_nSelectionCount(0)
    ,m_nAspect(0)
    ,m_nZoomValue(100)
    ,m_eZoomType(SvxZoomType::PERCENT)
    ,m_bShowRuler(true)
    ,m_bGridVisible(true)
    ,m_bGridUse(true)
    ,m_bShowProperties(true)
    ,m_bHelplinesMove(true)
    ,m_bChartEnabled(false)
    ,m_bChartEnabledAsked(false)
    ,m_bInGeneratePreview(false)
{
    // new Observer
    m_pReportControllerObserver = new OXReportControllerObserver(*this);
    registerProperty(u"ZoomValue"_ustr, PROPERTY_ID_ZOOMVALUE,
                     beans::PropertyAttribute::BOUND | beans::PropertyAttribute::TRANSIENT,
                     &m_nZoomValue, ::cppu::UnoType<sal_Int16>::get());

}

OReportController::~OReportController()
{
}

IMPLEMENT_FORWARD_XTYPEPROVIDER2(OReportController,OReportController_BASE,OReportController_Listener)
IMPLEMENT_FORWARD_XINTERFACE2(OReportController,OReportController_BASE,OReportController_Listener)

void OReportController::disposing()
{

    if ( m_pClipboardNotifier.is() )
    {
        m_pClipboardNotifier->ClearCallbackLink();
        m_pClipboardNotifier->RemoveListener( getView() );
        m_pClipboardNotifier.clear();
    }
    if ( m_xGroupsFloater )
    {
        SvtViewOptions aDlgOpt(EViewType::Window, m_xGroupsFloater->get_help_id());
        aDlgOpt.SetWindowState(m_xGroupsFloater->getDialog()->get_window_state(vcl::WindowDataMask::All));
        if (m_xGroupsFloater->getDialog()->get_visible())
            m_xGroupsFloater->response(RET_CANCEL);
        m_xGroupsFloater.reset();
    }

    try
    {
        m_xHoldAlive.clear();
        m_xColumns.clear();
        ::comphelper::disposeComponent( m_xRowSet );
        ::comphelper::disposeComponent( m_xRowSetMediator );
        ::comphelper::disposeComponent( m_xFormatter );
    }
    catch(const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION( "reportdesign", "Exception caught while disposing row sets.");
    }
    m_xRowSet.clear();
    m_xRowSetMediator.clear();

    if ( m_xReportDefinition.is() )
    {
        try
        {
            OSectionWindow* pSectionWindow = nullptr;
            if ( getDesignView() )
                pSectionWindow = getDesignView()->getMarkedSection();
            if ( pSectionWindow )
                pSectionWindow->getReportSection().deactivateOle();
            clearUndoManager();
            if ( m_aReportModel )
                listen(false);
            m_pReportControllerObserver->Clear();
            m_pReportControllerObserver.clear();
        }
        catch(const uno::Exception&)
        {
            DBG_UNHANDLED_EXCEPTION("reportdesign");
        }
    }

    {
        EventObject aDisposingEvent( *this );
        m_aSelectionListeners.disposeAndClear( aDisposingEvent );
    }

    OReportController_BASE::disposing();


    try
    {
        m_xReportDefinition.clear();
        m_aReportModel.reset();
        m_xFrameLoader.clear();
        m_xReportEngine.clear();
    }
    catch(const uno::Exception&)
    {
    }
    if ( getDesignView() )
        EndListening( *getDesignView() );
    clearView();
}

FeatureState OReportController::GetState(sal_uInt16 _nId) const
{
    FeatureState aReturn;
    // (disabled automatically)
    aReturn.bEnabled = false;
    // check this first
    if ( !getView() )
        return aReturn;

    switch (_nId)
    {
        case SID_RPT_TEXTDOCUMENT:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = (m_xReportDefinition.is() && m_xReportDefinition->getMimeType() == MIMETYPE_OASIS_OPENDOCUMENT_TEXT_ASCII);
            break;
        case SID_RPT_SPREADSHEET:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = (m_xReportDefinition.is() && m_xReportDefinition->getMimeType() == MIMETYPE_OASIS_OPENDOCUMENT_SPREADSHEET_ASCII);
            break;
        case SID_REPORTHEADER_WITHOUT_UNDO:
        case SID_REPORTFOOTER_WITHOUT_UNDO:
        case SID_REPORTHEADERFOOTER:
            {
                aReturn.bEnabled = isEditable();
                OUString sText = RptResId((m_xReportDefinition.is() && m_xReportDefinition->getReportHeaderOn()) ? RID_STR_REPORTHEADERFOOTER_DELETE : RID_STR_REPORTHEADERFOOTER_INSERT);
                aReturn.sTitle = sText;
            }
            break;
        case SID_PAGEHEADER_WITHOUT_UNDO:
        case SID_PAGEFOOTER_WITHOUT_UNDO:
        case SID_PAGEHEADERFOOTER:
            {
                aReturn.bEnabled = isEditable();
                OUString sText = RptResId((m_xReportDefinition.is() && m_xReportDefinition->getPageHeaderOn()) ? RID_STR_PAGEHEADERFOOTER_DELETE : RID_STR_PAGEHEADERFOOTER_INSERT);
                aReturn.sTitle = sText;
            }
            break;
        case SID_GROUP_APPEND:
        case SID_GROUP_REMOVE:
        case SID_GROUPHEADER_WITHOUT_UNDO:
        case SID_GROUPHEADER:
        case SID_GROUPFOOTER_WITHOUT_UNDO:
        case SID_GROUPFOOTER:
            aReturn.bEnabled = isEditable();
            break;
        case SID_ADD_CONTROL_PAIR:
            aReturn.bEnabled = isEditable();
            break;
        case SID_REDO:
        case SID_UNDO:
            {
                size_t ( SfxUndoManager::*retrieveCount )( bool const ) const =
                    ( _nId == SID_UNDO ) ? &SfxUndoManager::GetUndoActionCount : &SfxUndoManager::GetRedoActionCount;

                SfxUndoManager& rUndoManager( getUndoManager() );
                aReturn.bEnabled = ( rUndoManager.*retrieveCount )( SfxUndoManager::TopLevel ) > 0;
                if ( aReturn.bEnabled )
                {
                    // TODO: add "Undo/Redo: prefix"
                    OUString ( SfxUndoManager::*retrieveComment )( size_t, bool const ) const =
                        ( _nId == SID_UNDO ) ? &SfxUndoManager::GetUndoActionComment : &SfxUndoManager::GetRedoActionComment;
                    aReturn.sTitle = (rUndoManager.*retrieveComment)( 0, SfxUndoManager::TopLevel );
                }
            }
            break;
        case SID_GETUNDOSTRINGS:
        case SID_GETREDOSTRINGS:
            {
                size_t ( SfxUndoManager::*retrieveCount )( bool const ) const =
                    ( _nId == SID_GETUNDOSTRINGS ) ? &SfxUndoManager::GetUndoActionCount : &SfxUndoManager::GetRedoActionCount;

                OUString ( SfxUndoManager::*retrieveComment )( size_t, bool const ) const =
                    ( _nId == SID_GETUNDOSTRINGS ) ? &SfxUndoManager::GetUndoActionComment : &SfxUndoManager::GetRedoActionComment;

                SfxUndoManager& rUndoManager( getUndoManager() );
                size_t nCount(( rUndoManager.*retrieveCount )( SfxUndoManager::TopLevel ));
                Sequence<OUString> aSeq(nCount);
                auto aSeqRange = asNonConstRange(aSeq);
                for (size_t n = 0; n < nCount; ++n)
                    aSeqRange[n] = (rUndoManager.*retrieveComment)( n, SfxUndoManager::TopLevel );
                aReturn.aValue <<= aSeq;
                aReturn.bEnabled = true;
            }
            break;
        case SID_OBJECT_RESIZING:
        case SID_OBJECT_SMALLESTWIDTH:
        case SID_OBJECT_SMALLESTHEIGHT:
        case SID_OBJECT_GREATESTWIDTH:
        case SID_OBJECT_GREATESTHEIGHT:
            aReturn.bEnabled = isEditable() && getDesignView()->HasSelection();
            if ( aReturn.bEnabled )
                aReturn.bEnabled = m_nSelectionCount > 1;
            break;

        case SID_DISTRIBUTE_HLEFT:
        case SID_DISTRIBUTE_HCENTER:
        case SID_DISTRIBUTE_HDISTANCE:
        case SID_DISTRIBUTE_HRIGHT:
        case SID_DISTRIBUTE_VTOP:
        case SID_DISTRIBUTE_VCENTER:
        case SID_DISTRIBUTE_VDISTANCE:
        case SID_DISTRIBUTE_VBOTTOM:
            aReturn.bEnabled = isEditable() && getDesignView()->HasSelection();
            if ( aReturn.bEnabled )
            {
                OSectionView* pSectionView = getCurrentSectionView();
                aReturn.bEnabled = pSectionView && pSectionView->GetMarkedObjectList().GetMarkCount() > 2;
            }
            break;
        case SID_ARRANGEMENU:
        case SID_FRAME_DOWN:
        case SID_FRAME_UP:
        case SID_FRAME_TO_TOP:
        case SID_FRAME_TO_BOTTOM:
        case SID_OBJECT_HEAVEN:
        case SID_OBJECT_HELL:
            aReturn.bEnabled = isEditable() && getDesignView()->HasSelection();
            if ( aReturn.bEnabled )
            {
                OSectionView* pSectionView = getCurrentSectionView();
                aReturn.bEnabled = pSectionView && pSectionView->OnlyShapesMarked();
                if ( aReturn.bEnabled )
                {
                    if ( SID_OBJECT_HEAVEN == _nId )
                        aReturn.bEnabled = pSectionView->GetLayerIdOfMarkedObjects() != RPT_LAYER_FRONT;
                    else if ( SID_OBJECT_HELL == _nId )
                        aReturn.bEnabled = pSectionView->GetLayerIdOfMarkedObjects() != RPT_LAYER_BACK;
                }
            }
            break;

        case SID_SECTION_SHRINK:
        case SID_SECTION_SHRINK_TOP:
        case SID_SECTION_SHRINK_BOTTOM:
            {
                sal_Int32 nCount = 0;
                uno::Reference<report::XSection> xSection = getDesignView()->getCurrentSection();
                if ( xSection.is() )
                {
                    nCount = xSection->getCount();
                }
                aReturn.bEnabled = isEditable() && nCount > 0;
            }
            break;
        case SID_OBJECT_ALIGN:
        case SID_OBJECT_ALIGN_LEFT:
        case SID_OBJECT_ALIGN_CENTER:
        case SID_OBJECT_ALIGN_RIGHT:
        case SID_OBJECT_ALIGN_UP:
        case SID_OBJECT_ALIGN_MIDDLE:
        case SID_OBJECT_ALIGN_DOWN:
        case SID_SECTION_ALIGN:
        case SID_SECTION_ALIGN_LEFT:
        case SID_SECTION_ALIGN_CENTER:
        case SID_SECTION_ALIGN_RIGHT:
        case SID_SECTION_ALIGN_UP:
        case SID_SECTION_ALIGN_MIDDLE:
        case SID_SECTION_ALIGN_DOWN:
            aReturn.bEnabled = isEditable() && getDesignView()->HasSelection();
            break;
        case SID_CUT:
            aReturn.bEnabled = isEditable() && getDesignView()->HasSelection() && !getDesignView()->isHandleEvent();
            break;
        case SID_COPY:
            aReturn.bEnabled = getDesignView()->HasSelection() && !getDesignView()->isHandleEvent();
            break;
        case SID_PASTE:
            aReturn.bEnabled = isEditable()  && !getDesignView()->isHandleEvent() && getDesignView()->IsPasteAllowed();
            break;
        case SID_SELECTALL:
            aReturn.bEnabled = !getDesignView()->isHandleEvent();
            break;
        case SID_SELECTALL_IN_SECTION:
            aReturn.bEnabled = !getDesignView()->isHandleEvent();
            if ( aReturn.bEnabled )
                aReturn.bEnabled = getCurrentSectionView() != nullptr;
            break;
        case SID_ESCAPE:
            aReturn.bEnabled = getDesignView()->GetMode() == DlgEdMode::Insert;
            break;
        case SID_TERMINATE_INPLACEACTIVATION:
            aReturn.bEnabled = true;
            break;
        case SID_SELECT_ALL_EDITS:
        case SID_SELECT_ALL_LABELS:
            aReturn.bEnabled = true;
            break;
        case SID_RPT_NEW_FUNCTION:
            aReturn.bEnabled = isEditable();
            break;
        case SID_COLLAPSE_SECTION:
        case SID_EXPAND_SECTION:
        case SID_NEXT_MARK:
        case SID_PREV_MARK:
            aReturn.bEnabled = isEditable() && !getDesignView()->isHandleEvent();
            break;
        case SID_SELECT:
        case SID_SELECT_REPORT:
            aReturn.bEnabled = true;
            break;
        case SID_EXECUTE_REPORT:
            aReturn.bEnabled = isConnected() && m_xReportDefinition.is();
            break;
        case SID_DELETE:
            aReturn.bEnabled = isEditable() && getDesignView()->HasSelection() && !getDesignView()->isHandleEvent();
            if ( aReturn.bEnabled )
            {
                OSectionWindow* pSectionWindow = getDesignView()->getMarkedSection();
                if ( pSectionWindow )
                    aReturn.bEnabled = !pSectionWindow->getReportSection().isUiActive();
            }
            {
                OUString sText = RptResId(RID_STR_DELETE);
                aReturn.sTitle = sText;
            }
            break;
        case SID_GRID_VISIBLE:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = m_bGridVisible;
            break;
        case SID_GRID_USE:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = m_bGridUse;
            break;
        case SID_HELPLINES_MOVE:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = m_bHelplinesMove;
            break;
        case SID_RULER:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = m_bShowRuler;
            break;
        case SID_OBJECT_SELECT:
            aReturn.bEnabled = true;
            aReturn.bChecked = getDesignView()->GetMode() == DlgEdMode::Select;
            break;
        case SID_INSERT_DIAGRAM:
            aReturn.bEnabled = isEditable();
            aReturn.bInvisible = !m_bChartEnabled;
            aReturn.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::OLE2;
            break;
        case SID_FM_FIXEDTEXT:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::ReportDesignFixedText;
            break;
        case SID_INSERT_HFIXEDLINE:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::ReportDesignHorizontalFixedLine;
            break;
        case SID_INSERT_VFIXEDLINE:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::ReportDesignVerticalFixedLine;
            break;
        case SID_FM_EDIT:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::ReportDesignFormattedField;
            break;
        case SID_FM_IMAGECONTROL:
            aReturn.bEnabled = isEditable();
            aReturn.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::ReportDesignImageControl;
            break;
        case SID_DRAWTBX_CS_BASIC:
        case SID_DRAWTBX_CS_BASIC1:
        case SID_DRAWTBX_CS_BASIC2:
        case SID_DRAWTBX_CS_BASIC3:
        case SID_DRAWTBX_CS_BASIC4:
        case SID_DRAWTBX_CS_BASIC5:
        case SID_DRAWTBX_CS_BASIC6:
        case SID_DRAWTBX_CS_BASIC7:
        case SID_DRAWTBX_CS_BASIC8:
        case SID_DRAWTBX_CS_BASIC9:
        case SID_DRAWTBX_CS_BASIC10:
        case SID_DRAWTBX_CS_BASIC11:
        case SID_DRAWTBX_CS_BASIC12:
        case SID_DRAWTBX_CS_BASIC13:
        case SID_DRAWTBX_CS_BASIC14:
        case SID_DRAWTBX_CS_BASIC15:
        case SID_DRAWTBX_CS_BASIC16:
        case SID_DRAWTBX_CS_BASIC17:
        case SID_DRAWTBX_CS_BASIC18:
        case SID_DRAWTBX_CS_BASIC19:
        case SID_DRAWTBX_CS_BASIC20:
        case SID_DRAWTBX_CS_BASIC21:
        case SID_DRAWTBX_CS_BASIC22:
            impl_fillCustomShapeState_nothrow(u"diamond", aReturn);
            break;
        case SID_DRAWTBX_CS_SYMBOL:
        case SID_DRAWTBX_CS_SYMBOL1:
        case SID_DRAWTBX_CS_SYMBOL2:
        case SID_DRAWTBX_CS_SYMBOL3:
        case SID_DRAWTBX_CS_SYMBOL4:
        case SID_DRAWTBX_CS_SYMBOL5:
        case SID_DRAWTBX_CS_SYMBOL6:
        case SID_DRAWTBX_CS_SYMBOL7:
        case SID_DRAWTBX_CS_SYMBOL8:
        case SID_DRAWTBX_CS_SYMBOL9:
        case SID_DRAWTBX_CS_SYMBOL10:
        case SID_DRAWTBX_CS_SYMBOL11:
        case SID_DRAWTBX_CS_SYMBOL12:
        case SID_DRAWTBX_CS_SYMBOL13:
        case SID_DRAWTBX_CS_SYMBOL14:
        case SID_DRAWTBX_CS_SYMBOL15:
        case SID_DRAWTBX_CS_SYMBOL16:
        case SID_DRAWTBX_CS_SYMBOL17:
        case SID_DRAWTBX_CS_SYMBOL18:
            impl_fillCustomShapeState_nothrow(u"smiley", aReturn);
            break;
        case SID_DRAWTBX_CS_ARROW:
        case SID_DRAWTBX_CS_ARROW1:
        case SID_DRAWTBX_CS_ARROW2:
        case SID_DRAWTBX_CS_ARROW3:
        case SID_DRAWTBX_CS_ARROW4:
        case SID_DRAWTBX_CS_ARROW5:
        case SID_DRAWTBX_CS_ARROW6:
        case SID_DRAWTBX_CS_ARROW7:
        case SID_DRAWTBX_CS_ARROW8:
        case SID_DRAWTBX_CS_ARROW9:
        case SID_DRAWTBX_CS_ARROW10:
        case SID_DRAWTBX_CS_ARROW11:
        case SID_DRAWTBX_CS_ARROW12:
        case SID_DRAWTBX_CS_ARROW13:
        case SID_DRAWTBX_CS_ARROW14:
        case SID_DRAWTBX_CS_ARROW15:
        case SID_DRAWTBX_CS_ARROW16:
        case SID_DRAWTBX_CS_ARROW17:
        case SID_DRAWTBX_CS_ARROW18:
        case SID_DRAWTBX_CS_ARROW19:
        case SID_DRAWTBX_CS_ARROW20:
        case SID_DRAWTBX_CS_ARROW21:
        case SID_DRAWTBX_CS_ARROW22:
        case SID_DRAWTBX_CS_ARROW23:
        case SID_DRAWTBX_CS_ARROW24:
        case SID_DRAWTBX_CS_ARROW25:
        case SID_DRAWTBX_CS_ARROW26:
            impl_fillCustomShapeState_nothrow(u"left-right-arrow", aReturn);
            break;
        case SID_DRAWTBX_CS_STAR:
        case SID_DRAWTBX_CS_STAR1:
        case SID_DRAWTBX_CS_STAR2:
        case SID_DRAWTBX_CS_STAR3:
        case SID_DRAWTBX_CS_STAR4:
        case SID_DRAWTBX_CS_STAR5:
        case SID_DRAWTBX_CS_STAR6:
        case SID_DRAWTBX_CS_STAR7:
        case SID_DRAWTBX_CS_STAR8:
        case SID_DRAWTBX_CS_STAR9:
        case SID_DRAWTBX_CS_STAR10:
        case SID_DRAWTBX_CS_STAR11:
        case SID_DRAWTBX_CS_STAR12:
            impl_fillCustomShapeState_nothrow(u"star5", aReturn);
            break;
        case SID_DRAWTBX_CS_FLOWCHART:
        case SID_DRAWTBX_CS_FLOWCHART1:
        case SID_DRAWTBX_CS_FLOWCHART2:
        case SID_DRAWTBX_CS_FLOWCHART3:
        case SID_DRAWTBX_CS_FLOWCHART4:
        case SID_DRAWTBX_CS_FLOWCHART5:
        case SID_DRAWTBX_CS_FLOWCHART6:
        case SID_DRAWTBX_CS_FLOWCHART7:
        case SID_DRAWTBX_CS_FLOWCHART8:
        case SID_DRAWTBX_CS_FLOWCHART9:
        case SID_DRAWTBX_CS_FLOWCHART10:
        case SID_DRAWTBX_CS_FLOWCHART11:
        case SID_DRAWTBX_CS_FLOWCHART12:
        case SID_DRAWTBX_CS_FLOWCHART13:
        case SID_DRAWTBX_CS_FLOWCHART14:
        case SID_DRAWTBX_CS_FLOWCHART15:
        case SID_DRAWTBX_CS_FLOWCHART16:
        case SID_DRAWTBX_CS_FLOWCHART17:
        case SID_DRAWTBX_CS_FLOWCHART18:
        case SID_DRAWTBX_CS_FLOWCHART19:
        case SID_DRAWTBX_CS_FLOWCHART20:
        case SID_DRAWTBX_CS_FLOWCHART21:
        case SID_DRAWTBX_CS_FLOWCHART22:
        case SID_DRAWTBX_CS_FLOWCHART23:
        case SID_DRAWTBX_CS_FLOWCHART24:
        case SID_DRAWTBX_CS_FLOWCHART25:
        case SID_DRAWTBX_CS_FLOWCHART26:
        case SID_DRAWTBX_CS_FLOWCHART27:
        case SID_DRAWTBX_CS_FLOWCHART28:
            impl_fillCustomShapeState_nothrow(u"flowchart-internal-storage", aReturn);
            break;
        case SID_DRAWTBX_CS_CALLOUT:
        case SID_DRAWTBX_CS_CALLOUT1:
        case SID_DRAWTBX_CS_CALLOUT2:
        case SID_DRAWTBX_CS_CALLOUT3:
        case SID_DRAWTBX_CS_CALLOUT4:
        case SID_DRAWTBX_CS_CALLOUT5:
        case SID_DRAWTBX_CS_CALLOUT6:
        case SID_DRAWTBX_CS_CALLOUT7:
            impl_fillCustomShapeState_nothrow(u"round-rectangular-callout", aReturn);
            break;
        case SID_RPT_SHOWREPORTEXPLORER:
            aReturn.bEnabled = m_xReportDefinition.is();
            aReturn.bChecked = getDesignView() && getDesignView()->isReportExplorerVisible();
            break;
        case SID_FM_ADD_FIELD:
            aReturn.bEnabled = isConnected() && isEditable() && m_xReportDefinition.is()
                && !m_xReportDefinition->getCommand().isEmpty();
            aReturn.bChecked = getDesignView() && getDesignView()->isAddFieldVisible();
            break;
        case SID_SHOW_PROPERTYBROWSER:
            aReturn.bEnabled = true;
            aReturn.bChecked = m_bShowProperties;
            break;
        case SID_PROPERTYBROWSER_LAST_PAGE:
            aReturn.bEnabled = true;
            aReturn.aValue <<= m_sLastActivePage;
            break;
        case SID_SPLIT_POSITION:
            aReturn.bEnabled = true;
            aReturn.aValue <<= getSplitPos();
            break;
        case SID_SAVEDOC:
        case SID_SAVEASDOC:
        case SID_SAVEACOPY:
            aReturn.bEnabled = isConnected() && isEditable();
            break;
        case SID_EDITDOC:
            aReturn.bChecked = isEditable();
            break;
        case SID_PAGEDIALOG:
            aReturn.bEnabled = isEditable();
            break;
        case SID_BACKGROUND_COLOR:
            impl_fillState_nothrow(PROPERTY_CONTROLBACKGROUND,aReturn);
            break;
        case SID_ATTR_CHAR_COLOR_BACKGROUND:
            aReturn.bEnabled = isEditable();
            {
                uno::Reference<report::XSection> xSection = getDesignView()->getCurrentSection();
                if ( xSection.is() )
                    try
                    {
                        aReturn.aValue <<= xSection->getBackColor();
                        const uno::Reference< report::XReportControlModel> xControlModel(getDesignView()->getCurrentControlModel(),uno::UNO_QUERY);
                        aReturn.bEnabled = !xControlModel.is();
                    }
                    catch(const beans::UnknownPropertyException&)
                    {
                    }
                else
                    aReturn.bEnabled = false;
            }
            break;
        case SID_SORTINGANDGROUPING:
            aReturn.bEnabled = true;
            aReturn.bChecked = m_xGroupsFloater && m_xGroupsFloater->getDialog()->get_visible();
            break;
        case SID_ATTR_CHAR_WEIGHT:
        case SID_ATTR_CHAR_POSTURE:
        case SID_ATTR_CHAR_UNDERLINE:
            impl_fillState_nothrow(PROPERTY_FONTDESCRIPTOR,aReturn);
            if ( aReturn.bEnabled )
            {
                awt::FontDescriptor aFontDescriptor;
                aReturn.aValue >>= aFontDescriptor;
                aReturn.aValue.clear();

                switch(_nId)
                {
                    case SID_ATTR_CHAR_WEIGHT:
                        aReturn.bChecked = awt::FontWeight::BOLD == aFontDescriptor.Weight;
                        break;
                    case SID_ATTR_CHAR_POSTURE:
                        aReturn.bChecked = awt::FontSlant_ITALIC == aFontDescriptor.Slant;
                        break;
                    case SID_ATTR_CHAR_UNDERLINE:
                        aReturn.bChecked = awt::FontUnderline::SINGLE == aFontDescriptor.Underline;
                        break;
                    default:
                        ;
                    }
            }
            break;
        case SID_ATTR_CHAR_COLOR:
        case SID_ATTR_CHAR_COLOR2:
            impl_fillState_nothrow(PROPERTY_CHARCOLOR,aReturn);
            break;
        case SID_ATTR_CHAR_FONT:
            impl_fillState_nothrow(PROPERTY_FONTDESCRIPTOR,aReturn);
            break;
        case SID_ATTR_CHAR_FONTHEIGHT:
            impl_fillState_nothrow(PROPERTY_CHARHEIGHT,aReturn);
            if ( aReturn.aValue.hasValue() )
            {
                frame::status::FontHeight aFontHeight;
                aReturn.aValue >>= aFontHeight.Height;
                aReturn.aValue <<= aFontHeight; // another type is needed here, so
            }
            break;
        case SID_ATTR_PARA_ADJUST_LEFT:
        case SID_ATTR_PARA_ADJUST_CENTER:
        case SID_ATTR_PARA_ADJUST_RIGHT:
        case SID_ATTR_PARA_ADJUST_BLOCK:
            impl_fillState_nothrow(PROPERTY_PARAADJUST,aReturn);
            if ( aReturn.bEnabled )
            {
                ::sal_Int16 nParaAdjust = 0;
                if ( aReturn.aValue >>= nParaAdjust )
                {
                    switch(static_cast<style::ParagraphAdjust>(nParaAdjust))
                    {
                        case style::ParagraphAdjust_LEFT:
                            aReturn.bChecked = _nId == SID_ATTR_PARA_ADJUST_LEFT;
                            break;
                        case style::ParagraphAdjust_RIGHT:
                            aReturn.bChecked = _nId == SID_ATTR_PARA_ADJUST_RIGHT;
                            break;
                        case style::ParagraphAdjust_BLOCK:
                        case style::ParagraphAdjust_STRETCH:
                            aReturn.bChecked = _nId == SID_ATTR_PARA_ADJUST_BLOCK;
                            break;
                        case style::ParagraphAdjust_CENTER:
                            aReturn.bChecked = _nId == SID_ATTR_PARA_ADJUST_CENTER;
                            break;
                        default: break;
                    }
                }
                aReturn.aValue.clear();
            }
            break;

        case SID_INSERT_GRAPHIC:
            aReturn.bEnabled = m_xReportDefinition.is() && isEditable() && getDesignView()->getCurrentSection().is();
            break;
        case SID_CHAR_DLG:
        case SID_SETCONTROLDEFAULTS:
            aReturn.bEnabled = m_xReportDefinition.is() && isEditable();
            if ( aReturn.bEnabled )
            {
                ::std::vector< uno::Reference< uno::XInterface > > aSelection;
                getDesignView()->fillControlModelSelection(aSelection);
                aReturn.bEnabled = !aSelection.empty()
                    && std::all_of(aSelection.begin(), aSelection.end(), [](const uno::Reference<uno::XInterface>& rxInterface) {
                        return !uno::Reference<report::XFixedLine>(rxInterface, uno::UNO_QUERY).is()
                            && !uno::Reference<report::XImageControl>(rxInterface, uno::UNO_QUERY).is()
                            && uno::Reference<report::XReportControlFormat>(rxInterface, uno::UNO_QUERY).is(); });
            }
            break;
        case SID_CONDITIONALFORMATTING:
            {
                const uno::Reference< report::XFormattedField> xFormattedField(getDesignView()->getCurrentControlModel(),uno::UNO_QUERY);
                aReturn.bEnabled = xFormattedField.is();
            }
            break;
        case SID_INSERT_FLD_PGNUMBER:
        case SID_DATETIME:
            aReturn.bEnabled = m_xReportDefinition.is() && isEditable() && getDesignView()->getCurrentSection().is();
            break;
        case SID_EXPORTDOC:
        case SID_EXPORTDOCASPDF:
            aReturn.bEnabled = m_xReportDefinition.is();
            break;
        case SID_PRINTPREVIEW:
            aReturn.bEnabled = false;
            break;
        case SID_ATTR_ZOOM:
            aReturn.bEnabled = true;
            {
                SvxZoomItem aZoom(m_eZoomType,m_nZoomValue);
                aZoom.SetValueSet(SvxZoomEnableFlags::N50|SvxZoomEnableFlags::N75|SvxZoomEnableFlags::N100|SvxZoomEnableFlags::N200);
                aZoom.QueryValue(aReturn.aValue);
            }
            break;
        case SID_ATTR_ZOOMSLIDER:
            aReturn.bEnabled = true;
            {
                SvxZoomSliderItem aZoomSlider(m_nZoomValue,20,400);
                aZoomSlider.AddSnappingPoint(50);
                aZoomSlider.AddSnappingPoint(75);
                aZoomSlider.AddSnappingPoint(100);
                aZoomSlider.AddSnappingPoint(200);
                aZoomSlider.QueryValue(aReturn.aValue);
            }
            break;
        default:
            aReturn = OReportController_BASE::GetState(_nId);
    }
    return aReturn;
}


namespace
{
    /** extracts a background color from a dispatched SID_BACKGROUND_COLOR call

        The dispatch might originate from either the toolbar, or the conditional
        formatting dialog. In both cases, argument formats are different.
    */
    util::Color lcl_extractBackgroundColor( const Sequence< PropertyValue >& _rDispatchArgs )
    {
        util::Color aColor( COL_TRANSPARENT );
        if ( _rDispatchArgs.getLength() == 1 )
        {
            OSL_VERIFY( _rDispatchArgs[0].Value >>= aColor );
        }
        else
        {
            SequenceAsHashMap aMap( _rDispatchArgs );
            aColor = aMap.getUnpackedValueOrDefault( PROPERTY_FONTCOLOR, aColor );
        }
        return aColor;
    }
}


void OReportController::Execute(sal_uInt16 _nId, const Sequence< PropertyValue >& aArgs)
{
    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );

    bool bForceBroadcast = false;
    switch(_nId)
    {
        case SID_RPT_TEXTDOCUMENT:
            if ( m_xReportDefinition.is() )
                m_xReportDefinition->setMimeType( MIMETYPE_OASIS_OPENDOCUMENT_TEXT_ASCII );
            break;
        case SID_RPT_SPREADSHEET:
            if (m_xReportDefinition.is() )
                m_xReportDefinition->setMimeType( MIMETYPE_OASIS_OPENDOCUMENT_SPREADSHEET_ASCII );
            break;
        case SID_REPORTHEADER_WITHOUT_UNDO:
        case SID_REPORTFOOTER_WITHOUT_UNDO:
        case SID_REPORTHEADERFOOTER:
            switchReportSection(_nId);
            break;
        case SID_PAGEHEADER_WITHOUT_UNDO:
        case SID_PAGEFOOTER_WITHOUT_UNDO:
        case SID_PAGEHEADERFOOTER:
            switchPageSection(_nId);
            break;
        case SID_GROUP_APPEND:
        case SID_GROUP_REMOVE:
            modifyGroup(_nId == SID_GROUP_APPEND,aArgs);
            break;
        case SID_GROUPHEADER_WITHOUT_UNDO:
        case SID_GROUPHEADER:
            createGroupSection(SID_GROUPHEADER == _nId,true,aArgs);
            break;
        case SID_GROUPFOOTER_WITHOUT_UNDO:
        case SID_GROUPFOOTER:
            createGroupSection(SID_GROUPFOOTER == _nId,false,aArgs);
            break;
        case SID_ADD_CONTROL_PAIR:
            addPairControls(aArgs);
            break;
        case SID_REDO:
        case SID_UNDO:
        {
            const OXUndoEnvironment::OUndoMode aLock( m_aReportModel->GetUndoEnv() );
            bool ( SfxUndoManager::*doXDo )() =
                ( _nId == SID_UNDO ) ? &SfxUndoManager::Undo : &SfxUndoManager::Redo;
            SfxUndoManager& rUndoManager( getUndoManager() );

            sal_Int16 nCount(1);
            if (aArgs.hasElements() && aArgs[0].Name != "KeyModifier")
                aArgs[0].Value >>= nCount;
            while (nCount--)
                (rUndoManager.*doXDo)();
            InvalidateAll();
            if (m_xGroupsFloater && m_xGroupsFloater->getDialog()->get_visible())
                m_xGroupsFloater->UpdateData();
        }
        break;
        case SID_CUT:
            executeMethodWithUndo(RID_STR_UNDO_REMOVE_SELECTION,::std::mem_fn(&ODesignView::Cut));
            break;
        case SID_COPY:
            getDesignView()->Copy();
            break;
        case SID_PASTE:
            executeMethodWithUndo(RID_STR_UNDO_PASTE,::std::mem_fn(&ODesignView::Paste));
            break;

        case SID_FRAME_TO_TOP:
        case SID_FRAME_DOWN:
        case SID_FRAME_UP:
        case SID_FRAME_TO_BOTTOM:
        case SID_OBJECT_HEAVEN:
        case SID_OBJECT_HELL:
            changeZOrder(_nId);
            break;
        case SID_DISTRIBUTE_HLEFT:
        case SID_DISTRIBUTE_HCENTER:
        case SID_DISTRIBUTE_HDISTANCE:
        case SID_DISTRIBUTE_HRIGHT:
        case SID_DISTRIBUTE_VTOP:
        case SID_DISTRIBUTE_VCENTER:
        case SID_DISTRIBUTE_VDISTANCE:
        case SID_DISTRIBUTE_VBOTTOM:
            {
                OSectionView* pSectionView = getCurrentSectionView();
                if ( pSectionView )
                    pSectionView->DistributeMarkedObjects(_nId);
            }
            break;
        case SID_OBJECT_SMALLESTWIDTH:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::WIDTH_SMALLEST);
            break;
        case SID_OBJECT_SMALLESTHEIGHT:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::HEIGHT_SMALLEST);
            break;
        case SID_OBJECT_GREATESTWIDTH:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::WIDTH_GREATEST);
            break;
        case SID_OBJECT_GREATESTHEIGHT:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::HEIGHT_GREATEST);
            break;
        case SID_SECTION_ALIGN_LEFT:
        case SID_OBJECT_ALIGN_LEFT:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::LEFT,SID_SECTION_ALIGN_LEFT == _nId);
            break;
        case SID_SECTION_ALIGN_CENTER:
        case SID_OBJECT_ALIGN_CENTER:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::CENTER_HORIZONTAL,SID_SECTION_ALIGN_CENTER == _nId);
            break;
        case SID_SECTION_ALIGN_RIGHT:
        case SID_OBJECT_ALIGN_RIGHT:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::RIGHT,SID_SECTION_ALIGN_RIGHT == _nId);
            break;
        case SID_SECTION_ALIGN_UP:
        case SID_OBJECT_ALIGN_UP:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::TOP,SID_SECTION_ALIGN_UP == _nId);
            break;
        case SID_SECTION_ALIGN_MIDDLE:
        case SID_OBJECT_ALIGN_MIDDLE:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::CENTER_VERTICAL,SID_SECTION_ALIGN_MIDDLE == _nId);
            break;
        case SID_SECTION_ALIGN_DOWN:
        case SID_OBJECT_ALIGN_DOWN:
            alignControlsWithUndo(RID_STR_UNDO_ALIGNMENT,ControlModification::BOTTOM,SID_SECTION_ALIGN_DOWN == _nId);
            break;

        case SID_SECTION_SHRINK_BOTTOM:
        case SID_SECTION_SHRINK_TOP:
        case SID_SECTION_SHRINK:
            {
                uno::Reference<report::XSection> xSection = getDesignView()->getCurrentSection();
                shrinkSection(RID_STR_UNDO_SHRINK, xSection, _nId);
            }
            break;

        case SID_SELECTALL:
            getDesignView()->SelectAll(SdrObjKind::NONE);
            break;
        case SID_SELECTALL_IN_SECTION:
            {
                OSectionView* pSectionView = getCurrentSectionView();
                if ( pSectionView )
                    pSectionView->MarkAll();
            }
            break;
        case SID_ESCAPE:
            getDesignView()->SetMode(DlgEdMode::Select);
            InvalidateFeature( SID_OBJECT_SELECT );
            break;
        case SID_SELECT_ALL_EDITS:
            getDesignView()->SelectAll(SdrObjKind::ReportDesignFormattedField);
            break;
        case SID_SELECT_ALL_LABELS:
            getDesignView()->SelectAll(SdrObjKind::ReportDesignFixedText);
            break;
        case SID_TERMINATE_INPLACEACTIVATION:
            {
                OSectionWindow* pSection = getDesignView()->getMarkedSection();
                if ( pSection )
                    pSection->getReportSection().deactivateOle();
            }
            break;
        case SID_SELECT:
            if ( aArgs.getLength() == 1 )
                select(aArgs[0].Value);
            break;
        case SID_SELECT_REPORT:
            select(uno::Any(m_xReportDefinition));
            break;
        case SID_EXECUTE_REPORT:
            getView()->PostUserEvent(LINK(this, OReportController,OnExecuteReport));
            break;
        case SID_RPT_NEW_FUNCTION:
            createNewFunction(aArgs[0].Value);
            break;
        case SID_COLLAPSE_SECTION:
            collapseSection(true);
            break;
        case SID_EXPAND_SECTION:
            collapseSection(false);
            break;
        case SID_NEXT_MARK:
            markSection(true);
            break;
        case SID_PREV_MARK:
            markSection(false);
            break;
        case SID_DELETE:
            if ( aArgs.getLength() == 1 )
            {
                uno::Reference< report::XFunction> xFunction;
                aArgs[0].Value >>= xFunction;
                if ( xFunction.is() )
                {
                    uno::Reference< report::XFunctions> xFunctions(xFunction->getParent(),uno::UNO_QUERY_THROW);
                    sal_Int32 nIndex = getPositionInIndexAccess(xFunctions, xFunction);
                    const OUString sUndoAction = RptResId(RID_STR_UNDO_REMOVE_FUNCTION);
                    UndoContext aUndoContext( getUndoManager(), sUndoAction );
                    xFunctions->removeByIndex(nIndex);
                    select(uno::Any(xFunctions->getParent()));
                    InvalidateFeature( SID_UNDO );
                }
            }
            else
                executeMethodWithUndo(RID_STR_UNDO_REMOVE_SELECTION,::std::mem_fn(&ODesignView::Delete));
            break;
        case SID_GRID_USE:
            m_bGridUse = !m_bGridUse;
            getDesignView()->setGridSnap(m_bGridUse);
            break;
        case SID_HELPLINES_MOVE:
            m_bHelplinesMove = !m_bHelplinesMove;
            getDesignView()->setDragStripes(m_bHelplinesMove);
            break;
        case SID_GRID_VISIBLE:
            m_bGridVisible = !m_bGridVisible;
            getDesignView()->toggleGrid(m_bGridVisible);
            break;
        case SID_RULER:
            m_bShowRuler = !m_bShowRuler;
            getDesignView()->showRuler(m_bShowRuler);
            break;
        case SID_OBJECT_SELECT:
            getDesignView()->SetMode(DlgEdMode::Select);
            InvalidateAll();
            break;
        case SID_INSERT_DIAGRAM:
            getDesignView()->SetMode( DlgEdMode::Insert );
            getDesignView()->SetInsertObj( SdrObjKind::OLE2);
            createDefaultControl(aArgs);
            InvalidateAll();
            break;
        case SID_FM_FIXEDTEXT:
            getDesignView()->SetMode( DlgEdMode::Insert );
            getDesignView()->SetInsertObj( SdrObjKind::ReportDesignFixedText );
            createDefaultControl(aArgs);
            InvalidateAll();
            break;
        case SID_INSERT_HFIXEDLINE:
            getDesignView()->SetMode( DlgEdMode::Insert );
            getDesignView()->SetInsertObj( SdrObjKind::ReportDesignHorizontalFixedLine );
            createDefaultControl(aArgs);
            InvalidateAll();
            break;
        case SID_INSERT_VFIXEDLINE:
            getDesignView()->SetMode( DlgEdMode::Insert );
            getDesignView()->SetInsertObj( SdrObjKind::ReportDesignVerticalFixedLine );
            createDefaultControl(aArgs);
            InvalidateAll();
            break;
        case SID_FM_EDIT:
            getDesignView()->SetMode( DlgEdMode::Insert );
            getDesignView()->SetInsertObj( SdrObjKind::ReportDesignFormattedField );
            createDefaultControl(aArgs);
            InvalidateAll();
            break;
        case SID_FM_IMAGECONTROL:
            getDesignView()->SetMode( DlgEdMode::Insert );
            getDesignView()->SetInsertObj( SdrObjKind::ReportDesignImageControl );
            createDefaultControl(aArgs);
            InvalidateAll();
            break;
        case SID_DRAWTBX_CS_BASIC:
        case SID_DRAWTBX_CS_BASIC1:
        case SID_DRAWTBX_CS_BASIC2:
        case SID_DRAWTBX_CS_BASIC3:
        case SID_DRAWTBX_CS_BASIC4:
        case SID_DRAWTBX_CS_BASIC5:
        case SID_DRAWTBX_CS_BASIC6:
        case SID_DRAWTBX_CS_BASIC7:
        case SID_DRAWTBX_CS_BASIC8:
        case SID_DRAWTBX_CS_BASIC9:
        case SID_DRAWTBX_CS_BASIC10:
        case SID_DRAWTBX_CS_BASIC11:
        case SID_DRAWTBX_CS_BASIC12:
        case SID_DRAWTBX_CS_BASIC13:
        case SID_DRAWTBX_CS_BASIC14:
        case SID_DRAWTBX_CS_BASIC15:
        case SID_DRAWTBX_CS_BASIC16:
        case SID_DRAWTBX_CS_BASIC17:
        case SID_DRAWTBX_CS_BASIC18:
        case SID_DRAWTBX_CS_BASIC19:
        case SID_DRAWTBX_CS_BASIC20:
        case SID_DRAWTBX_CS_BASIC21:
        case SID_DRAWTBX_CS_BASIC22:
        case SID_DRAWTBX_CS_SYMBOL1:
        case SID_DRAWTBX_CS_SYMBOL2:
        case SID_DRAWTBX_CS_SYMBOL3:
        case SID_DRAWTBX_CS_SYMBOL4:
        case SID_DRAWTBX_CS_SYMBOL5:
        case SID_DRAWTBX_CS_SYMBOL6:
        case SID_DRAWTBX_CS_SYMBOL7:
        case SID_DRAWTBX_CS_SYMBOL8:
        case SID_DRAWTBX_CS_SYMBOL9:
        case SID_DRAWTBX_CS_SYMBOL10:
        case SID_DRAWTBX_CS_SYMBOL11:
        case SID_DRAWTBX_CS_SYMBOL12:
        case SID_DRAWTBX_CS_SYMBOL13:
        case SID_DRAWTBX_CS_SYMBOL14:
        case SID_DRAWTBX_CS_SYMBOL15:
        case SID_DRAWTBX_CS_SYMBOL16:
        case SID_DRAWTBX_CS_SYMBOL17:
        case SID_DRAWTBX_CS_SYMBOL18:
        case SID_DRAWTBX_CS_ARROW1:
        case SID_DRAWTBX_CS_ARROW2:
        case SID_DRAWTBX_CS_ARROW3:
        case SID_DRAWTBX_CS_ARROW4:
        case SID_DRAWTBX_CS_ARROW5:
        case SID_DRAWTBX_CS_ARROW6:
        case SID_DRAWTBX_CS_ARROW7:
        case SID_DRAWTBX_CS_ARROW8:
        case SID_DRAWTBX_CS_ARROW9:
        case SID_DRAWTBX_CS_ARROW10:
        case SID_DRAWTBX_CS_ARROW11:
        case SID_DRAWTBX_CS_ARROW12:
        case SID_DRAWTBX_CS_ARROW13:
        case SID_DRAWTBX_CS_ARROW14:
        case SID_DRAWTBX_CS_ARROW15:
        case SID_DRAWTBX_CS_ARROW16:
        case SID_DRAWTBX_CS_ARROW17:
        case SID_DRAWTBX_CS_ARROW18:
        case SID_DRAWTBX_CS_ARROW19:
        case SID_DRAWTBX_CS_ARROW20:
        case SID_DRAWTBX_CS_ARROW21:
        case SID_DRAWTBX_CS_ARROW22:
        case SID_DRAWTBX_CS_ARROW23:
        case SID_DRAWTBX_CS_ARROW24:
        case SID_DRAWTBX_CS_ARROW25:
        case SID_DRAWTBX_CS_ARROW26:
        case SID_DRAWTBX_CS_STAR1:
        case SID_DRAWTBX_CS_STAR2:
        case SID_DRAWTBX_CS_STAR3:
        case SID_DRAWTBX_CS_STAR4:
        case SID_DRAWTBX_CS_STAR5:
        case SID_DRAWTBX_CS_STAR6:
        case SID_DRAWTBX_CS_STAR7:
        case SID_DRAWTBX_CS_STAR8:
        case SID_DRAWTBX_CS_STAR9:
        case SID_DRAWTBX_CS_STAR10:
        case SID_DRAWTBX_CS_STAR11:
        case SID_DRAWTBX_CS_STAR12:
        case SID_DRAWTBX_CS_FLOWCHART1:
        case SID_DRAWTBX_CS_FLOWCHART2:
        case SID_DRAWTBX_CS_FLOWCHART3:
        case SID_DRAWTBX_CS_FLOWCHART4:
        case SID_DRAWTBX_CS_FLOWCHART5:
        case SID_DRAWTBX_CS_FLOWCHART6:
        case SID_DRAWTBX_CS_FLOWCHART7:
        case SID_DRAWTBX_CS_FLOWCHART8:
        case SID_DRAWTBX_CS_FLOWCHART9:
        case SID_DRAWTBX_CS_FLOWCHART10:
        case SID_DRAWTBX_CS_FLOWCHART11:
        case SID_DRAWTBX_CS_FLOWCHART12:
        case SID_DRAWTBX_CS_FLOWCHART13:
        case SID_DRAWTBX_CS_FLOWCHART14:
        case SID_DRAWTBX_CS_FLOWCHART15:
        case SID_DRAWTBX_CS_FLOWCHART16:
        case SID_DRAWTBX_CS_FLOWCHART17:
        case SID_DRAWTBX_CS_FLOWCHART18:
        case SID_DRAWTBX_CS_FLOWCHART19:
        case SID_DRAWTBX_CS_FLOWCHART20:
        case SID_DRAWTBX_CS_FLOWCHART21:
        case SID_DRAWTBX_CS_FLOWCHART22:
        case SID_DRAWTBX_CS_FLOWCHART23:
        case SID_DRAWTBX_CS_FLOWCHART24:
        case SID_DRAWTBX_CS_FLOWCHART25:
        case SID_DRAWTBX_CS_FLOWCHART26:
        case SID_DRAWTBX_CS_FLOWCHART27:
        case SID_DRAWTBX_CS_FLOWCHART28:
        case SID_DRAWTBX_CS_CALLOUT1:
        case SID_DRAWTBX_CS_CALLOUT2:
        case SID_DRAWTBX_CS_CALLOUT3:
        case SID_DRAWTBX_CS_CALLOUT4:
        case SID_DRAWTBX_CS_CALLOUT5:
        case SID_DRAWTBX_CS_CALLOUT6:
        case SID_DRAWTBX_CS_CALLOUT7:
        case SID_DRAWTBX_CS_SYMBOL:
        case SID_DRAWTBX_CS_ARROW:
        case SID_DRAWTBX_CS_FLOWCHART:
        case SID_DRAWTBX_CS_CALLOUT:
        case SID_DRAWTBX_CS_STAR:
            getDesignView()->SetMode( DlgEdMode::Insert );
            {
                URL aUrl = getURLForId(_nId);
                sal_Int32 nIndex = 1;
                std::u16string_view sType = o3tl::getToken(aUrl.Complete, 0,'.',nIndex);
                if ( nIndex == -1 || sType.empty() )
                {
                    switch(_nId)
                    {
                        case SID_DRAWTBX_CS_SYMBOL:
                            sType = u"smiley";
                            break;
                        case SID_DRAWTBX_CS_ARROW:
                            sType = u"left-right-arrow";
                            break;
                        case SID_DRAWTBX_CS_FLOWCHART:
                            sType = u"flowchart-internal-storage";
                            break;
                        case SID_DRAWTBX_CS_CALLOUT:
                            sType = u"round-rectangular-callout";
                            break;
                        case SID_DRAWTBX_CS_STAR:
                            sType = u"star5";
                            break;
                        default:
                            sType = u"diamond";
                    }
                }
                else
                    sType = o3tl::getToken(aUrl.Complete, 0,'.',nIndex);

                getDesignView()->SetInsertObj( SdrObjKind::CustomShape, OUString(sType));
                createDefaultControl(aArgs);
            }
            InvalidateAll();
            break;
        case SID_RPT_SHOWREPORTEXPLORER:
            if ( isUiVisible() )
                getDesignView()->toggleReportExplorer();
            break;
        case SID_FM_ADD_FIELD:
            if ( isUiVisible() )
                getDesignView()->toggleAddField();
            break;
        case SID_SHOW_PROPERTYBROWSER:
            if ( m_bShowProperties )
                m_sLastActivePage = getDesignView()->getCurrentPage();
            else
                getDesignView()->setCurrentPage(m_sLastActivePage);

            if ( isUiVisible() )
            {
                m_bShowProperties = !m_bShowProperties;
                if ( aArgs.getLength() == 1 )
                    aArgs[0].Value >>= m_bShowProperties;

                getDesignView()->togglePropertyBrowser(m_bShowProperties);
            }
            break;
        case SID_PROPERTYBROWSER_LAST_PAGE: // nothing to do
            m_sLastActivePage = getDesignView()->getCurrentPage();
            break;
        case SID_SPLIT_POSITION:
            getDesignView()->Resize();
            break;
        case SID_PAGEDIALOG:
        case SID_ATTR_CHAR_COLOR_BACKGROUND:
            {
                uno::Reference<report::XSection> xSection;
                if (aArgs.getLength() == 1 )
                    aArgs[0].Value >>= xSection;
                else if (_nId == SID_ATTR_CHAR_COLOR_BACKGROUND)
                    xSection.set(getDesignView()->getMarkedSection()->getReportSection().getSection());
                openPageDialog(xSection);
                bForceBroadcast = true;
            }
            break;
        case SID_SORTINGANDGROUPING:
            openSortingAndGroupingDialog();
            break;
        case SID_BACKGROUND_COLOR:
            {
                const util::Color aColor( lcl_extractBackgroundColor( aArgs ) );
                if ( !impl_setPropertyAtControls_throw(RID_STR_UNDO_CHANGEFONT,PROPERTY_CONTROLBACKGROUND,uno::Any(aColor),aArgs) )
                {
                    uno::Reference< report::XSection > xSection = getDesignView()->getCurrentSection();
                    if ( xSection.is() )
                    {
                        xSection->setBackColor( aColor );
                    }
                }
                bForceBroadcast = true;
            }
            break;
        case SID_ATTR_CHAR_WEIGHT:
        case SID_ATTR_CHAR_POSTURE:
        case SID_ATTR_CHAR_UNDERLINE:
            {
                uno::Reference< awt::XWindow> xWindow;
                ::std::vector< uno::Reference< uno::XInterface > > aControlsFormats;
                lcl_getReportControlFormat( aArgs, getDesignView(), xWindow, aControlsFormats );

                const OUString sUndoAction(RptResId(RID_STR_UNDO_CHANGEFONT));
                UndoContext aUndoContext( getUndoManager(), sUndoAction );

                for (const auto& rxControlFormat : aControlsFormats)
                {
                    uno::Reference< report::XReportControlFormat> xReportControlFormat(rxControlFormat,uno::UNO_QUERY);
                    lcl_setFontWPU_nothrow(xReportControlFormat,_nId);
                }
            }
            break;
        case SID_ATTR_CHAR_COLOR:
        case SID_ATTR_CHAR_COLOR2:
        case SID_ATTR_CHAR_COLOR_EXT:
            {
                const util::Color aColor( lcl_extractBackgroundColor( aArgs ) );
                impl_setPropertyAtControls_throw(RID_STR_UNDO_CHANGEFONT,PROPERTY_CHARCOLOR,uno::Any(aColor),aArgs);
                bForceBroadcast = true;
            }
            break;
        case SID_ATTR_CHAR_FONT:
            if ( aArgs.getLength() == 1 )
            {
                awt::FontDescriptor aFont;
                if ( aArgs[0].Value >>= aFont )
                {
                    impl_setPropertyAtControls_throw(RID_STR_UNDO_CHANGEFONT,PROPERTY_CHARFONTNAME,uno::Any(aFont.Name),aArgs);
                }
            }
            break;
        case SID_ATTR_CHAR_FONTHEIGHT:
            if ( aArgs.getLength() == 1 )
            {
                float fSelVal = 0.0;
                if ( aArgs[0].Value >>= fSelVal )
                    impl_setPropertyAtControls_throw(RID_STR_UNDO_CHANGEFONT,PROPERTY_CHARHEIGHT,aArgs[0].Value,aArgs);
            }
            break;
        case SID_ATTR_PARA_ADJUST_LEFT:
        case SID_ATTR_PARA_ADJUST_CENTER:
        case SID_ATTR_PARA_ADJUST_RIGHT:
        case SID_ATTR_PARA_ADJUST_BLOCK:
            {
                style::ParagraphAdjust eParagraphAdjust = style::ParagraphAdjust_LEFT;
                switch(_nId)
                {
                    case SID_ATTR_PARA_ADJUST_LEFT:
                        eParagraphAdjust = style::ParagraphAdjust_LEFT;
                        break;
                    case SID_ATTR_PARA_ADJUST_CENTER:
                        eParagraphAdjust = style::ParagraphAdjust_CENTER;
                        break;
                    case SID_ATTR_PARA_ADJUST_RIGHT:
                        eParagraphAdjust = style::ParagraphAdjust_RIGHT;
                        break;
                    case SID_ATTR_PARA_ADJUST_BLOCK:
                        eParagraphAdjust = style::ParagraphAdjust_BLOCK;
                        break;
                }
                impl_setPropertyAtControls_throw(RID_STR_UNDO_ALIGNMENT,PROPERTY_PARAADJUST,uno::Any(static_cast<sal_Int16>(eParagraphAdjust)),aArgs);

                InvalidateFeature(SID_ATTR_PARA_ADJUST_LEFT);
                InvalidateFeature(SID_ATTR_PARA_ADJUST_CENTER);
                InvalidateFeature(SID_ATTR_PARA_ADJUST_RIGHT);
                InvalidateFeature(SID_ATTR_PARA_ADJUST_BLOCK);
            }
            break;
        case SID_CHAR_DLG:
            {
                uno::Sequence< beans::NamedValue > aSettings;
                uno::Reference< awt::XWindow> xWindow;
                ::std::vector< uno::Reference< uno::XInterface > > aControlsFormats;
                lcl_getReportControlFormat( aArgs, getDesignView(), xWindow, aControlsFormats );

                if ( !aControlsFormats.empty() )
                {
                    const OUString sUndoAction( RptResId( RID_STR_UNDO_CHANGEFONT ) );
                    UndoContext aUndoContext( getUndoManager(), sUndoAction );

                    for (const auto& rxControlFormat : aControlsFormats)
                    {
                        uno::Reference< report::XReportControlFormat > xFormat( rxControlFormat, uno::UNO_QUERY );
                        if ( !xFormat.is() )
                            continue;

                        if ( !aSettings.hasElements() )
                        {
                            ::rptui::openCharDialog( xFormat, xWindow, aSettings );
                            if ( !aSettings.hasElements() )
                                break;
                        }

                        applyCharacterSettings( xFormat, aSettings );
                    }

                    InvalidateAll();
                }
            }
            break;
        case SID_INSERT_GRAPHIC:
            insertGraphic();
            break;
        case SID_SETCONTROLDEFAULTS:
            break;
        case SID_CONDITIONALFORMATTING:
            {
                uno::Reference< report::XFormattedField> xFormattedField(getDesignView()->getCurrentControlModel(),uno::UNO_QUERY);
                if ( xFormattedField.is() )
                {
                    ConditionalFormattingDialog aDlg(getFrameWeld(), xFormattedField, *this);
                    aDlg.run();
                }
            }
            break;
        case SID_DATETIME:
            if ( m_xReportDefinition.is() )
            {
                if ( !aArgs.hasElements() )
                {
                    ODateTimeDialog aDlg(getFrameWeld(), getDesignView()->getCurrentSection(), this);
                    aDlg.run();
                }
                else
                    createDateTime(aArgs);
            }
            break;
        case SID_INSERT_FLD_PGNUMBER:
            if ( m_xReportDefinition.is() )
            {
                if ( !aArgs.hasElements() )
                {
                    OPageNumberDialog aDlg(getFrameWeld(), m_xReportDefinition, this);
                    aDlg.run();
                }
                else
                    createPageNumber(aArgs);
            }
            break;
        case SID_EXPORTDOC:
        case SID_EXPORTDOCASPDF:
        case SID_PRINTPREVIEW:
            break;
        case SID_EDITDOC:
            if(isEditable())
            { // the state should be changed to not editable
                setModified(false);     // and we are not modified yet
            }
            setEditable(!isEditable());
            InvalidateAll();
            return;
        case SID_GROUP:
            break;
        case SID_ATTR_ZOOM:
            if ( !aArgs.hasElements() )
            {
                openZoomDialog();
            }
            else if ( aArgs.getLength() == 1 && aArgs[0].Name == "Zoom" )
            {
                SvxZoomItem aZoomItem;
                aZoomItem.PutValue(aArgs[0].Value, 0);
                m_nZoomValue = aZoomItem.GetValue();
                m_eZoomType = aZoomItem.GetType();
                impl_zoom_nothrow();
            }
            break;
        case SID_ATTR_ZOOMSLIDER:
            if ( aArgs.getLength() == 1 && aArgs[0].Name == "ZoomSlider" )
            {
                SvxZoomSliderItem aZoomSlider;
                aZoomSlider.PutValue(aArgs[0].Value, 0);
                m_nZoomValue = aZoomSlider.GetValue();
                m_eZoomType = SvxZoomType::PERCENT;
                impl_zoom_nothrow();
            }
            break;
        default:
            OReportController_BASE::Execute(_nId,aArgs);
    }
    InvalidateFeature(_nId,Reference< XStatusListener >(),bForceBroadcast);
}

void OReportController::impl_initialize( const ::comphelper::NamedValueCollection& rArguments )
{
    OReportController_BASE::impl_initialize(rArguments);

    rArguments.get_ensureType( PROPERTY_REPORTNAME, m_sName );
    if ( m_sName.isEmpty() )
        rArguments.get_ensureType( u"DocumentTitle"_ustr, m_sName );

    try
    {
        if ( m_xReportDefinition.is() )
        {
            getView()->initialize();    // show the windows and fill with our information

            m_aReportModel = reportdesign::OReportDefinition::getSdrModel(m_xReportDefinition);
            if ( !m_aReportModel )
                throw RuntimeException();
            m_aReportModel->attachController( *this );

            clearUndoManager();
            UndoSuppressor aSuppressUndo( getUndoManager() );

            setMode(::comphelper::NamedValueCollection::getOrDefault(getModel()->getArgs(), u"Mode", u"normal"_ustr));

            listen(true);
            setEditable( !m_aReportModel->IsReadOnly() );
            m_xFormatter.set(util::NumberFormatter::create(m_xContext), UNO_QUERY_THROW);
            m_xFormatter->attachNumberFormatsSupplier(Reference< XNumberFormatsSupplier>(m_xReportDefinition,uno::UNO_QUERY));

            utl::MediaDescriptor aDescriptor( m_xReportDefinition->getArgs() );
            OUString sHierarchicalDocumentName = aDescriptor.getUnpackedValueOrDefault(u"HierarchicalDocumentName"_ustr,OUString());

            if ( sHierarchicalDocumentName.isEmpty() && getConnection().is() )
            {
                uno::Reference<sdbcx::XTablesSupplier> xTablesSup(getConnection(),uno::UNO_QUERY_THROW);
                uno::Reference<container::XNameAccess> xTables = xTablesSup->getTables();
                const uno::Sequence< OUString > aNames( xTables->getElementNames() );

                if ( aNames.hasElements() )
                {
                    m_xReportDefinition->setCommand(aNames[0]);
                    m_xReportDefinition->setCommandType(sdb::CommandType::TABLE);
                }
            }

            m_aVisualAreaSize = m_xReportDefinition->getVisualAreaSize(0);

        }

        // check if chart is supported by the engine
        checkChartEnabled();
        // restore the view data
        getDesignView()->toggleGrid(m_bGridVisible);
        getDesignView()->showRuler(m_bShowRuler);
        getDesignView()->togglePropertyBrowser(m_bShowProperties);
        getDesignView()->setCurrentPage(m_sLastActivePage);
        getDesignView()->unmarkAllObjects();

        if ( m_nPageNum != -1 )
        {
            if ( m_nPageNum < m_aReportModel->GetPageCount() )
            {
                const OReportPage* pPage = dynamic_cast<OReportPage*>(m_aReportModel->GetPage(static_cast<sal_uInt16>(m_nPageNum)));
                if ( pPage )
                {
                    executeUnChecked(SID_SELECT,{ comphelper::makePropertyValue(u""_ustr, pPage->getSection() ) });
                }
            }
            else
                m_nPageNum = -1;
        }
        getDesignView()->collapseSections(m_aCollapsedSections);
        impl_zoom_nothrow();
        getDesignView()->Resize();
        getDesignView()->Invalidate();
        InvalidateAll();

        if ( m_bShowProperties && m_nPageNum == -1 )
        {
            m_sLastActivePage = "Data";
            getDesignView()->setCurrentPage(m_sLastActivePage);
            executeUnChecked(SID_SELECT_REPORT,{});
        }

        setModified(false);     // and we are not modified yet
    }
    catch(const SQLException&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}

IMPL_LINK( OReportController, OnCreateHdl, OAddFieldWindow& ,_rAddFieldDlg, void)
{
    weld::WaitObject aObj(getFrameWeld());
    uno::Sequence< beans::PropertyValue > aArgs = _rAddFieldDlg.getSelectedFieldDescriptors();
    // we use this way to create undo actions
    if ( aArgs.hasElements() )
    {
        executeChecked(SID_ADD_CONTROL_PAIR,aArgs);
    }
}

bool OReportController::Construct(vcl::Window* pParent)
{
    VclPtrInstance<ODesignView> pMyOwnView( pParent, m_xContext, *this );
    StartListening( *pMyOwnView );
    setView( pMyOwnView );

    // now that we have a view we can create the clipboard listener
    m_aSystemClipboard = TransferableDataHelper::CreateFromSystemClipboard( getView() );
    m_aSystemClipboard.StartClipboardListening( );
    m_pClipboardNotifier = new TransferableClipboardListener( LINK( this, OReportController, OnClipboardChanged ) );
    m_pClipboardNotifier->AddListener( getView() );

    OReportController_BASE::Construct(pParent);
    return true;
}

sal_Bool SAL_CALL OReportController::suspend(sal_Bool /*_bSuspend*/)
{
    if ( getBroadcastHelper().bInDispose || getBroadcastHelper().bDisposed )
        return true;

    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );

    if ( getView() && getView()->IsInModalMode() )
        return false;

    // this suspend will be handled in the DBAccess interceptor implementation
    return true;
}

void OReportController::describeSupportedFeatures()
{
    DBSubComponentController::describeSupportedFeatures();

    implDescribeSupportedFeature( u".uno:TextDocument"_ustr,              SID_RPT_TEXTDOCUMENT,           CommandGroup::APPLICATION );
    implDescribeSupportedFeature( u".uno:Spreadsheet"_ustr,               SID_RPT_SPREADSHEET,            CommandGroup::APPLICATION );

    implDescribeSupportedFeature( u".uno:Redo"_ustr,                      SID_REDO,                       CommandGroup::EDIT );
    implDescribeSupportedFeature( u".uno:Undo"_ustr,                      SID_UNDO,                       CommandGroup::EDIT );
    implDescribeSupportedFeature( u".uno:SelectAll"_ustr,                 SID_SELECTALL,                  CommandGroup::EDIT );
    implDescribeSupportedFeature( u".uno:SelectAllInSection"_ustr,        SID_SELECTALL_IN_SECTION,       CommandGroup::EDIT );
    implDescribeSupportedFeature( u".uno:Delete"_ustr,                    SID_DELETE,                     CommandGroup::EDIT );
    implDescribeSupportedFeature( u".uno:SelectReport"_ustr,              SID_SELECT_REPORT,              CommandGroup::EDIT );
    implDescribeSupportedFeature( u".uno:ExecuteReport"_ustr,             SID_EXECUTE_REPORT,             CommandGroup::EDIT );

    implDescribeSupportedFeature( u".uno:GridVisible"_ustr,               SID_GRID_VISIBLE,               CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:GridUse"_ustr,                   SID_GRID_USE,                   CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:HelplinesMove"_ustr,             SID_HELPLINES_MOVE,             CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:ShowRuler"_ustr,                 SID_RULER,                      CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:AddField"_ustr,                  SID_FM_ADD_FIELD,               CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:ReportNavigator"_ustr,           SID_RPT_SHOWREPORTEXPLORER,     CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:ControlProperties"_ustr,         SID_SHOW_PROPERTYBROWSER,       CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:DbSortingAndGrouping"_ustr,      SID_SORTINGANDGROUPING,         CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:PageHeaderFooter"_ustr,          SID_PAGEHEADERFOOTER,           CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:ReportHeaderFooter"_ustr,        SID_REPORTHEADERFOOTER,         CommandGroup::VIEW );
    implDescribeSupportedFeature( u".uno:ZoomSlider"_ustr,                SID_ATTR_ZOOMSLIDER );
    implDescribeSupportedFeature( u".uno:Zoom"_ustr,                      SID_ATTR_ZOOM,                  CommandGroup::VIEW );

    implDescribeSupportedFeature( u".uno:ConditionalFormatting"_ustr,     SID_CONDITIONALFORMATTING,      CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:PageDialog"_ustr,                SID_PAGEDIALOG,                 CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:ResetAttributes"_ustr,           SID_SETCONTROLDEFAULTS,         CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:Bold"_ustr,                      SID_ATTR_CHAR_WEIGHT,           CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:Italic"_ustr,                    SID_ATTR_CHAR_POSTURE,          CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:Underline"_ustr,                 SID_ATTR_CHAR_UNDERLINE,        CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DBBackgroundColor"_ustr,         SID_ATTR_CHAR_COLOR_BACKGROUND, CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:BackgroundColor"_ustr,           SID_BACKGROUND_COLOR,           CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:CharColorExt"_ustr,              SID_ATTR_CHAR_COLOR_EXT);
    implDescribeSupportedFeature( u".uno:Color"_ustr,                     SID_ATTR_CHAR_COLOR);
    implDescribeSupportedFeature( u".uno:FontColor"_ustr,                 SID_ATTR_CHAR_COLOR2,           CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:FontDialog"_ustr,                SID_CHAR_DLG,                   CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:LeftPara"_ustr,                  SID_ATTR_PARA_ADJUST_LEFT,      CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:CenterPara"_ustr,                SID_ATTR_PARA_ADJUST_CENTER,    CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:RightPara"_ustr,                 SID_ATTR_PARA_ADJUST_RIGHT,     CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:JustifyPara"_ustr,               SID_ATTR_PARA_ADJUST_BLOCK,     CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:FontHeight"_ustr,                SID_ATTR_CHAR_FONTHEIGHT,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:CharFontName"_ustr,              SID_ATTR_CHAR_FONT,             CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:ArrangeMenu"_ustr,               SID_ARRANGEMENU,                CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:BringToFront"_ustr,              SID_FRAME_TO_TOP,               CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:ObjectBackOne"_ustr,             SID_FRAME_DOWN,                 CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:ObjectForwardOne"_ustr,          SID_FRAME_UP,                   CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SendToBack"_ustr,                SID_FRAME_TO_BOTTOM,            CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SetObjectToForeground"_ustr,     SID_OBJECT_HEAVEN,              CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SetObjectToBackground"_ustr,     SID_OBJECT_HELL,                CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:ObjectAlign"_ustr,               SID_OBJECT_ALIGN,               CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:ObjectAlignLeft"_ustr,           SID_OBJECT_ALIGN_LEFT,          CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:AlignCenter"_ustr,               SID_OBJECT_ALIGN_CENTER,        CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:ObjectAlignRight"_ustr,          SID_OBJECT_ALIGN_RIGHT,         CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:AlignUp"_ustr,                   SID_OBJECT_ALIGN_UP,            CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:AlignMiddle"_ustr,               SID_OBJECT_ALIGN_MIDDLE,        CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:AlignDown"_ustr,                 SID_OBJECT_ALIGN_DOWN,          CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:SectionAlign"_ustr,              SID_SECTION_ALIGN );
    implDescribeSupportedFeature( u".uno:SectionAlignLeft"_ustr,          SID_SECTION_ALIGN_LEFT,         CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionAlignCenter"_ustr,        SID_SECTION_ALIGN_CENTER,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionAlignRight"_ustr,         SID_SECTION_ALIGN_RIGHT,        CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionAlignTop"_ustr,           SID_SECTION_ALIGN_UP,           CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionAlignMiddle"_ustr,        SID_SECTION_ALIGN_MIDDLE,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionAlignBottom"_ustr,        SID_SECTION_ALIGN_DOWN,         CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionShrink"_ustr,             SID_SECTION_SHRINK,             CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionShrinkTop"_ustr,          SID_SECTION_SHRINK_TOP,         CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SectionShrinkBottom"_ustr,       SID_SECTION_SHRINK_BOTTOM,      CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:ObjectResize"_ustr,              SID_OBJECT_RESIZING,            CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SmallestWidth"_ustr,             SID_OBJECT_SMALLESTWIDTH,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:SmallestHeight"_ustr,            SID_OBJECT_SMALLESTHEIGHT,      CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:GreatestWidth"_ustr,             SID_OBJECT_GREATESTWIDTH,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:GreatestHeight"_ustr,            SID_OBJECT_GREATESTHEIGHT,      CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:DistributeSelection"_ustr,       SID_DISTRIBUTE_DLG,             CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeHorzLeft"_ustr,        SID_DISTRIBUTE_HLEFT,           CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeHorzCenter"_ustr,      SID_DISTRIBUTE_HCENTER,         CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeHorzDistance"_ustr,    SID_DISTRIBUTE_HDISTANCE,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeHorzRight"_ustr,       SID_DISTRIBUTE_HRIGHT,          CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeVertTop"_ustr,         SID_DISTRIBUTE_VTOP,            CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeVertCenter"_ustr,      SID_DISTRIBUTE_VCENTER,         CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeVertDistance"_ustr,    SID_DISTRIBUTE_VDISTANCE,       CommandGroup::FORMAT );
    implDescribeSupportedFeature( u".uno:DistributeVertBottom"_ustr,      SID_DISTRIBUTE_VBOTTOM,         CommandGroup::FORMAT );

    implDescribeSupportedFeature( u".uno:ExportTo"_ustr,                  SID_EXPORTDOC,                  CommandGroup::APPLICATION );
    implDescribeSupportedFeature( u".uno:ExportToPDF"_ustr,               SID_EXPORTDOCASPDF,             CommandGroup::APPLICATION );
    implDescribeSupportedFeature( u".uno:PrintPreview"_ustr,              SID_PRINTPREVIEW,               CommandGroup::APPLICATION );

    implDescribeSupportedFeature( u".uno:NewDoc"_ustr,                    SID_NEWDOC,                     CommandGroup::DOCUMENT );
    implDescribeSupportedFeature( u".uno:Save"_ustr,                      SID_SAVEDOC,                    CommandGroup::DOCUMENT );
    implDescribeSupportedFeature( u".uno:SaveAs"_ustr,                    SID_SAVEASDOC,                  CommandGroup::DOCUMENT );
    implDescribeSupportedFeature( u".uno:SaveACopy"_ustr,                 SID_SAVEACOPY,                  CommandGroup::DOCUMENT );

    implDescribeSupportedFeature( u".uno:InsertPageNumberField"_ustr,     SID_INSERT_FLD_PGNUMBER,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:InsertDateTimeField"_ustr,       SID_DATETIME,                   CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:InsertObjectChart"_ustr,         SID_INSERT_DIAGRAM,             CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:InsertGraphic"_ustr,             SID_INSERT_GRAPHIC,             CommandGroup::INSERT );
    // controls
    implDescribeSupportedFeature( u".uno:SelectObject"_ustr,              SID_OBJECT_SELECT,              CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:Label"_ustr,                     SID_FM_FIXEDTEXT,               CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:Edit"_ustr,                      SID_FM_EDIT,                    CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ImageControl"_ustr,              SID_FM_IMAGECONTROL,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:HFixedLine"_ustr,                SID_INSERT_HFIXEDLINE,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:VFixedLine"_ustr,                SID_INSERT_VFIXEDLINE,          CommandGroup::INSERT );

    // shapes
    implDescribeSupportedFeature( u".uno:BasicShapes"_ustr,               SID_DRAWTBX_CS_BASIC,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.rectangle"_ustr,     SID_DRAWTBX_CS_BASIC1,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.round-rectangle"_ustr,SID_DRAWTBX_CS_BASIC2,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.quadrat"_ustr,       SID_DRAWTBX_CS_BASIC3,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.round-quadrat"_ustr, SID_DRAWTBX_CS_BASIC4,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.circle"_ustr,        SID_DRAWTBX_CS_BASIC5,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.ellipse"_ustr,       SID_DRAWTBX_CS_BASIC6,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.circle-pie"_ustr,    SID_DRAWTBX_CS_BASIC7,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.isosceles-triangle"_ustr,SID_DRAWTBX_CS_BASIC8,      CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.right-triangle"_ustr,SID_DRAWTBX_CS_BASIC9,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.trapezoid"_ustr,     SID_DRAWTBX_CS_BASIC10,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.diamond"_ustr,       SID_DRAWTBX_CS_BASIC11,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.parallelogram"_ustr, SID_DRAWTBX_CS_BASIC12,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.pentagon"_ustr,      SID_DRAWTBX_CS_BASIC13,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.hexagon"_ustr,       SID_DRAWTBX_CS_BASIC14,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.octagon"_ustr,       SID_DRAWTBX_CS_BASIC15,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.cross"_ustr,         SID_DRAWTBX_CS_BASIC16,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.ring"_ustr,          SID_DRAWTBX_CS_BASIC17,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.block-arc"_ustr,     SID_DRAWTBX_CS_BASIC18,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.can"_ustr,           SID_DRAWTBX_CS_BASIC19,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.cube"_ustr,          SID_DRAWTBX_CS_BASIC20,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.paper"_ustr,         SID_DRAWTBX_CS_BASIC21,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:BasicShapes.frame"_ustr,         SID_DRAWTBX_CS_BASIC22,         CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:SymbolShapes"_ustr,              SID_DRAWTBX_CS_SYMBOL,          CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:SymbolShapes.smiley"_ustr ,      SID_DRAWTBX_CS_SYMBOL1,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.sun"_ustr ,         SID_DRAWTBX_CS_SYMBOL2,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.moon"_ustr ,        SID_DRAWTBX_CS_SYMBOL3,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.lightning"_ustr ,   SID_DRAWTBX_CS_SYMBOL4,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.heart"_ustr ,       SID_DRAWTBX_CS_SYMBOL5,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.flower"_ustr ,      SID_DRAWTBX_CS_SYMBOL6,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.cloud"_ustr ,       SID_DRAWTBX_CS_SYMBOL7,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.forbidden"_ustr ,   SID_DRAWTBX_CS_SYMBOL8,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.puzzle"_ustr ,      SID_DRAWTBX_CS_SYMBOL9,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.bracket-pair"_ustr ,SID_DRAWTBX_CS_SYMBOL10,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.left-bracket"_ustr ,SID_DRAWTBX_CS_SYMBOL11,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.right-bracket"_ustr,SID_DRAWTBX_CS_SYMBOL12,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.brace-pair"_ustr ,  SID_DRAWTBX_CS_SYMBOL13,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.left-brace"_ustr ,  SID_DRAWTBX_CS_SYMBOL14,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.right-brace"_ustr , SID_DRAWTBX_CS_SYMBOL15,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.quad-bevel"_ustr ,  SID_DRAWTBX_CS_SYMBOL16,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.octagon-bevel"_ustr,SID_DRAWTBX_CS_SYMBOL17,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:SymbolShapes.diamond-bevel"_ustr,SID_DRAWTBX_CS_SYMBOL18,        CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:ArrowShapes.left-arrow"_ustr ,           SID_DRAWTBX_CS_ARROW1,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.right-arrow"_ustr ,          SID_DRAWTBX_CS_ARROW2,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-arrow"_ustr ,             SID_DRAWTBX_CS_ARROW3,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.down-arrow"_ustr ,           SID_DRAWTBX_CS_ARROW4,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.left-right-arrow"_ustr ,     SID_DRAWTBX_CS_ARROW5,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-down-arrow"_ustr ,        SID_DRAWTBX_CS_ARROW6,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-right-arrow"_ustr ,       SID_DRAWTBX_CS_ARROW7,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-right-down-arrow"_ustr ,  SID_DRAWTBX_CS_ARROW8,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.quad-arrow"_ustr ,           SID_DRAWTBX_CS_ARROW9,  CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.corner-right-arrow"_ustr ,   SID_DRAWTBX_CS_ARROW10, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.split-arrow"_ustr ,          SID_DRAWTBX_CS_ARROW11, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.striped-right-arrow"_ustr ,  SID_DRAWTBX_CS_ARROW12, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.notched-right-arrow"_ustr ,  SID_DRAWTBX_CS_ARROW13, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.pentagon-right"_ustr ,       SID_DRAWTBX_CS_ARROW14, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.chevron"_ustr ,              SID_DRAWTBX_CS_ARROW15, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.right-arrow-callout"_ustr ,  SID_DRAWTBX_CS_ARROW16, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.left-arrow-callout"_ustr ,   SID_DRAWTBX_CS_ARROW17, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-arrow-callout"_ustr ,     SID_DRAWTBX_CS_ARROW18, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.down-arrow-callout"_ustr ,   SID_DRAWTBX_CS_ARROW19, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.left-right-arrow-callout"_ustr,SID_DRAWTBX_CS_ARROW20,       CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-down-arrow-callout"_ustr ,SID_DRAWTBX_CS_ARROW21, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.up-right-arrow-callout"_ustr,SID_DRAWTBX_CS_ARROW22, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.quad-arrow-callout"_ustr ,   SID_DRAWTBX_CS_ARROW23, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.circular-arrow"_ustr ,       SID_DRAWTBX_CS_ARROW24, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.split-round-arrow"_ustr ,    SID_DRAWTBX_CS_ARROW25, CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:ArrowShapes.s-sharped-arrow"_ustr ,      SID_DRAWTBX_CS_ARROW26, CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:StarShapes.bang"_ustr ,                  SID_DRAWTBX_CS_STAR1,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.star4"_ustr ,                 SID_DRAWTBX_CS_STAR2,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.star5"_ustr ,                 SID_DRAWTBX_CS_STAR3,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.star6"_ustr ,                 SID_DRAWTBX_CS_STAR4,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.star8"_ustr ,                 SID_DRAWTBX_CS_STAR5,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.star12"_ustr ,                SID_DRAWTBX_CS_STAR6,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.star24"_ustr ,                SID_DRAWTBX_CS_STAR7,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.concave-star6"_ustr ,         SID_DRAWTBX_CS_STAR8,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.vertical-scroll"_ustr ,       SID_DRAWTBX_CS_STAR9,           CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.horizontal-scroll"_ustr ,     SID_DRAWTBX_CS_STAR10,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.signet"_ustr ,                SID_DRAWTBX_CS_STAR11,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes.doorplate"_ustr ,             SID_DRAWTBX_CS_STAR12,          CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-process"_ustr ,            SID_DRAWTBX_CS_FLOWCHART1,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-alternate-process"_ustr ,  SID_DRAWTBX_CS_FLOWCHART2,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-decision"_ustr ,           SID_DRAWTBX_CS_FLOWCHART3,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-data"_ustr ,               SID_DRAWTBX_CS_FLOWCHART4,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-predefined-process"_ustr , SID_DRAWTBX_CS_FLOWCHART5,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-internal-storage"_ustr ,   SID_DRAWTBX_CS_FLOWCHART6,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-document"_ustr ,           SID_DRAWTBX_CS_FLOWCHART7,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-multidocument"_ustr ,      SID_DRAWTBX_CS_FLOWCHART8,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-terminator"_ustr ,         SID_DRAWTBX_CS_FLOWCHART9,          CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-preparation"_ustr ,        SID_DRAWTBX_CS_FLOWCHART10,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-manual-input"_ustr ,       SID_DRAWTBX_CS_FLOWCHART11,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-manual-operation"_ustr ,   SID_DRAWTBX_CS_FLOWCHART12,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-connector"_ustr ,          SID_DRAWTBX_CS_FLOWCHART13,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-off-page-connector"_ustr , SID_DRAWTBX_CS_FLOWCHART14,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-card"_ustr ,               SID_DRAWTBX_CS_FLOWCHART15,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-punched-tape"_ustr ,       SID_DRAWTBX_CS_FLOWCHART16,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-summing-junction"_ustr ,   SID_DRAWTBX_CS_FLOWCHART17,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-or"_ustr ,                 SID_DRAWTBX_CS_FLOWCHART18,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-collate"_ustr ,            SID_DRAWTBX_CS_FLOWCHART19,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-sort"_ustr ,               SID_DRAWTBX_CS_FLOWCHART20,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-extract"_ustr ,            SID_DRAWTBX_CS_FLOWCHART21,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-merge"_ustr ,              SID_DRAWTBX_CS_FLOWCHART22,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-stored-data"_ustr ,        SID_DRAWTBX_CS_FLOWCHART23,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-delay"_ustr ,              SID_DRAWTBX_CS_FLOWCHART24,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-sequential-access"_ustr ,  SID_DRAWTBX_CS_FLOWCHART25,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-magnetic-disk"_ustr ,      SID_DRAWTBX_CS_FLOWCHART26,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-direct-access-storage"_ustr,SID_DRAWTBX_CS_FLOWCHART27,        CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:FlowChartShapes.flowchart-display"_ustr ,            SID_DRAWTBX_CS_FLOWCHART28,         CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:CalloutShapes.rectangular-callout"_ustr ,        SID_DRAWTBX_CS_CALLOUT1,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes.round-rectangular-callout"_ustr ,  SID_DRAWTBX_CS_CALLOUT2,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes.round-callout"_ustr ,              SID_DRAWTBX_CS_CALLOUT3,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes.cloud-callout"_ustr ,              SID_DRAWTBX_CS_CALLOUT4,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes.line-callout-1"_ustr ,             SID_DRAWTBX_CS_CALLOUT5,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes.line-callout-2"_ustr ,             SID_DRAWTBX_CS_CALLOUT6,            CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes.line-callout-3"_ustr ,             SID_DRAWTBX_CS_CALLOUT7,            CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:ArrowShapes"_ustr,               SID_DRAWTBX_CS_ARROW,           CommandGroup::INSERT );

    implDescribeSupportedFeature( u".uno:FlowChartShapes"_ustr,           SID_DRAWTBX_CS_FLOWCHART,       CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:CalloutShapes"_ustr,             SID_DRAWTBX_CS_CALLOUT,         CommandGroup::INSERT );
    implDescribeSupportedFeature( u".uno:StarShapes"_ustr,                SID_DRAWTBX_CS_STAR,            CommandGroup::INSERT );


    // keys
    implDescribeSupportedFeature( u".uno:Escape"_ustr,                    SID_ESCAPE);

    // internal one
    implDescribeSupportedFeature( u".uno:RPT_RPTHEADER_UNDO"_ustr,            SID_REPORTHEADER_WITHOUT_UNDO);
    implDescribeSupportedFeature( u".uno:RPT_RPTFOOTER_UNDO"_ustr,            SID_REPORTFOOTER_WITHOUT_UNDO);
    implDescribeSupportedFeature( u".uno:RPT_PGHEADER_UNDO"_ustr,             SID_PAGEHEADER_WITHOUT_UNDO);
    implDescribeSupportedFeature( u".uno:RPT_PGFOOTER_UNDO"_ustr,             SID_PAGEFOOTER_WITHOUT_UNDO);
    implDescribeSupportedFeature( u".uno:SID_GROUPHEADER"_ustr,               SID_GROUPHEADER);
    implDescribeSupportedFeature( u".uno:SID_GROUPHEADER_WITHOUT_UNDO"_ustr,  SID_GROUPHEADER_WITHOUT_UNDO);
    implDescribeSupportedFeature( u".uno:SID_GROUPFOOTER"_ustr,               SID_GROUPFOOTER);
    implDescribeSupportedFeature( u".uno:SID_GROUPFOOTER_WITHOUT_UNDO"_ustr,  SID_GROUPFOOTER_WITHOUT_UNDO);
    implDescribeSupportedFeature( u".uno:SID_GROUP_REMOVE"_ustr,              SID_GROUP_REMOVE);
    implDescribeSupportedFeature( u".uno:SID_GROUP_APPEND"_ustr,              SID_GROUP_APPEND);
    implDescribeSupportedFeature( u".uno:SID_ADD_CONTROL_PAIR"_ustr,          SID_ADD_CONTROL_PAIR);
    implDescribeSupportedFeature( u".uno:SplitPosition"_ustr,                 SID_SPLIT_POSITION);
    implDescribeSupportedFeature( u".uno:LastPropertyBrowserPage"_ustr,       SID_PROPERTYBROWSER_LAST_PAGE);
    implDescribeSupportedFeature( u".uno:Select"_ustr,                        SID_SELECT);
    implDescribeSupportedFeature( u".uno:InsertFunction"_ustr,                SID_RPT_NEW_FUNCTION);
    implDescribeSupportedFeature( u".uno:NextMark"_ustr,                      SID_NEXT_MARK);
    implDescribeSupportedFeature( u".uno:PrevMark"_ustr,                      SID_PREV_MARK);
    implDescribeSupportedFeature( u".uno:TerminateInplaceActivation"_ustr,    SID_TERMINATE_INPLACEACTIVATION);
    implDescribeSupportedFeature( u".uno:SelectAllLabels"_ustr,               SID_SELECT_ALL_LABELS);
    implDescribeSupportedFeature( u".uno:SelectAllEdits"_ustr,                SID_SELECT_ALL_EDITS);
    implDescribeSupportedFeature( u".uno:CollapseSection"_ustr,           SID_COLLAPSE_SECTION);
    implDescribeSupportedFeature( u".uno:ExpandSection"_ustr,             SID_EXPAND_SECTION);
    implDescribeSupportedFeature( u".uno:GetUndoStrings"_ustr,            SID_GETUNDOSTRINGS);
    implDescribeSupportedFeature( u".uno:GetRedoStrings"_ustr,            SID_GETREDOSTRINGS);
}

void OReportController::impl_onModifyChanged()
{
    try
    {
        if ( m_xReportDefinition.is() )
            m_xReportDefinition->setModified( impl_isModified() );
        DBSubComponentController::impl_onModifyChanged();
    }
    catch(const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}

void OReportController::onLoadedMenu(const Reference< frame::XLayoutManager >& _xLayoutManager)
{
    if ( !_xLayoutManager.is() )
        return;

    static const std::u16string_view s_sMenu[] = {
         u"private:resource/statusbar/statusbar"
        ,u"private:resource/toolbar/reportcontrols"
        ,u"private:resource/toolbar/drawbar"
        ,u"private:resource/toolbar/Formatting"
        ,u"private:resource/toolbar/alignmentbar"
        ,u"private:resource/toolbar/sectionalignmentbar"
        ,u"private:resource/toolbar/resizebar"
        ,u"private:resource/toolbar/sectionshrinkbar"
    };
    for (const auto & i : s_sMenu)
    {
        _xLayoutManager->createElement( OUString(i) );
        _xLayoutManager->requestElement( OUString(i) );
    }
}

void OReportController::notifyGroupSections(const ContainerEvent& _rEvent,bool _bShow)
{
    uno::Reference< report::XGroup> xGroup(_rEvent.Element,uno::UNO_QUERY);
    if ( !xGroup.is() )
        return;

    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );
    sal_Int32 nGroupPos = 0;
    _rEvent.Accessor >>= nGroupPos;

    if ( _bShow )
    {
        xGroup->addPropertyChangeListener(PROPERTY_HEADERON, static_cast<XPropertyChangeListener*>(this));
        xGroup->addPropertyChangeListener(PROPERTY_FOOTERON, static_cast<XPropertyChangeListener*>(this));
    }
    else
    {
        xGroup->removePropertyChangeListener(PROPERTY_HEADERON, static_cast<XPropertyChangeListener*>(this));
        xGroup->removePropertyChangeListener(PROPERTY_FOOTERON, static_cast<XPropertyChangeListener*>(this));
    }

    if ( xGroup->getHeaderOn() )
    {
        groupChange(xGroup,PROPERTY_HEADERON,nGroupPos,_bShow);
        if (_bShow)
        {
            m_pReportControllerObserver->AddSection(xGroup->getHeader());
        }
        else
        {
            m_pReportControllerObserver->RemoveSection(xGroup->getHeader());
        }
    }
    if ( xGroup->getFooterOn() )
    {
        groupChange(xGroup,PROPERTY_FOOTERON,nGroupPos,_bShow);
        if (_bShow)
        {
            m_pReportControllerObserver->AddSection(xGroup->getFooter());
        }
        else
        {
            m_pReportControllerObserver->RemoveSection(xGroup->getFooter());
        }
    }
}

// ::container::XContainerListener
void SAL_CALL OReportController::elementInserted( const ContainerEvent& _rEvent )
{
    notifyGroupSections(_rEvent,true);
}

void SAL_CALL OReportController::elementRemoved( const ContainerEvent& _rEvent )
{
    notifyGroupSections(_rEvent,false);
}

void SAL_CALL OReportController::elementReplaced( const ContainerEvent& /*_rEvent*/ )
{
    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );
    OSL_FAIL("Not yet implemented!");
}

void SAL_CALL OReportController::propertyChange( const beans::PropertyChangeEvent& evt )
{
    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );
    try
    {
        bool bShow = false;
        evt.NewValue >>= bShow;
        if ( evt.Source == m_xReportDefinition )
        {
            if ( evt.PropertyName == PROPERTY_REPORTHEADERON )
            {
                const sal_uInt16 nPosition = m_xReportDefinition->getPageHeaderOn() ? 1 : 0;
                if ( bShow )
                {
                    getDesignView()->addSection(m_xReportDefinition->getReportHeader(),DBREPORTHEADER,nPosition);
                    m_pReportControllerObserver->AddSection(m_xReportDefinition->getReportHeader());
                }
                else
                {
                    getDesignView()->removeSection(nPosition);
                }
            }
            else if ( evt.PropertyName == PROPERTY_REPORTFOOTERON )
            {
                sal_uInt16 nPosition = getDesignView()->getSectionCount();
                if ( m_xReportDefinition->getPageFooterOn() )
                    --nPosition;
                if ( bShow )
                {
                    getDesignView()->addSection(m_xReportDefinition->getReportFooter(),DBREPORTFOOTER,nPosition);
                    m_pReportControllerObserver->AddSection(m_xReportDefinition->getReportFooter());
                }
                else
                {
                    getDesignView()->removeSection(nPosition - 1);
                }
            }
            else if ( evt.PropertyName == PROPERTY_PAGEHEADERON )
            {
                if ( bShow )
                {
                    getDesignView()->addSection(m_xReportDefinition->getPageHeader(),DBPAGEHEADER,0);
                    m_pReportControllerObserver->AddSection(m_xReportDefinition->getPageHeader());
                }
                else
                {
                    getDesignView()->removeSection(sal_uInt16(0));
                }
            }
            else if ( evt.PropertyName == PROPERTY_PAGEFOOTERON )
            {
                if ( bShow )
                {
                    getDesignView()->addSection(m_xReportDefinition->getPageFooter(),DBPAGEFOOTER);
                    m_pReportControllerObserver->AddSection(m_xReportDefinition->getPageFooter());
                }
                else
                {
                    getDesignView()->removeSection(getDesignView()->getSectionCount() - 1);
                }
            }
            else if (   evt.PropertyName == PROPERTY_COMMAND
                    ||  evt.PropertyName == PROPERTY_COMMANDTYPE
                    ||  evt.PropertyName == PROPERTY_ESCAPEPROCESSING
                    ||  evt.PropertyName == PROPERTY_FILTER
                    )
            {
                m_xColumns.clear();
                m_xHoldAlive.clear();
                InvalidateFeature(SID_FM_ADD_FIELD);
            }
            /// TODO: check what we need to notify here TitleHelper
            /*else if (   evt.PropertyName.equals( PROPERTY_CAPTION ) )
                updateTitle();*/
        }
        else
        {
            uno::Reference< report::XGroup> xGroup(evt.Source,uno::UNO_QUERY);
            if ( xGroup.is() )
            {
                sal_Int32 nGroupPos = getGroupPosition(xGroup);

                groupChange(xGroup,evt.PropertyName,nGroupPos,bShow);
            }
        }
    }
    catch(const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}


void SAL_CALL OReportController::disposing( const lang::EventObject& Source )
{
    // simply disambiguate
    OReportController_BASE::disposing(Source);
}


static sal_uInt16 lcl_getNonVisibleGroupsBefore( const uno::Reference< report::XGroups>& _xGroups
                          ,sal_Int32 _nGroupPos
                          ,::std::function<bool(OGroupHelper *)> const & _pGroupMemberFunction)
{
    uno::Reference< report::XGroup> xGroup;
    sal_uInt16 nNonVisibleGroups = 0;
    sal_Int32 nCount = _xGroups->getCount();
    for( sal_Int32 i = 0; i < _nGroupPos && i < nCount; ++i)
    {
        xGroup.set(_xGroups->getByIndex(i),uno::UNO_QUERY);
        OSL_ENSURE(xGroup.is(),"Group is NULL! -> GPF");
        OGroupHelper aGroupHelper(xGroup);
        if ( !_pGroupMemberFunction(&aGroupHelper) )
            ++nNonVisibleGroups;
    }
    return nNonVisibleGroups;
}

void OReportController::groupChange( const uno::Reference< report::XGroup>& _xGroup,std::u16string_view _sPropName,sal_Int32 _nGroupPos,bool _bShow)
{
    ::std::function<bool(OGroupHelper *)> pMemFun = ::std::mem_fn(&OGroupHelper::getHeaderOn);
    ::std::function<uno::Reference<report::XSection>(OGroupHelper *)> pMemFunSection = ::std::mem_fn(&OGroupHelper::getHeader);
    OUString sColor(DBGROUPHEADER);
    sal_uInt16 nPosition = 0;
    bool bHandle = false;
    if ( _sPropName == PROPERTY_HEADERON )
    {
        nPosition = m_xReportDefinition->getPageHeaderOn() ? (m_xReportDefinition->getReportHeaderOn() ? 2 : 1) : (m_xReportDefinition->getReportHeaderOn() ? 1 : 0);
        nPosition += (static_cast<sal_uInt16>(_nGroupPos) - lcl_getNonVisibleGroupsBefore(m_xReportDefinition->getGroups(),_nGroupPos,pMemFun));
        bHandle = true;
    }
    else if ( _sPropName == PROPERTY_FOOTERON )
    {
        pMemFun = ::std::mem_fn(&OGroupHelper::getFooterOn);
        pMemFunSection = ::std::mem_fn(&OGroupHelper::getFooter);
        nPosition = getDesignView()->getSectionCount();

        if ( m_xReportDefinition->getPageFooterOn() )
            --nPosition;
        if ( m_xReportDefinition->getReportFooterOn() )
            --nPosition;
        sColor = DBGROUPFOOTER;
        nPosition -= (static_cast<sal_uInt16>(_nGroupPos) - lcl_getNonVisibleGroupsBefore(m_xReportDefinition->getGroups(),_nGroupPos,pMemFun));
        if ( !_bShow )
            --nPosition;
        bHandle = true;
    }
    if ( bHandle )
    {
        if ( _bShow )
        {
            OGroupHelper aGroupHelper(_xGroup);
            getDesignView()->addSection(pMemFunSection(&aGroupHelper),sColor,nPosition);
        }
        else
        {
            getDesignView()->removeSection(nPosition);
        }
    }
}

IMPL_LINK_NOARG(OReportController, OnClipboardChanged, TransferableDataHelper*, void)
{
    OnInvalidateClipboard();
}

void OReportController::OnInvalidateClipboard()
{
    InvalidateFeature(SID_CUT);
    InvalidateFeature(SID_COPY);
    InvalidateFeature(SID_PASTE);
}

static ItemInfoPackage& getItemInfoPackageOpenPageDlg()
{
    class ItemInfoPackageOpenPageDlg : public ItemInfoPackage
    {
        typedef std::array<ItemInfoStatic, RPTUI_ID_METRIC - RPTUI_ID_LRSPACE + 1> ItemInfoArrayOpenPageDlg;
        ItemInfoArrayOpenPageDlg maItemInfos {{
            // m_nWhich, m_pItem, m_nSlotID, m_nItemInfoFlags
            { RPTUI_ID_LRSPACE, new SvxLRSpaceItem(RPTUI_ID_LRSPACE), SID_ATTR_LRSPACE, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_ULSPACE, new SvxULSpaceItem(RPTUI_ID_ULSPACE), SID_ATTR_ULSPACE, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_PAGE, new SvxPageItem(RPTUI_ID_PAGE), SID_ATTR_PAGE, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_SIZE, new SvxSizeItem(RPTUI_ID_SIZE), SID_ATTR_PAGE_SIZE, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_PAGE_MODE, new SfxUInt16Item(RPTUI_ID_PAGE_MODE,SVX_PAGE_MODE_STANDARD), SID_ENUM_PAGE_MODE, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_START, new SfxUInt16Item(RPTUI_ID_START,PAPER_A4), SID_PAPER_START, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_END, new SfxUInt16Item(RPTUI_ID_END,PAPER_E), SID_PAPER_END, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_BRUSH, new SvxBrushItem(RPTUI_ID_BRUSH), SID_ATTR_BRUSH, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLSTYLE, new XFillStyleItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLCOLOR, new XFillColorItem(u""_ustr, COL_DEFAULT_SHAPE_FILLING), 0, SFX_ITEMINFOFLAG_SUPPORT_SURROGATE },
            { XATTR_FILLGRADIENT, new XFillGradientItem(basegfx::BGradient()), 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLHATCH, new XFillHatchItem(XHatch(COL_DEFAULT_SHAPE_STROKE)), 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBITMAP, nullptr, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLTRANSPARENCE, new XFillTransparenceItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_GRADIENTSTEPCOUNT, new XGradientStepCountItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_TILE, new XFillBmpTileItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_POS, new XFillBmpPosItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_SIZEX, new XFillBmpSizeXItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_SIZEY, new XFillBmpSizeYItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLFLOATTRANSPARENCE, new XFillFloatTransparenceItem(basegfx::BGradient(), false), 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_SECONDARYFILLCOLOR, new XSecondaryFillColorItem(u""_ustr, COL_DEFAULT_SHAPE_FILLING), 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_SIZELOG, new XFillBmpSizeLogItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_TILEOFFSETX, new XFillBmpTileOffsetXItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_TILEOFFSETY, new XFillBmpTileOffsetYItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_STRETCH, new XFillBmpStretchItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_POSOFFSETX, new XFillBmpPosOffsetXItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBMP_POSOFFSETY, new XFillBmpPosOffsetYItem, 0, SFX_ITEMINFOFLAG_NONE },
            { XATTR_FILLBACKGROUND, new XFillBackgroundItem, 0, SFX_ITEMINFOFLAG_NONE },
            { RPTUI_ID_METRIC, nullptr, SID_ATTR_METRIC, SFX_ITEMINFOFLAG_NONE },
        }};

        virtual const ItemInfoStatic& getItemInfoStatic(size_t nIndex) const override { return maItemInfos[nIndex]; }

    public:
        ItemInfoPackageOpenPageDlg()
        {
            const MeasurementSystem eSystem(SvtSysLocale().GetLocaleData().getMeasurementSystemEnum());
            const FieldUnit eUserMetric(MeasurementSystem::Metric == eSystem ? FieldUnit::CM : FieldUnit::INCH);
            setItemAtItemInfoStatic(
                new SfxUInt16Item(RPTUI_ID_METRIC,static_cast<sal_uInt16>(eUserMetric)),
                maItemInfos[RPTUI_ID_METRIC - RPTUI_ID_LRSPACE]);
        }

        virtual size_t size() const override { return maItemInfos.size(); }
        virtual const ItemInfo& getItemInfo(size_t nIndex, SfxItemPool& /*rPool*/) override
        {
            const ItemInfo& rRetval(maItemInfos[nIndex]);

            // return immediately if we have the static entry and Item
            if (nullptr != rRetval.getItem())
                return rRetval;

            if (XATTR_FILLBITMAP == rRetval.getWhich())
                return *new ItemInfoDynamic(rRetval, new XFillBitmapItem(Graphic()));

            // return in any case
            return rRetval;
        }
    };

    static std::unique_ptr<ItemInfoPackageOpenPageDlg> g_aItemInfoPackageOpenPageDlg;
    if (!g_aItemInfoPackageOpenPageDlg)
        g_aItemInfoPackageOpenPageDlg.reset(new ItemInfoPackageOpenPageDlg);
    return *g_aItemInfoPackageOpenPageDlg;
}

void OReportController::openPageDialog(const uno::Reference<report::XSection>& _xSection)
{
    if ( !m_xReportDefinition.is() )
        return;

    // UNO->ItemSet
    MeasurementSystem eSystem = SvtSysLocale().GetLocaleData().getMeasurementSystemEnum();
    FieldUnit eUserMetric = MeasurementSystem::Metric == eSystem ? FieldUnit::CM : FieldUnit::INCH;
    rtl::Reference<SfxItemPool> pPool(new SfxItemPool(u"ReportPageProperties"_ustr));
    pPool->registerItemInfoPackage(getItemInfoPackageOpenPageDlg());
    pPool->SetDefaultMetric( MapUnit::Map100thMM );    // ripped, don't understand why

    try
    {
        static const WhichRangesContainer pRanges(svl::Items<
            RPTUI_ID_LRSPACE, XATTR_FILL_LAST,
            SID_ATTR_METRIC,SID_ATTR_METRIC
        >);
        SfxItemSet aDescriptor(*pPool, pRanges);
        // fill it
        if ( _xSection.is() )
            aDescriptor.Put(SvxBrushItem(::Color(ColorTransparency, _xSection->getBackColor()),RPTUI_ID_BRUSH));
        else
        {
            aDescriptor.Put(SvxSizeItem(RPTUI_ID_SIZE,
                                        vcl::unohelper::ConvertToVCLSize(getStyleProperty<awt::Size>(m_xReportDefinition,PROPERTY_PAPERSIZE))));
            aDescriptor.Put(SvxLRSpaceItem(SvxIndentValue::twips(getStyleProperty<sal_Int32>(
                                               m_xReportDefinition, PROPERTY_LEFTMARGIN)),
                                           SvxIndentValue::twips(getStyleProperty<sal_Int32>(
                                               m_xReportDefinition, PROPERTY_RIGHTMARGIN)),
                                           SvxIndentValue::zero(), RPTUI_ID_LRSPACE));
            aDescriptor.Put(SvxULSpaceItem(static_cast<sal_uInt16>(getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_TOPMARGIN))
                                            ,static_cast<sal_uInt16>(getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_BOTTOMMARGIN)),RPTUI_ID_ULSPACE));
            aDescriptor.Put(SfxUInt16Item(SID_ATTR_METRIC,static_cast<sal_uInt16>(eUserMetric)));

            uno::Reference< style::XStyle> xPageStyle(getUsedStyle(m_xReportDefinition));
            if ( xPageStyle.is() )
            {
                SvxPageItem aPageItem(RPTUI_ID_PAGE);
                aPageItem.SetDescName(xPageStyle->getName());
                uno::Reference<beans::XPropertySet> xProp(xPageStyle,uno::UNO_QUERY_THROW);
                aPageItem.PutValue(xProp->getPropertyValue(PROPERTY_PAGESTYLELAYOUT),MID_PAGE_LAYOUT);
                aPageItem.SetLandscape(getStyleProperty<bool>(m_xReportDefinition,PROPERTY_ISLANDSCAPE));
                aPageItem.SetNumType(static_cast<SvxNumType>(getStyleProperty<sal_Int16>(m_xReportDefinition,PROPERTY_NUMBERINGTYPE)));
                aDescriptor.Put(aPageItem);
                aDescriptor.Put(SvxBrushItem(::Color(ColorTransparency, getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_BACKCOLOR)),RPTUI_ID_BRUSH));
            }
        }

        {   // want the dialog to be destroyed before our set
            ORptPageDialog aDlg(
                getFrameWeld(), &aDescriptor,_xSection.is()
                           ? u"BackgroundDialog"_ustr
                           : u"PageDialog"_ustr);
            if (aDlg.run() == RET_OK)
            {

                // ItemSet->UNO
                // UNO-properties
                const SfxItemSet* pSet = aDlg.GetOutputItemSet();
                if ( _xSection.is() )
                {
                    if ( const SvxBrushItem* pBrushItem = pSet->GetItemIfSet( RPTUI_ID_BRUSH ))
                        _xSection->setBackColor(sal_Int32(pBrushItem->GetColor()));
                }
                else
                {
                    uno::Reference< beans::XPropertySet> xProp(getUsedStyle(m_xReportDefinition),uno::UNO_QUERY_THROW);
                    const OUString sUndoAction(RptResId(RID_STR_UNDO_CHANGEPAGE));
                    UndoContext aUndoContext( getUndoManager(), sUndoAction );
                    if ( const SvxSizeItem* pSizeItem = pSet->GetItemIfSet( RPTUI_ID_SIZE ))
                    {
                        uno::Any aValue;
                        pSizeItem->QueryValue(aValue);
                        xProp->setPropertyValue(PROPERTY_PAPERSIZE,aValue);
                        resetZoomType();
                    }

                    if ( const SvxLRSpaceItem* pSpaceItem = pSet->GetItemIfSet( RPTUI_ID_LRSPACE ))
                    {
                        Any aValue;
                        pSpaceItem->QueryValue(aValue,MID_L_MARGIN);
                        xProp->setPropertyValue(PROPERTY_LEFTMARGIN,aValue);
                        pSpaceItem->QueryValue(aValue,MID_R_MARGIN);
                        xProp->setPropertyValue(PROPERTY_RIGHTMARGIN,aValue);
                    }
                    if ( const SvxULSpaceItem* pSpaceItem = pSet->GetItemIfSet( RPTUI_ID_ULSPACE ))
                    {
                        xProp->setPropertyValue(PROPERTY_TOPMARGIN,uno::Any(pSpaceItem->GetUpper()));
                        xProp->setPropertyValue(PROPERTY_BOTTOMMARGIN,uno::Any(pSpaceItem->GetLower()));
                    }
                    if ( const SvxPageItem* pPageItem = pSet->GetItemIfSet( RPTUI_ID_PAGE ))
                    {
                        xProp->setPropertyValue(PROPERTY_ISLANDSCAPE,uno::Any(pPageItem->IsLandscape()));
                        xProp->setPropertyValue(PROPERTY_NUMBERINGTYPE,uno::Any(static_cast<sal_Int16>(pPageItem->GetNumType())));
                        uno::Any aValue;
                        pPageItem->QueryValue(aValue,MID_PAGE_LAYOUT);
                        xProp->setPropertyValue(PROPERTY_PAGESTYLELAYOUT,aValue);
                        resetZoomType();
                    }
                    if ( const SvxBrushItem* pBrushItem = pSet->GetItemIfSet( RPTUI_ID_BRUSH ))
                    {
                        ::Color aBackColor = pBrushItem->GetColor();
                        xProp->setPropertyValue(PROPERTY_BACKTRANSPARENT,uno::Any(aBackColor == COL_TRANSPARENT));
                        xProp->setPropertyValue(PROPERTY_BACKCOLOR,uno::Any(aBackColor));
                    }
                }
            }
        }
    }
    catch(const Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
    pPool.clear();
}


sal_Bool SAL_CALL OReportController::attachModel(const uno::Reference< frame::XModel > & xModel)
{
    ::osl::MutexGuard aGuard( getMutex() );

    uno::Reference< report::XReportDefinition > xReportDefinition( xModel, UNO_QUERY );
    if ( !xReportDefinition.is() )
        return false;

    uno::Reference< document::XUndoManagerSupplier > xTestSuppUndo( xModel, UNO_QUERY );
    if ( !xTestSuppUndo.is() )
        return false;

    m_xReportDefinition = std::move(xReportDefinition);
    return true;
}


void OReportController::openSortingAndGroupingDialog()
{
    if ( !m_xReportDefinition.is() )
        return;
    if (!m_xGroupsFloater)
    {
        m_xGroupsFloater = std::make_shared<OGroupsSortingDialog>(getFrameWeld(), !isEditable(), this);
        SvtViewOptions aDlgOpt(EViewType::Window, m_xGroupsFloater->get_help_id());
        if ( aDlgOpt.Exists() )
            m_xGroupsFloater->getDialog()->set_window_state(aDlgOpt.GetWindowState());
    }
    if (isUiVisible())
    {
        if (!m_xGroupsFloater->getDialog()->get_visible())
            weld::DialogController::runAsync(m_xGroupsFloater, [this](sal_Int32 /*nResult*/) { m_xGroupsFloater.reset(); });
        else
            m_xGroupsFloater->response(RET_CANCEL);
    }
}

sal_Int32 OReportController::getGroupPosition(const uno::Reference< report::XGroup >& _xGroup)
{
    return rptui::getPositionInIndexAccess(m_xReportDefinition->getGroups(),_xGroup);
}


void OReportController::Notify(SfxBroadcaster & /* _rBc */, SfxHint const & _rHint)
{
    if (_rHint.GetId() != SfxHintId::ReportDesignDlgEd)
        return;
    const DlgEdHint* pDlgEdHint = static_cast<const DlgEdHint*>(&_rHint);
    if (pDlgEdHint->GetKind() != RPTUI_HINT_SELECTIONCHANGED)
        return;

    const sal_Int32 nSelectionCount = getDesignView()->getMarkedObjectCount();
    if ( m_nSelectionCount != nSelectionCount )
    {
        m_nSelectionCount = nSelectionCount;
        InvalidateAll();
    }
    lang::EventObject aEvent(*this);
    m_aSelectionListeners.forEach(
        [&aEvent] (uno::Reference<view::XSelectionChangeListener> const& xListener) {
            return xListener->selectionChanged(aEvent);
        });
}

void OReportController::executeMethodWithUndo(TranslateId pUndoStrId,const ::std::function<void(ODesignView *)>& _pMemfun)
{
    const OUString sUndoAction = RptResId(pUndoStrId);
    UndoContext aUndoContext( getUndoManager(), sUndoAction );
    _pMemfun( getDesignView() );
    InvalidateFeature( SID_UNDO );
}

void OReportController::alignControlsWithUndo(TranslateId pUndoStrId, ControlModification _nControlModification, bool _bAlignAtSection)
{
    const OUString sUndoAction = RptResId(pUndoStrId);
    UndoContext aUndoContext( getUndoManager(), sUndoAction );
    getDesignView()->alignMarkedObjects(_nControlModification,_bAlignAtSection);
    InvalidateFeature( SID_UNDO );
}

void OReportController::shrinkSectionBottom(const uno::Reference<report::XSection>& _xSection)
{
    const sal_Int32 nElements = _xSection->getCount();
    if (nElements == 0)
    {
        // there are no elements
        return;
    }
    const sal_Int32 nSectionHeight = _xSection->getHeight();
    sal_Int32 nMaxPositionY = 0;
    uno::Reference< report::XReportComponent> xReportComponent;

    // for every component get its Y-position and compare it to the current Y-position
    for (int i=0;i<nElements;i++)
    {
        xReportComponent.set(_xSection->getByIndex(i), uno::UNO_QUERY);
        const sal_Int32 nReportComponentPositionY = xReportComponent->getPositionY();
        const sal_Int32 nReportComponentHeight = xReportComponent->getHeight();
        const sal_Int32 nReportComponentPositionYAndHeight = nReportComponentPositionY + nReportComponentHeight;
        nMaxPositionY = std::max(nReportComponentPositionYAndHeight, nMaxPositionY);
    }
    // now we know the minimal Y-Position and maximal Y-Position

    if (nMaxPositionY > (nSectionHeight - 7) ) // Magic Number, we use a little bit less heights for right positioning
    {
        // the lowest position is already 0
        return;
    }
    _xSection->setHeight(nMaxPositionY);
}

void OReportController::shrinkSectionTop(const uno::Reference<report::XSection>& _xSection)
{
    const sal_Int32 nElements = _xSection->getCount();
    if (nElements == 0)
    {
        // there are no elements
        return;
    }

    const sal_Int32 nSectionHeight = _xSection->getHeight();
    sal_Int32 nMinPositionY = nSectionHeight;
    uno::Reference< report::XReportComponent> xReportComponent;

    // for every component get its Y-position and compare it to the current Y-position
    for (int i=0;i<nElements;i++)
    {
        xReportComponent.set(_xSection->getByIndex(i), uno::UNO_QUERY);
        const sal_Int32 nReportComponentPositionY = xReportComponent->getPositionY();
        nMinPositionY = std::min(nReportComponentPositionY, nMinPositionY);
    }
    // now we know the minimal Y-Position and maximal Y-Position
    if (nMinPositionY == 0)
    {
        // the lowest position is already 0
        return;
    }
    for (int i=0;i<nElements;i++)
    {
        xReportComponent.set(_xSection->getByIndex(i), uno::UNO_QUERY);
        const sal_Int32 nReportComponentPositionY = xReportComponent->getPositionY();
        const sal_Int32 nNewPositionY = nReportComponentPositionY - nMinPositionY;
        xReportComponent->setPositionY(nNewPositionY);
    }
    const sal_Int32 nNewSectionHeight = nSectionHeight - nMinPositionY;
    _xSection->setHeight(nNewSectionHeight);
}

void OReportController::shrinkSection(TranslateId pUndoStrId, const uno::Reference<report::XSection>& _xSection, sal_Int32 _nSid)
{
    if ( _xSection.is() )
    {
        const OUString sUndoAction = RptResId(pUndoStrId);
        UndoContext aUndoContext( getUndoManager(), sUndoAction );

        if (_nSid == SID_SECTION_SHRINK)
        {
            shrinkSectionTop(_xSection);
            shrinkSectionBottom(_xSection);
        }
        else if (_nSid == SID_SECTION_SHRINK_TOP)
        {
            shrinkSectionTop(_xSection);
        }
        else if (_nSid == SID_SECTION_SHRINK_BOTTOM)
        {
            shrinkSectionBottom(_xSection);
        }
    }

    InvalidateFeature( SID_UNDO );
}


uno::Any SAL_CALL OReportController::getViewData()
{
    ::osl::MutexGuard aGuard( getMutex() );

    const sal_Int32 nCommandIDs[] =
    {
        SID_GRID_VISIBLE,
        SID_GRID_USE,
        SID_HELPLINES_MOVE,
        SID_RULER,
        SID_SHOW_PROPERTYBROWSER,
        SID_PROPERTYBROWSER_LAST_PAGE,
        SID_SPLIT_POSITION
    };

    ::comphelper::NamedValueCollection aCommandProperties;
    for (sal_Int32 nCommandID : nCommandIDs)
    {
        const FeatureState aFeatureState = GetState( nCommandID );

        OUString sCommandURL( getURLForId( nCommandID ).Main );
        OSL_ENSURE( sCommandURL.startsWith( ".uno:" ), "OReportController::getViewData: illegal command URL!" );
        sCommandURL = sCommandURL.copy( 5 );

        Any aCommandState;
        if ( aFeatureState.bChecked.has_value() )
            aCommandState <<= *aFeatureState.bChecked;
        else if ( aFeatureState.aValue.hasValue() )
            aCommandState = aFeatureState.aValue;

        aCommandProperties.put( sCommandURL, aCommandState );
    }

    ::comphelper::NamedValueCollection aViewData;
    aViewData.put( u"CommandProperties"_ustr, aCommandProperties.getPropertyValues() );

    if ( getDesignView() )
    {
        ::std::vector<sal_uInt16> aCollapsedPositions;
        getDesignView()->fillCollapsedSections(aCollapsedPositions);
        if ( !aCollapsedPositions.empty() )
        {
            uno::Sequence<beans::PropertyValue> aCollapsedSections(aCollapsedPositions.size());
            beans::PropertyValue* pCollapsedIter = aCollapsedSections.getArray();
            sal_Int32 i = 1;
            for (const auto& rPos : aCollapsedPositions)
            {
                pCollapsedIter->Name = PROPERTY_SECTION + OUString::number(i);
                pCollapsedIter->Value <<= static_cast<sal_Int32>(rPos);
                ++pCollapsedIter;
                ++i;
            }

            aViewData.put( u"CollapsedSections"_ustr, aCollapsedSections );
        }

        OSectionWindow* pSectionWindow = getDesignView()->getMarkedSection();
        if ( pSectionWindow )
        {
            aViewData.put( u"MarkedSection"_ustr, static_cast<sal_Int32>(pSectionWindow->getReportSection().getPage()->GetPageNum()) );
        }
    }

    aViewData.put( u"ZoomFactor"_ustr, m_nZoomValue );
    return uno::Any( aViewData.getPropertyValues() );
}

void SAL_CALL OReportController::restoreViewData(const uno::Any& i_data)
{
    ::osl::MutexGuard aGuard( getMutex() );

    try
    {
        const ::comphelper::NamedValueCollection aViewData( i_data );

        m_aCollapsedSections = aViewData.getOrDefault( u"CollapsedSections"_ustr, m_aCollapsedSections );
        m_nPageNum = aViewData.getOrDefault( u"MarkedSection"_ustr, m_nPageNum );
        m_nZoomValue = aViewData.getOrDefault( u"ZoomFactor"_ustr, m_nZoomValue );
        // TODO: setting those 3 members is not enough - in theory, restoreViewData can be called when the
        // view is fully alive, so we need to reflect those 3 values in the view.
        // (At the moment, the method is called only during construction phase)


        ::comphelper::NamedValueCollection aCommandProperties( aViewData.get( u"CommandProperties"_ustr ) );
        const ::std::vector< OUString > aCommandNames( aCommandProperties.getNames() );

        for ( const auto& rCommandName : aCommandNames )
        {
            const Any& rCommandValue = aCommandProperties.get( rCommandName );
            if ( !rCommandValue.hasValue() )
                continue;

            if ( getView() )
            {
                util::URL aCommand;
                aCommand.Complete = ".uno:" + rCommandName;

                executeUnChecked( aCommand, { comphelper::makePropertyValue(u"Value"_ustr, rCommandValue) } );
            }
            else
            {
                if ( rCommandName == "ShowRuler" )
                    OSL_VERIFY( rCommandValue >>= m_bShowRuler );
                else if ( rCommandName == "HelplinesMove" )
                    OSL_VERIFY( rCommandValue >>= m_bHelplinesMove );
                else if ( rCommandName == "GridVisible" )
                    OSL_VERIFY( rCommandValue >>= m_bGridVisible );
                else if ( rCommandName == "GridUse" )
                    OSL_VERIFY( rCommandValue >>= m_bGridUse );
                else if ( rCommandName == "ControlProperties" )
                    OSL_VERIFY( rCommandValue >>= m_bShowProperties );
                else if ( rCommandName == "LastPropertyBrowserPage" )
                    OSL_VERIFY( rCommandValue >>= m_sLastActivePage );
                else if ( rCommandName == "SplitPosition" )
                    OSL_VERIFY( rCommandValue >>= m_nSplitPos );
            }
        }
    }
    catch(const IllegalArgumentException&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}

Reference<XFrame> OReportController::getXFrame()
{
    if ( !m_xFrameLoader.is() )
    {
        m_xFrameLoader.set( frame::Desktop::create(m_xContext) );
    }
    const sal_Int32 nFrameSearchFlag = frame::FrameSearchFlag::TASKS | frame::FrameSearchFlag::CREATE;
    Reference<XFrame> xFrame = m_xFrameLoader->findFrame(u"_blank"_ustr,nFrameSearchFlag);
    return xFrame;
}


uno::Reference<frame::XModel> OReportController::executeReport()
{
    OSL_ENSURE(m_xReportDefinition.is(),"Where is my report?");

    uno::Reference<frame::XModel> xModel;
    if ( m_xReportDefinition.is() )
    {
        TranslateId pErrorId = RID_ERR_NO_COMMAND;
        bool bEnabled = !m_xReportDefinition->getCommand().isEmpty();
        if ( bEnabled )
        {
            bEnabled = false;
            const sal_uInt16 nCount = m_aReportModel->GetPageCount();
            sal_uInt16 i = 0;
            for (; i < nCount && !bEnabled ; ++i)
            {
                const SdrPage* pPage = m_aReportModel->GetPage(i);
                bEnabled = pPage->GetObjCount() != 0;
            }
            if ( !bEnabled )
                pErrorId = RID_ERR_NO_OBJECTS;
        }

        dbtools::SQLExceptionInfo aInfo;
        if ( !bEnabled )
        {
            sdb::SQLContext aFirstMessage(RptResId(pErrorId), {}, {}, 0, {}, {});
            aInfo = aFirstMessage;
            if ( isEditable() )
            {
                sal_uInt16 nCommand = 0;
                if (pErrorId != RID_ERR_NO_COMMAND)
                {
                    if ( !m_bShowProperties )
                        executeUnChecked(SID_SHOW_PROPERTYBROWSER, {});

                    m_sLastActivePage = "Data";
                    getDesignView()->setCurrentPage(m_sLastActivePage);
                    nCommand = SID_SELECT_REPORT;
                }
                else if ( getDesignView() && !getDesignView()->isAddFieldVisible() )
                {
                    nCommand = SID_FM_ADD_FIELD;
                }
                if ( nCommand )
                {
                    executeUnChecked(nCommand, {});
                }
            }
        }
        else
        {
            m_bInGeneratePreview = true;
            try
            {
                weld::WaitObject aWait(getFrameWeld()); // cursor
                if ( !m_xReportEngine.is() )
                    m_xReportEngine.set( report::ReportEngine::create(m_xContext) );
                m_xReportEngine->setReportDefinition(m_xReportDefinition);
                m_xReportEngine->setActiveConnection(getConnection());
                Reference<XFrame> xFrame = getXFrame();
                xModel = m_xReportEngine->createDocumentAlive(xFrame);
            }
            catch(const sdbc::SQLException&)
            {   // SQLExceptions and derived exceptions must not be translated
                aInfo = ::cppu::getCaughtException();
            }
            catch(const uno::Exception& e)
            {
                uno::Any aCaughtException( ::cppu::getCaughtException() );

                // maybe our third message: the message which is wrapped in the exception we caught
                css::uno::Any aOptThirdMessage;
                if (lang::WrappedTargetException aWrapped; aCaughtException >>= aWrapped)
                {
                    sdbc::SQLException aThirdMessage;
                    aThirdMessage.Message = aWrapped.Message;
                    aThirdMessage.Context = aWrapped.Context;
                    if ( !aThirdMessage.Message.isEmpty() )
                        aOptThirdMessage <<= aThirdMessage;
                }


                // our second message: the message of the exception we caught
                sdbc::SQLException aSecondMessage(e.Message, e.Context, {}, 0, aOptThirdMessage);

                // our first message says: we caught an exception
                OUString sInfo(RptResId(RID_STR_CAUGHT_FOREIGN_EXCEPTION));
                sInfo = sInfo.replaceAll("$type$", aCaughtException.getValueTypeName());
                sdb::SQLContext aFirstMessage(sInfo, {}, {}, 0, css::uno::Any(aSecondMessage), {});

                aInfo = aFirstMessage;
            }
            if (aInfo.isValid())
            {
                const OUString suSQLContext = RptResId( RID_STR_COULD_NOT_CREATE_REPORT );
                aInfo.prepend(suSQLContext);
            }
            m_bInGeneratePreview = false;
        }

        if (aInfo.isValid())
        {
            showError(aInfo);
        }
    }
    return xModel;
}

uno::Reference< frame::XModel >  SAL_CALL OReportController::getModel()
{
    return m_xReportDefinition;
}

uno::Reference< sdbc::XRowSet > const & OReportController::getRowSet()
{
    OSL_PRECOND( m_xReportDefinition.is(), "OReportController::getRowSet: no report definition?!" );

    if ( m_xRowSet.is() || !m_xReportDefinition.is() )
        return m_xRowSet;

    try
    {
        uno::Reference< sdbc::XRowSet > xRowSet(
            getORB()->getServiceManager()->createInstanceWithContext(u"com.sun.star.sdb.RowSet"_ustr, getORB()),
            uno::UNO_QUERY );
        uno::Reference< beans::XPropertySet> xRowSetProp( xRowSet, uno::UNO_QUERY_THROW );

        xRowSetProp->setPropertyValue( PROPERTY_ACTIVECONNECTION, uno::Any( getConnection() ) );
        xRowSetProp->setPropertyValue( PROPERTY_APPLYFILTER, uno::Any( true ) );

        auto aNoConverter = std::make_shared<AnyConverter>();
        TPropertyNamePair aPropertyMediation;
        aPropertyMediation.emplace( PROPERTY_COMMAND, TPropertyConverter(PROPERTY_COMMAND,aNoConverter) );
        aPropertyMediation.emplace( PROPERTY_COMMANDTYPE, TPropertyConverter(PROPERTY_COMMANDTYPE,aNoConverter) );
        aPropertyMediation.emplace( PROPERTY_ESCAPEPROCESSING, TPropertyConverter(PROPERTY_ESCAPEPROCESSING,aNoConverter) );
        aPropertyMediation.emplace( PROPERTY_FILTER, TPropertyConverter(PROPERTY_FILTER,aNoConverter) );

        m_xRowSetMediator = new OPropertyMediator( m_xReportDefinition, xRowSetProp, std::move(aPropertyMediation) );
        m_xRowSet = std::move(xRowSet);
    }
    catch(const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }

    return m_xRowSet;
}

void OReportController::insertGraphic()
{
    const OUString sTitle(RptResId(RID_STR_IMPORT_GRAPHIC));
    // build some arguments for the upcoming dialog
    try
    {
        uno::Reference< report::XSection> xSection = getDesignView()->getCurrentSection();
        ::sfx2::FileDialogHelper aDialog(ui::dialogs::TemplateDescription::FILEOPEN_LINK_PREVIEW, FileDialogFlags::Graphic, getFrameWeld());
        aDialog.SetContext(sfx2::FileDialogHelper::ReportInsertImage);
        aDialog.SetTitle( sTitle );

        uno::Reference< ui::dialogs::XFilePickerControlAccess > xController(aDialog.GetFilePicker(), UNO_QUERY_THROW);
        xController->setValue(ui::dialogs::ExtendedFilePickerElementIds::CHECKBOX_PREVIEW, 0, css::uno::Any(true));
        xController->enableControl(ui::dialogs::ExtendedFilePickerElementIds::CHECKBOX_LINK, false/*sal_True*/);
        xController->setValue( ui::dialogs::ExtendedFilePickerElementIds::CHECKBOX_LINK, 0, css::uno::Any(true) );

        if ( ERRCODE_NONE == aDialog.Execute() )
        {
            bool bLink = true;
            xController->getValue( ui::dialogs::ExtendedFilePickerElementIds::CHECKBOX_LINK, 0) >>= bLink;
            uno::Sequence<beans::PropertyValue> aArgs( comphelper::InitPropertySequence({
                    { PROPERTY_IMAGEURL, Any(aDialog.GetPath()) },
                    { PROPERTY_PRESERVEIRI, Any(bLink) }
                }));
            createControl(aArgs,xSection,OUString(),SdrObjKind::ReportDesignImageControl);
        }
    }
    catch(const Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}


sal_Bool SAL_CALL OReportController::select( const Any& aSelection )
{
    ::osl::MutexGuard aGuard( getMutex() );
    if ( !getDesignView() )
        return true;

    getDesignView()->unmarkAllObjects();
    getDesignView()->SetMode(DlgEdMode::Select);

    uno::Sequence< uno::Reference<report::XReportComponent> > aElements;
    if ( aSelection >>= aElements )
    {
        if ( aElements.hasElements() )
            getDesignView()->showProperties(uno::Reference<uno::XInterface>(aElements[0],uno::UNO_QUERY));
        getDesignView()->setMarked(aElements, true);
    }
    else
    {
        uno::Reference<uno::XInterface> xObject(aSelection,uno::UNO_QUERY);
        uno::Reference<report::XReportComponent> xProp(xObject,uno::UNO_QUERY);
        if ( xProp.is() )
        {
            getDesignView()->showProperties(xObject);
            aElements = { xProp };
            getDesignView()->setMarked(aElements, true);
        }
        else
        {
            uno::Reference<report::XSection> xSection(aSelection,uno::UNO_QUERY);
            if ( !xSection.is() && xObject.is() )
                getDesignView()->showProperties(xObject);
            getDesignView()->setMarked(xSection,xSection.is());
        }
    }
    InvalidateAll();
    return true;
}

Any SAL_CALL OReportController::getSelection(  )
{
    ::osl::MutexGuard aGuard( getMutex() );
    Any aRet;
    if ( getDesignView() )
    {
        aRet = getDesignView()->getCurrentlyShownProperty();
        if ( !aRet.hasValue() )
            aRet <<= getDesignView()->getCurrentSection();
    }
    return aRet;
}

void SAL_CALL OReportController::addSelectionChangeListener( const Reference< view::XSelectionChangeListener >& Listener )
{
    m_aSelectionListeners.addInterface( Listener );
}

void SAL_CALL OReportController::removeSelectionChangeListener( const Reference< view::XSelectionChangeListener >& Listener )
{
    m_aSelectionListeners.removeInterface( Listener );
}

void OReportController::createNewFunction(const uno::Any& _aValue)
{
    uno::Reference< container::XIndexContainer> xFunctions(_aValue,uno::UNO_QUERY_THROW);
    const OUString sNewName = RptResId(RID_STR_FUNCTION);
    uno::Reference< report::XFunction> xFunction(report::Function::create(m_xContext));
    xFunction->setName(sNewName);
    // the call below will also create an undo action -> listener
    xFunctions->insertByIndex(xFunctions->getCount(),uno::Any(xFunction));
}

IMPL_LINK_NOARG( OReportController, OnExecuteReport, void*, void )
{
    executeReport();
}

void OReportController::createControl(const Sequence< PropertyValue >& _aArgs,const uno::Reference< report::XSection>& _xSection,const OUString& _sFunction,SdrObjKind _nObjectId)
{
    SequenceAsHashMap aMap(_aArgs);
    getDesignView()->setMarked(_xSection, true);
    OSectionWindow* pSectionWindow = getDesignView()->getMarkedSection();
    if ( !pSectionWindow )
        return;

    OSL_ENSURE(pSectionWindow->getReportSection().getSection() == _xSection,"Invalid section after marking the correct one.");

    sal_Int32 nLeftMargin = getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_LEFTMARGIN);
    const sal_Int32 nRightMargin = getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_RIGHTMARGIN);
    const sal_Int32 nPaperWidth = getStyleProperty<awt::Size>(m_xReportDefinition,PROPERTY_PAPERSIZE).Width - nRightMargin;
    awt::Point aPos = aMap.getUnpackedValueOrDefault(PROPERTY_POSITION,awt::Point(nLeftMargin,0));
    if ( aPos.X < nLeftMargin )
        aPos.X = nLeftMargin;

    rtl::Reference<SdrObject> pNewControl;
    uno::Reference< report::XReportComponent> xShapeProp;
    if ( _nObjectId == SdrObjKind::CustomShape )
    {
        pNewControl = SdrObjFactory::MakeNewObject(
            *m_aReportModel,
            SdrInventor::ReportDesign,
            _nObjectId);
        xShapeProp.set(pNewControl->getUnoShape(),uno::UNO_QUERY);
        OUString sCustomShapeType = getDesignView()->GetInsertObjString();
        if ( sCustomShapeType.isEmpty() )
            sCustomShapeType = "diamond";
        OReportSection::createDefault(sCustomShapeType,pNewControl.get());
        pNewControl->SetLogicRect(tools::Rectangle(3000,500,6000,3500)); // switch height and width
    }
    else if ( _nObjectId == SdrObjKind::OLE2 || SdrObjKind::ReportDesignSubReport == _nObjectId  )
    {
        pNewControl = SdrObjFactory::MakeNewObject(
            *m_aReportModel,
            SdrInventor::ReportDesign,
            _nObjectId);

        pNewControl->SetLogicRect(tools::Rectangle(3000,500,8000,5500)); // switch height and width
        xShapeProp.set(pNewControl->getUnoShape(),uno::UNO_QUERY_THROW);
        OOle2Obj* pObj = dynamic_cast<OOle2Obj*>(pNewControl.get());
        if ( pObj && !pObj->IsEmpty() )
        {
            pObj->initializeChart(getModel());
        }
    }
    else
    {
        rtl::Reference<SdrUnoObj> pLabel;
        rtl::Reference<SdrUnoObj> pControl;

        FmFormView::createControlLabelPair(
            getDesignView()->GetOutDev(),
            nLeftMargin,
            0,
            nullptr,
            nullptr,
            _nObjectId,
            SdrInventor::ReportDesign,
            SdrObjKind::ReportDesignFixedText,

            // tdf#118963 Need a SdrModel for SdrObject creation. Dereferencing
            // m_aReportModel seems pretty safe, it's done in other places, initialized
            // in impl_initialize and throws a RuntimeException if not existing.
            *m_aReportModel,

            pLabel,
            pControl);

        pLabel.clear();

        pNewControl = pControl;
        OUnoObject* pObj = dynamic_cast<OUnoObject*>(pNewControl.get());
        assert(pObj);
        if(pObj)
        {
            uno::Reference<beans::XPropertySet> xUnoProp(pObj->GetUnoControlModel(),uno::UNO_QUERY);
            xShapeProp.set(pObj->getUnoShape(),uno::UNO_QUERY);
            uno::Reference<beans::XPropertySetInfo> xShapeInfo = xShapeProp->getPropertySetInfo();
            uno::Reference<beans::XPropertySetInfo> xInfo = xUnoProp->getPropertySetInfo();

            const OUString sProps[] = {   PROPERTY_NAME
                                          ,PROPERTY_FONTDESCRIPTOR
                                          ,PROPERTY_FONTDESCRIPTORASIAN
                                          ,PROPERTY_FONTDESCRIPTORCOMPLEX
                                          ,PROPERTY_ORIENTATION
                                          ,PROPERTY_BORDER
                                          ,PROPERTY_FORMATSSUPPLIER
                                          ,PROPERTY_BACKGROUNDCOLOR
            };
            for(const auto & sProp : sProps)
            {
                if ( xInfo->hasPropertyByName(sProp) && xShapeInfo->hasPropertyByName(sProp) )
                    xUnoProp->setPropertyValue(sProp,xShapeProp->getPropertyValue(sProp));
            }

            if ( xInfo->hasPropertyByName(PROPERTY_BORDER) && xShapeInfo->hasPropertyByName(PROPERTY_CONTROLBORDER) )
                xUnoProp->setPropertyValue(PROPERTY_BORDER,xShapeProp->getPropertyValue(PROPERTY_CONTROLBORDER));


            if ( xInfo->hasPropertyByName(PROPERTY_DATAFIELD) && !_sFunction.isEmpty() )
            {
                ReportFormula aFunctionFormula( ReportFormula::Expression, _sFunction );
                xUnoProp->setPropertyValue( PROPERTY_DATAFIELD, uno::Any( aFunctionFormula.getCompleteFormula() ) );
            }

            sal_Int32 nFormatKey = aMap.getUnpackedValueOrDefault(PROPERTY_FORMATKEY,sal_Int32(0));
            if ( nFormatKey && xInfo->hasPropertyByName(PROPERTY_FORMATKEY) )
                xUnoProp->setPropertyValue( PROPERTY_FORMATKEY, uno::Any( nFormatKey ) );

            OUString sUrl = aMap.getUnpackedValueOrDefault(PROPERTY_IMAGEURL,OUString());
            if ( !sUrl.isEmpty() && xInfo->hasPropertyByName(PROPERTY_IMAGEURL) )
                xUnoProp->setPropertyValue( PROPERTY_IMAGEURL, uno::Any( sUrl ) );

            pObj->CreateMediator(true);

            if ( _nObjectId == SdrObjKind::ReportDesignFixedText ) // special case for fixed text
                xUnoProp->setPropertyValue(PROPERTY_LABEL,uno::Any(OUnoObject::GetDefaultName(pObj)));
            else if ( _nObjectId == SdrObjKind::ReportDesignVerticalFixedLine )
            {
                awt::Size aOlSize = xShapeProp->getSize();
                xShapeProp->setSize(awt::Size(aOlSize.Height,aOlSize.Width)); // switch height and width
            }
        }
    }

    const sal_Int32 nShapeWidth = aMap.getUnpackedValueOrDefault(PROPERTY_WIDTH,xShapeProp->getWidth());
    if ( nShapeWidth != xShapeProp->getWidth() )
        xShapeProp->setWidth( nShapeWidth );

    const bool bChangedPos = (aPos.X + nShapeWidth) > nPaperWidth;
    if ( bChangedPos )
        aPos.X = nPaperWidth - nShapeWidth;
    xShapeProp->setPosition(aPos);

    correctOverlapping(pNewControl.get(),pSectionWindow->getReportSection());
}

void OReportController::createDateTime(const Sequence< PropertyValue >& _aArgs)
{
    getDesignView()->unmarkAllObjects();

    const OUString sUndoAction(RptResId(RID_STR_UNDO_INSERT_CONTROL));
    UndoContext aUndoContext( getUndoManager(), sUndoAction );

    SequenceAsHashMap aMap(_aArgs);
    aMap.createItemIfMissing(PROPERTY_FORMATKEY,aMap.getUnpackedValueOrDefault(PROPERTY_FORMATKEYDATE,sal_Int32(0)));

    uno::Reference< report::XSection> xSection = aMap.getUnpackedValueOrDefault(PROPERTY_SECTION,uno::Reference< report::XSection>());
    OUString sFunction;

    bool bDate = aMap.getUnpackedValueOrDefault(PROPERTY_DATE_STATE, false);
    if ( bDate )
    {
        sFunction = "TODAY()";
        createControl(aMap.getAsConstPropertyValueList(),xSection,sFunction);
    }
    bool bTime = aMap.getUnpackedValueOrDefault(PROPERTY_TIME_STATE, false);
    if ( bTime )
    {
        sFunction = "TIMEVALUE(NOW())";
        aMap[PROPERTY_FORMATKEY] <<= aMap.getUnpackedValueOrDefault(PROPERTY_FORMATKEYTIME,sal_Int32(0));
        createControl(aMap.getAsConstPropertyValueList(),xSection,sFunction);
    }
}

void OReportController::createPageNumber(const Sequence< PropertyValue >& _aArgs)
{
    getDesignView()->unmarkAllObjects();

    const OUString sUndoAction(RptResId(RID_STR_UNDO_INSERT_CONTROL));
    UndoContext aUndoContext( getUndoManager(), sUndoAction );

    if ( !m_xReportDefinition->getPageHeaderOn() )
    {
        uno::Sequence< beans::PropertyValue > aArgs;
        executeChecked(SID_PAGEHEADERFOOTER,aArgs);
    }

    SequenceAsHashMap aMap(_aArgs);
    bool bStateOfPage = aMap.getUnpackedValueOrDefault(PROPERTY_STATE, false);

    OUString sFunction( RptResId(STR_RPT_PN_PAGE) );
    sFunction = sFunction.replaceFirst("#PAGENUMBER#", "PageNumber()");

    if ( bStateOfPage )
    {
        sFunction += RptResId(STR_RPT_PN_PAGE_OF);
        sFunction = sFunction.replaceFirst("#PAGECOUNT#", "PageCount()");
    }

    bool bInPageHeader = aMap.getUnpackedValueOrDefault(PROPERTY_PAGEHEADERON, true);
    createControl(_aArgs,bInPageHeader ? m_xReportDefinition->getPageHeader() : m_xReportDefinition->getPageFooter(),sFunction);
}


void OReportController::addPairControls(const Sequence< PropertyValue >& aArgs)
{
    getDesignView()->unmarkAllObjects();

    // the FormatKey determines which field is required
    OSectionWindow* pSectionWindow[2];
    pSectionWindow[0] = getDesignView()->getMarkedSection();

    if ( !pSectionWindow[0] )
    {
        select(uno::Any(m_xReportDefinition->getDetail()));
        pSectionWindow[0] = getDesignView()->getMarkedSection();
        if ( !pSectionWindow[0] )
            return;
    }

    uno::Reference<report::XSection> xCurrentSection = getDesignView()->getCurrentSection();
    UndoContext aUndoContext(getUndoManager(), RptResId(RID_STR_UNDO_INSERT_CONTROL));

    try
    {
        bool bHandleOnlyOne = false;
        for(const PropertyValue& rArg : aArgs)
        {
            if (bHandleOnlyOne)
                break;
            Sequence< PropertyValue > aValue;
            if ( !(rArg.Value >>= aValue) )
            {   // the sequence has only one element which already contains the descriptor
                bHandleOnlyOne = true;
                aValue = aArgs;
            }
            svx::ODataAccessDescriptor aDescriptor(aValue);
            SequenceAsHashMap aMap(aValue);
            uno::Reference<report::XSection> xSection = aMap.getUnpackedValueOrDefault(u"Section"_ustr,xCurrentSection);
            uno::Reference<report::XReportDefinition> xReportDefinition = xSection->getReportDefinition();

            getDesignView()->setMarked(xSection, true);
            pSectionWindow[0] = getDesignView()->getMarkedSection();

            sal_Int32 nLeftMargin = getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_LEFTMARGIN);
            awt::Point aPos = aMap.getUnpackedValueOrDefault(PROPERTY_POSITION,awt::Point(nLeftMargin,0));
            if ( aPos.X < nLeftMargin )
                aPos.X = nLeftMargin;

            // LLA: new feature, add the Label in dependency of the given DND_ACTION one section up, normal or one section down
            sal_Int8 nDNDAction = aMap.getUnpackedValueOrDefault(u"DNDAction"_ustr, sal_Int8(0));
            pSectionWindow[1] = pSectionWindow[0];
            bool bLabelAboveTextField = nDNDAction == DND_ACTION_COPY;
            if ( bLabelAboveTextField || nDNDAction == DND_ACTION_LINK )
            {
                // Add the Label one Section up
                pSectionWindow[1] = getDesignView()->getMarkedSection(
                    bLabelAboveTextField ? NearSectionAccess::PREVIOUS : NearSectionAccess::POST);
                if (!pSectionWindow[1])
                {
                    // maybe out of bounds
                    pSectionWindow[1] = pSectionWindow[0];
                }
            }
            // clear all selections
            getDesignView()->unmarkAllObjects();

            uno::Reference< beans::XPropertySet > xField( aDescriptor[ svx::DataAccessDescriptorProperty::ColumnObject ], uno::UNO_QUERY );
            uno::Reference< lang::XComponent > xHoldAlive;
            if ( !xField.is() )
            {
                OUString sCommand;
                OUString sColumnName;
                sal_Int32 nCommandType( -1 );
                OSL_VERIFY( aDescriptor[ svx::DataAccessDescriptorProperty::Command ] >>= sCommand );
                OSL_VERIFY( aDescriptor[ svx::DataAccessDescriptorProperty::ColumnName ] >>= sColumnName );
                OSL_VERIFY( aDescriptor[ svx::DataAccessDescriptorProperty::CommandType ] >>= nCommandType );

                uno::Reference< container::XNameAccess > xColumns;
                uno::Reference< sdbc::XConnection > xConnection( getConnection() );
                if ( !sCommand.isEmpty() && nCommandType != -1 && !sColumnName.isEmpty() && xConnection.is() )
                {
                    if ( xReportDefinition->getCommand().isEmpty() )
                    {
                        xReportDefinition->setCommand(sCommand);
                        xReportDefinition->setCommandType(nCommandType);
                    }

                    xColumns = dbtools::getFieldsByCommandDescriptor(xConnection,nCommandType,sCommand,xHoldAlive);
                    if ( xColumns.is() && xColumns->hasByName(sColumnName) )
                        xField.set( xColumns->getByName( sColumnName ), uno::UNO_QUERY );
                }

                if ( !xField.is() )
                {
                #if OSL_DEBUG_LEVEL > 0
                    try
                    {
                        uno::Reference< beans::XPropertySet > xRowSetProps( getRowSet(), UNO_QUERY_THROW );
                        OUString sRowSetCommand;
                        sal_Int32 nRowSetCommandType( -1 );
                        OSL_VERIFY( xRowSetProps->getPropertyValue( PROPERTY_COMMAND ) >>= sRowSetCommand );
                        OSL_VERIFY( xRowSetProps->getPropertyValue( PROPERTY_COMMANDTYPE ) >>= nRowSetCommandType );
                        OSL_ENSURE( ( sRowSetCommand == sCommand ) && ( nCommandType == nRowSetCommandType ),
                            "OReportController::addPairControls: this only works for a data source which equals our current settings!" );
                        // if this asserts, then either our row set and our report definition are not in sync, or somebody
                        // requested the creation of a control/pair for another data source than what our report
                        // definition is bound to - which is not supported for the parameters case, since we
                        // can retrieve parameters from the RowSet only.
                    }
                    catch(const Exception&)
                    {
                        DBG_UNHANDLED_EXCEPTION("reportdesign");
                    }
                #endif

                    // no column name - perhaps a parameter name?
                    uno::Reference< sdb::XParametersSupplier > xSuppParam( getRowSet(), uno::UNO_QUERY_THROW );
                    uno::Reference< container::XIndexAccess > xParams( xSuppParam->getParameters(), uno::UNO_SET_THROW );
                    sal_Int32 nParamCount( xParams->getCount() );
                    for ( sal_Int32 i=0; i<nParamCount; ++i)
                    {
                        uno::Reference< beans::XPropertySet > xParamCol( xParams->getByIndex(i), uno::UNO_QUERY_THROW );
                        OUString sParamName;
                        OSL_VERIFY( xParamCol->getPropertyValue(u"Name"_ustr) >>= sParamName );
                        if ( sParamName == sColumnName )
                        {
                            xField = std::move(xParamCol);
                            break;
                        }
                    }
                }
            }
            if ( !xField.is() )
                continue;

            SdrObjKind nOBJID = SdrObjKind::NONE;
            sal_Int32 nDataType = sdbc::DataType::BINARY;
            xField->getPropertyValue(PROPERTY_TYPE) >>= nDataType;
            switch ( nDataType )
            {
                case sdbc::DataType::BINARY:
                case sdbc::DataType::VARBINARY:
                case sdbc::DataType::LONGVARBINARY:
                    nOBJID = SdrObjKind::ReportDesignImageControl;
                    break;
                default:
                    nOBJID = SdrObjKind::ReportDesignFormattedField;
                    break;
            }

            if ( nOBJID == SdrObjKind::NONE )
                continue;

            Reference< util::XNumberFormatsSupplier >  xSupplier = getReportNumberFormatter()->getNumberFormatsSupplier();
            if ( !xSupplier.is() )
                continue;

            Reference< XNumberFormats >  xNumberFormats(xSupplier->getNumberFormats());
            rtl::Reference<SdrUnoObj> pControl[2];
            const sal_Int32 nRightMargin = getStyleProperty<sal_Int32>(m_xReportDefinition,PROPERTY_RIGHTMARGIN);
            const sal_Int32 nPaperWidth = getStyleProperty<awt::Size>(m_xReportDefinition,PROPERTY_PAPERSIZE).Width - nRightMargin;
            OSectionView* pSectionViews[2];
            pSectionViews[0] = &pSectionWindow[1]->getReportSection().getSectionView();
            pSectionViews[1] = &pSectionWindow[0]->getReportSection().getSectionView();

            // find this in svx
            FmFormView::createControlLabelPair(
                getDesignView()->GetOutDev(),
                nLeftMargin,
                0,
                xField,
                xNumberFormats,
                nOBJID,
                SdrInventor::ReportDesign,
                SdrObjKind::ReportDesignFixedText,

                // tdf#118963 Need a SdrModel for SdrObject creation. Dereferencing
                // m_aReportModel seems pretty safe, it's done in other places, initialized
                // in impl_initialize and throws a RuntimeException if not existing.
                *m_aReportModel,

                pControl[0],
                pControl[1]);

            if ( pControl[0] && pControl[1] )
            {
                SdrPageView* pPgViews[2];
                pPgViews[0] = pSectionViews[0]->GetSdrPageView();
                pPgViews[1] = pSectionViews[1]->GetSdrPageView();
                if ( pPgViews[0] && pPgViews[1] )
                {
                    OUString sDefaultName;
                    size_t i = 0;
                    rtl::Reference<OUnoObject> pObjs[2];
                    for(i = 0; i < SAL_N_ELEMENTS(pControl); ++i)
                    {
                        pObjs[i] = dynamic_cast<OUnoObject*>(pControl[i].get());
                        assert(pObjs[i]);
                        uno::Reference<beans::XPropertySet> xUnoProp(pObjs[i]->GetUnoControlModel(),uno::UNO_QUERY_THROW);
                        uno::Reference< report::XReportComponent> xShapeProp(pObjs[i]->getUnoShape(),uno::UNO_QUERY_THROW);
                        xUnoProp->setPropertyValue(PROPERTY_NAME,xShapeProp->getPropertyValue(PROPERTY_NAME));

                        uno::Reference<beans::XPropertySetInfo> xShapeInfo = xShapeProp->getPropertySetInfo();
                        uno::Reference<beans::XPropertySetInfo> xInfo = xUnoProp->getPropertySetInfo();
                        const OUString sProps[] = {   PROPERTY_FONTDESCRIPTOR
                                                            ,PROPERTY_FONTDESCRIPTORASIAN
                                                            ,PROPERTY_FONTDESCRIPTORCOMPLEX
                                                            ,PROPERTY_BORDER
                                                            ,PROPERTY_BACKGROUNDCOLOR
                        };
                        for(const auto & sProp : sProps)
                        {
                            if ( xInfo->hasPropertyByName(sProp) && xShapeInfo->hasPropertyByName(sProp) )
                                xUnoProp->setPropertyValue(sProp,xShapeProp->getPropertyValue(sProp));
                        }
                        if ( xInfo->hasPropertyByName(PROPERTY_DATAFIELD) )
                        {
                            OUString sName;
                            xUnoProp->getPropertyValue(PROPERTY_DATAFIELD) >>= sName;
                            sDefaultName = sName;
                            xUnoProp->setPropertyValue(PROPERTY_NAME,uno::Any(sDefaultName));

                            ReportFormula aFormula( ReportFormula::Field, sName );
                            xUnoProp->setPropertyValue( PROPERTY_DATAFIELD, uno::Any( aFormula.getCompleteFormula() ) );
                        }

                        if ( xInfo->hasPropertyByName(PROPERTY_BORDER) && xShapeInfo->hasPropertyByName(PROPERTY_CONTROLBORDER) )
                            xUnoProp->setPropertyValue(PROPERTY_BORDER,xShapeProp->getPropertyValue(PROPERTY_CONTROLBORDER));

                        pObjs[i]->CreateMediator(true);

                        const sal_Int32 nShapeWidth = xShapeProp->getWidth();
                        const bool bChangedPos = (aPos.X + nShapeWidth) > nPaperWidth;
                        if ( bChangedPos )
                            aPos.X = nPaperWidth - nShapeWidth;
                        xShapeProp->setPosition(aPos);
                        if ( bChangedPos )
                            aPos.Y += xShapeProp->getHeight();
                        aPos.X += nShapeWidth;
                    }
                    OUString sLabel;
                    if ( xField->getPropertySetInfo()->hasPropertyByName(PROPERTY_LABEL) )
                        xField->getPropertyValue(PROPERTY_LABEL) >>= sLabel;

                    if (pSectionViews[0] != pSectionViews[1] &&
                        nOBJID == SdrObjKind::ReportDesignFormattedField) // we want this nice feature only at FORMATTEDFIELD
                    {
                        uno::Reference< report::XReportComponent> xShapePropLabel(pObjs[0]->getUnoShape(),uno::UNO_QUERY_THROW);
                        uno::Reference< report::XReportComponent> xShapePropTextField(pObjs[1]->getUnoShape(),uno::UNO_QUERY_THROW);
                        if ( !sLabel.isEmpty() )
                            xShapePropTextField->setName(sLabel);
                        awt::Point aPosLabel = xShapePropLabel->getPosition();
                        awt::Point aPosTextField = xShapePropTextField->getPosition();
                        aPosTextField.X = aPosLabel.X;
                        xShapePropTextField->setPosition(aPosTextField);
                        if (bLabelAboveTextField)
                        {
                            // move the label down near the splitter
                            const uno::Reference<report::XSection> xLabelSection = pSectionWindow[1]->getReportSection().getSection();
                            aPosLabel.Y = xLabelSection->getHeight() - xShapePropLabel->getHeight();
                        }
                        else
                        {
                            // move the label up to the splitter
                            aPosLabel.Y = 0;
                        }
                        xShapePropLabel->setPosition(aPosLabel);
                    }
                    rtl::Reference<OUnoObject> pObj = dynamic_cast<OUnoObject*>(pControl[0].get());
                    assert(pObj);
                    uno::Reference< report::XFixedText> xShapeProp(pObj->getUnoShape(),uno::UNO_QUERY_THROW);
                    xShapeProp->setName(xShapeProp->getName() + sDefaultName );

                    for(i = 0; i < SAL_N_ELEMENTS(pControl); ++i) // insert controls
                    {
                        correctOverlapping(pControl[i].get(), pSectionWindow[1-i]->getReportSection());
                    }

                    if (!bLabelAboveTextField )
                    {
                        if ( pSectionViews[0] == pSectionViews[1] )
                        {
                            tools::Rectangle aLabel = getRectangleFromControl(pControl[0].get());
                            tools::Rectangle aTextfield = getRectangleFromControl(pControl[1].get());

                            // create a Union of the given Label and Textfield
                            tools::Rectangle aLabelAndTextfield( aLabel );
                            aLabelAndTextfield.Union(aTextfield);

                            // check if there exists other fields and if yes, move down
                            bool bOverlapping = true;
                            bool bHasToMove = false;
                            while ( bOverlapping )
                            {
                                const SdrObject* pOverlappedObj = isOver(aLabelAndTextfield, *pSectionWindow[0]->getReportSection().getPage(), *pSectionViews[0], true, pControl, 2);
                                bOverlapping = pOverlappedObj != nullptr;
                                if ( bOverlapping )
                                {
                                    const tools::Rectangle& aLogicRect = pOverlappedObj->GetLogicRect();
                                    aLabelAndTextfield.Move(0,aLogicRect.Top() + aLogicRect.getOpenHeight() - aLabelAndTextfield.Top());
                                    bHasToMove = true;
                                }
                            }

                            if (bHasToMove)
                            {
                                // There was a move down, we need to move the Label and the Textfield down
                                aLabel.Move(0, aLabelAndTextfield.Top() - aLabel.Top());
                                aTextfield.Move(0, aLabelAndTextfield.Top() - aTextfield.Top());

                                uno::Reference< report::XReportComponent> xLabel(pControl[0]->getUnoShape(),uno::UNO_QUERY_THROW);
                                xLabel->setPositionY(aLabel.Top());

                                uno::Reference< report::XReportComponent> xTextfield(pControl[1]->getUnoShape(),uno::UNO_QUERY_THROW);
                                xTextfield->setPositionY(aTextfield.Top());
                            }
                        }
                    }
                }
            }
        }
    }
    catch(const Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}


OSectionView* OReportController::getCurrentSectionView() const
{
    OSectionView* pSectionView = nullptr;
    OSectionWindow* pSectionWindow = getDesignView()->getMarkedSection();
    if ( pSectionWindow )
        pSectionView = &pSectionWindow->getReportSection().getSectionView();
    return pSectionView;
}

void OReportController::changeZOrder(sal_Int32 _nId)
{
    OSectionView* pSectionView = getCurrentSectionView();
    if ( !pSectionView )
        return;

    switch(_nId)
    {
        case SID_FRAME_TO_BOTTOM:
            pSectionView->PutMarkedToBtm();
            break;
        case SID_FRAME_TO_TOP:
            pSectionView->PutMarkedToTop();
            break;
        case SID_FRAME_DOWN:
            pSectionView->MovMarkedToBtm();
            break;
        case SID_FRAME_UP:
            pSectionView->MovMarkedToTop();
            break;

        case SID_OBJECT_HEAVEN:
            pSectionView->SetMarkedToLayer( RPT_LAYER_FRONT );
            break;
        case SID_OBJECT_HELL:
            pSectionView->SetMarkedToLayer( RPT_LAYER_BACK );
            break;
    }
}

void OReportController::listen(const bool _bAdd)
{
    const OUString aProps [] = {    PROPERTY_REPORTHEADERON,PROPERTY_REPORTFOOTERON
                                            ,PROPERTY_PAGEHEADERON,PROPERTY_PAGEFOOTERON
                                            ,PROPERTY_COMMAND, PROPERTY_COMMANDTYPE,PROPERTY_CAPTION
    };

    void (SAL_CALL XPropertySet::*pPropertyListenerAction)( const OUString&, const uno::Reference< XPropertyChangeListener >& ) =
        _bAdd ? &XPropertySet::addPropertyChangeListener : &XPropertySet::removePropertyChangeListener;

    for (const auto & aProp : aProps)
        (m_xReportDefinition.get()->*pPropertyListenerAction)( aProp, static_cast< XPropertyChangeListener* >( this ) );

    OXUndoEnvironment& rUndoEnv = m_aReportModel->GetUndoEnv();
    uno::Reference< XPropertyChangeListener > xUndo = &rUndoEnv;
    const uno::Sequence< beans::Property> aSeq = m_xReportDefinition->getPropertySetInfo()->getProperties();
    const OUString* pPropsBegin = &aProps[0];
    const OUString* pPropsEnd   = pPropsBegin + SAL_N_ELEMENTS(aProps) - 3;
    for(const beans::Property& rProp : aSeq)
    {
        if ( ::std::find(pPropsBegin,pPropsEnd,rProp.Name) == pPropsEnd )
            (m_xReportDefinition.get()->*pPropertyListenerAction)( rProp.Name, xUndo );
    }

    // Add Listeners to UndoEnvironment
    void (OXUndoEnvironment::*pElementUndoFunction)( const uno::Reference< uno::XInterface >& ) =
        _bAdd ? &OXUndoEnvironment::AddElement : &OXUndoEnvironment::RemoveElement;

    (rUndoEnv.*pElementUndoFunction)( m_xReportDefinition->getStyleFamilies() );
    (rUndoEnv.*pElementUndoFunction)( m_xReportDefinition->getFunctions() );

    // Add Listeners to ReportControllerObserver
    OXReportControllerObserver& rObserver = *m_pReportControllerObserver;

    if ( m_xReportDefinition->getPageHeaderOn() && _bAdd )
    {
        getDesignView()->addSection(m_xReportDefinition->getPageHeader(),DBPAGEHEADER);
        rObserver.AddSection(m_xReportDefinition->getPageHeader());
    }
    if ( m_xReportDefinition->getReportHeaderOn() && _bAdd )
    {
        getDesignView()->addSection(m_xReportDefinition->getReportHeader(),DBREPORTHEADER);
        rObserver.AddSection(m_xReportDefinition->getReportHeader());
    }

    uno::Reference< report::XGroups > xGroups = m_xReportDefinition->getGroups();
    const sal_Int32 nCount = xGroups->getCount();
    _bAdd ? xGroups->addContainerListener(&rUndoEnv) : xGroups->removeContainerListener(&rUndoEnv);
    _bAdd ? xGroups->addContainerListener(&rObserver) : xGroups->removeContainerListener(&rObserver);

    for (sal_Int32 i=0;i<nCount ; ++i)
    {
        uno::Reference< report::XGroup > xGroup(xGroups->getByIndex(i),uno::UNO_QUERY);
        (xGroup.get()->*pPropertyListenerAction)( PROPERTY_HEADERON, static_cast< XPropertyChangeListener* >( this ) );
        (xGroup.get()->*pPropertyListenerAction)( PROPERTY_FOOTERON, static_cast< XPropertyChangeListener* >( this ) );

        (rUndoEnv.*pElementUndoFunction)( xGroup );
        (rUndoEnv.*pElementUndoFunction)( xGroup->getFunctions() );
        if ( xGroup->getHeaderOn() && _bAdd )
        {
            getDesignView()->addSection(xGroup->getHeader(),DBGROUPHEADER);
            rObserver.AddSection(xGroup->getHeader());
        }
    }

    if ( _bAdd )
    {
        getDesignView()->addSection(m_xReportDefinition->getDetail(),DBDETAIL);
        rObserver.AddSection(m_xReportDefinition->getDetail());

        for (sal_Int32 i=nCount;i > 0 ; --i)
        {
            uno::Reference< report::XGroup > xGroup(xGroups->getByIndex(i-1),uno::UNO_QUERY);
            if ( xGroup->getFooterOn() )
            {
                getDesignView()->addSection(xGroup->getFooter(),DBGROUPFOOTER);
                rObserver.AddSection(xGroup->getFooter());
            }
        }
        if ( m_xReportDefinition->getReportFooterOn() )
        {
            getDesignView()->addSection(m_xReportDefinition->getReportFooter(),DBREPORTFOOTER);
            rObserver.AddSection(m_xReportDefinition->getReportFooter());
        }
        if ( m_xReportDefinition->getPageFooterOn())
        {
            getDesignView()->addSection(m_xReportDefinition->getPageFooter(),DBPAGEFOOTER);
            rObserver.AddSection(m_xReportDefinition->getPageFooter());
        }

        xGroups->addContainerListener(static_cast<XContainerListener*>(this));
        m_xReportDefinition->addModifyListener(static_cast<XModifyListener*>(this));
    }
    else /* ! _bAdd */
    {
        rObserver.RemoveSection(m_xReportDefinition->getDetail());
        xGroups->removeContainerListener(static_cast<XContainerListener*>(this));
        m_xReportDefinition->removeModifyListener(static_cast<XModifyListener*>(this));
        m_aReportModel->detachController();
    }
}

void OReportController::switchReportSection(const sal_Int16 _nId)
{
    OSL_ENSURE(_nId == SID_REPORTHEADER_WITHOUT_UNDO || _nId == SID_REPORTFOOTER_WITHOUT_UNDO || _nId == SID_REPORTHEADERFOOTER ,"Illegal id given!");

    if ( !m_xReportDefinition.is() )
        return;

    const OXUndoEnvironment::OUndoEnvLock aLock( m_aReportModel->GetUndoEnv() );
    const bool bSwitchOn = !m_xReportDefinition->getReportHeaderOn();

    std::unique_ptr< UndoContext > pUndoContext;
    if ( SID_REPORTHEADERFOOTER == _nId )
    {
        const OUString sUndoAction(RptResId(bSwitchOn ? RID_STR_UNDO_ADD_REPORTHEADERFOOTER : RID_STR_UNDO_REMOVE_REPORTHEADERFOOTER));
        pUndoContext.reset( new UndoContext( getUndoManager(), sUndoAction ) );

        addUndoAction(std::make_unique<OReportSectionUndo>(*m_aReportModel,SID_REPORTHEADER_WITHOUT_UNDO
                                                        ,::std::mem_fn(&OReportHelper::getReportHeader)
                                                        ,m_xReportDefinition
                                                        ,bSwitchOn ? Inserted : Removed
                                                        ));

        addUndoAction(std::make_unique<OReportSectionUndo>(*m_aReportModel,SID_REPORTFOOTER_WITHOUT_UNDO
                                                        ,::std::mem_fn(&OReportHelper::getReportFooter)
                                                        ,m_xReportDefinition
                                                        ,bSwitchOn ? Inserted : Removed
                                                        ));
    }

    switch( _nId )
    {
        case SID_REPORTHEADER_WITHOUT_UNDO:
            m_xReportDefinition->setReportHeaderOn( bSwitchOn );
            break;
        case SID_REPORTFOOTER_WITHOUT_UNDO:
            m_xReportDefinition->setReportFooterOn( !m_xReportDefinition->getReportFooterOn() );
            break;
        case SID_REPORTHEADERFOOTER:
            m_xReportDefinition->setReportHeaderOn( bSwitchOn );
            m_xReportDefinition->setReportFooterOn( bSwitchOn );
            break;
    }

    if ( SID_REPORTHEADERFOOTER == _nId )
        pUndoContext.reset();
    getView()->Resize();
}

void OReportController::switchPageSection(const sal_Int16 _nId)
{
    OSL_ENSURE(_nId == SID_PAGEHEADERFOOTER || _nId == SID_PAGEHEADER_WITHOUT_UNDO || _nId == SID_PAGEFOOTER_WITHOUT_UNDO ,"Illegal id given!");
    if ( !m_xReportDefinition.is() )
        return;

    const OXUndoEnvironment::OUndoEnvLock aLock( m_aReportModel->GetUndoEnv() );
    const bool bSwitchOn = !m_xReportDefinition->getPageHeaderOn();

    std::unique_ptr< UndoContext > pUndoContext;
    if ( SID_PAGEHEADERFOOTER == _nId )
    {
        const OUString sUndoAction(RptResId(bSwitchOn ? RID_STR_UNDO_ADD_REPORTHEADERFOOTER : RID_STR_UNDO_REMOVE_REPORTHEADERFOOTER));
        pUndoContext.reset( new UndoContext( getUndoManager(), sUndoAction ) );

        addUndoAction(std::make_unique<OReportSectionUndo>(*m_aReportModel
                                                        ,SID_PAGEHEADER_WITHOUT_UNDO
                                                        ,::std::mem_fn(&OReportHelper::getPageHeader)
                                                        ,m_xReportDefinition
                                                        ,bSwitchOn ? Inserted : Removed
                                                        ));

        addUndoAction(std::make_unique<OReportSectionUndo>(*m_aReportModel
                                                        ,SID_PAGEFOOTER_WITHOUT_UNDO
                                                        ,::std::mem_fn(&OReportHelper::getPageFooter)
                                                        ,m_xReportDefinition
                                                        ,bSwitchOn ? Inserted : Removed
                                                        ));
    }
    switch( _nId )
    {
        case SID_PAGEHEADER_WITHOUT_UNDO:
            m_xReportDefinition->setPageHeaderOn( bSwitchOn );
            break;
        case SID_PAGEFOOTER_WITHOUT_UNDO:
            m_xReportDefinition->setPageFooterOn( !m_xReportDefinition->getPageFooterOn() );
            break;
        case SID_PAGEHEADERFOOTER:
            m_xReportDefinition->setPageHeaderOn( bSwitchOn );
            m_xReportDefinition->setPageFooterOn( bSwitchOn );
            break;
    }
    if ( SID_PAGEHEADERFOOTER == _nId )
        pUndoContext.reset();
    getView()->Resize();
}

void OReportController::modifyGroup(const bool _bAppend, const Sequence< PropertyValue >& _aArgs)
{
    if ( !m_xReportDefinition.is() )
        return;

    try
    {
        const SequenceAsHashMap aMap( _aArgs );
        uno::Reference< report::XGroup > xGroup = aMap.getUnpackedValueOrDefault( PROPERTY_GROUP, uno::Reference< report::XGroup >() );
        if ( !xGroup.is() )
            return;

        OXUndoEnvironment& rUndoEnv = m_aReportModel->GetUndoEnv();
        uno::Reference< report::XGroups > xGroups = m_xReportDefinition->getGroups();
        if ( _bAppend )
        {
            const sal_Int32 nPos = aMap.getUnpackedValueOrDefault( PROPERTY_POSITIONY, xGroups->getCount() );
            xGroups->insertByIndex( nPos, uno::Any( xGroup ) );
            rUndoEnv.AddElement( xGroup->getFunctions() );
        }

        addUndoAction( std::make_unique<OGroupUndo>(
            *m_aReportModel,
            _bAppend ? RID_STR_UNDO_APPEND_GROUP : RID_STR_UNDO_REMOVE_GROUP,
            _bAppend ? Inserted : Removed,
            xGroup,
            m_xReportDefinition
        ) );

        if ( !_bAppend )
        {
            rUndoEnv.RemoveElement( xGroup->getFunctions() );
            const sal_Int32 nPos = getGroupPosition( xGroup );
            const OXUndoEnvironment::OUndoEnvLock aLock( m_aReportModel->GetUndoEnv() );
            xGroups->removeByIndex( nPos );
        }
    }
    catch(const Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
}


void OReportController::createGroupSection(const bool _bUndo,const bool _bHeader, const Sequence< PropertyValue >& _aArgs)
{
    if ( !m_xReportDefinition.is() )
        return;

    const SequenceAsHashMap aMap(_aArgs);
    const bool bSwitchOn = aMap.getUnpackedValueOrDefault(_bHeader ? PROPERTY_HEADERON : PROPERTY_FOOTERON, false);
    uno::Reference< report::XGroup> xGroup = aMap.getUnpackedValueOrDefault(PROPERTY_GROUP,uno::Reference< report::XGroup>());
    if ( !xGroup.is() )
        return;

    const OXUndoEnvironment::OUndoEnvLock aLock(m_aReportModel->GetUndoEnv());
    if ( _bUndo )
        addUndoAction(std::make_unique<OGroupSectionUndo>(*m_aReportModel
                                                        ,_bHeader ? SID_GROUPHEADER_WITHOUT_UNDO : SID_GROUPFOOTER_WITHOUT_UNDO
                                                        ,_bHeader ? ::std::mem_fn(&OGroupHelper::getHeader) : ::std::mem_fn(&OGroupHelper::getFooter)
                                                        ,xGroup
                                                        ,bSwitchOn ? Inserted : Removed
                                                        , ( _bHeader ?
                                                                (bSwitchOn ? RID_STR_UNDO_ADD_GROUP_HEADER : RID_STR_UNDO_REMOVE_GROUP_HEADER)
                                                               :(bSwitchOn ? RID_STR_UNDO_ADD_GROUP_FOOTER : RID_STR_UNDO_REMOVE_GROUP_FOOTER)
                                                          )
                                                        ));

    if ( _bHeader )
        xGroup->setHeaderOn( bSwitchOn );
    else
        xGroup->setFooterOn( bSwitchOn );
}

void OReportController::collapseSection(const bool _bCollapse)
{
    OSectionWindow *pSection = getDesignView()->getMarkedSection();
    if ( pSection )
    {
        pSection->setCollapsed(_bCollapse);
    }
}

void OReportController::markSection(const bool _bNext)
{
    OSectionWindow *pSection = getDesignView()->getMarkedSection();
    if ( pSection )
    {
        OSectionWindow* pPrevSection = getDesignView()->getMarkedSection(
            _bNext ? NearSectionAccess::POST : NearSectionAccess::PREVIOUS);
        if ( pPrevSection != pSection && pPrevSection )
            select(uno::Any(pPrevSection->getReportSection().getSection()));
        else
            select(uno::Any(m_xReportDefinition));
    }
    else
    {
        getDesignView()->markSection(_bNext ? 0 : getDesignView()->getSectionCount() - 1);
        pSection = getDesignView()->getMarkedSection();
        if ( pSection )
            select(uno::Any(pSection->getReportSection().getSection()));
    }
}

void OReportController::createDefaultControl(const uno::Sequence< beans::PropertyValue>& _aArgs)
{
    uno::Reference< report::XSection > xSection = getDesignView()->getCurrentSection();
    if ( !xSection.is() )
        xSection = m_xReportDefinition->getDetail();

    if ( !xSection.is() )
        return;

    const beans::PropertyValue* pIter = _aArgs.getConstArray();
    const beans::PropertyValue* pEnd  = pIter + _aArgs.getLength();
    const beans::PropertyValue* pKeyModifier = ::std::find_if(pIter, pEnd,
        [] (const beans::PropertyValue& x) -> bool {
            return x.Name == "KeyModifier";
        });
    sal_Int16 nKeyModifier = 0;
    if ( pKeyModifier == pEnd || ((pKeyModifier->Value >>= nKeyModifier) && nKeyModifier == KEY_MOD1) )
    {
        Sequence< PropertyValue > aCreateArgs;
        getDesignView()->unmarkAllObjects();
        createControl(aCreateArgs,xSection,OUString(),getDesignView()->GetInsertObj());
    }
}


void OReportController::checkChartEnabled()
{
    if ( m_bChartEnabledAsked )
        return;

    m_bChartEnabledAsked = true;

    try
    {
        ::utl::OConfigurationTreeRoot aConfiguration(
            ::utl::OConfigurationTreeRoot::createWithComponentContext( m_xContext, u"/org.openoffice.Office.ReportDesign"_ustr ) );

        bool bChartEnabled = false;
        static constexpr OUString sPropertyName( u"UserData/Chart"_ustr );
        if ( aConfiguration.hasByHierarchicalName(sPropertyName) )
            aConfiguration.getNodeValue( sPropertyName ) >>= bChartEnabled;
        m_bChartEnabled = bChartEnabled;
    }
    catch(const Exception&)
    {
    }
}


// css.frame.XTitle
OUString SAL_CALL OReportController::getTitle()
{
    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );

    uno::Reference< frame::XTitle> xTitle(m_xReportDefinition,uno::UNO_QUERY_THROW);

    return xTitle->getTitle ();
}

void OReportController::getPropertyDefaultByHandle( sal_Int32 /*_nHandle*/, Any& _rDefault ) const
{
    _rDefault <<= sal_Int16(100);
}

// comphelper::OPropertyArrayUsageHelper
::cppu::IPropertyArrayHelper* OReportController::createArrayHelper( ) const
{
    Sequence< Property > aProps;
    describeProperties(aProps);
    return new ::cppu::OPropertyArrayHelper(aProps);
}


// cppu::OPropertySetHelper
::cppu::IPropertyArrayHelper& SAL_CALL OReportController::getInfoHelper()
{
    return *::comphelper::OPropertyArrayUsageHelper<OReportController_BASE>::getArrayHelper();
}

void SAL_CALL OReportController::setFastPropertyValue_NoBroadcast(sal_Int32 _nHandle,const Any& _aValue)
{
    if ( _nHandle == PROPERTY_ID_ZOOMVALUE )
    {
        _aValue >>= m_nZoomValue;
        impl_zoom_nothrow();
    }
}
void SAL_CALL OReportController::setMode( const OUString& aMode )
{
    ::osl::MutexGuard aGuard( getMutex() );
    m_sMode = aMode;
}
OUString SAL_CALL OReportController::getMode(  )
{
    ::osl::MutexGuard aGuard( getMutex() );
    return m_sMode;
}
css::uno::Sequence< OUString > SAL_CALL OReportController::getSupportedModes(  )
{
    return uno::Sequence< OUString> { u"remote"_ustr, u"normal"_ustr };
}
sal_Bool SAL_CALL OReportController::supportsMode( const OUString& aMode )
{
    uno::Sequence< OUString> aModes = getSupportedModes();
    return comphelper::findValue(aModes, aMode) != -1;
}

bool OReportController::isUiVisible() const
{
    return m_sMode != "remote";
}

void OReportController::impl_fillState_nothrow(const OUString& _sProperty,dbaui::FeatureState& _rState) const
{
    _rState.bEnabled = isEditable();
    if ( !_rState.bEnabled )
        return;

    ::std::vector< uno::Reference< uno::XInterface > > aSelection;
    getDesignView()->fillControlModelSelection(aSelection);
    _rState.bEnabled = !aSelection.empty();
    if ( !_rState.bEnabled )
        return;

    uno::Any aTemp;
    ::std::vector< uno::Reference< uno::XInterface > >::const_iterator aIter = aSelection.begin();
    for(; aIter != aSelection.end() && _rState.bEnabled ;++aIter)
    {
        uno::Reference< beans::XPropertySet> xProp(*aIter,uno::UNO_QUERY);
        try
        {
            uno::Any aTemp2 = xProp->getPropertyValue(_sProperty);
            if ( aIter == aSelection.begin() )
            {
                aTemp = std::move(aTemp2);
            }
            else if ( aTemp != aTemp2 )
                break;
        }
        catch(const beans::UnknownPropertyException&)
        {
            _rState.bEnabled = false;
        }
    }
    if ( aIter == aSelection.end() )
        _rState.aValue = std::move(aTemp);
}

void OReportController::impl_zoom_nothrow()
{
    Fraction aZoom(m_nZoomValue,100);
    setZoomFactor( aZoom, *getDesignView() );
    getDesignView()->zoom(aZoom);
    InvalidateFeature(SID_ATTR_ZOOM,Reference< XStatusListener >(), true);
    InvalidateFeature(SID_ATTR_ZOOMSLIDER,Reference< XStatusListener >(), true);
}

bool OReportController::isFormatCommandEnabled(sal_uInt16 _nCommand,const uno::Reference< report::XReportControlFormat>& _xReportControlFormat)
{
    bool bRet = false;
    if ( _xReportControlFormat.is() && !uno::Reference< report::XFixedLine>(_xReportControlFormat,uno::UNO_QUERY).is() ) // this command is really often called so we need a short cut here
    {
        try
        {
            const awt::FontDescriptor aFontDescriptor = _xReportControlFormat->getFontDescriptor();

            switch(_nCommand)
            {
                case SID_ATTR_CHAR_WEIGHT:
                    bRet = awt::FontWeight::BOLD == aFontDescriptor.Weight;
                    break;
                case SID_ATTR_CHAR_POSTURE:
                    bRet = awt::FontSlant_ITALIC == aFontDescriptor.Slant;
                    break;
                case SID_ATTR_CHAR_UNDERLINE:
                    bRet = awt::FontUnderline::SINGLE == aFontDescriptor.Underline;
                    break;
                default:
                    ;
            }
        }
        catch(const uno::Exception&)
        {
        }
    }
    return bRet;
}

bool OReportController::impl_setPropertyAtControls_throw(TranslateId pUndoResId,const OUString& _sProperty,const uno::Any& _aValue,const Sequence< PropertyValue >& _aArgs)
{
    ::std::vector< uno::Reference< uno::XInterface > > aSelection;
    uno::Reference< awt::XWindow> xWindow;
    lcl_getReportControlFormat( _aArgs, getDesignView(), xWindow, aSelection );

    const OUString sUndoAction = RptResId( pUndoResId );
    UndoContext aUndoContext( getUndoManager(), sUndoAction );

    for (const auto& rxInterface : aSelection)
    {
        const uno::Reference< beans::XPropertySet > xControlModel(rxInterface,uno::UNO_QUERY);
        if ( xControlModel.is() )
            // tdf#117795: some elements may have not some property
            // eg class "OFixedLine" doesn't have property "CharFontName"
            // so in this case, instead of crashing when selecting all and changing font
            // just display a warning
            try
            {
                xControlModel->setPropertyValue(_sProperty,_aValue);
            }
            catch(const UnknownPropertyException&)
            {
                TOOLS_WARN_EXCEPTION("reportdesign", "");
            }
    }

    return !aSelection.empty();
}

void OReportController::impl_fillCustomShapeState_nothrow(std::u16string_view sCustomShapeType,
                                                          dbaui::FeatureState& _rState) const
{
    _rState.bEnabled = isEditable();
    _rState.bChecked = getDesignView()->GetInsertObj() == SdrObjKind::CustomShape
                       && getDesignView()->GetInsertObjString() == sCustomShapeType;
}


OSectionWindow* OReportController::getSectionWindow(const css::uno::Reference< css::report::XSection>& _xSection) const
{
    if ( getDesignView() )
    {
        return getDesignView()->getSectionWindow(_xSection);
    }

    // throw NullPointerException?
    return nullptr;
}

static ItemInfoPackage& getItemInfoPackageOpenZoomDlg()
{
    class ItemInfoPackageOpenZoomDlg : public ItemInfoPackage
    {
        typedef std::array<ItemInfoStatic, 1> ItemInfoArrayOpenZoomDlg;
        ItemInfoArrayOpenZoomDlg maItemInfos {{
            // m_nWhich, m_pItem, m_nSlotID, m_nItemInfoFlags
            { SID_ATTR_ZOOM, new SvxZoomItem, 0, SFX_ITEMINFOFLAG_NONE }
        }};

        virtual const ItemInfoStatic& getItemInfoStatic(size_t nIndex) const override { return maItemInfos[nIndex]; }

    public:
        virtual size_t size() const override { return maItemInfos.size(); }
        virtual const ItemInfo& getItemInfo(size_t nIndex, SfxItemPool& /*rPool*/) override { return maItemInfos[nIndex]; }
    };

    static std::unique_ptr<ItemInfoPackageOpenZoomDlg> g_aItemInfoPackageOpenZoomDlg;
    if (!g_aItemInfoPackageOpenZoomDlg)
        g_aItemInfoPackageOpenZoomDlg.reset(new ItemInfoPackageOpenZoomDlg);
    return *g_aItemInfoPackageOpenZoomDlg;
}

void OReportController::openZoomDialog()
{
    SvxAbstractDialogFactory* pFact = SvxAbstractDialogFactory::Create();
    rtl::Reference<SfxItemPool> pPool(new SfxItemPool(u"ZoomProperties"_ustr));
    pPool->registerItemInfoPackage(getItemInfoPackageOpenZoomDlg());
    pPool->SetDefaultMetric( MapUnit::Map100thMM );    // ripped, don't understand why

    try
    {
        SfxItemSetFixed<SID_ATTR_ZOOM,SID_ATTR_ZOOM> aDescriptor(*pPool);
        // fill it
        SvxZoomItem aZoomItem( m_eZoomType, m_nZoomValue, SID_ATTR_ZOOM );
        aZoomItem.SetValueSet(SvxZoomEnableFlags::N100|SvxZoomEnableFlags::WHOLEPAGE|SvxZoomEnableFlags::PAGEWIDTH);
        aDescriptor.Put(aZoomItem);

        ScopedVclPtr<AbstractSvxZoomDialog> pDlg(pFact->CreateSvxZoomDialog(nullptr, aDescriptor));
        pDlg->SetLimits( 20, 400 );
        bool bCancel = ( RET_CANCEL == pDlg->Execute() );

        if ( !bCancel )
        {
            const SvxZoomItem&  rZoomItem = pDlg->GetOutputItemSet()->Get( SID_ATTR_ZOOM );
            m_eZoomType = rZoomItem.GetType();
            m_nZoomValue = rZoomItem.GetValue();
            if ( m_eZoomType != SvxZoomType::PERCENT )
                m_nZoomValue = getDesignView()->getZoomFactor( m_eZoomType );

            impl_zoom_nothrow();
        }
    }
    catch(const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("reportdesign");
    }
    pPool.clear();
}


// XVisualObject
void SAL_CALL OReportController::setVisualAreaSize( ::sal_Int64 _nAspect, const awt::Size& _aSize )
{
    ::osl::MutexGuard aGuard( getMutex() );
    bool bChanged =
            (m_aVisualAreaSize.Width != _aSize.Width ||
             m_aVisualAreaSize.Height != _aSize.Height);
    m_aVisualAreaSize = _aSize;
    if( bChanged )
            setModified( true );
    m_nAspect = _nAspect;
}

awt::Size SAL_CALL OReportController::getVisualAreaSize( ::sal_Int64 /*nAspect*/ )
{
    ::osl::MutexGuard aGuard( getMutex() );
    return m_aVisualAreaSize;
}

embed::VisualRepresentation SAL_CALL OReportController::getPreferredVisualRepresentation( ::sal_Int64 _nAspect )
{
    SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard aGuard( getMutex() );
    embed::VisualRepresentation aResult;
    if ( !m_bInGeneratePreview )
    {
        m_bInGeneratePreview = true;
        try
        {
            if ( !m_xReportEngine.is() )
                m_xReportEngine.set( report::ReportEngine::create(m_xContext) );
            const sal_Int32 nOldMaxRows = m_xReportEngine->getMaxRows();
            m_xReportEngine->setMaxRows(MAX_ROWS_FOR_PREVIEW);
            m_xReportEngine->setReportDefinition(m_xReportDefinition);
            m_xReportEngine->setActiveConnection(getConnection());
            try
            {
                Reference<embed::XVisualObject> xTransfer(m_xReportEngine->createDocumentModel(),UNO_QUERY);
                if ( xTransfer.is() )
                {
                    xTransfer->setVisualAreaSize(m_nAspect,m_aVisualAreaSize);
                    aResult = xTransfer->getPreferredVisualRepresentation( _nAspect );
                }
            }
            catch(const uno::Exception&)
            {
            }
            m_xReportEngine->setMaxRows(nOldMaxRows);
        }
        catch(const uno::Exception&)
        {
        }
        m_bInGeneratePreview = false;
    }
    return aResult;
}

::sal_Int32 SAL_CALL OReportController::getMapUnit( ::sal_Int64 /*nAspect*/ )
{
    return embed::EmbedMapUnits::ONE_100TH_MM;
}

uno::Reference< container::XNameAccess > const & OReportController::getColumns() const
{
    if ( !m_xColumns.is() && m_xReportDefinition.is() && !m_xReportDefinition->getCommand().isEmpty() )
    {
        m_xColumns = dbtools::getFieldsByCommandDescriptor(getConnection(),m_xReportDefinition->getCommandType(),m_xReportDefinition->getCommand(),m_xHoldAlive);
    }
    return m_xColumns;
}

OUString OReportController::getColumnLabel_throw(const OUString& i_sColumnName) const
{
    OUString sLabel;
    uno::Reference< container::XNameAccess > xColumns = getColumns();
    if ( xColumns.is() && xColumns->hasByName(i_sColumnName) )
    {
        uno::Reference< beans::XPropertySet> xColumn(xColumns->getByName(i_sColumnName),uno::UNO_QUERY_THROW);
        if ( xColumn->getPropertySetInfo()->hasPropertyByName(PROPERTY_LABEL) )
            xColumn->getPropertyValue(PROPERTY_LABEL) >>= sLabel;
    }
    return sLabel;
}


SfxUndoManager& OReportController::getUndoManager() const
{
    DBG_TESTSOLARMUTEX();
        // this is expected to be called during UI actions, so the SM is assumed to be locked

    std::shared_ptr< OReportModel > pReportModel( getSdrModel() );
    ENSURE_OR_THROW( !!pReportModel, "no access to our model" );

    SfxUndoManager* pUndoManager( pReportModel->GetSdrUndoManager() );
    ENSURE_OR_THROW( pUndoManager != nullptr, "no access to our model's UndoManager" );

    return *pUndoManager;
}


void OReportController::clearUndoManager() const
{
    getUndoManager().Clear();
}


void OReportController::addUndoAction( std::unique_ptr<SfxUndoAction> i_pAction )
{
    getUndoManager().AddUndoAction( std::move(i_pAction) );

    InvalidateFeature( SID_UNDO );
    InvalidateFeature( SID_REDO );
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
reportdesign_OReportController_get_implementation(
    css::uno::XComponentContext* context, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new OReportController(context));
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
