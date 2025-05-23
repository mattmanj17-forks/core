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
#ifndef INCLUDED_SW_INC_UNOBASECLASS_HXX
#define INCLUDED_SW_INC_UNOBASECLASS_HXX

#include <memory>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/container/XEnumeration.hpp>

#include <cppuhelper/implbase.hxx>

#include <o3tl/typed_flags_set.hxx>

#include <vcl/svapp.hxx>

class SwDoc;
class SwUnoTableCursor;

typedef ::cppu::WeakImplHelper
<   css::lang::XServiceInfo
,   css::container::XEnumeration
>
SwSimpleEnumeration_Base;

enum class CursorType
{
    Body,
    Frame,
    TableText,
    Footnote,
    Header,
    Footer,
    Redline,
    All,         // for Search&Replace
    Selection,   // create a paragraph enumeration from
                        // a text range or cursor
    SelectionInTable,
    Meta,         // meta/meta-field
    ContentControl,
};

namespace sw {

enum class DeleteAndInsertMode
{
    Default = 0,
    ForceExpandHints = (1<<0),
    ForceReplace = (1<<1),
};

} // namespace sw

namespace o3tl
{
    template<> struct typed_flags<::sw::DeleteAndInsertMode> : is_typed_flags<::sw::DeleteAndInsertMode, 0x03> {};
}

/*
    Start/EndAction or Start/EndAllAction
*/
class UnoActionContext
{
private:
        SwDoc * m_pDoc;

public:
        UnoActionContext(SwDoc *const pDoc);
        ~UnoActionContext() COVERITY_NOEXCEPT_FALSE;
};

/*
    interrupt Actions for a little while
    FIXME: this is a vile abomination that may cause recursive layout actions!
    C'thulhu fhtagn.
*/
class UnoActionRemoveContext
{
private:
    SwDoc *const m_pDoc;

public:
    UnoActionRemoveContext(SwDoc *const pDoc);
    UnoActionRemoveContext(SwUnoTableCursor const& rCursor);
    ~UnoActionRemoveContext() COVERITY_NOEXCEPT_FALSE;
};

namespace sw {
    template<typename T>
    struct UnoImplPtrDeleter
    {
        void operator()(T* pUnoImpl)
        {
            SolarMutexGuard g; // #i105557#: call dtor with locked solar mutex
            delete pUnoImpl;
        }
    };
    /// Smart pointer class ensuring that the pointed object is deleted with a locked SolarMutex.
    template<typename T>
    using UnoImplPtr = std::unique_ptr<T, UnoImplPtrDeleter<T> >;

} // namespace sw

#endif // INCLUDED_SW_INC_UNOBASECLASS_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
