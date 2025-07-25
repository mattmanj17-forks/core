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

#include "salgeom.hxx"

class SalGraphics;

/// A non-visible drawable/buffer (e.g. an X11 Pixmap).
class VCL_PLUGIN_PUBLIC SalVirtualDevice
        : public SalGeometryProvider
{
public:
    SalVirtualDevice() {}
    virtual ~SalVirtualDevice() override;

    // SalGeometryProvider
    virtual bool IsOffScreen() const override { return true; }

    // SalGraphics or NULL, but two Graphics for all SalVirtualDevices
    // must be returned
    virtual SalGraphics*    AcquireGraphics() = 0;
    virtual void            ReleaseGraphics( SalGraphics* pGraphics ) = 0;

    // Set new size, without saving the old contents
    virtual bool            SetSize( tools::Long nNewDX, tools::Long nNewDY, bool bAlphaMaskTransparent ) = 0;

    // Set new size using a buffer at the given address
    virtual bool            SetSizeUsingBuffer( tools::Long /*nNewDX*/, tools::Long /*nNewDY*/,
                                                sal_uInt8 * /*pBuffer*/)
    {
        // Only the headless/svp virtual device has an implementation.
        assert(false && "unsupported");
        return false;
    }
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
