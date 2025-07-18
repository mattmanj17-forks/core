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

#include <ChartModel.hxx>
#include <ChartTypeManager.hxx>
#include <ChartTypeTemplate.hxx>
#include <servicenames.hxx>
#include <DataSource.hxx>
#include <DataSourceHelper.hxx>
#include <ChartType.hxx>
#include <DisposeHelper.hxx>
#include <ControllerLockGuard.hxx>
#include <InternalDataProvider.hxx>
#include <ObjectIdentifier.hxx>
#include <BaseCoordinateSystem.hxx>
#include "PageBackground.hxx"
#include <CloneHelper.hxx>
#include <NameContainer.hxx>
#include "UndoManager.hxx"

#include <ChartColorPaletteHelper.hxx>
#include <ChartView.hxx>
#include <PopupRequest.hxx>
#include <ModifyListenerHelper.hxx>
#include <RangeHighlighter.hxx>
#include <Diagram.hxx>
#include <ChartDocumentWrapper.hxx>
#include <comphelper/dumpxmltostring.hxx>

#include <com/sun/star/chart/ChartDataRowSource.hpp>
#include <com/sun/star/chart2/data/XPivotTableDataProvider.hpp>

#include <comphelper/processfactory.hxx>
#include <comphelper/propertysequence.hxx>
#include <cppuhelper/supportsservice.hxx>

#include <svl/numformat.hxx>
#include <svl/numuno.hxx>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/view/XSelectionSupplier.hpp>
#include <com/sun/star/embed/EmbedMapUnits.hpp>
#include <com/sun/star/embed/Aspects.hpp>
#include <com/sun/star/datatransfer/UnsupportedFlavorException.hpp>
#include <com/sun/star/datatransfer/XTransferable.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/drawing/XDrawPage.hpp>
#include <com/sun/star/drawing/FillStyle.hpp>
#include <com/sun/star/drawing/XShapes.hpp>
#include <com/sun/star/document/DocumentProperties.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/util/CloseVetoException.hpp>
#include <com/sun/star/util/XModifyBroadcaster.hpp>

#include <sal/log.hxx>
#include <utility>
#include <comphelper/diagnose_ex.hxx>
#include <libxml/xmlwriter.h>

#include <sfx2/objsh.hxx>
#include <com/sun/star/util/XTheme.hpp>
#include <docmodel/uno/UnoTheme.hxx>
#include <docmodel/theme/Theme.hxx>

using ::com::sun::star::uno::Sequence;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Any;
using ::osl::MutexGuard;

using namespace ::com::sun::star;
using namespace ::apphelper;

namespace
{
constexpr OUString lcl_aGDIMetaFileMIMEType(
    u"application/x-openoffice-gdimetafile;windows_formatname=\"GDIMetaFile\""_ustr);
constexpr OUString lcl_aGDIMetaFileMIMETypeHighContrast(
    u"application/x-openoffice-highcontrast-gdimetafile;windows_formatname=\"GDIMetaFile\""_ustr);

} // anonymous namespace

// ChartModel Constructor and Destructor

namespace chart
{

namespace
{
SfxObjectShell* getParentShell(const uno::Reference<frame::XModel>& xDocModel)
{
    uno::Reference<lang::XUnoTunnel> xUnoTunnel(xDocModel, uno::UNO_QUERY);
    if (xUnoTunnel.is())
    {
        return comphelper::getFromUnoTunnel<SfxObjectShell>(xUnoTunnel);
    }
    return nullptr;
}
}

ChartModel::ChartModel(uno::Reference<uno::XComponentContext > xContext)
    : m_aLifeTimeManager( this, this )
    , m_bReadOnly( false )
    , m_bModified( false )
    , m_nInLoad(0)
    , m_bUpdateNotificationsPending(false)
    , mbTimeBased(false)
    , m_aControllers( m_aModelMutex )
    , m_nControllerLockCount(0)
    , m_xContext(std::move( xContext ))
    , m_aVisualAreaSize( ChartModel::getDefaultPageSize() )
    , m_xPageBackground( new PageBackground )
    , m_xXMLNamespaceMap( new NameContainer() )
    , m_eColorPaletteType(ChartColorPaletteType::Unknown)
    , m_nColorPaletteIndex(0)
    , mnStart(0)
    , mnEnd(0)
{
    osl_atomic_increment(&m_refCount);
    {
        m_xOldModelAgg = new wrapper::ChartDocumentWrapper(m_xContext);
        m_xOldModelAgg->setDelegator( *this );
    }

    {
        m_xPageBackground->addModifyListener( this );
        m_xChartTypeManager = new ::chart::ChartTypeManager( m_xContext );
    }
    osl_atomic_decrement(&m_refCount);
}

ChartModel::ChartModel( const ChartModel & rOther )
    : impl::ChartModel_Base(rOther)
    // not copy the listener
    , SfxListener()
    , m_aLifeTimeManager( this, this )
    , m_bReadOnly( rOther.m_bReadOnly )
    , m_bModified( rOther.m_bModified )
    , m_nInLoad(0)
    , m_bUpdateNotificationsPending(false)
    , mbTimeBased(rOther.mbTimeBased)
    , m_aResource( rOther.m_aResource )
    , m_aMediaDescriptor( rOther.m_aMediaDescriptor )
    , m_aControllers( m_aModelMutex )
    , m_nControllerLockCount(0)
    , m_xContext( rOther.m_xContext )
    // @note: the old model aggregate must not be shared with other models if it
    // is, you get mutex deadlocks
    //, m_xOldModelAgg( nullptr ) //rOther.m_xOldModelAgg )
    // m_xStorage( nullptr ) //rOther.m_xStorage )
    , m_aVisualAreaSize( rOther.m_aVisualAreaSize )
    , m_aGraphicObjectVector( rOther.m_aGraphicObjectVector )
    , m_xDataProvider( rOther.m_xDataProvider )
    , m_xInternalDataProvider( rOther.m_xInternalDataProvider )
    , m_eColorPaletteType(ChartColorPaletteType::Unknown)
    , m_nColorPaletteIndex(0)
    , mnStart(rOther.mnStart)
    , mnEnd(rOther.mnEnd)
{
    osl_atomic_increment(&m_refCount);
    {
        m_xOldModelAgg = new wrapper::ChartDocumentWrapper(m_xContext);
        m_xOldModelAgg->setDelegator( *this );

        Reference< util::XModifyListener > xListener;
        rtl::Reference< Title > xNewTitle;
        if ( rOther.m_xTitle )
            xNewTitle = new Title(*rOther.m_xTitle);
        rtl::Reference< ::chart::Diagram > xNewDiagram;
        if (rOther.m_xDiagram.is())
            xNewDiagram = new ::chart::Diagram( *rOther.m_xDiagram );
        rtl::Reference< ::chart::PageBackground > xNewPageBackground = new PageBackground( *rOther.m_xPageBackground );

        {
            rtl::Reference< ::chart::ChartTypeManager > xChartTypeManager; // does not implement XCloneable
            rtl::Reference< ::chart::NameContainer > xXMLNamespaceMap = new NameContainer( *rOther.m_xXMLNamespaceMap );

            {
                MutexGuard aGuard( m_aModelMutex );
                xListener = this;
                m_xTitle = xNewTitle;
                m_xDiagram = xNewDiagram;
                m_xPageBackground = xNewPageBackground;
                m_xChartTypeManager = std::move(xChartTypeManager);
                m_xXMLNamespaceMap = std::move(xXMLNamespaceMap);
            }
        }

        ModifyListenerHelper::addListener( xNewTitle, xListener );
        if( xNewDiagram && xListener)
            xNewDiagram->addModifyListener( xListener );
        if( xNewPageBackground && xListener)
            xNewPageBackground->addModifyListener( xListener );
        xListener.clear();
    }
    osl_atomic_decrement(&m_refCount);
}

ChartModel::~ChartModel()
{
    if (SfxObjectShell* pShell = getParentShell(m_xParent))
        EndListening(*pShell);
    if( m_xOldModelAgg.is())
        m_xOldModelAgg->setDelegator( nullptr );
}

void SAL_CALL ChartModel::initialize( const Sequence< Any >& /*rArguments*/ )
{
    //#i113722# avoid duplicate creation

    //maybe additional todo?:
    //support argument "EmbeddedObject"?
    //support argument "EmbeddedScriptSupport"?
    //support argument "DocumentRecoverySupport"?
}

ChartView* ChartModel::getChartView() const
{
    return mxChartView.get();
}

// private methods

OUString ChartModel::impl_g_getLocation()
{

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return OUString(); //behave passive if already disposed or closed or throw exception @todo?
    //mutex is acquired
    return m_aResource;
}

bool ChartModel::impl_isControllerConnected( const uno::Reference< frame::XController >& xController )
{
    try
    {
        std::vector< uno::Reference<uno::XInterface> > aSeq = m_aControllers.getElements();
        for( const auto & r : aSeq )
        {
            if( r == xController )
                return true;
        }
    }
    catch (const uno::Exception&)
    {
    }
    return false;
}

uno::Reference< frame::XController > ChartModel::impl_getCurrentController()
{
        //@todo? hold only weak references to controllers

    // get the last active controller of this model
    if( m_xCurrentController.is() )
        return m_xCurrentController;

    // get the first controller of this model
    if( m_aControllers.getLength() )
    {
        uno::Reference<uno::XInterface> xI = m_aControllers.getInterface(0);
        return uno::Reference<frame::XController>( xI, uno::UNO_QUERY );
    }

    //return nothing if no controllers are connected at all
    return uno::Reference< frame::XController > ();
}

void ChartModel::impl_notifyCloseListeners()
{
    std::unique_lock aGuard(m_aLifeTimeManager.m_aAccessMutex);
    if( m_aLifeTimeManager.m_aCloseListeners.getLength(aGuard) )
    {
        lang::EventObject aEvent( static_cast< lang::XComponent*>(this) );
        m_aLifeTimeManager.m_aCloseListeners.notifyEach(aGuard, &util::XCloseListener::notifyClosing, aEvent);
    }
}

void ChartModel::impl_adjustAdditionalShapesPositionAndSize( const awt::Size& aVisualAreaSize )
{
    uno::Reference< beans::XPropertySet > xProperties( static_cast< ::cppu::OWeakObject* >( this ), uno::UNO_QUERY );
    if ( !xProperties.is() )
        return;

    uno::Reference< drawing::XShapes > xShapes;
    xProperties->getPropertyValue( u"AdditionalShapes"_ustr ) >>= xShapes;
    if ( !xShapes.is() )
        return;

    sal_Int32 nCount = xShapes->getCount();
    for ( sal_Int32 i = 0; i < nCount; ++i )
    {
        Reference< drawing::XShape > xShape;
        if ( xShapes->getByIndex( i ) >>= xShape )
        {
            if ( xShape.is() )
            {
                awt::Point aPos( xShape->getPosition() );
                awt::Size aSize( xShape->getSize() );

                double fWidth = static_cast< double >( aVisualAreaSize.Width ) / m_aVisualAreaSize.Width;
                double fHeight = static_cast< double >( aVisualAreaSize.Height ) / m_aVisualAreaSize.Height;

                aPos.X = static_cast< tools::Long >( aPos.X * fWidth );
                aPos.Y = static_cast< tools::Long >( aPos.Y * fHeight );
                aSize.Width = static_cast< tools::Long >( aSize.Width * fWidth );
                aSize.Height = static_cast< tools::Long >( aSize.Height * fHeight );

                xShape->setPosition( aPos );
                xShape->setSize( aSize );
            }
        }
    }
}

// lang::XServiceInfo

OUString SAL_CALL ChartModel::getImplementationName()
{
    return u"com.sun.star.comp.chart2.ChartModel"_ustr;
}

sal_Bool SAL_CALL ChartModel::supportsService( const OUString& rServiceName )
{
    return cppu::supportsService(this, rServiceName);
}

css::uno::Sequence< OUString > SAL_CALL ChartModel::getSupportedServiceNames()
{
    return {
        u"com.sun.star.chart2.ChartDocument"_ustr,
        u"com.sun.star.document.OfficeDocument"_ustr,
        u"com.sun.star.chart.ChartDocument"_ustr
    };
}

// frame::XModel (required interface)

sal_Bool SAL_CALL ChartModel::attachResource( const OUString& rURL
        , const uno::Sequence< beans::PropertyValue >& rMediaDescriptor )
{
    /*
    The method attachResource() is used by the frame loader implementations
    to inform the model about its URL and MediaDescriptor.
    */

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return false; //behave passive if already disposed or closed or throw exception @todo?
    //mutex is acquired

    if(!m_aResource.isEmpty())//we have a resource already //@todo? or is setting a new resource allowed?
        return false;
    m_aResource = rURL;
    m_aMediaDescriptor = rMediaDescriptor;

    //@todo ? check rURL ??
    //@todo ? evaluate m_aMediaDescriptor;
    //@todo ? ... ??? --> nothing, this method is only for setting information

    return true;
}

OUString SAL_CALL ChartModel::getURL()
{
    return impl_g_getLocation();
}

uno::Sequence< beans::PropertyValue > SAL_CALL ChartModel::getArgs()
{
    /*
    The method getArgs() returns a sequence of property values
    that report the resource description according to com.sun.star.document.MediaDescriptor,
    specified on loading or saving with storeAsURL.
    */

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return uno::Sequence< beans::PropertyValue >(); //behave passive if already disposed or closed or throw exception @todo?
    //mutex is acquired

    return m_aMediaDescriptor;
}

void SAL_CALL ChartModel::connectController( const uno::Reference< frame::XController >& xController )
{
    //@todo? this method is declared as oneway -> ...?

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return ; //behave passive if already disposed or closed
    //mutex is acquired

    //--add controller
    m_aControllers.addInterface(xController);
}

void SAL_CALL ChartModel::disconnectController( const uno::Reference< frame::XController >& xController )
{
    //@todo? this method is declared as oneway -> ...?

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return; //behave passive if already disposed or closed

    //--remove controller
    m_aControllers.removeInterface(xController);

    //case: current controller is disconnected:
    if( m_xCurrentController == xController )
        m_xCurrentController.clear();

    if (m_xRangeHighlighter)
    {
        m_xRangeHighlighter->dispose();
        m_xRangeHighlighter.clear();
    }
    DisposeHelper::DisposeAndClear(m_xPopupRequest);
}

void SAL_CALL ChartModel::lockControllers()
{
    /*
    suspends some notifications to the controllers which are used for display updates.

    The calls to lockControllers() and unlockControllers() may be nested
    and even overlapping, but they must be in pairs. While there is at least one lock
    remaining, some notifications for display updates are not broadcasted.
    */

    //@todo? this method is declared as oneway -> ...?

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return; //behave passive if already disposed or closed or throw exception @todo?
    ++m_nControllerLockCount;
}

void SAL_CALL ChartModel::unlockControllers()
{
    /*
    resumes the notifications which were suspended by lockControllers() .

    The calls to lockControllers() and unlockControllers() may be nested
    and even overlapping, but they must be in pairs. While there is at least one lock
    remaining, some notifications for display updates are not broadcasted.
    */

    //@todo? this method is declared as oneway -> ...?

    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return; //behave passive if already disposed or closed or throw exception @todo?
    if( m_nControllerLockCount == 0 )
    {
        SAL_WARN("chart2",  "ChartModel: unlockControllers called with m_nControllerLockCount == 0" );
        return;
    }
    --m_nControllerLockCount;
    if( m_nControllerLockCount == 0 && m_bUpdateNotificationsPending  )
    {
        aGuard.clear();
        impl_notifyModifiedListeners();
    }
}

sal_Bool SAL_CALL ChartModel::hasControllersLocked()
{
    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        return false; //behave passive if already disposed or closed or throw exception @todo?
    return ( m_nControllerLockCount != 0 ) ;
}

uno::Reference< frame::XController > SAL_CALL ChartModel::getCurrentController()
{
    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        throw lang::DisposedException(
                u"getCurrentController was called on an already disposed or closed model"_ustr,
                static_cast< ::cppu::OWeakObject* >(this) );

    return impl_getCurrentController();
}

void SAL_CALL ChartModel::setCurrentController( const uno::Reference< frame::XController >& xController )
{
    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        throw lang::DisposedException(
                u"setCurrentController was called on an already disposed or closed model"_ustr,
                static_cast< ::cppu::OWeakObject* >(this) );

    //OSL_ENSURE( impl_isControllerConnected(xController), "setCurrentController is called with a Controller which is not connected" );
    if(!impl_isControllerConnected(xController))
        throw container::NoSuchElementException(
                u"setCurrentController is called with a Controller which is not connected"_ustr,
                static_cast< ::cppu::OWeakObject* >(this) );

    m_xCurrentController = xController;

    if (m_xRangeHighlighter)
    {
        m_xRangeHighlighter->dispose();
        m_xRangeHighlighter.clear();
    }
    DisposeHelper::DisposeAndClear(m_xPopupRequest);
}

uno::Reference< uno::XInterface > SAL_CALL ChartModel::getCurrentSelection()
{
    LifeTimeGuard aGuard(m_aLifeTimeManager);
    if(!aGuard.startApiCall())
        throw lang::DisposedException(
                u"getCurrentSelection was called on an already disposed or closed model"_ustr,
                static_cast< ::cppu::OWeakObject* >(this) );

    uno::Reference< uno::XInterface > xReturn;
    uno::Reference< frame::XController > xController = impl_getCurrentController();

    aGuard.clear();
    if( xController.is() )
    {
        uno::Reference< view::XSelectionSupplier >  xSelectionSupl( xController, uno::UNO_QUERY );
        if ( xSelectionSupl.is() )
        {
            uno::Any aSel = xSelectionSupl->getSelection();
            OUString aObjectCID;
            if( aSel >>= aObjectCID )
                xReturn.set( ObjectIdentifier::getObjectPropertySet( aObjectCID, this));
        }
    }
    return xReturn;
}

// lang::XComponent (base of XModel)
void SAL_CALL ChartModel::dispose()
{
    Reference< XInterface > xKeepAlive( *this );

    //This object should release all resources and references in the
    //easiest possible manner
    //This object must notify all registered listeners using the method
    //<member>XEventListener::disposing</member>

    //hold no mutex
    if( !m_aLifeTimeManager.dispose() )
        return;

    //--release all resources and references
    //// @todo

    if ( m_xDiagram.is() )
        m_xDiagram->removeModifyListener( this );

    if ( m_xDataProvider.is() )
    {
        Reference<util::XModifyBroadcaster> xModifyBroadcaster( m_xDataProvider, uno::UNO_QUERY );
        if ( xModifyBroadcaster.is() )
            xModifyBroadcaster->removeModifyListener( this );
    }

    m_xDataProvider.clear();
    m_xInternalDataProvider.clear();
    m_xNumberFormatsSupplier.clear();
    m_xOwnNumberFormatsSupplier.clear();
    m_xChartTypeManager.clear();
    m_xDiagram.clear();
    m_xTitle.clear();
    m_xPageBackground.clear();
    m_xXMLNamespaceMap.clear();

    m_xStorage.clear();
        // just clear, don't dispose - we're not the owner

    if ( m_pUndoManager.is() )
        m_pUndoManager->disposing();
    m_pUndoManager.clear();
        // that's important, since the UndoManager implementation delegates its ref counting to ourself.

    if( m_xOldModelAgg.is())  // #i120828#, to release cyclic reference to ChartModel object
        m_xOldModelAgg->setDelegator( nullptr );

    m_aControllers.disposeAndClear( lang::EventObject( static_cast< cppu::OWeakObject * >( this )));
    m_xCurrentController.clear();

    if (m_xRangeHighlighter)
    {
        m_xRangeHighlighter->dispose();
        m_xRangeHighlighter.clear();
    }
    DisposeHelper::DisposeAndClear(m_xPopupRequest);

    if( m_xOldModelAgg.is())
        m_xOldModelAgg->setDelegator( nullptr );
}

void SAL_CALL ChartModel::addEventListener( const uno::Reference< lang::XEventListener > & xListener )
{
    if( m_aLifeTimeManager.impl_isDisposedOrClosed() )
        return; //behave passive if already disposed or closed

    std::unique_lock aGuard(m_aLifeTimeManager.m_aAccessMutex);
    m_aLifeTimeManager.m_aEventListeners.addInterface( aGuard, xListener );
}

void SAL_CALL ChartModel::removeEventListener( const uno::Reference< lang::XEventListener > & xListener )
{
    if( m_aLifeTimeManager.impl_isDisposedOrClosed(false) )
        return; //behave passive if already disposed or closed

    std::unique_lock aGuard(m_aLifeTimeManager.m_aAccessMutex);
    m_aLifeTimeManager.m_aEventListeners.removeInterface( aGuard, xListener );
}

// util::XCloseBroadcaster (base of XCloseable)
void SAL_CALL ChartModel::addCloseListener( const uno::Reference<   util::XCloseListener > & xListener )
{
    m_aLifeTimeManager.g_addCloseListener( xListener );
}

void SAL_CALL ChartModel::removeCloseListener( const uno::Reference< util::XCloseListener > & xListener )
{
    if( m_aLifeTimeManager.impl_isDisposedOrClosed(false) )
        return; //behave passive if already disposed or closed

    std::unique_lock aGuard(m_aLifeTimeManager.m_aAccessMutex);
    m_aLifeTimeManager.m_aCloseListeners.removeInterface( aGuard, xListener );
}

// util::XCloseable
void SAL_CALL ChartModel::close( sal_Bool bDeliverOwnership )
{
    //hold no mutex

    if( !m_aLifeTimeManager.g_close_startTryClose( bDeliverOwnership ) )
        return;
    //no mutex is acquired

    // At the end of this method may we must dispose ourself ...
    // and may nobody from outside hold a reference to us ...
    // then it's a good idea to do that by ourself.
    uno::Reference< uno::XInterface > xSelfHold( static_cast< ::cppu::OWeakObject* >(this) );

    //the listeners have had no veto
    //check whether we self can close
    {
        util::CloseVetoException aVetoException(
                        u"the model itself could not be closed"_ustr,
                        static_cast< ::cppu::OWeakObject* >(this) );

        m_aLifeTimeManager.g_close_isNeedToCancelLongLastingCalls( bDeliverOwnership, aVetoException );
    }
    m_aLifeTimeManager.g_close_endTryClose_doClose();

    // BM @todo: is it ok to call the listeners here?
    impl_notifyCloseListeners();
}

// lang::XTypeProvider
uno::Sequence< uno::Type > SAL_CALL ChartModel::getTypes()
{
    uno::Reference< lang::XTypeProvider > xAggTypeProvider;
    if( (m_xOldModelAgg->queryAggregation( cppu::UnoType<decltype(xAggTypeProvider)>::get()) >>= xAggTypeProvider)
        && xAggTypeProvider.is())
    {
        return comphelper::concatSequences(
            impl::ChartModel_Base::getTypes(),
            xAggTypeProvider->getTypes());
    }

    return impl::ChartModel_Base::getTypes();
}

// document::XDocumentPropertiesSupplier
uno::Reference< document::XDocumentProperties > SAL_CALL
        ChartModel::getDocumentProperties()
{
    ::osl::MutexGuard aGuard( m_aModelMutex );
    if ( !m_xDocumentProperties.is() )
    {
        m_xDocumentProperties.set( document::DocumentProperties::create( ::comphelper::getProcessComponentContext() ) );
    }
    return m_xDocumentProperties;
}

// document::XDocumentPropertiesSupplier
Reference< document::XUndoManager > SAL_CALL ChartModel::getUndoManager(  )
{
    ::osl::MutexGuard aGuard( m_aModelMutex );
    if ( !m_pUndoManager.is() )
        m_pUndoManager.set( new UndoManager( *this, m_aModelMutex ) );
    return m_pUndoManager;
}

// chart2::XChartDocument

uno::Reference< chart2::XDiagram > SAL_CALL ChartModel::getFirstDiagram()
{
    MutexGuard aGuard( m_aModelMutex );
    return m_xDiagram;
}

void SAL_CALL ChartModel::setFirstDiagram( const uno::Reference< chart2::XDiagram >& xDiagram )
{
    rtl::Reference< ::chart::Diagram > xOldDiagram;
    Reference< util::XModifyListener > xListener;
    {
        MutexGuard aGuard( m_aModelMutex );
        if( xDiagram.get() == m_xDiagram.get() )
            return;
        xOldDiagram = m_xDiagram;
        assert(!xDiagram || dynamic_cast<::chart::Diagram*>(xDiagram.get()));
        m_xDiagram = dynamic_cast<::chart::Diagram*>(xDiagram.get());
        xListener = this;
    }
    //don't keep the mutex locked while calling out
    if( xOldDiagram && xListener )
        xOldDiagram->removeModifyListener( xListener );
    ModifyListenerHelper::addListener( xDiagram, xListener );
    setModified( true );
}

Reference< chart2::data::XDataSource > ChartModel::impl_createDefaultData()
{
    Reference< chart2::data::XDataSource > xDataSource;
    if( hasInternalDataProvider() )
    {
        //init internal dataprovider
        {
            beans::NamedValue aParam( u"CreateDefaultData"_ustr ,uno::Any(true) );
            uno::Sequence< uno::Any > aArgs{ uno::Any(aParam) };
            m_xInternalDataProvider->initialize(aArgs);
        }
        //create data
        uno::Sequence<beans::PropertyValue> aArgs( comphelper::InitPropertySequence({
            { "CellRangeRepresentation", uno::Any( u"all"_ustr ) },
            { "HasCategories", uno::Any( true ) },
            { "FirstCellAsLabel", uno::Any( true ) },
            { "DataRowSource", uno::Any( css::chart::ChartDataRowSource_COLUMNS ) }
            }));
        xDataSource = m_xInternalDataProvider->createDataSource( aArgs );
    }
    return xDataSource;
}

void SAL_CALL ChartModel::createInternalDataProvider( sal_Bool bCloneExistingData )
{
    // don't lock the mutex, because this call calls out to code that tries to
    // lock the solar mutex. On the other hand, a paint locks the solar mutex
    // and calls to the model lock the model's mutex => deadlock
    // @todo: lock a separate mutex in the InternalData class
    if( !hasInternalDataProvider() )
    {
        if( bCloneExistingData )
            m_xInternalDataProvider = new InternalDataProvider( this, /*bConnectToModel*/true, /*bDefaultDataInColumns*/ true );
        else
        {
            m_xInternalDataProvider = new InternalDataProvider( nullptr, /*bConnectToModel*/true, /*bDefaultDataInColumns*/ true );
            m_xInternalDataProvider->setChartModel(this);
        }
        m_xDataProvider.set( m_xInternalDataProvider );
    }
    setModified( true );
}

void ChartModel::removeDataProviders()
{
    if (m_xInternalDataProvider.is())
        m_xInternalDataProvider.clear();
    if (m_xDataProvider.is())
        m_xDataProvider.clear();
}

void ChartModel::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("ChartModel"));
    (void)xmlTextWriterWriteFormatAttribute(pWriter, BAD_CAST("ptr"), "%p", this);

    if (mxChartView.is())
    {
        mxChartView->dumpAsXml(pWriter);
    }

    (void)xmlTextWriterEndElement(pWriter);
}

sal_Bool SAL_CALL ChartModel::hasInternalDataProvider()
{
    return m_xDataProvider.is() && m_xInternalDataProvider.is();
}

uno::Reference< chart2::data::XDataProvider > SAL_CALL ChartModel::getDataProvider()
{
    MutexGuard aGuard( m_aModelMutex );
    return m_xDataProvider;
}

// ____ XDataReceiver ____

void SAL_CALL ChartModel::attachDataProvider( const uno::Reference< chart2::data::XDataProvider >& xDataProvider )
{
    {
        MutexGuard aGuard( m_aModelMutex );
        uno::Reference< beans::XPropertySet > xProp( xDataProvider, uno::UNO_QUERY );
        if( xProp.is() )
        {
            try
            {
                bool bIncludeHiddenCells = isIncludeHiddenCells();
                xProp->setPropertyValue(u"IncludeHiddenCells"_ustr, uno::Any(bIncludeHiddenCells));
            }
            catch (const beans::UnknownPropertyException&)
            {
            }
        }

        uno::Reference<util::XModifyBroadcaster> xModifyBroadcaster(xDataProvider, uno::UNO_QUERY);
        if (xModifyBroadcaster.is())
        {
            xModifyBroadcaster->addModifyListener(this);
        }

        m_xDataProvider.set( xDataProvider );
        m_xInternalDataProvider.clear();

        //the numberformatter is kept independent of the data provider!
    }
    setModified( true );
}

void SAL_CALL ChartModel::attachNumberFormatsSupplier( const uno::Reference< util::XNumberFormatsSupplier >& xNewSupplier )
{
    {
        // Mostly the supplier is SvNumberFormatsSupplierObj, but sometimes it is reportdesign::OReportDefinition
        MutexGuard aGuard( m_aModelMutex );
        if( xNewSupplier == m_xNumberFormatsSupplier )
            return;
        if( xNewSupplier == uno::Reference<XNumberFormatsSupplier>(m_xOwnNumberFormatsSupplier) )
            return;
        if( m_xOwnNumberFormatsSupplier.is() && xNewSupplier.is() )
        {
            //@todo
            //merge missing numberformats from own to new formatter
        }
        else if( !xNewSupplier.is() )
        {
            if( m_xNumberFormatsSupplier.is() )
            {
                //@todo
                //merge missing numberformats from old numberformatter to own numberformatter
                //create own numberformatter if necessary
            }
        }

        m_xNumberFormatsSupplier.set( xNewSupplier );
        m_xOwnNumberFormatsSupplier.clear();
    }
    setModified( true );
}

void SAL_CALL ChartModel::setArguments( const Sequence< beans::PropertyValue >& aArguments )
{
    {
        MutexGuard aGuard( m_aModelMutex );
        if( !m_xDataProvider.is() )
            return;
        lockControllers();

        try
        {
            Reference< chart2::data::XDataSource > xDataSource( m_xDataProvider->createDataSource( aArguments ) );
            if( xDataSource.is() )
            {
                rtl::Reference< Diagram > xDia = getFirstChartDiagram();
                if( !xDia.is() )
                {
                    rtl::Reference< ::chart::ChartTypeTemplate > xTemplate( impl_createDefaultChartTypeTemplate() );
                    if( xTemplate.is())
                        setFirstDiagram( xTemplate->createDiagramByDataSource( xDataSource, aArguments ) );
                }
                else
                    xDia->setDiagramData( xDataSource, aArguments );
            }
        }
        catch (const lang::IllegalArgumentException&)
        {
            throw;
        }
        catch (const uno::Exception&)
        {
            DBG_UNHANDLED_EXCEPTION("chart2");
        }
        unlockControllers();
    }
    setModified( true );
}

Sequence< OUString > SAL_CALL ChartModel::getUsedRangeRepresentations()
{
    return comphelper::containerToSequence(DataSourceHelper::getUsedDataRanges( this ));
}

Reference< chart2::data::XDataSource > SAL_CALL ChartModel::getUsedData()
{
    return DataSourceHelper::getUsedData( *this );
}

Reference< chart2::data::XRangeHighlighter > SAL_CALL ChartModel::getRangeHighlighter()
{
    if( ! m_xRangeHighlighter.is())
        m_xRangeHighlighter = new RangeHighlighter( this );
    return m_xRangeHighlighter;
}

Reference<awt::XRequestCallback> SAL_CALL ChartModel::getPopupRequest()
{
    if (!m_xPopupRequest.is())
        m_xPopupRequest.set(new PopupRequest);
    return m_xPopupRequest;
}

rtl::Reference< ::chart::ChartTypeTemplate > ChartModel::impl_createDefaultChartTypeTemplate()
{
    rtl::Reference< ::chart::ChartTypeTemplate > xTemplate;
    if( m_xChartTypeManager.is() )
        xTemplate = m_xChartTypeManager->createTemplate( u"com.sun.star.chart2.template.Column"_ustr );
    return xTemplate;
}

void SAL_CALL ChartModel::setChartTypeManager( const uno::Reference< chart2::XChartTypeManager >& xNewManager )
{
    {
        MutexGuard aGuard( m_aModelMutex );
        m_xChartTypeManager = dynamic_cast<::chart::ChartTypeManager*>(xNewManager.get());
        assert(!xNewManager || m_xChartTypeManager);
    }
    setModified( true );
}

uno::Reference< chart2::XChartTypeManager > SAL_CALL ChartModel::getChartTypeManager()
{
    MutexGuard aGuard( m_aModelMutex );
    return m_xChartTypeManager;
}

uno::Reference< beans::XPropertySet > SAL_CALL ChartModel::getPageBackground()
{
    MutexGuard aGuard( m_aModelMutex );
    return m_xPageBackground;
}

void SAL_CALL ChartModel::createDefaultChart()
{
    insertDefaultChart();
}

// ____ XTitled ____
uno::Reference< chart2::XTitle > SAL_CALL ChartModel::getTitleObject()
{
    MutexGuard aGuard( m_aModelMutex );
    return m_xTitle;
}

rtl::Reference< Title > ChartModel::getTitleObject2() const
{
    MutexGuard aGuard( m_aModelMutex );
    return m_xTitle;
}

void SAL_CALL ChartModel::setTitleObject( const uno::Reference< chart2::XTitle >& xNewTitle )
{
    rtl::Reference<Title> xTitle = dynamic_cast<Title*>(xNewTitle.get());
    assert(!xNewTitle || xTitle);
    setTitleObject(xTitle);
}

void ChartModel::setTitleObject( const rtl::Reference< Title >& xTitle )
{
    {
        MutexGuard aGuard( m_aModelMutex );
        if( m_xTitle.is() )
            ModifyListenerHelper::removeListener( m_xTitle, this );
        m_xTitle = xTitle;
        ModifyListenerHelper::addListener( m_xTitle, this );
    }
    setModified( true );
}

// ____ XInterface (for old API wrapper) ____
uno::Any SAL_CALL ChartModel::queryInterface( const uno::Type& aType )
{
    uno::Any aResult( impl::ChartModel_Base::queryInterface( aType ));

    if( ! aResult.hasValue())
    {
        // try old API wrapper
        try
        {
            if( m_xOldModelAgg.is())
                aResult = m_xOldModelAgg->queryAggregation( aType );
        }
        catch (const uno::Exception&)
        {
            DBG_UNHANDLED_EXCEPTION("chart2");
        }
    }

    return aResult;
}

// ____ XCloneable ____
Reference< util::XCloneable > SAL_CALL ChartModel::createClone()
{
    std::unique_lock aGuard(m_aLifeTimeManager.m_aAccessMutex);
    return Reference< util::XCloneable >( new ChartModel( *this ));
}

// ____ XVisualObject ____
void SAL_CALL ChartModel::setVisualAreaSize( ::sal_Int64 nAspect, const awt::Size& aSize )
{
    if( nAspect == embed::Aspects::MSOLE_CONTENT )
    {
        ControllerLockGuard aLockGuard( *this );
        bool bChanged =
            (m_aVisualAreaSize.Width != aSize.Width ||
             m_aVisualAreaSize.Height != aSize.Height);

        // #i12587# support for shapes in chart
        if ( bChanged )
        {
            impl_adjustAdditionalShapesPositionAndSize( aSize );
        }

        m_aVisualAreaSize = aSize;
        if( bChanged )
            setModified( true );
    }
    else
    {
        OSL_FAIL( "setVisualAreaSize: Aspect not implemented yet.");
    }
}

awt::Size SAL_CALL ChartModel::getVisualAreaSize( ::sal_Int64 nAspect )
{
    OSL_ENSURE( nAspect == embed::Aspects::MSOLE_CONTENT,
                "No aspects other than content are supported" );
    // other possible aspects are MSOLE_THUMBNAIL, MSOLE_ICON and MSOLE_DOCPRINT

    return m_aVisualAreaSize;
}

embed::VisualRepresentation SAL_CALL ChartModel::getPreferredVisualRepresentation( ::sal_Int64 nAspect )
{
    OSL_ENSURE( nAspect == embed::Aspects::MSOLE_CONTENT,
                "No aspects other than content are supported" );

    embed::VisualRepresentation aResult;

    try
    {
        Sequence< sal_Int8 > aMetafile;

        //get view from old api wrapper
        Reference< datatransfer::XTransferable > xTransferable( createChartView() );
        if( xTransferable.is() )
        {
            datatransfer::DataFlavor aDataFlavor( lcl_aGDIMetaFileMIMEType,
                    u"GDIMetaFile"_ustr,
                    cppu::UnoType<uno::Sequence< sal_Int8 >>::get() );

            uno::Any aData( xTransferable->getTransferData( aDataFlavor ) );
            aData >>= aMetafile;
        }

        aResult.Flavor.MimeType = lcl_aGDIMetaFileMIMEType;
        aResult.Flavor.DataType = cppu::UnoType<decltype(aMetafile)>::get();

        aResult.Data <<= aMetafile;
    }
    catch (const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("chart2");
    }

    return aResult;
}

::sal_Int32 SAL_CALL ChartModel::getMapUnit( ::sal_Int64 nAspect )
{
    OSL_ENSURE( nAspect == embed::Aspects::MSOLE_CONTENT,
                "No aspects other than content are supported" );
    return embed::EmbedMapUnits::ONE_100TH_MM;
}

// ____ datatransfer::XTransferable ____
uno::Any SAL_CALL ChartModel::getTransferData( const datatransfer::DataFlavor& aFlavor )
{
    uno::Any aResult;
    if( !isDataFlavorSupported( aFlavor ) )
        throw datatransfer::UnsupportedFlavorException(
            aFlavor.MimeType, static_cast< ::cppu::OWeakObject* >( this ));

    try
    {
        //get view from old api wrapper
        Reference< datatransfer::XTransferable > xTransferable( createChartView() );
        if( xTransferable.is() &&
            xTransferable->isDataFlavorSupported( aFlavor ))
        {
            aResult = xTransferable->getTransferData( aFlavor );
        }
    }
    catch (const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION("chart2");
    }

    return aResult;
}

Sequence< datatransfer::DataFlavor > SAL_CALL ChartModel::getTransferDataFlavors()
{
    return { datatransfer::DataFlavor( lcl_aGDIMetaFileMIMETypeHighContrast,
        u"GDIMetaFile"_ustr,
        cppu::UnoType<uno::Sequence< sal_Int8 >>::get() ) };
}

sal_Bool SAL_CALL ChartModel::isDataFlavorSupported( const datatransfer::DataFlavor& aFlavor )
{
    return aFlavor.MimeType == lcl_aGDIMetaFileMIMETypeHighContrast;
}

namespace
{
enum eServiceType
{
    SERVICE_DASH_TABLE,
    SERVICE_GRADIENT_TABLE,
    SERVICE_HATCH_TABLE,
    SERVICE_BITMAP_TABLE,
    SERVICE_TRANSP_GRADIENT_TABLE,
    SERVICE_MARKER_TABLE,
    SERVICE_NAMESPACE_MAP
};

typedef std::map< OUString, enum eServiceType > tServiceNameMap;

tServiceNameMap & lcl_getStaticServiceNameMap()
{
    static tServiceNameMap aServiceNameMap{
        {"com.sun.star.drawing.DashTable",                    SERVICE_DASH_TABLE},
        {"com.sun.star.drawing.GradientTable",                SERVICE_GRADIENT_TABLE},
        {"com.sun.star.drawing.HatchTable",                   SERVICE_HATCH_TABLE},
        {"com.sun.star.drawing.BitmapTable",                  SERVICE_BITMAP_TABLE},
        {"com.sun.star.drawing.TransparencyGradientTable",    SERVICE_TRANSP_GRADIENT_TABLE},
        {"com.sun.star.drawing.MarkerTable",                  SERVICE_MARKER_TABLE},
        {"com.sun.star.xml.NamespaceMap",                     SERVICE_NAMESPACE_MAP}};
    return aServiceNameMap;
}
}
// ____ XMultiServiceFactory ____
Reference< uno::XInterface > SAL_CALL ChartModel::createInstance( const OUString& rServiceSpecifier )
{
    tServiceNameMap & rMap = lcl_getStaticServiceNameMap();

    tServiceNameMap::const_iterator aIt( rMap.find( rServiceSpecifier ));
    if( aIt != rMap.end())
    {
        switch( (*aIt).second )
        {
            case SERVICE_DASH_TABLE:
            case SERVICE_GRADIENT_TABLE:
            case SERVICE_HATCH_TABLE:
            case SERVICE_BITMAP_TABLE:
            case SERVICE_TRANSP_GRADIENT_TABLE:
            case SERVICE_MARKER_TABLE:
                {
                    if(!mxChartView.is())
                    {
                        mxChartView = new ChartView( m_xContext, *this);
                    }
                    return mxChartView->createInstance( rServiceSpecifier );
                }
                break;
            case SERVICE_NAMESPACE_MAP:
                return static_cast<cppu::OWeakObject*>(m_xXMLNamespaceMap.get());
        }
    }
    else if(rServiceSpecifier == CHART_VIEW_SERVICE_NAME)
    {
        return static_cast< ::cppu::OWeakObject* >( createChartView().get() );
    }
    else
    {
        if( m_xOldModelAgg.is() )
        {
            Any aAny = m_xOldModelAgg->queryAggregation( cppu::UnoType<lang::XMultiServiceFactory>::get());
            uno::Reference< lang::XMultiServiceFactory > xOldModelFactory;
            if( (aAny >>= xOldModelFactory) && xOldModelFactory.is() )
            {
                return xOldModelFactory->createInstance( rServiceSpecifier );
            }
        }
    }
    return nullptr;
}

const rtl::Reference<ChartView>& ChartModel::createChartView()
{
    if(!mxChartView.is())
        mxChartView = new ChartView( m_xContext, *this);
    return mxChartView;
}

Reference< uno::XInterface > SAL_CALL ChartModel::createInstanceWithArguments(
            const OUString& rServiceSpecifier , const Sequence< Any >& Arguments )
{
    OSL_ENSURE( Arguments.hasElements(), "createInstanceWithArguments: Warning: Arguments are ignored" );
    return createInstance( rServiceSpecifier );
}

Sequence< OUString > SAL_CALL ChartModel::getAvailableServiceNames()
{
    uno::Sequence< OUString > aResult;

    if( m_xOldModelAgg.is())
    {
        Any aAny = m_xOldModelAgg->queryAggregation( cppu::UnoType<lang::XMultiServiceFactory>::get());
        uno::Reference< lang::XMultiServiceFactory > xOldModelFactory;
        if( (aAny >>= xOldModelFactory) && xOldModelFactory.is() )
        {
            return xOldModelFactory->getAvailableServiceNames();
        }
    }
    return aResult;
}

Reference< util::XNumberFormatsSupplier > const & ChartModel::getNumberFormatsSupplier()
{
    if( !m_xNumberFormatsSupplier.is() )
    {
        if( !m_xOwnNumberFormatsSupplier.is() )
        {
            m_apSvNumberFormatter.reset( new SvNumberFormatter( m_xContext, LANGUAGE_SYSTEM ) );
            if (m_aNullDate)
            {
                m_apSvNumberFormatter->ChangeNullDate(m_aNullDate->Day, m_aNullDate->Month, m_aNullDate->Year);
            }
            m_xOwnNumberFormatsSupplier = new SvNumberFormatsSupplierObj( m_apSvNumberFormatter.get() );
            //pOwnNumberFormatter->ChangeStandardPrec( 15 ); todo?
        }
        m_xNumberFormatsSupplier = m_xOwnNumberFormatsSupplier;
    }
    return m_xNumberFormatsSupplier;
}

// ____ XUnoTunnel ___
::sal_Int64 SAL_CALL ChartModel::getSomething( const Sequence< ::sal_Int8 >& aIdentifier )
{
    if( comphelper::isUnoTunnelId<SvNumberFormatsSupplierObj>(aIdentifier) )
    {
        Reference< lang::XUnoTunnel > xTunnel( getNumberFormatsSupplier(), uno::UNO_QUERY );
        if( xTunnel.is() )
            return xTunnel->getSomething( aIdentifier );
    }
    return 0;
}

// ____ XNumberFormatsSupplier ____
uno::Reference< beans::XPropertySet > SAL_CALL ChartModel::getNumberFormatSettings()
{
    Reference< util::XNumberFormatsSupplier > xSupplier( getNumberFormatsSupplier() );
    if( xSupplier.is() )
        return xSupplier->getNumberFormatSettings();
    return uno::Reference< beans::XPropertySet >();
}

uno::Reference< util::XNumberFormats > SAL_CALL ChartModel::getNumberFormats()
{
    Reference< util::XNumberFormatsSupplier > xSupplier( getNumberFormatsSupplier() );
    if( xSupplier.is() )
        return xSupplier->getNumberFormats();
    return uno::Reference< util::XNumberFormats >();
}

// ____ XChild ____
Reference< uno::XInterface > SAL_CALL ChartModel::getParent()
{
    return Reference< uno::XInterface >(m_xParent,uno::UNO_QUERY);
}

void SAL_CALL ChartModel::setParent( const Reference< uno::XInterface >& Parent )
{
    if( Parent != m_xParent )
    {
        if (SfxObjectShell* pShell = getParentShell(m_xParent))
            EndListening(*pShell);
        m_xParent.set( Parent, uno::UNO_QUERY );
        if (SfxObjectShell* pShell = getParentShell(m_xParent))
            StartListening(*pShell);
    }
}

// ____ XDataSource ____
uno::Sequence< Reference< chart2::data::XLabeledDataSequence > > SAL_CALL ChartModel::getDataSequences()
{
    rtl::Reference< DataSource > xSource = DataSourceHelper::getUsedData( *this );
    if( xSource.is())
        return xSource->getDataSequences();

    return uno::Sequence< Reference< chart2::data::XLabeledDataSequence > >();
}

//XDumper
OUString SAL_CALL ChartModel::dump(OUString const & kind)
{
    if (kind.isEmpty()) {
        return comphelper::dumpXmlToString([this](auto writer) { return dumpAsXml(writer); });
    }

    // kind == "shapes":
    uno::Reference< qa::XDumper > xDumper( createChartView() );
    if (xDumper.is())
        return xDumper->dump(kind);

    return OUString();
}

void ChartModel::setTimeBasedRange(sal_Int32 nStart, sal_Int32 nEnd)
{
    mnStart = nStart;
    mnEnd = nEnd;
    mbTimeBased = true;
}

void ChartModel::update()
{
    if(!mxChartView.is())
    {
        mxChartView = new ChartView( m_xContext, *this);
    }
    mxChartView->setViewDirty();
    mxChartView->update();
}

bool ChartModel::isDataFromSpreadsheet()
{
    return !isDataFromPivotTable() && !hasInternalDataProvider();
}

bool ChartModel::isDataFromPivotTable() const
{
    uno::Reference<chart2::data::XPivotTableDataProvider> xPivotTableDataProvider(m_xDataProvider, uno::UNO_QUERY);
    return xPivotTableDataProvider.is();
}

rtl::Reference< BaseCoordinateSystem > ChartModel::getFirstCoordinateSystem()
{
    if( m_xDiagram )
    {
        auto aCooSysSeq( m_xDiagram->getBaseCoordinateSystems() );
        if( !aCooSysSeq.empty() )
            return aCooSysSeq[0];
    }
    return nullptr;
}

std::vector< rtl::Reference< DataSeries > > ChartModel::getDataSeries()
{
    if( m_xDiagram)
        return m_xDiagram->getDataSeries();

    return {};
}

rtl::Reference< ChartType > ChartModel::getChartTypeOfSeries( const rtl::Reference< DataSeries >& xGivenDataSeries )
{
    return m_xDiagram ? m_xDiagram->getChartTypeOfSeries( xGivenDataSeries ) : nullptr;
}

// static
awt::Size ChartModel::getDefaultPageSize()
{
    return awt::Size( 16000, 9000 );
}

awt::Size ChartModel::getPageSize()
{
    return getVisualAreaSize( embed::Aspects::MSOLE_CONTENT );
}

void ChartModel::triggerRangeHighlighting()
{
    getRangeHighlighter();
    uno::Reference< view::XSelectionChangeListener > xSelectionChangeListener( m_xRangeHighlighter );
    //trigger selection of cell range
    lang::EventObject aEvent( xSelectionChangeListener );
    xSelectionChangeListener->selectionChanged( aEvent );
}

bool ChartModel::isIncludeHiddenCells()
{
    bool bIncluded = true;  // hidden cells are included by default.

    if (!m_xDiagram)
        return bIncluded;

    try
    {
        m_xDiagram->getPropertyValue(u"IncludeHiddenCells"_ustr) >>= bIncluded;
    }
    catch( const beans::UnknownPropertyException& )
    {
    }

    return bIncluded;
}

bool ChartModel::setIncludeHiddenCells( bool bIncludeHiddenCells )
{
    bool bChanged = false;
    try
    {
        ControllerLockGuard aLockedControllers( *this );

        uno::Reference< beans::XPropertySet > xDiagramProperties( getFirstDiagram(), uno::UNO_QUERY );
        if (!xDiagramProperties)
            return false;

        bool bOldValue = bIncludeHiddenCells;
        xDiagramProperties->getPropertyValue( u"IncludeHiddenCells"_ustr ) >>= bOldValue;
        if( bOldValue == bIncludeHiddenCells )
            bChanged = true;

        //set the property on all instances in all cases to get the different objects in sync!

        uno::Any aNewValue(bIncludeHiddenCells);

        try
        {
            uno::Reference< beans::XPropertySet > xDataProviderProperties( getDataProvider(), uno::UNO_QUERY );
            if( xDataProviderProperties.is() )
                xDataProviderProperties->setPropertyValue(u"IncludeHiddenCells"_ustr, aNewValue );
        }
        catch( const beans::UnknownPropertyException& )
        {
            //the property is optional!
        }

        try
        {
            rtl::Reference< DataSource > xUsedData = DataSourceHelper::getUsedData( *this );
            if( xUsedData.is() )
            {
                uno::Reference< beans::XPropertySet > xProp;
                const uno::Sequence< uno::Reference< chart2::data::XLabeledDataSequence > > aData( xUsedData->getDataSequences());
                for( uno::Reference< chart2::data::XLabeledDataSequence > const & labeledData : aData )
                {
                    xProp.set( uno::Reference< beans::XPropertySet >( labeledData->getValues(), uno::UNO_QUERY ) );
                    if(xProp.is())
                        xProp->setPropertyValue(u"IncludeHiddenCells"_ustr, aNewValue );
                    xProp.set( uno::Reference< beans::XPropertySet >( labeledData->getLabel(), uno::UNO_QUERY ) );
                    if(xProp.is())
                        xProp->setPropertyValue(u"IncludeHiddenCells"_ustr, aNewValue );
                }
            }
        }
        catch( const beans::UnknownPropertyException& )
        {
            //the property is optional!
        }

        xDiagramProperties->setPropertyValue( u"IncludeHiddenCells"_ustr, aNewValue);
    }
    catch (const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("chart2", "" );
    }
    return bChanged;
}

void ChartModel::Notify(SfxBroadcaster& /*rBC*/, const SfxHint& rHint)
{
    if (rHint.GetId() == SfxHintId::ThemeColorsChanged)
    {
        onDocumentThemeChanged();
    }
}

std::shared_ptr<model::Theme> ChartModel::getDocumentTheme() const
{
    std::shared_ptr<model::Theme> pTheme;
    uno::Any aThemeValue;

    auto pParent = const_cast<ChartModel*>(this)->getParent();
    uno::Reference<frame::XModel> xDocModel(pParent, uno::UNO_QUERY);
    uno::Reference<text::XTextDocument> xTextDoc(xDocModel, uno::UNO_QUERY);

    if (!xTextDoc.is()) // Calc, Impress
    {
        uno::Reference<beans::XPropertySet> xPropSet(xDocModel, uno::UNO_QUERY);
        if (xPropSet.is())
        {
            aThemeValue = xPropSet->getPropertyValue("Theme");
        }
    }
    else // Writer
    {
        uno::Reference<drawing::XDrawPageSupplier> xDrawPageSupplier(xDocModel, uno::UNO_QUERY);
        if (xDrawPageSupplier.is())
        {
            uno::Reference<drawing::XDrawPage> xDrawPage = xDrawPageSupplier->getDrawPage();
            if (xDrawPage.is())
            {
                uno::Reference<beans::XPropertySet> xPropSet(xDrawPage, uno::UNO_QUERY);
                if (xPropSet.is())
                {
                    aThemeValue = xPropSet->getPropertyValue("Theme");
                }
            }
        }
    }

    uno::Reference<util::XTheme> xTheme(aThemeValue, uno::UNO_QUERY);
    if (xTheme.is())
    {
        if (auto* pUnoTheme = dynamic_cast<UnoTheme*>(xTheme.get()))
        {
            pTheme = pUnoTheme->getTheme();
        }
    }
    else
    {
        pTheme = model::Theme::FromAny(aThemeValue);
    }

    return pTheme;
}

void ChartModel::setColorPalette(ChartColorPaletteType eType, sal_uInt32 nIndex)
{
    m_eColorPaletteType = eType;
    m_nColorPaletteIndex = nIndex;
}

void ChartModel::clearColorPalette() { setColorPalette(ChartColorPaletteType::Unknown, 0); }

bool ChartModel::usesColorPalette() const
{
    return m_eColorPaletteType != ChartColorPaletteType::Unknown;
}

std::optional<ChartColorPalette> ChartModel::getCurrentColorPalette() const
{
    if (!usesColorPalette())
    {
        SAL_WARN("chart2", "ChartModel::getCurrentColorPalette: no palette is in use");
        return std::nullopt;
    }

    const std::shared_ptr<model::Theme> pTheme = getDocumentTheme();
    // when pTheme is null, ChartColorPaletteHelper uses a default theme
    const ChartColorPaletteHelper aColorPaletteHelper(pTheme);
    return aColorPaletteHelper.getColorPalette(getColorPaletteType(), getColorPaletteIndex());
}

void ChartModel::applyColorPaletteToDataSeries(const ChartColorPalette& rColorPalette)
{
    const rtl::Reference<Diagram> xDiagram = getFirstChartDiagram();
    const auto xDataSeriesArray = xDiagram->getDataSeries();
    for (size_t i = 0; i < xDataSeriesArray.size(); ++i)
    {
        const uno::Reference<beans::XPropertySet> xPropSet = xDataSeriesArray[i];
        const size_t nPaletteIndex = i % rColorPalette.size();
        xPropSet->setPropertyValue("FillStyle", uno::Any(drawing::FillStyle_SOLID));
        xPropSet->setPropertyValue("FillColor", uno::Any(rColorPalette[nPaletteIndex]));
    }
}

void ChartModel::onDocumentThemeChanged()
{
    if (const auto oColorPalette = getCurrentColorPalette())
    {
        applyColorPaletteToDataSeries(*oColorPalette);
        setModified(true);
    }
}

void ChartModel::changeNullDate(const css::util::DateTime& aNullDate)
{
    if (m_aNullDate == aNullDate)
        return;

    m_aNullDate = aNullDate;
    if (m_apSvNumberFormatter)
    {
        m_apSvNumberFormatter->ChangeNullDate(aNullDate.Day, aNullDate.Month, aNullDate.Year);
    }
}

std::optional<css::util::DateTime> ChartModel::getNullDate() const
{
    return m_aNullDate;
}

}  // namespace chart

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface *
com_sun_star_comp_chart2_ChartModel_get_implementation(css::uno::XComponentContext *context,
        css::uno::Sequence<css::uno::Any> const &)
{
    return cppu::acquire(new ::chart::ChartModel(context));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
