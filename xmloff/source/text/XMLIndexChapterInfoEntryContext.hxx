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

#include "XMLIndexSimpleEntryContext.hxx"
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/uno/Sequence.h>
#include <com/sun/star/beans/PropertyValue.hpp>


class XMLIndexTemplateContext;

/**
 * Import chapter info index entry templates
 */
class XMLIndexChapterInfoEntryContext : public XMLIndexSimpleEntryContext
{
    // chapter format
    sal_Int16 nChapterInfo;
    bool bChapterInfoOK;
    bool bTOC;
    sal_Int16 nOutlineLevel;
    bool bOutlineLevelOK;

public:


    XMLIndexChapterInfoEntryContext(
        SvXMLImport& rImport,
        XMLIndexTemplateContext& rTemplate,
        bool bTOC );

    virtual ~XMLIndexChapterInfoEntryContext() override;

protected:

    /** process parameters */
    virtual void SAL_CALL startFastElement(
        sal_Int32 nElement,
        const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList ) override;

    /** fill property values for this template entry */
    virtual void FillPropertyValues(
        css::uno::Sequence<css::beans::PropertyValue> & rValues) override;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
