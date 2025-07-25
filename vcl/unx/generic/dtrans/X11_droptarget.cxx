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

#include <cppuhelper/supportsservice.hxx>
#include "X11_selection.hxx"

using namespace x11;
using namespace com::sun::star::uno;
using namespace com::sun::star::lang;
using namespace com::sun::star::awt;
using namespace com::sun::star::datatransfer;
using namespace com::sun::star::datatransfer::dnd;

X11DropTarget::X11DropTarget()
    : m_aTargetWindow(None)
{
}

X11DropTarget::~X11DropTarget()
{
    if( m_xSelectionManager.is() )
        m_xSelectionManager->deregisterDropTarget( m_aTargetWindow );
}

void X11DropTarget::initialize(::Window aWindow)
{
    m_xSelectionManager = &SelectionManager::get();
    m_xSelectionManager->initialize();

    if( m_xSelectionManager->getDisplay() ) // #136582# sanity check
    {
        m_xSelectionManager->registerDropTarget( aWindow, this );
        m_aTargetWindow = aWindow;
        setActive(true);
    }
}

// XServiceInfo
OUString X11DropTarget::getImplementationName()
{
    return u"com.sun.star.datatransfer.dnd.XdndDropTarget"_ustr;
}

sal_Bool X11DropTarget::supportsService(const OUString& ServiceName)
{
    return cppu::supportsService(this, ServiceName);
}

Sequence<OUString> X11DropTarget::getSupportedServiceNames()
{
    return Xdnd_dropTarget_getSupportedServiceNames();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
