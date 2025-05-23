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

#include <sal/config.h>

#include <map>
#include <unordered_map>

#include <com/sun/star/xml/sax/XFastAttributeList.hpp>
#include <com/sun/star/drawing/XDrawPage.hpp>
#include <com/sun/star/form/XFormsSupplier2.hpp>
#include <rtl/ref.hxx>
#include "formattributes.hxx"
#include "eventimport.hxx"

class SvXMLImport;
class SvXMLImportContext;
class SvXMLStyleContext;
class SvXMLStylesContext;

    // unfortunately, we can't put this into our namespace, as the macro expands to (amongst others) a forward
    // declaration of the class name, which then would be in the namespace, too

namespace xmloff
{
    //= OFormLayerXMLImport_Impl
    class OFormLayerXMLImport_Impl
                : public ODefaultEventAttacherManager
    {
        friend class OFormLayerXMLImport;

        SvXMLImport&                        m_rImporter;
        OAttribute2Property                 m_aAttributeMetaData;

        /// the supplier for the forms of the currently imported page
        css::uno::Reference< css::form::XFormsSupplier2 >
                                            m_xCurrentPageFormsSupp;
        rtl::Reference<SvXMLStylesContext>  m_xAutoStyles;

        typedef std::map< OUString, css::uno::Reference< css::beans::XPropertySet > > MapString2PropertySet;
        typedef std::unordered_map<css::uno::Reference<css::drawing::XDrawPage>, MapString2PropertySet> MapDrawPage2Map;

        MapDrawPage2Map         m_aControlIds;          // ids of the controls on all known page
        MapDrawPage2Map::iterator m_aCurrentPageIds;      // ifs of the controls on the current page

        typedef ::std::pair< css::uno::Reference< css::beans::XPropertySet >, OUString >
                                ModelStringPair;
        ::std::vector< ModelStringPair >
                                m_aControlReferences;   // control reference descriptions for current page
        ::std::vector< ModelStringPair >
                                m_aCellValueBindings;   // information about controls bound to spreadsheet cells
        ::std::vector< ModelStringPair >
                                m_aCellRangeListSources;// information about controls bound to spreadsheet cell range list sources

        ::std::vector< ModelStringPair >
                                m_aXFormsValueBindings; // collect xforms:bind attributes to be resolved

        ::std::vector< ModelStringPair >
                                m_aXFormsListBindings; // collect forms:xforms-list-source attributes to be resolved

        ::std::vector< ModelStringPair >
                                m_aXFormsSubmissions;   // collect xforms:submission attributes to be resolved

    public:
        // IControlIdMap
        void    registerControlId(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControl,
            const OUString& _rId);
        void    registerControlReferences(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControl,
            const OUString& _rReferringControls);

        // OFormLayerXMLImport_Impl
        OAttribute2Property&         getAttributeMap()   { return m_aAttributeMetaData; }
        SvXMLImport&                 getGlobalContext()  { return m_rImporter; }
        const SvXMLStyleContext*            getStyleElement(const OUString& _rStyleName) const;
        void                                enterEventContext();
        void                                leaveEventContext();
        void                                applyControlNumberStyle(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControlModel,
            const OUString& _rControlNumberStyleName
        );
        void                        registerCellValueBinding(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControlModel,
            const OUString& _rCellAddress
        );

        void                        registerCellRangeListSource(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControlModel,
            const OUString& _rCellRangeAddress
        );

        void                        registerXFormsValueBinding(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControlModel,
            const OUString& _rBindingID
        );

        void                        registerXFormsListBinding(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControlModel,
            const OUString& _rBindingID
        );

        void                        registerXFormsSubmission(
            const css::uno::Reference< css::beans::XPropertySet >& _rxControlModel,
            const OUString& _rSubmissionID
        );

        ~OFormLayerXMLImport_Impl() override;

    private:
        explicit OFormLayerXMLImport_Impl(SvXMLImport& _rImporter);

        /** start importing the forms of the given page
        */
        void startPage(
            const css::uno::Reference< css::drawing::XDrawPage >& _rxDrawPage);

        /** end importing the forms of the current page
        */
        void endPage();

        /** creates an import context for the office:forms element
        */
        static SvXMLImportContext* createOfficeFormsContext(
            SvXMLImport& _rImport);

        /** create an <type>SvXMLImportContext</type> instance which is able to import the &lt;form:form&gt;
            element.
        */
        SvXMLImportContext* createContext(
            sal_Int32 nElement,
            const css::uno::Reference< css::xml::sax::XFastAttributeList >& _rxAttribs);

        /** get the control with the given id
        */
        css::uno::Reference< css::beans::XPropertySet >
                lookupControlId(const OUString& _rControlId);

        /** announces the auto-style context to the form importer
        */
        void setAutoStyleContext(SvXMLStylesContext* _pNewContext);

        /** to be called when the document has been completely imported

            <p>For some documents (currently: only some spreadsheet documents) it's necessary
            do to a post processing, since not all information from the file can be processed
            if the document is not completed, yet.</p>
        */
        void documentDone( );
    };

}   // namespace xmloff

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
