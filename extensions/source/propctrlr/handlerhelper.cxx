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

#include "handlerhelper.hxx"
#include <yesno.hrc>
#include "modulepcr.hxx"

#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/inspection/StringRepresentation.hpp>
#include <com/sun/star/util/XModifiable.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <com/sun/star/inspection/LineDescriptor.hpp>
#include <com/sun/star/inspection/PropertyControlType.hpp>
#include <com/sun/star/inspection/XStringListControl.hpp>
#include <com/sun/star/inspection/XNumericControl.hpp>
#include <comphelper/diagnose_ex.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld.hxx>
#include <vcl/weldutils.hxx>

#include <algorithm>


namespace pcr
{


    using namespace ::com::sun::star::uno;
    using namespace ::com::sun::star::lang;
    using namespace ::com::sun::star::awt;
    using namespace ::com::sun::star::util;
    using namespace ::com::sun::star::beans;
    using namespace ::com::sun::star::script;
    using namespace ::com::sun::star::inspection;


    //= PropertyHandlerHelper


    void PropertyHandlerHelper::describePropertyLine( const Property& _rProperty,
        LineDescriptor& /* [out] */ _out_rDescriptor, const Reference< XPropertyControlFactory >& _rxControlFactory )
    {
        // display the pure property name - no L10N
        _out_rDescriptor.DisplayName = _rProperty.Name;

        OSL_PRECOND( _rxControlFactory.is(), "PropertyHandlerHelper::describePropertyLine: no factory -> no control!" );
        if ( !_rxControlFactory.is() )
            return;

        bool bReadOnlyControl = requiresReadOnlyControl( _rProperty.Attributes );

        // special handling for booleans (this will become a list)
        if ( _rProperty.Type.getTypeClass() == TypeClass_BOOLEAN )
        {
            _out_rDescriptor.Control = createListBoxControl(_rxControlFactory, RID_RSC_ENUM_YESNO, SAL_N_ELEMENTS(RID_RSC_ENUM_YESNO), bReadOnlyControl);
            return;
        }

        sal_Int16 nControlType = PropertyControlType::TextField;
        switch ( _rProperty.Type.getTypeClass() )
        {
        case TypeClass_BYTE:
        case TypeClass_SHORT:
        case TypeClass_UNSIGNED_SHORT:
        case TypeClass_LONG:
        case TypeClass_UNSIGNED_LONG:
        case TypeClass_HYPER:
        case TypeClass_UNSIGNED_HYPER:
        case TypeClass_FLOAT:
        case TypeClass_DOUBLE:
            nControlType = PropertyControlType::NumericField;
            break;

        case TypeClass_SEQUENCE:
            nControlType = PropertyControlType::StringListField;
            break;

        default:
            OSL_FAIL( "PropertyHandlerHelper::describePropertyLine: don't know how to represent this at the UI!" );
            [[fallthrough]];

        case TypeClass_STRING:
            nControlType = PropertyControlType::TextField;
            break;
        }

        // create a control
        _out_rDescriptor.Control = _rxControlFactory->createPropertyControl( nControlType, bReadOnlyControl );
    }


    namespace
    {
        Reference< XPropertyControl > lcl_implCreateListLikeControl(
                const Reference< XPropertyControlFactory >& _rxControlFactory,
                std::vector< OUString >&& _rInitialListEntries,
                bool _bReadOnlyControl,
                bool _bSorted,
                bool _bTrueIfListBoxFalseIfComboBox
            )
        {
            Reference< XStringListControl > xListControl(
                _rxControlFactory->createPropertyControl(
                    _bTrueIfListBoxFalseIfComboBox ? PropertyControlType::ListBox : PropertyControlType::ComboBox, _bReadOnlyControl
                ),
                UNO_QUERY_THROW
            );

            if ( _bSorted )
                std::sort( _rInitialListEntries.begin(), _rInitialListEntries.end() );

            for (auto const& initialEntry : _rInitialListEntries)
                xListControl->appendListEntry(initialEntry);
            return xListControl;
        }
    }

    Reference< XPropertyControl > PropertyHandlerHelper::createListBoxControl( const Reference< XPropertyControlFactory >& _rxControlFactory,
                std::vector< OUString >&& _rInitialListEntries, bool _bReadOnlyControl, bool _bSorted )
    {
        return lcl_implCreateListLikeControl(_rxControlFactory, std::move(_rInitialListEntries), _bReadOnlyControl, _bSorted, true);
    }

    Reference< XPropertyControl > PropertyHandlerHelper::createListBoxControl( const Reference< XPropertyControlFactory >& _rxControlFactory,
                const TranslateId* pTransIds, size_t nElements, bool _bReadOnlyControl )
    {
        std::vector<OUString> aInitialListEntries;
        for (size_t i = 0; i < nElements; ++i)
            aInitialListEntries.push_back(PcrRes(pTransIds[i]));
        return lcl_implCreateListLikeControl(_rxControlFactory, std::move(aInitialListEntries), _bReadOnlyControl, /*_bSorted*/false, true);
    }

    Reference< XPropertyControl > PropertyHandlerHelper::createComboBoxControl( const Reference< XPropertyControlFactory >& _rxControlFactory,
                std::vector< OUString >&& _rInitialListEntries, bool _bSorted )
    {
        return lcl_implCreateListLikeControl( _rxControlFactory, std::move(_rInitialListEntries), /*_bReadOnlyControl*/false, _bSorted, false );
    }


    Reference< XPropertyControl > PropertyHandlerHelper::createNumericControl( const Reference< XPropertyControlFactory >& _rxControlFactory,
            sal_Int16 _nDigits, const Optional< double >& _rMinValue, const Optional< double >& _rMaxValue )
    {
        Reference< XNumericControl > xNumericControl(
            _rxControlFactory->createPropertyControl( PropertyControlType::NumericField, /*_bReadOnlyControl*/false ),
            UNO_QUERY_THROW
        );

        xNumericControl->setDecimalDigits( _nDigits );
        xNumericControl->setMinValue( _rMinValue );
        xNumericControl->setMaxValue( _rMaxValue );

        return xNumericControl;
    }


    Any PropertyHandlerHelper::convertToPropertyValue( const Reference< XComponentContext >& _rxContext,const Reference< XTypeConverter >& _rxTypeConverter,
        const Property& _rProperty, const Any& _rControlValue )
    {
        Any aPropertyValue( _rControlValue );
        if ( !aPropertyValue.hasValue() )
            // NULL is converted to NULL
            return aPropertyValue;

        if ( aPropertyValue.getValueType().equals( _rProperty.Type ) )
            // nothing to do, type is already as desired
            return aPropertyValue;

        if ( _rControlValue.getValueTypeClass() == TypeClass_STRING )
        {
            OUString sControlValue;
            _rControlValue >>= sControlValue;

            Reference< XStringRepresentation > xConversionHelper = StringRepresentation::create( _rxContext,_rxTypeConverter );
            aPropertyValue = xConversionHelper->convertToPropertyValue( sControlValue, _rProperty.Type );
        }
        else
        {
            try
            {
                if ( _rxTypeConverter.is() )
                    aPropertyValue = _rxTypeConverter->convertTo( _rControlValue, _rProperty.Type );
            }
            catch( const Exception& )
            {
                TOOLS_WARN_EXCEPTION("extensions.propctrlr",
                                     "caught an exception while converting via TypeConverter!");
            }
        }

        return aPropertyValue;
    }


    Any PropertyHandlerHelper::convertToControlValue( const Reference< XComponentContext >& _rxContext,const Reference< XTypeConverter >& _rxTypeConverter,
        const Any& _rPropertyValue, const Type& _rControlValueType )
    {
        Any aControlValue( _rPropertyValue );
        if ( !aControlValue.hasValue() )
            // NULL is converted to NULL
            return aControlValue;

        if ( _rControlValueType.getTypeClass() == TypeClass_STRING )
        {
            Reference< XStringRepresentation > xConversionHelper = StringRepresentation::create( _rxContext,_rxTypeConverter );
            aControlValue <<= xConversionHelper->convertToControlValue( _rPropertyValue );
        }
        else
        {
            try
            {
                if ( _rxTypeConverter.is() )
                    aControlValue = _rxTypeConverter->convertTo( _rPropertyValue, _rControlValueType );
            }
            catch( const Exception& )
            {
                TOOLS_WARN_EXCEPTION("extensions.propctrlr",
                                     "caught an exception while converting via TypeConverter!");
            }
        }

        return aControlValue;
    }


    void PropertyHandlerHelper::setContextDocumentModified( const Reference<XComponentContext> & rContext )
    {
        try
        {
            Reference< XModifiable > xDocumentModifiable( getContextDocument_throw(rContext), UNO_QUERY_THROW );
            xDocumentModifiable->setModified( true );
        }
        catch( const Exception& )
        {
            DBG_UNHANDLED_EXCEPTION("extensions.propctrlr");
        }
    }

    Reference< XInterface > PropertyHandlerHelper::getContextDocument( const Reference<XComponentContext> & rContext )
    {
        Reference< XInterface > xI;
        try
        {
            xI = getContextDocument_throw( rContext );
        }
        catch( const Exception& )
        {
            TOOLS_WARN_EXCEPTION( "extensions.propctrlr", "PropertyHandler::getContextValueByName" );
        }
        return xI;
    }

    Reference< XInterface > PropertyHandlerHelper::getContextDocument_throw( const Reference<XComponentContext> & rContext )
    {
        Reference< XInterface > xI;
        Any aReturn = rContext->getValueByName( u"ContextDocument"_ustr );
        aReturn >>= xI;
        return xI;
    }

    weld::Window* PropertyHandlerHelper::getDialogParentFrame(const Reference<XComponentContext>& rContext)
    {
        weld::Window* pInspectorWindow = nullptr;
        try
        {
            Reference< XWindow > xInspectorWindow(rContext->getValueByName( u"DialogParentWindow"_ustr ), UNO_QUERY_THROW);
            pInspectorWindow = Application::GetFrameWeld(xInspectorWindow);
        }
        catch( const Exception& )
        {
            DBG_UNHANDLED_EXCEPTION("extensions.propctrlr");
        }
        return pInspectorWindow;
    }

    std::unique_ptr<weld::Builder> PropertyHandlerHelper::makeBuilder(const OUString& rUIFile, const Reference<XComponentContext>& rContext)
    {
        Reference<XWindow> xWindow(rContext->getValueByName(u"BuilderParent"_ustr), UNO_QUERY_THROW);
        weld::TransportAsXWindow& rTunnel = dynamic_cast<weld::TransportAsXWindow&>(*xWindow);
        return Application::CreateBuilder(rTunnel.getWidget(), rUIFile);
    }

    void PropertyHandlerHelper::setBuilderParent(const css::uno::Reference<css::uno::XComponentContext>& rContext, weld::Widget* pParent)
    {
        Reference<css::container::XNameContainer> xName(rContext, UNO_QUERY_THROW);
        Reference<XWindow> xWindow(new weld::TransportAsXWindow(pParent));
        xName->insertByName(u"BuilderParent"_ustr, Any(xWindow));
    }

    void PropertyHandlerHelper::clearBuilderParent(const css::uno::Reference<css::uno::XComponentContext>& rContext)
    {
        Reference<css::container::XNameContainer> xName(rContext, UNO_QUERY_THROW);
        xName->removeByName(u"BuilderParent"_ustr);
    }

} // namespace pcr


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
