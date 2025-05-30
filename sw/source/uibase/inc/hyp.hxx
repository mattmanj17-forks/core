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

#ifndef INCLUDED_SW_SOURCE_UIBASE_INC_HYP_HXX
#define INCLUDED_SW_SOURCE_UIBASE_INC_HYP_HXX

#include <editeng/splwrap.hxx>
#include <com/sun/star/linguistic2/XHyphenator.hpp>
#include <swdllapi.h>

class SwView;

class SW_DLLPUBLIC SwHyphWrapper final : public SvxSpellWrapper {
private:
    SwView& m_rView;
    sal_uInt16      m_nPageCount;     // page count for progress view
    sal_uInt16      m_nPageStart;     // 1st checked page
    bool            m_bInSelection : 1; // separating selected text
    bool            m_bAutomatic : 1; // insert separators without further inquiry
    bool            m_bInfoBox : 1;   // display info-box when ending

    virtual void SpellStart( SvxSpellArea eSpell ) override;
    virtual void SpellContinue() override;
    virtual void SpellEnd( ) override;
    virtual bool SpellMore() override;
    virtual void InsertHyphen( const sal_Int32 nPos ) override; // insert hyphen

public:
    SwHyphWrapper( SwView& rView,
                   css::uno::Reference< css::linguistic2::XHyphenator > const &rxHyph,
                   bool bStart, bool bOther, bool bSelect );
    virtual ~SwHyphWrapper() override;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
