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

#include <sal/config.h>

#include <string_view>

#include <ChartController.hxx>
#include <ChartView.hxx>
#include <PositionAndSizeHelper.hxx>
#include <ObjectIdentifier.hxx>
#include <ChartWindow.hxx>
#include <ResId.hxx>
#include <ChartModel.hxx>
#include <ChartType.hxx>
#include <DiagramHelper.hxx>
#include <Diagram.hxx>
#include <TitleHelper.hxx>
#include "UndoGuard.hxx"
#include <ControllerLockGuard.hxx>
#include <ObjectNameProvider.hxx>
#include <strings.hrc>
#include "DragMethod_PieSegment.hxx"
#include "DragMethod_RotateDiagram.hxx"
#include <ObjectHierarchy.hxx>
#include <RelativePositionHelper.hxx>
#include <chartview/DrawModelWrapper.hxx>
#include <RegressionCurveHelper.hxx>
#include <RegressionCurveModel.hxx>
#include <StatisticsHelper.hxx>
#include <DataSeries.hxx>
#include <DataSeriesHelper.hxx>
#include <DataSeriesProperties.hxx>
#include <Axis.hxx>
#include <AxisHelper.hxx>
#include <LegendHelper.hxx>
#include <servicenames_charttypes.hxx>
#include "DrawCommandDispatch.hxx"
#include <PopupRequest.hxx>
#include <ControllerCommandDispatch.hxx>

#include <com/sun/star/chart2/RelativePosition.hpp>
#include <com/sun/star/chart2/RelativeSize.hpp>
#include <com/sun/star/chart2/data/XPivotTableDataProvider.hpp>

#include <com/sun/star/awt/PopupMenuDirection.hpp>
#include <com/sun/star/frame/DispatchHelper.hpp>
#include <com/sun/star/frame/FrameSearchFlag.hpp>
#include <com/sun/star/frame/XPopupMenuController.hpp>
#include <com/sun/star/awt/Rectangle.hpp>

#include <comphelper/lok.hxx>
#include <comphelper/propertysequence.hxx>
#include <comphelper/propertyvalue.hxx>

#include <sfx2/viewsh.hxx>
#include <svx/ActionDescriptionProvider.hxx>
#include <svx/obj3d.hxx>
#include <svx/scene3d.hxx>
#include <svx/svddrgmt.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>
#include <vcl/weld.hxx>
#include <vcl/ptrstyle.hxx>
#include <svtools/acceleratorexecute.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <toolkit/awt/vclxmenu.hxx>
#include <sal/log.hxx>
#include <o3tl/string_view.hxx>

#include <boost/property_tree/json_parser.hpp>
#include <sfx2/dispatch.hxx>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#define DRGPIX    2     // Drag MinMove in Pixel

using namespace ::com::sun::star;
using namespace ::com::sun::star::chart2;
using namespace ::chart::DataSeriesProperties;
using ::com::sun::star::uno::Reference;

namespace chart
{

namespace
{
bool lcl_GrowAndShiftLogic(
    RelativePosition &  rInOutRelPos,
    RelativeSize &      rInOutRelSize,
    const awt::Size &   rRefSize,
    double              fGrowLogicX,
    double              fGrowLogicY )
{
    if( rRefSize.Width == 0 ||
        rRefSize.Height == 0 )
        return false;

    double fRelativeGrowX = fGrowLogicX / rRefSize.Width;
    double fRelativeGrowY = fGrowLogicY / rRefSize.Height;

    return ::chart::RelativePositionHelper::centerGrow(
        rInOutRelPos, rInOutRelSize,
        fRelativeGrowX, fRelativeGrowY );
}

bool lcl_MoveObjectLogic(
    RelativePosition &  rInOutRelPos,
    RelativeSize const & rObjectSize,
    const awt::Size &   rRefSize,
    double              fShiftLogicX,
    double              fShiftLogicY )
{
    if( rRefSize.Width == 0 ||
        rRefSize.Height == 0 )
        return false;

    double fRelativeShiftX = fShiftLogicX / rRefSize.Width;
    double fRelativeShiftY = fShiftLogicY / rRefSize.Height;

    return ::chart::RelativePositionHelper::moveObject(
        rInOutRelPos, rObjectSize,
        fRelativeShiftX, fRelativeShiftY );
}

void lcl_insertMenuCommand(
    const uno::Reference< awt::XPopupMenu > & xMenu,
    sal_Int16 nId, const OUString & rCommand )
{
    xMenu->insertItem( nId, u""_ustr, 0, -1 );
    xMenu->setCommand( nId, rCommand );
}

OUString lcl_getFormatCommandForObjectCID( std::u16string_view rCID )
{
    OUString aDispatchCommand( u".uno:FormatSelection"_ustr );

    ObjectType eObjectType = ObjectIdentifier::getObjectType( rCID );

    switch(eObjectType)
    {
        case OBJECTTYPE_DIAGRAM:
        case OBJECTTYPE_DIAGRAM_WALL:
            aDispatchCommand = ".uno:FormatWall";
            break;
        case OBJECTTYPE_DIAGRAM_FLOOR:
            aDispatchCommand = ".uno:FormatFloor";
            break;
        case OBJECTTYPE_PAGE:
            aDispatchCommand = ".uno:FormatChartArea";
            break;
        case OBJECTTYPE_LEGEND:
            aDispatchCommand = ".uno:FormatLegend";
            break;
        case OBJECTTYPE_TITLE:
            aDispatchCommand = ".uno:FormatTitle";
            break;
        case OBJECTTYPE_LEGEND_ENTRY:
        case OBJECTTYPE_DATA_SERIES:
            aDispatchCommand = ".uno:FormatDataSeries";
            break;
        case OBJECTTYPE_AXIS:
        case OBJECTTYPE_AXIS_UNITLABEL:
            aDispatchCommand = ".uno:FormatAxis";
            break;
        case OBJECTTYPE_GRID:
            aDispatchCommand = ".uno:FormatMajorGrid";
            break;
        case OBJECTTYPE_SUBGRID:
            aDispatchCommand = ".uno:FormatMinorGrid";
            break;
        case OBJECTTYPE_DATA_LABELS:
            aDispatchCommand = ".uno:FormatDataLabels";
            break;
        case OBJECTTYPE_DATA_LABEL:
            aDispatchCommand = ".uno:FormatDataLabel";
            break;
        case OBJECTTYPE_DATA_POINT:
            aDispatchCommand = ".uno:FormatDataPoint";
            break;
        case OBJECTTYPE_DATA_AVERAGE_LINE:
            aDispatchCommand = ".uno:FormatMeanValue";
            break;
        case OBJECTTYPE_DATA_ERRORS_X:
            aDispatchCommand = ".uno:FormatXErrorBars";
            break;
        case OBJECTTYPE_DATA_ERRORS_Y:
            aDispatchCommand = ".uno:FormatYErrorBars";
            break;
        case OBJECTTYPE_DATA_ERRORS_Z:
            aDispatchCommand = ".uno:FormatZErrorBars";
            break;
        case OBJECTTYPE_DATA_CURVE:
            aDispatchCommand = ".uno:FormatTrendline";
            break;
        case OBJECTTYPE_DATA_CURVE_EQUATION:
            aDispatchCommand = ".uno:FormatTrendlineEquation";
            break;
        case OBJECTTYPE_DATA_STOCK_RANGE:
            aDispatchCommand = ".uno:FormatSelection";
            break;
        case OBJECTTYPE_DATA_STOCK_LOSS:
            aDispatchCommand = ".uno:FormatStockLoss";
            break;
        case OBJECTTYPE_DATA_STOCK_GAIN:
            aDispatchCommand = ".uno:FormatStockGain";
            break;
        default: //OBJECTTYPE_UNKNOWN
            break;
    }
    return aDispatchCommand;
}

} // anonymous namespace

// awt::XWindow
void SAL_CALL ChartController::setPosSize(
    sal_Int32 X,
    sal_Int32 Y,
    sal_Int32 Width,
    sal_Int32 Height,
    sal_Int16 Flags )
{
    SolarMutexGuard aGuard;
    auto pChartWindow(GetChartWindow());

    if(!(m_xViewWindow.is() && pChartWindow))
        return;

    Size aLogicSize = pChartWindow->PixelToLogic( Size( Width, Height ), MapMode( MapUnit::Map100thMM )  );

    //todo: for standalone chart: detect whether we are standalone
    //change map mode to fit new size
    awt::Size aModelPageSize = getChartModel()->getPageSize();
    sal_Int32 nScaleXNumerator = aLogicSize.Width();
    sal_Int32 nScaleXDenominator = aModelPageSize.Width;
    sal_Int32 nScaleYNumerator = aLogicSize.Height();
    sal_Int32 nScaleYDenominator = aModelPageSize.Height;
    MapMode aNewMapMode(
                MapUnit::Map100thMM,
                Point(0,0),
                Fraction(nScaleXNumerator, nScaleXDenominator),
                Fraction(nScaleYNumerator, nScaleYDenominator) );
    pChartWindow->SetMapMode(aNewMapMode);
    pChartWindow->setPosSizePixel( X, Y, Width, Height, static_cast<PosSizeFlags>(Flags) );

    //#i75867# poor quality of ole's alternative view with 3D scenes and zoomfactors besides 100%
    if( m_xChartView.is() )
    {
        auto aZoomFactors(::comphelper::InitPropertySequence({
            { "ScaleXNumerator", uno::Any( nScaleXNumerator ) },
            { "ScaleXDenominator", uno::Any( nScaleXDenominator ) },
            { "ScaleYNumerator", uno::Any( nScaleYNumerator ) },
            { "ScaleYDenominator", uno::Any( nScaleYDenominator ) }
        }));
        m_xChartView->setPropertyValue( u"ZoomFactors"_ustr, uno::Any( aZoomFactors ));
    }

    //a correct work area is at least necessary for correct values in the position and  size dialog and for dragging area
    if(m_pDrawViewWrapper)
    {
        tools::Rectangle aRect(Point(0,0), pChartWindow->GetOutDev()->GetOutputSize());
        m_pDrawViewWrapper->SetWorkArea( aRect );
    }
    pChartWindow->Invalidate();
}

awt::Rectangle SAL_CALL ChartController::getPosSize()
{
    awt::Rectangle aRet(0, 0, 0, 0);

    if (m_xViewWindow.is())
        aRet = m_xViewWindow->getPosSize();

    return aRet;
}

void SAL_CALL ChartController::setVisible( sal_Bool Visible )
{
    if (m_xViewWindow.is())
        m_xViewWindow->setVisible(Visible);
}

void SAL_CALL ChartController::setEnable( sal_Bool Enable )
{
    if (m_xViewWindow.is())
        m_xViewWindow->setEnable(Enable);
}

void SAL_CALL ChartController::setFocus()
{
    if (m_xViewWindow.is())
        m_xViewWindow->setFocus();
}

void SAL_CALL ChartController::addWindowListener(
    const uno::Reference< awt::XWindowListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->addWindowListener(xListener);
}

void SAL_CALL ChartController::removeWindowListener(
    const uno::Reference< awt::XWindowListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->removeWindowListener(xListener);
}

void SAL_CALL ChartController::addFocusListener(
    const uno::Reference< awt::XFocusListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->addFocusListener(xListener);
}

void SAL_CALL ChartController::removeFocusListener(
    const uno::Reference< awt::XFocusListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->removeFocusListener(xListener);
}

void SAL_CALL ChartController::addKeyListener(
    const uno::Reference< awt::XKeyListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->addKeyListener(xListener);
}

void SAL_CALL ChartController::removeKeyListener(
    const uno::Reference< awt::XKeyListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->removeKeyListener(xListener);
}

void SAL_CALL ChartController::addMouseListener(
    const uno::Reference< awt::XMouseListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->addMouseListener(xListener);
}

void SAL_CALL ChartController::removeMouseListener(
    const uno::Reference< awt::XMouseListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->removeMouseListener(xListener);
}

void SAL_CALL ChartController::addMouseMotionListener(
    const uno::Reference< awt::XMouseMotionListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->addMouseMotionListener(xListener);
}

void SAL_CALL ChartController::removeMouseMotionListener(
    const uno::Reference< awt::XMouseMotionListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->removeMouseMotionListener(xListener);
}

void SAL_CALL ChartController::addPaintListener(
    const uno::Reference< awt::XPaintListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->addPaintListener(xListener);
}

void SAL_CALL ChartController::removePaintListener(
    const uno::Reference< awt::XPaintListener >& xListener )
{
    if (m_xViewWindow.is())
        m_xViewWindow->removePaintListener(xListener);
}

// impl vcl window controller methods
void ChartController::PrePaint()
{
    // forward VCLs PrePaint window event to DrawingLayer
    DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();

    if (pDrawViewWrapper)
    {
        pDrawViewWrapper->PrePaint();
    }
}

void ChartController::execute_Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    try
    {
        rtl::Reference<ChartModel> xModel(getChartModel());
        //OSL_ENSURE( xModel.is(), "ChartController::execute_Paint: have no model to paint");
        if (!xModel.is())
            return;

        //better performance for big data
        if (m_xChartView.is())
        {
            awt::Size aResolution(1000, 1000);
            {
                SolarMutexGuard aGuard;
                auto pChartWindow(GetChartWindow());
                if (pChartWindow)
                {
                    aResolution.Width = pChartWindow->GetSizePixel().Width();
                    aResolution.Height = pChartWindow->GetSizePixel().Height();
                }
            }
            m_xChartView->setPropertyValue( u"Resolution"_ustr, uno::Any( aResolution ));
        }

        if (m_xChartView.is())
            m_xChartView->update();

        {
            SolarMutexGuard aGuard;
            DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
            if (pDrawViewWrapper)
                pDrawViewWrapper->CompleteRedraw(&rRenderContext, vcl::Region(rRect));
        }
    }
    catch( const uno::Exception & )
    {
        DBG_UNHANDLED_EXCEPTION("chart2");
    }
    catch( ... )
    {
    }
}

static bool isDoubleClick( const MouseEvent& rMEvt )
{
    return rMEvt.GetClicks() == 2 && rMEvt.IsLeft() &&
        !rMEvt.IsMod1() && !rMEvt.IsMod2() && !rMEvt.IsShift();
}

void ChartController::startDoubleClickWaiting()
{
    SolarMutexGuard aGuard;

    m_bWaitingForDoubleClick = true;

    sal_uInt64 nDblClkTime = 500;
    auto pChartWindow(GetChartWindow());
    if( pChartWindow )
    {
        const MouseSettings& rMSettings = pChartWindow->GetSettings().GetMouseSettings();
        nDblClkTime = rMSettings.GetDoubleClickTime();
    }
    m_aDoubleClickTimer.SetTimeout( nDblClkTime );
    m_aDoubleClickTimer.Start();
}

void ChartController::stopDoubleClickWaiting()
{
    m_aDoubleClickTimer.Stop();
    m_bWaitingForDoubleClick = false;
}

IMPL_LINK_NOARG(ChartController, DoubleClickWaitingHdl, Timer *, void)
{
    m_bWaitingForDoubleClick = false;

    if( m_bWaitingForMouseUp || !m_aSelection.maybeSwitchSelectionAfterSingleClickWasEnsured() )
        return;

    impl_selectObjectAndNotiy();
    SolarMutexGuard aGuard;
    auto pChartWindow(GetChartWindow());
    if( pChartWindow )
    {
        vcl::Window::PointerState aPointerState( pChartWindow->GetPointerState() );
        MouseEvent aMouseEvent(
                        aPointerState.maPos,
                        1/*nClicks*/,
                        MouseEventModifiers::NONE,
                        static_cast< sal_uInt16 >( aPointerState.mnState )/*nButtons*/,
                        0/*nModifier*/ );
        impl_SetMousePointer( aMouseEvent );
    }
}

void ChartController::execute_MouseButtonDown( const MouseEvent& rMEvt )
{
    SolarMutexGuard aGuard;

    m_bWaitingForMouseUp = true;
    m_bFieldButtonDown = false;

    if( isDoubleClick(rMEvt) )
        stopDoubleClickWaiting();
    else
        startDoubleClickWaiting();

    m_aSelection.remindSelectionBeforeMouseDown();

    DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
    auto pChartWindow(GetChartWindow());
    if(!pChartWindow || !pDrawViewWrapper )
        return;

    Point aMPos = pChartWindow->PixelToLogic(rMEvt.GetPosPixel());

    // Check if button was clicked
    SdrObject* pObject = pDrawViewWrapper->getHitObject(aMPos);
    if (pObject)
    {
        OUString aCID = pObject->GetName();
        if (aCID.startsWith("FieldButton"))
        {
            m_bFieldButtonDown = true;
            return; // Don't take any action if button was clicked
        }
    }

    if ( rMEvt.GetButtons() == MOUSE_LEFT )
    {
        pChartWindow->GrabFocus();
        pChartWindow->CaptureMouse();
    }

    if( pDrawViewWrapper->IsTextEdit() )
    {
        SdrViewEvent aVEvt;
        if ( pDrawViewWrapper->IsTextEditHit( aMPos ) ||
             // #i12587# support for shapes in chart
             ( rMEvt.IsRight() && pDrawViewWrapper->PickAnything( rMEvt, SdrMouseEventKind::BUTTONDOWN, aVEvt ) == SdrHitKind::MarkedObject ) )
        {
            pDrawViewWrapper->MouseButtonDown(rMEvt, pChartWindow->GetOutDev());
            return;
        }
        else
        {
            EndTextEdit();
        }
    }

    //abort running action
    if( pDrawViewWrapper->IsAction() )
    {
        if( rMEvt.IsRight() )
            pDrawViewWrapper->BckAction();
        return;
    }

    if( isDoubleClick(rMEvt) ) //do not change selection if double click
        return;//double click is handled further in mousebutton up

    SdrHdl* pHitSelectionHdl = nullptr;
    //switch from move to resize if handle is hit on a resizable object
    if( m_aSelection.isResizeableObjectSelected() )
        pHitSelectionHdl = pDrawViewWrapper->PickHandle( aMPos );
    //only change selection if no selection handles are hit
    if( !pHitSelectionHdl )
    {
        // #i12587# support for shapes in chart
        if ( m_eDrawMode == CHARTDRAW_INSERT &&
             ( !pDrawViewWrapper->IsMarkedHit( aMPos ) || !m_aSelection.isDragableObjectSelected() ) )
        {
            if ( m_aSelection.hasSelection() )
            {
                m_aSelection.clearSelection();
            }
            if ( !pDrawViewWrapper->IsAction() )
            {
                if ( pDrawViewWrapper->GetCurrentObjIdentifier() == SdrObjKind::Caption )
                {
                    Size aCaptionSize( 2268, 1134 );
                    pDrawViewWrapper->BegCreateCaptionObj( aMPos, aCaptionSize );
                }
                else
                {
                    pDrawViewWrapper->BegCreateObj( aMPos);
                }
                SdrObject* pObj = pDrawViewWrapper->GetCreateObj();
                DrawCommandDispatch* pDrawCommandDispatch = m_aDispatchContainer.getDrawCommandDispatch();
                if ( pObj && m_pDrawModelWrapper && pDrawCommandDispatch )
                {
                    SfxItemSet aSet( m_pDrawModelWrapper->GetItemPool() );
                    pDrawCommandDispatch->setAttributes( pObj );
                    pDrawCommandDispatch->setLineEnds( aSet );
                    pObj->SetMergedItemSet( aSet );
                }
            }
            impl_SetMousePointer( rMEvt );
            return;
        }

        m_aSelection.adaptSelectionToNewPos(
                        aMPos,
                        pDrawViewWrapper,
                        rMEvt.IsRight(),
                        m_bWaitingForDoubleClick );

        if( !m_aSelection.isRotateableObjectSelected( getChartModel() ) )
        {
                m_eDragMode = SdrDragMode::Move;
                pDrawViewWrapper->SetDragMode(m_eDragMode);
        }

        m_aSelection.applySelection(pDrawViewWrapper);
    }
    if( m_aSelection.isDragableObjectSelected()
        && !rMEvt.IsRight() )
    {
        //start drag
        sal_uInt16  nDrgLog = static_cast<sal_uInt16>(pChartWindow->PixelToLogic(Size(DRGPIX,0)).Width());
        SdrDragMethod* pDragMethod = nullptr;

        //change selection to 3D scene if rotate mode
        SdrDragMode eDragMode = pDrawViewWrapper->GetDragMode();
        if( eDragMode==SdrDragMode::Rotate )
        {
            E3dScene* pScene = SelectionHelper::getSceneToRotate( pDrawViewWrapper->getNamedSdrObject( m_aSelection.getSelectedCID() ) );
            if( pScene )
            {
                DragMethod_RotateDiagram::RotationDirection eRotationDirection(DragMethod_RotateDiagram::ROTATIONDIRECTION_FREE);
                if(pHitSelectionHdl)
                {
                    SdrHdlKind eKind = pHitSelectionHdl->GetKind();
                    if( eKind==SdrHdlKind::Upper || eKind==SdrHdlKind::Lower )
                        eRotationDirection = DragMethod_RotateDiagram::ROTATIONDIRECTION_X;
                    else if( eKind==SdrHdlKind::Left || eKind==SdrHdlKind::Right )
                        eRotationDirection = DragMethod_RotateDiagram::ROTATIONDIRECTION_Y;
                    else if( eKind==SdrHdlKind::UpperLeft || eKind==SdrHdlKind::UpperRight || eKind==SdrHdlKind::LowerLeft || eKind==SdrHdlKind::LowerRight )
                        eRotationDirection = DragMethod_RotateDiagram::ROTATIONDIRECTION_Z;
                }
                pDragMethod = new DragMethod_RotateDiagram( *pDrawViewWrapper, m_aSelection.getSelectedCID(), getChartModel(), eRotationDirection );
            }
        }
        else
        {
            std::u16string_view aDragMethodServiceName( ObjectIdentifier::getDragMethodServiceName( m_aSelection.getSelectedCID() ) );
            if( aDragMethodServiceName == ObjectIdentifier::getPieSegmentDragMethodServiceName() )
                pDragMethod = new DragMethod_PieSegment( *pDrawViewWrapper, m_aSelection.getSelectedCID(), getChartModel() );
        }
        pDrawViewWrapper->SdrView::BegDragObj(aMPos, nullptr, pHitSelectionHdl, nDrgLog, pDragMethod);
    }

    impl_SetMousePointer( rMEvt );
}

void ChartController::execute_MouseMove( const MouseEvent& rMEvt )
{
    SolarMutexGuard aGuard;

    DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
    auto pChartWindow(GetChartWindow());
    if(!pChartWindow || !pDrawViewWrapper)
        return;

    if( m_pDrawViewWrapper->IsTextEdit() )
    {
        if( m_pDrawViewWrapper->MouseMove(rMEvt,pChartWindow->GetOutDev()) )
            return;
    }

    if(pDrawViewWrapper->IsAction())
    {
        pDrawViewWrapper->MovAction( pChartWindow->PixelToLogic( rMEvt.GetPosPixel() ) );
    }

    impl_SetMousePointer( rMEvt );
}

void ChartController::execute_MouseButtonUp( const MouseEvent& rMEvt )
{
    ControllerLockGuardUNO aCLGuard( getChartModel() );
    bool bMouseUpWithoutMouseDown = !m_bWaitingForMouseUp;
    m_bWaitingForMouseUp = false;
    bool bNotifySelectionChange = false;
    bool bEditText = false;
    {
        SolarMutexGuard aGuard;

        DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
        auto pChartWindow(GetChartWindow());
        if(!pChartWindow || !pDrawViewWrapper)
            return;

        Point aMPos = pChartWindow->PixelToLogic(rMEvt.GetPosPixel());

        // Check if button was clicked
        if (m_bFieldButtonDown)
        {
            m_bFieldButtonDown = false;
            SdrObject* pObject = pDrawViewWrapper->getHitObject(aMPos);
            if (pObject)
            {
                OUString aCID = pObject->GetName();
                if (aCID.startsWith("FieldButton"))
                {
                    sendPopupRequest(aCID, pObject->GetCurrentBoundRect());
                    return;
                }
            }
        }

        if(pDrawViewWrapper->IsTextEdit())
        {
            if( pDrawViewWrapper->MouseButtonUp(rMEvt,pChartWindow->GetOutDev()) )
                return;
        }

        // #i12587# support for shapes in chart
        if ( m_eDrawMode == CHARTDRAW_INSERT && pDrawViewWrapper->IsCreateObj() )
        {
            pDrawViewWrapper->EndCreateObj( SdrCreateCmd::ForceEnd );
            {
                HiddenUndoContext aUndoContext( m_xUndoManager );
                    // don't want the positioning Undo action to appear in the UI
                impl_switchDiagramPositioningToExcludingPositioning();
            }
            if ( pDrawViewWrapper->GetMarkedObjectList().GetMarkCount() != 0 )
            {
                if ( pDrawViewWrapper->GetCurrentObjIdentifier() == SdrObjKind::Text )
                {
                    executeDispatch_EditText();
                }
                else
                {
                    SdrObject* pObj = pDrawViewWrapper->getSelectedObject();
                    if ( pObj )
                    {
                        uno::Reference< drawing::XShape > xShape( pObj->getUnoShape(), uno::UNO_QUERY );
                        if ( xShape.is() )
                        {
                            m_aSelection.setSelection( xShape );
                            m_aSelection.applySelection( pDrawViewWrapper );
                        }
                    }
                }
            }
            else
            {
                m_aSelection.adaptSelectionToNewPos( aMPos, pDrawViewWrapper, rMEvt.IsRight(), m_bWaitingForDoubleClick );
                m_aSelection.applySelection( pDrawViewWrapper );
                setDrawMode( CHARTDRAW_SELECT );
            }
        }
        else if ( pDrawViewWrapper->IsDragObj() )
        {
            bool bDraggingDone = false;
            SdrDragMethod* pDragMethod = pDrawViewWrapper->SdrView::GetDragMethod();
            bool bIsMoveOnly = pDragMethod && pDragMethod->getMoveOnly();
            DragMethod_Base* pChartDragMethod = dynamic_cast< DragMethod_Base* >(pDragMethod);
            if( pChartDragMethod )
            {
                UndoGuard aUndoGuard( pChartDragMethod->getUndoDescription(),
                        m_xUndoManager );

                if( pDrawViewWrapper->EndDragObj() )
                {
                    bDraggingDone = true;
                    aUndoGuard.commit();
                }
            }

            if( !bDraggingDone && pDrawViewWrapper->EndDragObj() )
            {
                try
                {
                    //end move or size
                    SdrObject* pObj = pDrawViewWrapper->getSelectedObject();
                    if( pObj )
                    {
                        tools::Rectangle aObjectRect = pObj->GetSnapRect();
                        tools::Rectangle aOldObjectRect = pObj->GetLastBoundRect();
                        awt::Size aPageSize( getChartModel()->getPageSize() );
                        tools::Rectangle aPageRect( 0,0,aPageSize.Width,aPageSize.Height );

                        const E3dObject* pE3dObject(DynCastE3dObject(pObj));
                        if(nullptr != pE3dObject)
                        {
                            E3dScene* pScene(pE3dObject->getRootE3dSceneFromE3dObject());
                            if(nullptr != pScene)
                            {
                                aObjectRect = pScene->GetSnapRect();
                            }
                        }

                        ActionDescriptionProvider::ActionType eActionType(ActionDescriptionProvider::ActionType::Move);
                        if( !bIsMoveOnly && m_aSelection.isResizeableObjectSelected() )
                            eActionType = ActionDescriptionProvider::ActionType::Resize;

                        ObjectType eObjectType = ObjectIdentifier::getObjectType( m_aSelection.getSelectedCID() );

                        UndoGuard aUndoGuard(
                            ActionDescriptionProvider::createDescription( eActionType, ObjectNameProvider::getName( eObjectType)),
                            m_xUndoManager );

                        bool bChanged = false;
                        rtl::Reference< ChartModel > xModel = getChartModel();
                        if ( eObjectType == OBJECTTYPE_LEGEND )
                            bChanged = DiagramHelper::switchDiagramPositioningToExcludingPositioning( *xModel, false , true );

                        bool bMoved = PositionAndSizeHelper::moveObject( m_aSelection.getSelectedCID()
                                        , xModel
                                        , awt::Rectangle(aObjectRect.Left(),aObjectRect.Top(),aObjectRect.getOpenWidth(),aObjectRect.getOpenHeight())
                                        , awt::Rectangle(aOldObjectRect.Left(), aOldObjectRect.Top(), 0, 0)
                                        , awt::Rectangle(aPageRect.Left(),aPageRect.Top(),aPageRect.getOpenWidth(),aPageRect.getOpenHeight()) );

                        if( bMoved || bChanged )
                        {
                            bDraggingDone = true;
                            aUndoGuard.commit();
                        }
                    }
                }
                catch( const uno::Exception & )
                {
                    DBG_UNHANDLED_EXCEPTION("chart2");
                }
                //all wanted model changes will take effect
                //and all unwanted view modifications are cleaned
            }

            if( !bDraggingDone ) //mouse wasn't moved while dragging
            {
                bool bClickedTwiceOnDragableObject = SelectionHelper::isDragableObjectHitTwice( aMPos, m_aSelection.getSelectedCID(), *pDrawViewWrapper );
                bool bIsRotateable = m_aSelection.isRotateableObjectSelected( getChartModel() );

                //toggle between move and rotate
                if( bIsRotateable && bClickedTwiceOnDragableObject && m_eDragMode==SdrDragMode::Move )
                    m_eDragMode=SdrDragMode::Rotate;
                else
                    m_eDragMode=SdrDragMode::Move;

                pDrawViewWrapper->SetDragMode(m_eDragMode);

                if( !m_bWaitingForDoubleClick && m_aSelection.maybeSwitchSelectionAfterSingleClickWasEnsured() )
                {
                    impl_selectObjectAndNotiy();
                }
            }
            else
                m_aSelection.resetPossibleSelectionAfterSingleClickWasEnsured();
        }

        //@todo ForcePointer(&rMEvt);
        pChartWindow->ReleaseMouse();

        // In tiled rendering drag mode could be not yet over on the call
        // that should handle the double-click, so better to perform this check
        // always.
        if( isDoubleClick(rMEvt) && !bMouseUpWithoutMouseDown /*#i106966#*/ )
        {
            Point aMousePixel = rMEvt.GetPosPixel();
            execute_DoubleClick( &aMousePixel, bEditText );
        }

        if( m_aSelection.isSelectionDifferentFromBeforeMouseDown() )
            bNotifySelectionChange = true;
    }

    impl_SetMousePointer( rMEvt );

    if(bNotifySelectionChange || bEditText)
        impl_notifySelectionChangeListeners();
}

void ChartController::execute_DoubleClick( const Point* pMousePixel, bool &bEditText )
{
    const SfxViewShell* pViewShell = SfxViewShell::Current();
    bool notAllowed = pViewShell && (pViewShell->isLOKMobilePhone() || pViewShell->IsLokReadOnlyView());
    if (notAllowed)
        return;

    if ( m_aSelection.hasSelection() )
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        if ( !aCID.isEmpty() )
        {
            ObjectType eObjectType = ObjectIdentifier::getObjectType( aCID );
            if ( eObjectType == OBJECTTYPE_TITLE )
            {
                bEditText = true;
            }
        }
        else
        {
            // #i12587# support for shapes in chart
            SdrObject* pObj = DrawViewWrapper::getSdrObject( m_aSelection.getSelectedAdditionalShape() );
            if ( DynCastSdrTextObj(pObj) !=  nullptr )
            {
                bEditText = true;
            }
        }
    }

    if ( bEditText )
    {
        executeDispatch_EditText( pMousePixel );
    }
    else
    {
        executeDispatch_ObjectProperties();
    }
}

void ChartController::execute_Resize()
{
    SolarMutexGuard aGuard;
    auto pChartWindow(GetChartWindow());
    if(pChartWindow)
        pChartWindow->Invalidate();
}

void ChartController::execute_Command( const CommandEvent& rCEvt )
{
    SolarMutexGuard aGuard;
    auto pChartWindow(GetChartWindow());
    DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
    if(!pChartWindow || !pDrawViewWrapper)
        return;
    bool bIsAction = m_pDrawViewWrapper->IsAction();

    // pop-up menu
    if(rCEvt.GetCommand() == CommandEventId::ContextMenu && !bIsAction)
    {
        pChartWindow->ReleaseMouse();

        if( m_aSelection.isSelectionDifferentFromBeforeMouseDown() )
            impl_notifySelectionChangeListeners();

        rtl::Reference< VCLXPopupMenu > xPopupMenu = new VCLXPopupMenu();

        Point aPos( rCEvt.GetMousePosPixel() );
        if( !rCEvt.IsMouseEvent() )
        {
            aPos = pChartWindow->GetPointerState().maPos;
        }

        OUString aMenuName;
        if ( isShapeContext() )
            // #i12587# support for shapes in chart
            aMenuName = m_pDrawViewWrapper->IsTextEdit() ? std::u16string_view( u"drawtext" ) : std::u16string_view( u"draw" );
        else
        {
            ObjectType eObjectType = ObjectIdentifier::getObjectType( m_aSelection.getSelectedCID() );

            // todo: the context menu should be specified by an xml file in uiconfig
            sal_Int16 nUniqueId = 1;
            if (eObjectType != OBJECTTYPE_DATA_TABLE)
            {
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:Cut"_ustr );
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:Copy"_ustr );
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:Paste"_ustr );
                xPopupMenu->insertSeparator( -1 );
            }

            rtl::Reference< Diagram > xDiagram = getFirstDiagram();

            OUString aFormatCommand( lcl_getFormatCommandForObjectCID( m_aSelection.getSelectedCID() ) );
            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, aFormatCommand );
            if (eObjectType == OBJECTTYPE_TITLE && m_pDrawViewWrapper->IsTextEdit())
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FontDialog"_ustr );

            //some commands for dataseries and points:

            if( eObjectType == OBJECTTYPE_DATA_SERIES || eObjectType == OBJECTTYPE_DATA_POINT )
            {
                bool bIsPoint = ( eObjectType == OBJECTTYPE_DATA_POINT );
                rtl::Reference< DataSeries > xSeries = ObjectIdentifier::getDataSeriesForCID( m_aSelection.getSelectedCID(), getChartModel() );
                rtl::Reference< RegressionCurveModel > xTrendline = RegressionCurveHelper::getFirstCurveNotMeanValueLine( xSeries );
                bool bHasEquation = RegressionCurveHelper::hasEquation( xTrendline );
                rtl::Reference< RegressionCurveModel > xMeanValue = RegressionCurveHelper::getMeanValueLine( xSeries );
                bool bHasYErrorBars = StatisticsHelper::hasErrorBars( xSeries );
                bool bHasXErrorBars = StatisticsHelper::hasErrorBars( xSeries, false );
                bool bHasDataLabelsAtSeries = xSeries->hasDataLabelsAtSeries();
                bool bHasDataLabelsAtPoints = xSeries->hasDataLabelsAtPoints();
                bool bHasDataLabelAtPoint = false;
                sal_Int32 nPointIndex = -1;
                if( bIsPoint )
                {
                    nPointIndex = ObjectIdentifier::getIndexFromParticleOrCID( m_aSelection.getSelectedCID() );
                    bHasDataLabelAtPoint = xSeries->hasDataLabelAtPoint( nPointIndex );
                }
                bool bSelectedPointIsFormatted = false;
                bool bHasFormattedDataPointsOtherThanSelected = false;

                if( xSeries.is() )
                {
                    uno::Sequence< sal_Int32 > aAttributedDataPointIndexList;
                    // "AttributedDataPoints"
                    if( xSeries->getFastPropertyValue( PROP_DATASERIES_ATTRIBUTED_DATA_POINTS ) >>= aAttributedDataPointIndexList )
                    {
                        if( aAttributedDataPointIndexList.hasElements() )
                        {
                            if( bIsPoint )
                            {
                                auto aIt = std::find( aAttributedDataPointIndexList.begin(), aAttributedDataPointIndexList.end(), nPointIndex );
                                if (aIt != aAttributedDataPointIndexList.end())
                                    bSelectedPointIsFormatted = true;
                                else
                                    bHasFormattedDataPointsOtherThanSelected = true;
                            }
                            else
                                bHasFormattedDataPointsOtherThanSelected = true;
                        }
                    }
                }

                if( bIsPoint )
                {
                    if( bHasDataLabelAtPoint )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatDataLabel"_ustr );
                    if( !bHasDataLabelAtPoint )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertDataLabel"_ustr );
                    else
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteDataLabel"_ustr );
                    if( bSelectedPointIsFormatted )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:ResetDataPoint"_ustr );

                    xPopupMenu->insertSeparator( -1 );

                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatDataSeries"_ustr );
                }

                rtl::Reference< ChartType > xChartType( xDiagram->getChartTypeOfSeries( xSeries ) );
                if( xChartType->getChartType() == CHART2_SERVICE_NAME_CHARTTYPE_CANDLESTICK )
                {
                    try
                    {
                        bool bJapaneseStyle = false;
                        xChartType->getPropertyValue( u"Japanese"_ustr ) >>= bJapaneseStyle;

                        if( bJapaneseStyle )
                        {
                            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatStockLoss"_ustr );
                            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatStockGain"_ustr );
                        }
                    }
                    catch( const uno::Exception & )
                    {
                        DBG_UNHANDLED_EXCEPTION("chart2");
                    }
                }

                if( bHasDataLabelsAtSeries )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatDataLabels"_ustr );
                if( bHasEquation )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatTrendlineEquation"_ustr );
                if( xMeanValue.is() )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatMeanValue"_ustr );
                if( bHasXErrorBars )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatXErrorBars"_ustr );
                if( bHasYErrorBars )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatYErrorBars"_ustr );

                xPopupMenu->insertSeparator( -1 );

                if( !bHasDataLabelsAtSeries )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertDataLabels"_ustr );

                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertTrendline"_ustr );

                if( !xMeanValue.is() )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertMeanValue"_ustr );
                if( !bHasXErrorBars )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertXErrorBars"_ustr );
                if( !bHasYErrorBars )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertYErrorBars"_ustr );
                if( bHasDataLabelsAtSeries || ( bHasDataLabelsAtPoints && bHasFormattedDataPointsOtherThanSelected ) )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteDataLabels"_ustr );
                if( bHasEquation )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteTrendlineEquation"_ustr );
                if( xMeanValue.is() )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteMeanValue"_ustr );
                if( bHasXErrorBars )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteXErrorBars"_ustr );
                if( bHasYErrorBars )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteYErrorBars"_ustr );

                if( bHasFormattedDataPointsOtherThanSelected )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:ResetAllDataPoints"_ustr );

                xPopupMenu->insertSeparator( -1 );

                lcl_insertMenuCommand( xPopupMenu, nUniqueId, u".uno:ArrangeRow"_ustr );
                rtl::Reference< VCLXPopupMenu > xArrangePopupMenu = new VCLXPopupMenu();
                sal_Int16 nSubId = nUniqueId + 1;
                lcl_insertMenuCommand( xArrangePopupMenu, nSubId++, u".uno:Forward"_ustr );
                lcl_insertMenuCommand( xArrangePopupMenu, nSubId, u".uno:Backward"_ustr );
                xPopupMenu->setPopupMenu( nUniqueId, xArrangePopupMenu );
                nUniqueId = nSubId;
                ++nUniqueId;
            }
            else if( eObjectType == OBJECTTYPE_DATA_CURVE )
            {
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:DeleteTrendline"_ustr );
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:FormatTrendlineEquation"_ustr );
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:InsertTrendlineEquation"_ustr );
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:InsertTrendlineEquationAndR2"_ustr );
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:InsertR2Value"_ustr );
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:DeleteTrendlineEquation"_ustr );
                lcl_insertMenuCommand( xPopupMenu,  nUniqueId++, u".uno:DeleteR2Value"_ustr );
            }
            else if( eObjectType == OBJECTTYPE_DATA_CURVE_EQUATION )
            {
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertR2Value"_ustr );
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteR2Value"_ustr );
            }

            //some commands for axes: and grids

            else if( eObjectType  == OBJECTTYPE_AXIS || eObjectType == OBJECTTYPE_GRID || eObjectType == OBJECTTYPE_SUBGRID )
            {
                rtl::Reference< Axis > xAxis = ObjectIdentifier::getAxisForCID( m_aSelection.getSelectedCID(), getChartModel() );
                if( xAxis.is() && xDiagram.is() )
                {
                    sal_Int32 nDimensionIndex = -1;
                    sal_Int32 nCooSysIndex = -1;
                    sal_Int32 nAxisIndex = -1;
                    AxisHelper::getIndicesForAxis( xAxis, xDiagram, nCooSysIndex, nDimensionIndex, nAxisIndex );
                    bool bIsSecondaryAxis = nAxisIndex!=0;
                    bool bIsAxisVisible = AxisHelper::isAxisVisible( xAxis );
                    bool bIsMajorGridVisible = AxisHelper::isGridShown( nDimensionIndex, nCooSysIndex, true /*bMainGrid*/, xDiagram );
                    bool bIsMinorGridVisible = AxisHelper::isGridShown( nDimensionIndex, nCooSysIndex, false /*bMainGrid*/, xDiagram );
                    bool bHasTitle = !TitleHelper::getCompleteString( xAxis->getTitleObject2() ).isEmpty();

                    if( eObjectType  != OBJECTTYPE_AXIS && bIsAxisVisible )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatAxis"_ustr );
                    if( eObjectType != OBJECTTYPE_GRID && bIsMajorGridVisible && !bIsSecondaryAxis )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatMajorGrid"_ustr );
                    if( eObjectType != OBJECTTYPE_SUBGRID && bIsMinorGridVisible && !bIsSecondaryAxis )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatMinorGrid"_ustr );

                    xPopupMenu->insertSeparator( -1 );

                    if( eObjectType  != OBJECTTYPE_AXIS && !bIsAxisVisible )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertAxis"_ustr );
                    if( eObjectType != OBJECTTYPE_GRID && !bIsMajorGridVisible && !bIsSecondaryAxis )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertMajorGrid"_ustr );
                    if( eObjectType != OBJECTTYPE_SUBGRID && !bIsMinorGridVisible && !bIsSecondaryAxis )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertMinorGrid"_ustr );
                    if( !bHasTitle )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertAxisTitle"_ustr );

                    if( bIsAxisVisible )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteAxis"_ustr );
                    if( bIsMajorGridVisible && !bIsSecondaryAxis )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteMajorGrid"_ustr );
                    if( bIsMinorGridVisible && !bIsSecondaryAxis )
                        lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteMinorGrid"_ustr );
                    if (bIsAxisVisible)
                        lcl_insertMenuCommand(xPopupMenu,  nUniqueId++, u".uno:InsertDataTable"_ustr);
                }
            }
            else if (eObjectType == OBJECTTYPE_DATA_TABLE)
            {
                lcl_insertMenuCommand(xPopupMenu,  nUniqueId++, u".uno:DeleteDataTable"_ustr);
            }

            if( eObjectType == OBJECTTYPE_DATA_STOCK_LOSS )
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatStockGain"_ustr );
            else if( eObjectType == OBJECTTYPE_DATA_STOCK_GAIN )
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:FormatStockLoss"_ustr );

            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:TransformDialog"_ustr );

            if( eObjectType == OBJECTTYPE_PAGE || eObjectType == OBJECTTYPE_DIAGRAM
                || eObjectType == OBJECTTYPE_DIAGRAM_WALL
                || eObjectType == OBJECTTYPE_DIAGRAM_FLOOR
                || eObjectType == OBJECTTYPE_UNKNOWN )
            {
                if( eObjectType != OBJECTTYPE_UNKNOWN )
                    xPopupMenu->insertSeparator( -1 );
                bool bHasLegend = LegendHelper::hasLegend( xDiagram );
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertTitles"_ustr );
                if( !bHasLegend )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertLegend"_ustr );
                lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:InsertRemoveAxes"_ustr );
                if( bHasLegend )
                    lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DeleteLegend"_ustr );
            }

            xPopupMenu->insertSeparator( -1 );
            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DiagramType"_ustr );
            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DataRanges"_ustr );
            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:DiagramData"_ustr );
            lcl_insertMenuCommand( xPopupMenu, nUniqueId++, u".uno:View3D"_ustr );
        }

        css::uno::Sequence< css::uno::Any > aArgs{
            css::uno::Any(comphelper::makePropertyValue( u"IsContextMenu"_ustr, true )),
            css::uno::Any(comphelper::makePropertyValue( u"Frame"_ustr, m_xFrame )),
            css::uno::Any(comphelper::makePropertyValue( u"Value"_ustr, aMenuName ))
        };

        css::uno::Reference< css::frame::XPopupMenuController > xPopupController(
            m_xCC->getServiceManager()->createInstanceWithArgumentsAndContext(
            u"com.sun.star.comp.framework.ResourceMenuController"_ustr, aArgs, m_xCC ), css::uno::UNO_QUERY );

        if ( !xPopupController.is() || !xPopupMenu.is() )
            return;

        xPopupController->setPopupMenu( xPopupMenu );

        if (comphelper::LibreOfficeKit::isActive())
        {
            if (SfxViewShell* pViewShell = SfxViewShell::Current())
            {
                const ControllerCommandDispatch* pCommandDispatch = m_aDispatchContainer.getChartDispatcher();
                if (pCommandDispatch)
                {
                    for (int nPos = 0, nCount = xPopupMenu->getItemCount(); nPos < nCount; ++nPos)
                    {
                        auto nItemId = xPopupMenu->getItemId(nPos);
                        OUString aCommandURL = xPopupMenu->getCommand(nItemId);
                        if (!pCommandDispatch->commandAvailable(aCommandURL))
                            xPopupMenu->enableItem(nItemId, false);
                    }
                }

                boost::property_tree::ptree aMenu = SfxDispatcher::fillPopupMenu(xPopupMenu);
                boost::property_tree::ptree aRoot;
                aRoot.add_child("menu", aMenu);

                std::stringstream aStream;
                boost::property_tree::write_json(aStream, aRoot, true);
                pViewShell->libreOfficeKitViewCallback(LOK_CALLBACK_CONTEXT_MENU, OString(aStream.str()));
            }
        }
        else
        {
            xPopupMenu->execute( css::uno::Reference< css::awt::XWindowPeer >( m_xFrame->getContainerWindow(), css::uno::UNO_QUERY ),
                                 css::awt::Rectangle( aPos.X(), aPos.Y(), 0, 0 ),
                                 css::awt::PopupMenuDirection::EXECUTE_DEFAULT );
        }

        css::uno::Reference< css::lang::XComponent > xComponent( xPopupController, css::uno::UNO_QUERY );
        if ( xComponent.is() )
            xComponent->dispose();
    }
    else if( ( rCEvt.GetCommand() == CommandEventId::StartExtTextInput ) ||
             ( rCEvt.GetCommand() == CommandEventId::ExtTextInput ) ||
             ( rCEvt.GetCommand() == CommandEventId::EndExtTextInput ) ||
             ( rCEvt.GetCommand() == CommandEventId::InputContextChange ) )
    {
        //#i84417# enable editing with IME
        m_pDrawViewWrapper->Command( rCEvt, pChartWindow );
    }
}

bool ChartController::execute_KeyInput( const KeyEvent& rKEvt )
{
    SolarMutexGuard aGuard;
    bool bReturn=false;

    DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
    auto pChartWindow(GetChartWindow());
    if (!pChartWindow || !pDrawViewWrapper)
        return bReturn;

    // handle accelerators
    if (!m_apAccelExecute && m_xFrame.is() && m_xCC.is())
    {
        m_apAccelExecute = ::svt::AcceleratorExecute::createAcceleratorHelper();
        OSL_ASSERT(m_apAccelExecute);
        if (m_apAccelExecute)
            m_apAccelExecute->init( m_xCC, m_xFrame );
    }

    vcl::KeyCode aKeyCode( rKEvt.GetKeyCode());
    sal_uInt16 nCode = aKeyCode.GetCode();
    bool bAlternate = aKeyCode.IsMod2();
    bool bCtrl = aKeyCode.IsMod1();

    if (m_apAccelExecute)
        bReturn = m_apAccelExecute->execute( aKeyCode );
    if( bReturn )
        return bReturn;

    if( pDrawViewWrapper->IsTextEdit() )
    {
        if( pDrawViewWrapper->KeyInput(rKEvt, pChartWindow) )
        {
            bReturn = true;
            if( nCode == KEY_ESCAPE )
            {
                EndTextEdit();
            }
        }
    }

    // keyboard accessibility
    ObjectType eObjectType = ObjectIdentifier::getObjectType( m_aSelection.getSelectedCID() );
    if( ! bReturn )
    {
        // Navigation (Tab/F3/Home/End)
        rtl::Reference<::chart::ChartModel> xChartDoc( getChartModel() );
        ObjectKeyNavigation aObjNav( m_aSelection.getSelectedOID(), xChartDoc, m_xChartView.get() );
        awt::KeyEvent aKeyEvent( ::svt::AcceleratorExecute::st_VCLKey2AWTKey( aKeyCode ));
        bReturn = aObjNav.handleKeyEvent( aKeyEvent );
        if( bReturn )
        {
            const ObjectIdentifier& aNewOID = aObjNav.getCurrentSelection();
            uno::Any aNewSelection;
            if ( aNewOID.isValid() && !ObjectHierarchy::isRootNode( aNewOID ) )
            {
                aNewSelection = aNewOID.getAny();
            }
            if ( m_eDragMode == SdrDragMode::Rotate && !SelectionHelper::isRotateableObject( aNewOID.getObjectCID(), getChartModel() ) )
            {
                m_eDragMode = SdrDragMode::Move;
            }
            bReturn = select( aNewSelection );
        }
    }

    // Position and Size (+/-/arrow-keys) or pie segment dragging
    if( ! bReturn  )
    {
        // pie segment dragging
        // note: could also be done for data series
        if( eObjectType == OBJECTTYPE_DATA_POINT &&
            ObjectIdentifier::getDragMethodServiceName( m_aSelection.getSelectedCID() ) ==
                ObjectIdentifier::getPieSegmentDragMethodServiceName())
        {
            bool bDrag = false;
            bool bDragInside = false;
            if( nCode == KEY_ADD ||
                nCode == KEY_SUBTRACT )
            {
                bDrag = true;
                bDragInside = ( nCode == KEY_SUBTRACT );
            }
            else if(
                nCode == KEY_LEFT ||
                nCode == KEY_RIGHT ||
                nCode == KEY_UP ||
                nCode == KEY_DOWN )
            {
                bDrag = true;
                std::u16string_view aParameter( ObjectIdentifier::getDragParameterString( m_aSelection.getSelectedCID() ));
                sal_Int32 nOffsetPercentDummy( 0 );
                awt::Point aMinimumPosition( 0, 0 );
                awt::Point aMaximumPosition( 0, 0 );
                ObjectIdentifier::parsePieSegmentDragParameterString(
                    aParameter, nOffsetPercentDummy, aMinimumPosition, aMaximumPosition );
                aMaximumPosition.Y -= aMinimumPosition.Y;
                aMaximumPosition.X -= aMinimumPosition.X;

                bDragInside =
                    (nCode == KEY_RIGHT && (aMaximumPosition.X < 0)) ||
                    (nCode == KEY_LEFT  && (aMaximumPosition.X > 0)) ||
                    (nCode == KEY_DOWN  && (aMaximumPosition.Y < 0)) ||
                    (nCode == KEY_UP    && (aMaximumPosition.Y > 0));
            }

            if( bDrag )
            {
                double fAmount = bAlternate ? 0.01 : 0.05;
                if( bDragInside )
                    fAmount *= -1.0;

                bReturn = impl_DragDataPoint( m_aSelection.getSelectedCID(), fAmount );
            }
        }
        else
        {
            // size
            if( nCode == KEY_ADD ||
                nCode == KEY_SUBTRACT )
            {
                if( eObjectType == OBJECTTYPE_DIAGRAM )
                {
                    // default 1 mm in each direction
                    double fGrowAmountX = 200.0;
                    double fGrowAmountY = 200.0;
                    if (bAlternate)
                    {
                        // together with Alt-key: 1 px in each direction
                        Size aPixelSize = pChartWindow->PixelToLogic( Size( 2, 2 ));
                        fGrowAmountX = static_cast< double >( aPixelSize.Width());
                        fGrowAmountY = static_cast< double >( aPixelSize.Height());
                    }
                    if( nCode == KEY_SUBTRACT )
                    {
                        fGrowAmountX = -fGrowAmountX;
                        fGrowAmountY = -fGrowAmountY;
                    }
                    bReturn = impl_moveOrResizeObject(
                        m_aSelection.getSelectedCID(), CENTERED_RESIZE_OBJECT, fGrowAmountX, fGrowAmountY );
                }
            }
            // position
            else if( nCode == KEY_LEFT  ||
                     nCode == KEY_RIGHT ||
                     nCode == KEY_UP ||
                     nCode == KEY_DOWN )
            {
                if( m_aSelection.isDragableObjectSelected() )
                {
                    // default 1 mm
                    double fShiftAmountX = 100.0;
                    double fShiftAmountY = 100.0;
                    if (bAlternate)
                    {
                        // together with Alt-key: 1 px
                        Size aPixelSize = pChartWindow->PixelToLogic( Size( 1, 1 ));
                        fShiftAmountX = static_cast< double >( aPixelSize.Width());
                        fShiftAmountY = static_cast< double >( aPixelSize.Height());
                    }
                    switch( nCode )
                    {
                        case KEY_LEFT:
                            fShiftAmountX = -fShiftAmountX;
                            fShiftAmountY = 0.0;
                            break;
                        case KEY_RIGHT:
                            fShiftAmountY = 0.0;
                            break;
                        case KEY_UP:
                            fShiftAmountX = 0.0;
                            fShiftAmountY = -fShiftAmountY;
                            break;
                        case KEY_DOWN:
                            fShiftAmountX = 0.0;
                            break;
                    }
                    if( !m_aSelection.getSelectedCID().isEmpty() )
                    {
                        //move chart objects
                        if (eObjectType == OBJECTTYPE_DATA_LABEL)
                        {
                            SdrObject* pObj = pDrawViewWrapper->getSelectedObject();
                            if (pObj)
                            {
                                tools::Rectangle aRect = pObj->GetSnapRect();
                                awt::Size aPageSize(getChartModel()->getPageSize());
                                if ((fShiftAmountX > 0.0 && (aRect.Right() + fShiftAmountX > aPageSize.Width)) ||
                                    (fShiftAmountX < 0.0 && (aRect.Left() + fShiftAmountX < 0)) ||
                                    (fShiftAmountY > 0.0 && (aRect.Bottom() + fShiftAmountY > aPageSize.Height)) ||
                                    (fShiftAmountY < 0.0 && (aRect.Top() + fShiftAmountY < 0)))
                                    bReturn = false;
                                else
                                    bReturn = PositionAndSizeHelper::moveObject(
                                        m_aSelection.getSelectedCID(), getChartModel(),
                                        awt::Rectangle(aRect.Left() + fShiftAmountX, aRect.Top() + fShiftAmountY, aRect.getOpenWidth(), aRect.getOpenHeight()),
                                        awt::Rectangle(aRect.Left(), aRect.Top(), 0, 0),
                                        awt::Rectangle(0, 0, aPageSize.Width, aPageSize.Height));
                            }
                        }
                        else
                            bReturn = impl_moveOrResizeObject(
                                m_aSelection.getSelectedCID(), MOVE_OBJECT, fShiftAmountX, fShiftAmountY );
                    }
                    else
                    {
                        //move additional shapes
                        uno::Reference< drawing::XShape > xShape( m_aSelection.getSelectedAdditionalShape() );
                        if( xShape.is() )
                        {
                            awt::Point aPos( xShape->getPosition() );
                            awt::Size aSize( xShape->getSize() );
                            awt::Size aPageSize( getChartModel()->getPageSize() );
                            aPos.X = static_cast< tools::Long >( static_cast< double >( aPos.X ) + fShiftAmountX );
                            aPos.Y = static_cast< tools::Long >( static_cast< double >( aPos.Y ) + fShiftAmountY );
                            if( aPos.X + aSize.Width > aPageSize.Width )
                                aPos.X = aPageSize.Width - aSize.Width;
                            if( aPos.X < 0 )
                                aPos.X = 0;
                            if( aPos.Y + aSize.Height > aPageSize.Height )
                                aPos.Y = aPageSize.Height - aSize.Height;
                            if( aPos.Y < 0 )
                                aPos.Y = 0;

                            xShape->setPosition( aPos );
                        }
                    }
                }
            }
        }
    }

    // dumping the shape
    if( !bReturn && bCtrl && nCode == KEY_F12)
    {
        rtl::Reference< ChartModel > xChartModel = getChartModel();
        if(xChartModel.is())
        {
            OUString aDump = xChartModel->dump(u"shapes"_ustr);
            SAL_WARN("chart2", aDump);
        }
    }

    // text edit
    if( ! bReturn &&
        nCode == KEY_F2 )
    {
        if( eObjectType == OBJECTTYPE_TITLE )
        {
            executeDispatch_EditText();
            bReturn = true;
        }
    }

    // deactivate inplace mode (this code should be unnecessary, but
    // unfortunately is not)
    if( ! bReturn &&
        nCode == KEY_ESCAPE )
    {
        uno::Reference< frame::XDispatchHelper > xDispatchHelper( frame::DispatchHelper::create(m_xCC) );
        uno::Sequence< beans::PropertyValue > aArgs;
        xDispatchHelper->executeDispatch(
            uno::Reference< frame::XDispatchProvider >( m_xFrame, uno::UNO_QUERY ),
            u".uno:TerminateInplaceActivation"_ustr,
            u"_parent"_ustr,
            frame::FrameSearchFlag::PARENT,
            aArgs );
        bReturn = true;
    }

    if( ! bReturn &&
        (nCode == KEY_DELETE || nCode == KEY_BACKSPACE ))
    {
        bReturn = executeDispatch_Delete();
        if( ! bReturn )
        {
            std::shared_ptr<weld::MessageDialog> xInfoBox(Application::CreateMessageDialog(pChartWindow->GetFrameWeld(),
                                                          VclMessageType::Info, VclButtonsType::Ok,
                                                          SchResId(STR_ACTION_NOTPOSSIBLE)));
            xInfoBox->runAsync(xInfoBox, [] (int) {});
        }
    }

    return bReturn;
}

bool ChartController::requestQuickHelp(
    ::Point aAtLogicPosition,
    bool bIsBalloonHelp,
    OUString & rOutQuickHelpText,
    awt::Rectangle & rOutEqualRect )
{
    rtl::Reference<::chart::ChartModel> xChartModel;
    if( m_aModel.is())
        xChartModel = getChartModel();
    if( !xChartModel.is())
        return false;

    // help text
    OUString aCID;
    if( m_pDrawViewWrapper )
    {
        aCID = SelectionHelper::getHitObjectCID(
            aAtLogicPosition, *m_pDrawViewWrapper );
    }
    bool bResult( !aCID.isEmpty());

    if( bResult )
    {
        // get help text
        rOutQuickHelpText = ObjectNameProvider::getHelpText( aCID, xChartModel, bIsBalloonHelp /* bVerbose */ );

        // set rectangle
        if( m_xChartView )
            rOutEqualRect = m_xChartView->getRectangleOfObject( aCID, true );
    }

    return bResult;
}

// XSelectionSupplier (optional interface)
sal_Bool SAL_CALL ChartController::select( const uno::Any& rSelection )
{
    bool bSuccess = false;

    if ( rSelection.hasValue() )
    {
        if (rSelection.getValueType() == cppu::UnoType<OUString>::get())
        {
            OUString aNewCID;
            if ( ( rSelection >>= aNewCID ) && m_aSelection.setSelection( aNewCID ) )
            {
                bSuccess = true;
            }
        }
        else if (uno::Reference<drawing::XShape> xShape; rSelection >>= xShape)
        {
            if (m_aSelection.setSelection(xShape))
            {
                bSuccess = true;
            }
        }
    }
    else
    {
        if ( m_aSelection.hasSelection() )
        {
            m_aSelection.clearSelection();
            bSuccess = true;
        }
    }

    if ( bSuccess )
    {
        SolarMutexGuard aGuard;
        if ( m_pDrawViewWrapper && m_pDrawViewWrapper->IsTextEdit() )
        {
            EndTextEdit();
        }
        impl_selectObjectAndNotiy();
        auto pChartWindow(GetChartWindow());
        if ( pChartWindow )
        {
            pChartWindow->Invalidate();
        }
        return true;
    }

    return false;
}

uno::Any SAL_CALL ChartController::getSelection()
{
    uno::Any aReturn;
    if ( m_aSelection.hasSelection() )
    {
        OUString aCID( m_aSelection.getSelectedCID() );
        if ( !aCID.isEmpty() )
        {
            aReturn <<= aCID;
        }
        else
        {
            // #i12587# support for shapes in chart
            aReturn <<= m_aSelection.getSelectedAdditionalShape();
        }
    }
    return aReturn;
}

void SAL_CALL ChartController::addSelectionChangeListener( const uno::Reference<view::XSelectionChangeListener> & xListener )
{
    SolarMutexGuard aGuard;
    if( impl_isDisposedOrSuspended() )//@todo? allow adding of listeners in suspend mode?
        return; //behave passive if already disposed or suspended

    //--add listener
    std::unique_lock aGuard2(m_aLifeTimeManager.m_aAccessMutex);
    m_aLifeTimeManager.m_aSelectionChangeListeners.addInterface( aGuard2, xListener );
}

void SAL_CALL ChartController::removeSelectionChangeListener( const uno::Reference<view::XSelectionChangeListener> & xListener )
{
    SolarMutexGuard aGuard;
    if( impl_isDisposedOrSuspended() ) //@todo? allow removing of listeners in suspend mode?
        return; //behave passive if already disposed or suspended

    //--remove listener
    std::unique_lock aGuard2(m_aLifeTimeManager.m_aAccessMutex);
    m_aLifeTimeManager.m_aSelectionChangeListeners.removeInterface( aGuard2, xListener );
}

void ChartController::impl_notifySelectionChangeListeners()
{
    std::unique_lock aGuard(m_aLifeTimeManager.m_aAccessMutex);
    if( m_aLifeTimeManager.m_aSelectionChangeListeners.getLength(aGuard) )
    {
        uno::Reference< view::XSelectionSupplier > xSelectionSupplier(this);
        lang::EventObject aEvent( xSelectionSupplier );
        m_aLifeTimeManager.m_aSelectionChangeListeners.notifyEach(aGuard, &view::XSelectionChangeListener::selectionChanged, aEvent);
    }
}

void ChartController::impl_selectObjectAndNotiy()
{
    {
        SolarMutexGuard aGuard;
        DrawViewWrapper* pDrawViewWrapper = m_pDrawViewWrapper.get();
        if( pDrawViewWrapper )
        {
            pDrawViewWrapper->SetDragMode( m_eDragMode );
            m_aSelection.applySelection( m_pDrawViewWrapper.get() );
        }
    }
    impl_notifySelectionChangeListeners();
}

bool ChartController::impl_moveOrResizeObject(
    const OUString & rCID,
    eMoveOrResizeType eType,
    double fAmountLogicX,
    double fAmountLogicY )
{
    bool bResult = false;
    bool bNeedResize = ( eType == CENTERED_RESIZE_OBJECT );

    rtl::Reference<::chart::ChartModel> xChartModel( getChartModel() );
    uno::Reference< beans::XPropertySet > xObjProp(
        ObjectIdentifier::getObjectPropertySet( rCID, xChartModel ));
    if( xObjProp.is())
    {
        awt::Size aRefSize = xChartModel->getPageSize();

        chart2::RelativePosition aRelPos;
        chart2::RelativeSize     aRelSize;
        bool bDeterminePos  = !(xObjProp->getPropertyValue( u"RelativePosition"_ustr) >>= aRelPos);
        bool bDetermineSize = !bNeedResize || !(xObjProp->getPropertyValue( u"RelativeSize"_ustr) >>= aRelSize);

        if( ( bDeterminePos || bDetermineSize ) &&
            ( aRefSize.Width > 0 && aRefSize.Height > 0 ) )
        {
            ChartView * pValueProvider( m_xChartView.get() );
            if( pValueProvider )
            {
                awt::Rectangle aRect( pValueProvider->getRectangleOfObject( rCID ));
                double fWidth = static_cast< double >( aRefSize.Width );
                double fHeight = static_cast< double >( aRefSize.Height );
                if( bDetermineSize )
                {
                    aRelSize.Primary   = static_cast< double >( aRect.Width ) / fWidth;
                    aRelSize.Secondary = static_cast< double >( aRect.Height ) / fHeight;
                }
                if( bDeterminePos )
                {
                    if( bNeedResize && aRelSize.Primary > 0.0 && aRelSize.Secondary > 0.0 )
                    {
                        aRelPos.Primary   = (static_cast< double >( aRect.X ) / fWidth) +
                            (aRelSize.Primary / 2.0);
                        aRelPos.Secondary = (static_cast< double >( aRect.Y ) / fHeight) +
                            (aRelSize.Secondary / 2.0);
                        aRelPos.Anchor = drawing::Alignment_CENTER;
                    }
                    else
                    {
                        aRelPos.Primary   = static_cast< double >( aRect.X ) / fWidth;
                        aRelPos.Secondary = static_cast< double >( aRect.Y ) / fHeight;
                        aRelPos.Anchor = drawing::Alignment_TOP_LEFT;
                    }
                }
            }
        }

        if( eType == CENTERED_RESIZE_OBJECT )
            bResult = lcl_GrowAndShiftLogic( aRelPos, aRelSize, aRefSize, fAmountLogicX, fAmountLogicY );
        else if( eType == MOVE_OBJECT )
            bResult = lcl_MoveObjectLogic( aRelPos, aRelSize, aRefSize, fAmountLogicX, fAmountLogicY );

        if( bResult )
        {
            ActionDescriptionProvider::ActionType eActionType(ActionDescriptionProvider::ActionType::Move);
            if( bNeedResize )
                eActionType = ActionDescriptionProvider::ActionType::Resize;

            ObjectType eObjectType = ObjectIdentifier::getObjectType( rCID );
            UndoGuard aUndoGuard( ActionDescriptionProvider::createDescription(
                    eActionType, ObjectNameProvider::getName( eObjectType )), m_xUndoManager );
            {
                ControllerLockGuardUNO aCLGuard( xChartModel );
                xObjProp->setPropertyValue( u"RelativePosition"_ustr, uno::Any( aRelPos ));
                if( bNeedResize || (eObjectType == OBJECTTYPE_DIAGRAM) )//Also set an explicit size at the diagram when an explicit position is set
                    xObjProp->setPropertyValue( u"RelativeSize"_ustr, uno::Any( aRelSize ));
            }
            aUndoGuard.commit();
        }
    }
    return bResult;
}

bool ChartController::impl_DragDataPoint( std::u16string_view rCID, double fAdditionalOffset )
{
    bool bResult = false;
    if( fAdditionalOffset < -1.0 || fAdditionalOffset > 1.0 || fAdditionalOffset == 0.0 )
        return bResult;

    sal_Int32 nDataPointIndex = ObjectIdentifier::getIndexFromParticleOrCID( rCID );
    rtl::Reference< DataSeries > xSeries =
        ObjectIdentifier::getDataSeriesForCID( rCID, getChartModel() );
    if( xSeries.is())
    {
        try
        {
            uno::Reference< beans::XPropertySet > xPointProp( xSeries->getDataPointByIndex( nDataPointIndex ));
            double fOffset = 0.0;
            if( xPointProp.is() &&
                (xPointProp->getPropertyValue( u"Offset"_ustr ) >>= fOffset ) &&
                (( fAdditionalOffset > 0.0 && fOffset < 1.0 ) || (fOffset > 0.0)) )
            {
                fOffset += fAdditionalOffset;
                if( fOffset > 1.0 )
                    fOffset = 1.0;
                else if( fOffset < 0.0 )
                    fOffset = 0.0;
                xPointProp->setPropertyValue( u"Offset"_ustr, uno::Any( fOffset ));
                bResult = true;
            }
        }
        catch( const uno::Exception & )
        {
            DBG_UNHANDLED_EXCEPTION("chart2");
        }
    }

    return bResult;
}

void ChartController::impl_SetMousePointer( const MouseEvent & rEvent )
{
    SolarMutexGuard aGuard;
    auto pChartWindow(GetChartWindow());

    if (!m_pDrawViewWrapper || !pChartWindow)
        return;

    Point aMousePos( pChartWindow->PixelToLogic( rEvent.GetPosPixel()));
    sal_uInt16 nModifier = rEvent.GetModifier();
    bool bLeftDown = rEvent.IsLeft();

    // Check if object is for field button and set the normal arrow pointer in this case
    SdrObject* pObject = m_pDrawViewWrapper->getHitObject(aMousePos);
    if (pObject && pObject->GetName().startsWith("FieldButton"))
    {
        pChartWindow->SetPointer(PointerStyle::Arrow);
        return;
    }

    if ( m_pDrawViewWrapper->IsTextEdit() )
    {
        if( m_pDrawViewWrapper->IsTextEditHit( aMousePos ) )
        {
            pChartWindow->SetPointer( m_pDrawViewWrapper->GetPreferredPointer(
                aMousePos, pChartWindow->GetOutDev(), nModifier, bLeftDown ) );
            return;
        }
    }
    else if( m_pDrawViewWrapper->IsAction() )
    {
        return;//don't change pointer during running action
    }

    SdrHdl* pHitSelectionHdl = nullptr;
    if( m_aSelection.isResizeableObjectSelected() )
        pHitSelectionHdl = m_pDrawViewWrapper->PickHandle( aMousePos );

    if( pHitSelectionHdl )
    {
        PointerStyle aPointer = m_pDrawViewWrapper->GetPreferredPointer(
            aMousePos, pChartWindow->GetOutDev(), nModifier, bLeftDown );
        bool bForceArrowPointer = false;

        ObjectIdentifier aOID( m_aSelection.getSelectedOID() );

        switch( aPointer)
        {
            case PointerStyle::NSize:
            case PointerStyle::SSize:
            case PointerStyle::WSize:
            case PointerStyle::ESize:
            case PointerStyle::NWSize:
            case PointerStyle::NESize:
            case PointerStyle::SWSize:
            case PointerStyle::SESize:
                if( ! m_aSelection.isResizeableObjectSelected() )
                    bForceArrowPointer = true;
                break;
            case PointerStyle::Move:
                if ( !aOID.isDragableObject() )
                    bForceArrowPointer = true;
                break;
            case PointerStyle::MovePoint:
            case PointerStyle::MoveBezierWeight:
                // there is no point-editing in a chart
                // the PointerStyle::MoveBezierWeight appears in 3d data points
                bForceArrowPointer = true;
                break;
            default:
                break;
        }

        if( bForceArrowPointer )
            pChartWindow->SetPointer( PointerStyle::Arrow );
        else
            pChartWindow->SetPointer( aPointer );

        return;
    }

    // #i12587# support for shapes in chart
    if ( m_eDrawMode == CHARTDRAW_INSERT &&
         ( !m_pDrawViewWrapper->IsMarkedHit( aMousePos ) || !m_aSelection.isDragableObjectSelected() ) )
    {
        PointerStyle ePointerStyle = PointerStyle::DrawRect;
        SdrObjKind eKind = m_pDrawViewWrapper->GetCurrentObjIdentifier();
        switch ( eKind )
        {
            case SdrObjKind::Line:
                {
                    ePointerStyle = PointerStyle::DrawLine;
                }
                break;
            case SdrObjKind::Rectangle:
            case SdrObjKind::CustomShape:
                {
                    ePointerStyle = PointerStyle::DrawRect;
                }
                break;
            case SdrObjKind::CircleOrEllipse:
                {
                    ePointerStyle = PointerStyle::DrawEllipse;
                }
                break;
            case SdrObjKind::FreehandLine:
                {
                    ePointerStyle = PointerStyle::DrawPolygon;
                }
                break;
            case SdrObjKind::Text:
                {
                    ePointerStyle = PointerStyle::DrawText;
                }
                break;
            case SdrObjKind::Caption:
                {
                    ePointerStyle = PointerStyle::DrawCaption;
                }
                break;
            default:
                {
                    ePointerStyle = PointerStyle::DrawRect;
                }
                break;
        }
        pChartWindow->SetPointer( ePointerStyle );
        return;
    }

    OUString aHitObjectCID(
        SelectionHelper::getHitObjectCID(
            aMousePos, *m_pDrawViewWrapper, true /*bGetDiagramInsteadOf_Wall*/ ));

    if( m_pDrawViewWrapper->IsTextEdit() )
    {
        if( aHitObjectCID == m_aSelection.getSelectedCID() )
        {
            pChartWindow->SetPointer( PointerStyle::Arrow );
            return;
        }
    }

    if( aHitObjectCID.isEmpty() )
    {
        //additional shape was hit
        pChartWindow->SetPointer( PointerStyle::Move );
    }
    else if( ObjectIdentifier::isDragableObject( aHitObjectCID ) )
    {
        if( (m_eDragMode == SdrDragMode::Rotate)
            && SelectionHelper::isRotateableObject( aHitObjectCID
                , getChartModel() ) )
            pChartWindow->SetPointer( PointerStyle::Rotate );
        else
        {
            ObjectType eHitObjectType = ObjectIdentifier::getObjectType( aHitObjectCID );
            if( eHitObjectType == OBJECTTYPE_DATA_POINT )
            {
                if( !ObjectIdentifier::areSiblings(aHitObjectCID,m_aSelection.getSelectedCID())
                    && !ObjectIdentifier::areIdenticalObjects(aHitObjectCID,m_aSelection.getSelectedCID()) )
                {
                    pChartWindow->SetPointer( PointerStyle::Arrow );
                    return;
                }
            }
            pChartWindow->SetPointer( PointerStyle::Move );
        }
    }
    else
        pChartWindow->SetPointer( PointerStyle::Arrow );
}

void ChartController::sendPopupRequest(std::u16string_view rCID, tools::Rectangle aRectangle)
{
    ChartModel* pChartModel = m_aModel->getModel().get();
    if (!pChartModel)
        return;

    uno::Reference<chart2::data::XPivotTableDataProvider> xPivotTableDataProvider;
    xPivotTableDataProvider.set(pChartModel->getDataProvider(), uno::UNO_QUERY);
    if (!xPivotTableDataProvider.is())
        return;

    OUString sPivotTableName = xPivotTableDataProvider->getPivotTableName();

    css::uno::Reference<css::awt::XRequestCallback> xPopupRequest = pChartModel->getPopupRequest();
    PopupRequest* pPopupRequest = dynamic_cast<PopupRequest*>(xPopupRequest.get());
    if (!pPopupRequest)
        return;

    // Get dimension index from CID
    size_t nStartPos = rCID.rfind('.');
    nStartPos = (nStartPos == std::u16string_view::npos) ? 0 : (nStartPos + 1);
    sal_Int32 nEndPos = rCID.size();
    std::u16string_view sDimensionIndex = rCID.substr(nStartPos, nEndPos - nStartPos);
    sal_Int32 nDimensionIndex = o3tl::toInt32(sDimensionIndex);

    awt::Rectangle xRectangle {
        sal_Int32(aRectangle.Left()),
        sal_Int32(aRectangle.Top()),
        sal_Int32(aRectangle.GetWidth()),
        sal_Int32(aRectangle.GetHeight())
    };

    uno::Sequence<beans::PropertyValue> aCallbackData = comphelper::InitPropertySequence(
    {
        {"Rectangle",      uno::Any(xRectangle)},
        {"DimensionIndex", uno::Any(sal_Int32(nDimensionIndex))},
        {"PivotTableName", uno::Any(sPivotTableName)},
    });

    pPopupRequest->getCallback()->notify(uno::Any(aCallbackData));
}

} //namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
