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

#include "pushbuttonnavigation.hxx"
#include <com/sun/star/beans/XPropertyState.hpp>
#include "formstrings.hxx"
#include <comphelper/extract.hxx>
#include <comphelper/property.hxx>
#include <o3tl/string_view.hxx>
#include <osl/diagnose.h>
#include <comphelper/diagnose_ex.hxx>


namespace pcr
{


    using namespace ::com::sun::star::uno;
    using namespace ::com::sun::star::beans;
    using namespace ::com::sun::star::form;


    namespace
    {
        const sal_Int32 s_nFirstVirtualButtonType = 1 + sal_Int32(FormButtonType_URL);

        const char* const pNavigationURLs[] =
        {
            ".uno:FormController/moveToFirst",
            ".uno:FormController/moveToPrev",
            ".uno:FormController/moveToNext",
            ".uno:FormController/moveToLast",
            ".uno:FormController/saveRecord",
            ".uno:FormController/undoRecord",
            ".uno:FormController/moveToNew",
            ".uno:FormController/deleteRecord",
            ".uno:FormController/refreshForm",
            nullptr
        };

        sal_Int32 lcl_getNavigationURLIndex( std::u16string_view _rNavURL )
        {
            const char* const* pLookup = pNavigationURLs;
            while ( *pLookup )
            {
                if ( o3tl::equalsAscii( _rNavURL, *pLookup ) )
                    return pLookup - pNavigationURLs;
                ++pLookup;
            }
            return -1;
        }

        const char* lcl_getNavigationURL( sal_Int32 _nButtonTypeIndex )
        {
            const char* const* pLookup = pNavigationURLs;
            while ( _nButtonTypeIndex-- && *pLookup++ )
                ;
            OSL_ENSURE( *pLookup, "lcl_getNavigationURL: invalid index!" );
            return *pLookup;
        }
    }


    //= PushButtonNavigation


    PushButtonNavigation::PushButtonNavigation( const Reference< XPropertySet >& _rxControlModel )
        :m_xControlModel( _rxControlModel )
        ,m_bIsPushButton( false )
    {
        OSL_ENSURE( m_xControlModel.is(), "PushButtonNavigation::PushButtonNavigation: invalid control model!" );

        try
        {
            m_bIsPushButton = ::comphelper::hasProperty( PROPERTY_BUTTONTYPE, m_xControlModel );
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::PushButtonNavigation" );
        }
    }


    FormButtonType PushButtonNavigation::implGetCurrentButtonType() const
    {
        sal_Int32 nButtonType = sal_Int32(FormButtonType_PUSH);
        if ( !m_xControlModel.is() )
            return static_cast<FormButtonType>(nButtonType);
        OSL_VERIFY( ::cppu::enum2int( nButtonType, m_xControlModel->getPropertyValue( PROPERTY_BUTTONTYPE ) ) );

        if ( nButtonType == sal_Int32(FormButtonType_URL) )
        {
            // there's a chance that this is a "virtual" button type
            // (which are realized by special URLs)
            OUString sTargetURL;
            m_xControlModel->getPropertyValue( PROPERTY_TARGET_URL ) >>= sTargetURL;

            sal_Int32 nNavigationURLIndex = lcl_getNavigationURLIndex( sTargetURL );
            if ( nNavigationURLIndex >= 0)
                // it actually *is* a virtual button type
                nButtonType = s_nFirstVirtualButtonType + nNavigationURLIndex;
        }
        return static_cast<FormButtonType>(nButtonType);
    }


    Any PushButtonNavigation::getCurrentButtonType() const
    {
        OSL_ENSURE( m_bIsPushButton, "PushButtonNavigation::getCurrentButtonType: not expected to be called for forms!" );
        Any aReturn;

        try
        {
            aReturn <<= implGetCurrentButtonType();
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::getCurrentButtonType" );
        }
        return aReturn;
    }


    void PushButtonNavigation::setCurrentButtonType( const Any& _rValue ) const
    {
        OSL_ENSURE( m_bIsPushButton, "PushButtonNavigation::setCurrentButtonType: not expected to be called for forms!" );
        if ( !m_xControlModel.is() )
            return;

        try
        {
            sal_Int32 nButtonType = sal_Int32(FormButtonType_PUSH);
            OSL_VERIFY( ::cppu::enum2int( nButtonType, _rValue ) );
            OUString sTargetURL;

            bool bIsVirtualButtonType = nButtonType >= s_nFirstVirtualButtonType;
            if ( bIsVirtualButtonType )
            {
                const char* pURL = lcl_getNavigationURL( nButtonType - s_nFirstVirtualButtonType );
                sTargetURL = OUString::createFromAscii( pURL );

                nButtonType = sal_Int32(FormButtonType_URL);
            }

            m_xControlModel->setPropertyValue( PROPERTY_BUTTONTYPE, Any( static_cast< FormButtonType >( nButtonType ) ) );
            m_xControlModel->setPropertyValue( PROPERTY_TARGET_URL, Any( sTargetURL ) );
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::setCurrentButtonType" );
        }
    }


    PropertyState PushButtonNavigation::getCurrentButtonTypeState( ) const
    {
        OSL_ENSURE( m_bIsPushButton, "PushButtonNavigation::getCurrentButtonTypeState: not expected to be called for forms!" );
        PropertyState eState = PropertyState_DIRECT_VALUE;

        try
        {
            Reference< XPropertyState > xStateAccess( m_xControlModel, UNO_QUERY );
            if ( xStateAccess.is() )
            {
                // let's see what the model says about the ButtonType property
                eState = xStateAccess->getPropertyState( PROPERTY_BUTTONTYPE );
                if ( eState == PropertyState_DIRECT_VALUE )
                {
                    sal_Int32 nRealButtonType = sal_Int32(FormButtonType_PUSH);
                    OSL_VERIFY( ::cppu::enum2int( nRealButtonType, m_xControlModel->getPropertyValue( PROPERTY_BUTTONTYPE ) ) );
                    // perhaps it's one of the virtual button types?
                    if ( sal_Int32(FormButtonType_URL) == nRealButtonType )
                    {
                        // yes, it is -> rely on the state of the URL property
                        eState = xStateAccess->getPropertyState( PROPERTY_TARGET_URL );
                    }
                }
            }
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::getCurrentButtonTypeState" );
        }

        return eState;
    }


    Any PushButtonNavigation::getCurrentTargetURL() const
    {
        Any aReturn;
        if ( !m_xControlModel.is() )
            return aReturn;

        try
        {
            aReturn = m_xControlModel->getPropertyValue( PROPERTY_TARGET_URL );
            if ( m_bIsPushButton )
            {
                FormButtonType nCurrentButtonType = implGetCurrentButtonType();
                bool bIsVirtualButtonType = nCurrentButtonType >= FormButtonType(s_nFirstVirtualButtonType);
                if ( bIsVirtualButtonType )
                {
                    // pretend (to the user) that there's no URL set - since
                    // virtual button types imply a special (technical) URL which
                    // the user should not see
                    aReturn <<= OUString();
                }
            }
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::getCurrentTargetURL" );
        }
        return aReturn;
    }


    void PushButtonNavigation::setCurrentTargetURL( const Any& _rValue ) const
    {
        if ( !m_xControlModel.is() )
            return;

        try
        {
            m_xControlModel->setPropertyValue( PROPERTY_TARGET_URL, _rValue );
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::setCurrentTargetURL" );
        }
    }


    PropertyState PushButtonNavigation::getCurrentTargetURLState( ) const
    {
        PropertyState eState = PropertyState_DIRECT_VALUE;

        try
        {
            Reference< XPropertyState > xStateAccess( m_xControlModel, UNO_QUERY );
            if ( xStateAccess.is() )
            {
                eState = xStateAccess->getPropertyState( PROPERTY_TARGET_URL );
            }
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PushButtonNavigation::setCurrentTargetURL" );
        }

        return eState;
    }


    bool PushButtonNavigation::currentButtonTypeIsOpenURL() const
    {
        FormButtonType nButtonType( FormButtonType_PUSH );
        try
        {
            nButtonType = implGetCurrentButtonType();
        }
        catch( const Exception& )
        {
            DBG_UNHANDLED_EXCEPTION("extensions.propctrlr");
        }
        return nButtonType == FormButtonType_URL;
    }


    bool PushButtonNavigation::hasNonEmptyCurrentTargetURL() const
    {
        OUString sTargetURL;
        OSL_VERIFY( getCurrentTargetURL() >>= sTargetURL );
        return !sTargetURL.isEmpty();
    }


}   // namespace pcr


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
