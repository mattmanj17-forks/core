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

#include <com/sun/star/awt/grid/XMutableGridDataModel.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <comphelper/compbase.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <o3tl/safeint.hxx>
#include <osl/diagnose.h>

#include <algorithm>
#include <vector>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::awt;
using namespace ::com::sun::star::awt::grid;
using namespace ::com::sun::star::lang;

namespace {

typedef ::comphelper::WeakComponentImplHelper    <   XMutableGridDataModel
                                            ,   XServiceInfo
                                            >   DefaultGridDataModel_Base;

class DefaultGridDataModel: public DefaultGridDataModel_Base
{
public:
    DefaultGridDataModel();
    DefaultGridDataModel( DefaultGridDataModel const & i_copySource );

    // XMutableGridDataModel
    virtual void SAL_CALL addRow( const Any& i_heading, const css::uno::Sequence< css::uno::Any >& Data ) override;
    virtual void SAL_CALL addRows( const css::uno::Sequence< css::uno::Any>& Headings, const css::uno::Sequence< css::uno::Sequence< css::uno::Any > >& Data ) override;
    virtual void SAL_CALL insertRow( ::sal_Int32 i_index, const css::uno::Any& i_heading, const css::uno::Sequence< css::uno::Any >& Data ) override;
    virtual void SAL_CALL insertRows( ::sal_Int32 i_index, const css::uno::Sequence< css::uno::Any>& Headings, const css::uno::Sequence< css::uno::Sequence< css::uno::Any > >& Data ) override;
    virtual void SAL_CALL removeRow( ::sal_Int32 RowIndex ) override;
    virtual void SAL_CALL removeAllRows(  ) override;
    virtual void SAL_CALL updateCellData( ::sal_Int32 ColumnIndex, ::sal_Int32 RowIndex, const css::uno::Any& Value ) override;
    virtual void SAL_CALL updateRowData( const css::uno::Sequence< ::sal_Int32 >& ColumnIndexes, ::sal_Int32 RowIndex, const css::uno::Sequence< css::uno::Any >& Values ) override;
    virtual void SAL_CALL updateRowHeading( ::sal_Int32 RowIndex, const css::uno::Any& Heading ) override;
    virtual void SAL_CALL updateCellToolTip( ::sal_Int32 ColumnIndex, ::sal_Int32 RowIndex, const css::uno::Any& Value ) override;
    virtual void SAL_CALL updateRowToolTip( ::sal_Int32 RowIndex, const css::uno::Any& Value ) override;
    virtual void SAL_CALL addGridDataListener( const css::uno::Reference< css::awt::grid::XGridDataListener >& Listener ) override;
    virtual void SAL_CALL removeGridDataListener( const css::uno::Reference< css::awt::grid::XGridDataListener >& Listener ) override;

    // XGridDataModel
    virtual ::sal_Int32 SAL_CALL getRowCount() override;
    virtual ::sal_Int32 SAL_CALL getColumnCount() override;
    virtual css::uno::Any SAL_CALL getCellData( ::sal_Int32 Column, ::sal_Int32 Row ) override;
    virtual css::uno::Any SAL_CALL getCellToolTip( ::sal_Int32 Column, ::sal_Int32 Row ) override;
    virtual css::uno::Any SAL_CALL getRowHeading( ::sal_Int32 RowIndex ) override;
    virtual css::uno::Sequence< css::uno::Any > SAL_CALL getRowData( ::sal_Int32 RowIndex ) override;

    // OComponentHelper
    virtual void disposing( std::unique_lock<std::mutex>& ) override;

    // XCloneable
    virtual css::uno::Reference< css::util::XCloneable > SAL_CALL createClone(  ) override;

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName(  ) override;
    virtual sal_Bool SAL_CALL supportsService( const OUString& ServiceName ) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames(  ) override;

private:
    typedef ::std::pair< Any, Any >     CellData;
    typedef ::std::vector< CellData >   RowData;
    typedef ::std::vector< RowData >    GridData;

    void broadcast(
        GridDataEvent const & i_event,
        void ( SAL_CALL css::awt::grid::XGridDataListener::*i_listenerMethod )( css::awt::grid::GridDataEvent const & ),
        std::unique_lock<std::mutex>& i_instanceLock
    );

    void    impl_insertRow( std::unique_lock<std::mutex>& rGuard, sal_Int32 const i_position, Any const & i_heading, Sequence< Any > const & i_rowData, sal_Int32 const i_assumedColCount = -1 );

    ::sal_Int32 impl_getRowCount(std::unique_lock<std::mutex>&) const { return sal_Int32( m_aData.size() ); }

    CellData const &    impl_getCellData_throw( std::unique_lock<std::mutex>& rGuard, sal_Int32 const i_columnIndex, sal_Int32 const i_rowIndex ) const;
    CellData&           impl_getCellDataAccess_throw( std::unique_lock<std::mutex>& rGuard, sal_Int32 const i_columnIndex, sal_Int32 const i_rowIndex );
    RowData&            impl_getRowDataAccess_throw( std::unique_lock<std::mutex>& rGuard, sal_Int32 const i_rowIndex, size_t const i_requiredColumnCount );

    GridData m_aData;
    ::std::vector< css::uno::Any > m_aRowHeaders;
    sal_Int32 m_nColumnCount;
    comphelper::OInterfaceContainerHelper4<XGridDataListener> maGridDataListeners;
};

    DefaultGridDataModel::DefaultGridDataModel()
        :m_nColumnCount(0)
    {
    }


    DefaultGridDataModel::DefaultGridDataModel( DefaultGridDataModel const & i_copySource )
        :m_aData( i_copySource.m_aData )
        ,m_aRowHeaders( i_copySource.m_aRowHeaders )
        ,m_nColumnCount( i_copySource.m_nColumnCount )
    {
    }

    void DefaultGridDataModel::broadcast( GridDataEvent const & i_event,
        void ( SAL_CALL XGridDataListener::*i_listenerMethod )( GridDataEvent const & ), std::unique_lock<std::mutex>& i_instanceLock )
    {
        maGridDataListeners.notifyEach( i_instanceLock, i_listenerMethod, i_event );
    }


    ::sal_Int32 SAL_CALL DefaultGridDataModel::getRowCount()
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);
        return impl_getRowCount(aGuard);
    }


    ::sal_Int32 SAL_CALL DefaultGridDataModel::getColumnCount()
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);
        return m_nColumnCount;
    }


    DefaultGridDataModel::CellData const & DefaultGridDataModel::impl_getCellData_throw( std::unique_lock<std::mutex>& /*rGuard*/, sal_Int32 const i_column, sal_Int32 const i_row ) const
    {
        if  (   ( i_row < 0 ) || ( o3tl::make_unsigned( i_row ) > m_aData.size() )
            ||  ( i_column < 0 ) || ( i_column > m_nColumnCount )
            )
            throw IndexOutOfBoundsException( OUString(), *const_cast< DefaultGridDataModel* >( this ) );

        RowData const & rRow( m_aData[ i_row ] );
        if ( o3tl::make_unsigned( i_column ) < rRow.size() )
            return rRow[ i_column ];

        static CellData s_aEmpty;
        return s_aEmpty;
    }


    DefaultGridDataModel::RowData& DefaultGridDataModel::impl_getRowDataAccess_throw( std::unique_lock<std::mutex>& /*rGuard*/, sal_Int32 const i_rowIndex, size_t const i_requiredColumnCount )
    {
        OSL_ENSURE( i_requiredColumnCount <= o3tl::make_unsigned( m_nColumnCount ), "DefaultGridDataModel::impl_getRowDataAccess_throw: invalid column count!" );
        if  ( ( i_rowIndex < 0 ) || ( o3tl::make_unsigned( i_rowIndex ) >= m_aData.size() ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        RowData& rRowData( m_aData[ i_rowIndex ] );
        if ( rRowData.size() < i_requiredColumnCount )
            rRowData.resize( i_requiredColumnCount );
        return rRowData;
    }


    DefaultGridDataModel::CellData& DefaultGridDataModel::impl_getCellDataAccess_throw( std::unique_lock<std::mutex>& rGuard, sal_Int32 const i_columnIndex, sal_Int32 const i_rowIndex )
    {
        if  ( ( i_columnIndex < 0 ) || ( i_columnIndex >= m_nColumnCount ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        RowData& rRowData( impl_getRowDataAccess_throw( rGuard, i_rowIndex, size_t( i_columnIndex + 1 ) ) );
        return rRowData[ i_columnIndex ];
    }


    Any SAL_CALL DefaultGridDataModel::getCellData( ::sal_Int32 i_column, ::sal_Int32 i_row )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);
        return impl_getCellData_throw( aGuard, i_column, i_row ).first;
    }


    Any SAL_CALL DefaultGridDataModel::getCellToolTip( ::sal_Int32 i_column, ::sal_Int32 i_row )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);
        return impl_getCellData_throw( aGuard, i_column, i_row ).second;
    }


    Any SAL_CALL DefaultGridDataModel::getRowHeading( ::sal_Int32 i_row )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        if ( ( i_row < 0 ) || ( o3tl::make_unsigned( i_row ) >= m_aRowHeaders.size() ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        return m_aRowHeaders[ i_row ];
    }


    Sequence< Any > SAL_CALL DefaultGridDataModel::getRowData( ::sal_Int32 i_rowIndex )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        Sequence< Any > resultData( m_nColumnCount );
        RowData& rRowData = impl_getRowDataAccess_throw( aGuard, i_rowIndex, m_nColumnCount );

        ::std::transform( rRowData.begin(), rRowData.end(), resultData.getArray(),
                          [] ( const CellData& rCellData ) -> const Any&
                          { return rCellData.first; });
        return resultData;
    }


    void DefaultGridDataModel::impl_insertRow( std::unique_lock<std::mutex>& /*rGuard*/, sal_Int32 const i_position, Any const & i_heading, Sequence< Any > const & i_rowData, sal_Int32 const i_assumedColCount )
    {
        OSL_PRECOND( ( i_assumedColCount <= 0 ) || ( i_assumedColCount >= i_rowData.getLength() ),
            "DefaultGridDataModel::impl_insertRow: invalid column count!" );

        // insert heading
        m_aRowHeaders.insert( m_aRowHeaders.begin() + i_position, i_heading );

        // create new data row
        RowData newRow( i_assumedColCount > 0 ? i_assumedColCount : i_rowData.getLength() );
        RowData::iterator cellData = newRow.begin();
        for ( const Any& rData : i_rowData )
        {
            cellData->first = rData;
            ++cellData;
        }

        // insert data row
        m_aData.insert( m_aData.begin() + i_position, newRow );
    }


    void SAL_CALL DefaultGridDataModel::addRow( const Any& i_heading, const Sequence< Any >& i_data )
    {
        insertRow( getRowCount(), i_heading, i_data );
    }


    void SAL_CALL DefaultGridDataModel::addRows( const Sequence< Any >& i_headings, const Sequence< Sequence< Any > >& i_data )
    {
        insertRows( getRowCount(), i_headings, i_data );
    }


    void SAL_CALL DefaultGridDataModel::insertRow( ::sal_Int32 i_index, const Any& i_heading, const Sequence< Any >& i_data )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        if ( ( i_index < 0 ) || ( i_index > impl_getRowCount(aGuard) ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        // actually insert the row
        impl_insertRow( aGuard, i_index, i_heading, i_data );

        // update column count
        sal_Int32 const columnCount = i_data.getLength();
        if ( columnCount > m_nColumnCount )
            m_nColumnCount = columnCount;

        broadcast(
            GridDataEvent( *this, -1, -1, i_index, i_index ),
            &XGridDataListener::rowsInserted,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::insertRows( ::sal_Int32 i_index, const Sequence< Any>& i_headings, const Sequence< Sequence< Any > >& i_data )
    {
        if ( i_headings.getLength() != i_data.getLength() )
            throw IllegalArgumentException( OUString(), *this, -1 );

        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        if ( ( i_index < 0 ) || ( i_index > impl_getRowCount(aGuard) ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        sal_Int32 const rowCount = i_headings.getLength();
        if ( rowCount == 0 )
            return;

        // determine max col count in the new data
        auto pData = std::max_element(i_data.begin(), i_data.end(),
            [](const Sequence< Any >& a, const Sequence< Any >& b) { return a.getLength() < b.getLength(); });
        sal_Int32 maxColCount = pData->getLength();

        if ( maxColCount < m_nColumnCount )
            maxColCount = m_nColumnCount;

        for ( sal_Int32 row=0; row<rowCount;  ++row )
        {
            impl_insertRow( aGuard, i_index + row, i_headings[row], i_data[row], maxColCount );
        }

        if ( maxColCount > m_nColumnCount )
            m_nColumnCount = maxColCount;

        broadcast(
            GridDataEvent( *this, -1, -1, i_index, i_index + rowCount - 1 ),
            &XGridDataListener::rowsInserted,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::removeRow( ::sal_Int32 i_rowIndex )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        if ( ( i_rowIndex < 0 ) || ( o3tl::make_unsigned( i_rowIndex ) >= m_aData.size() ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        m_aRowHeaders.erase( m_aRowHeaders.begin() + i_rowIndex );
        m_aData.erase( m_aData.begin() + i_rowIndex );

        broadcast(
            GridDataEvent( *this, -1, -1, i_rowIndex, i_rowIndex ),
            &XGridDataListener::rowsRemoved,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::removeAllRows(  )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        m_aRowHeaders.clear();
        m_aData.clear();

        broadcast(
            GridDataEvent( *this, -1, -1, -1, -1 ),
            &XGridDataListener::rowsRemoved,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::updateCellData( ::sal_Int32 i_columnIndex, ::sal_Int32 i_rowIndex, const Any& i_value )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        impl_getCellDataAccess_throw( aGuard, i_columnIndex, i_rowIndex ).first = i_value;

        broadcast(
            GridDataEvent( *this, i_columnIndex, i_columnIndex, i_rowIndex, i_rowIndex ),
            &XGridDataListener::dataChanged,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::updateRowData( const Sequence< ::sal_Int32 >& i_columnIndexes, ::sal_Int32 i_rowIndex, const Sequence< Any >& i_values )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        if  ( ( i_rowIndex < 0 ) || ( o3tl::make_unsigned( i_rowIndex ) >= m_aData.size() ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        if ( i_columnIndexes.getLength() != i_values.getLength() )
            throw IllegalArgumentException( OUString(), *this, 1 );

        sal_Int32 const columnCount = i_columnIndexes.getLength();
        if ( columnCount == 0 )
            return;

        for ( sal_Int32 const columnIndex : i_columnIndexes )
        {
            if ( ( columnIndex < 0 ) || ( columnIndex > m_nColumnCount ) )
                throw IndexOutOfBoundsException( OUString(), *this );
        }

        RowData& rDataRow = m_aData[ i_rowIndex ];
        for ( sal_Int32 col = 0; col < columnCount; ++col )
        {
            sal_Int32 const columnIndex = i_columnIndexes[ col ];
            if ( o3tl::make_unsigned( columnIndex ) >= rDataRow.size() )
                rDataRow.resize( columnIndex + 1 );

            rDataRow[ columnIndex ].first = i_values[ col ];
        }

        auto aPair = ::std::minmax_element( i_columnIndexes.begin(), i_columnIndexes.end() );
        sal_Int32 const firstAffectedColumn = *aPair.first;
        sal_Int32 const lastAffectedColumn = *aPair.second;
        broadcast(
            GridDataEvent( *this, firstAffectedColumn, lastAffectedColumn, i_rowIndex, i_rowIndex ),
            &XGridDataListener::dataChanged,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::updateRowHeading( ::sal_Int32 i_rowIndex, const Any& i_heading )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        if  ( ( i_rowIndex < 0 ) || ( o3tl::make_unsigned( i_rowIndex ) >= m_aRowHeaders.size() ) )
            throw IndexOutOfBoundsException( OUString(), *this );

        m_aRowHeaders[ i_rowIndex ] = i_heading;

        broadcast(
            GridDataEvent( *this, -1, -1, i_rowIndex, i_rowIndex ),
            &XGridDataListener::rowHeadingChanged,
            aGuard
        );
    }


    void SAL_CALL DefaultGridDataModel::updateCellToolTip( ::sal_Int32 i_columnIndex, ::sal_Int32 i_rowIndex, const Any& i_value )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);
        impl_getCellDataAccess_throw( aGuard, i_columnIndex, i_rowIndex ).second = i_value;
    }


    void SAL_CALL DefaultGridDataModel::updateRowToolTip( ::sal_Int32 i_rowIndex, const Any& i_value )
    {
        std::unique_lock aGuard(m_aMutex);
        throwIfDisposed(aGuard);

        RowData& rRowData = impl_getRowDataAccess_throw( aGuard, i_rowIndex, m_nColumnCount );
        for ( auto& rCell : rRowData )
            rCell.second = i_value;
    }


    void SAL_CALL DefaultGridDataModel::addGridDataListener( const Reference< grid::XGridDataListener >& i_listener )
    {
        std::unique_lock aGuard(m_aMutex);
        maGridDataListeners.addInterface( aGuard, i_listener );
    }


    void SAL_CALL DefaultGridDataModel::removeGridDataListener( const Reference< grid::XGridDataListener >& i_listener )
    {
        std::unique_lock aGuard(m_aMutex);
        maGridDataListeners.removeInterface( aGuard, i_listener );
    }


    void DefaultGridDataModel::disposing(std::unique_lock<std::mutex>& rGuard)
    {
        css::lang::EventObject aEvent;
        aEvent.Source.set( *this );
        maGridDataListeners.disposeAndClear(rGuard, aEvent);

        GridData().swap(m_aData);
        std::vector<Any>().swap(m_aRowHeaders);
        m_nColumnCount = 0;
    }


    OUString SAL_CALL DefaultGridDataModel::getImplementationName(  )
    {
        return u"stardiv.Toolkit.DefaultGridDataModel"_ustr;
    }

    sal_Bool SAL_CALL DefaultGridDataModel::supportsService( const OUString& ServiceName )
    {
        return cppu::supportsService(this, ServiceName);
    }

    Sequence< OUString > SAL_CALL DefaultGridDataModel::getSupportedServiceNames(  )
    {
        return { u"com.sun.star.awt.grid.DefaultGridDataModel"_ustr };
    }


    Reference< css::util::XCloneable > SAL_CALL DefaultGridDataModel::createClone(  )
    {
        std::unique_lock aGuard(m_aMutex);
        return new DefaultGridDataModel( *this );
    }

}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface *
stardiv_Toolkit_DefaultGridDataModel_get_implementation(
    css::uno::XComponentContext *,
    css::uno::Sequence<css::uno::Any> const &)
{
    return cppu::acquire(new DefaultGridDataModel());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
