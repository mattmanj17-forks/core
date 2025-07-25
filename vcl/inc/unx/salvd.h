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

#include <X11/Xlib.h>

#include <vcl/salgtype.hxx>

#include <unx/saldisp.hxx>
#include <unx/saltype.h>
#include <salvd.hxx>

#include <memory>

class X11SalGraphics;
typedef struct _cairo_surface cairo_surface_t;

class X11SalVirtualDevice final : public SalVirtualDevice
{
    SalDisplay      *pDisplay_;
    std::unique_ptr<X11SalGraphics> pGraphics_;

    Pixmap          hDrawable_;
    SalX11Screen    m_nXScreen;

    int             nDX_;
    int             nDY_;
    sal_uInt16      nDepth_;
    bool        bGraphics_;         // is Graphics used
    bool        bExternPixmap_;
    cairo_surface_t* m_pSurface;
    bool m_bOwnsSurface; // nearly always true, except for edge case of tdf#127529

public:
    X11SalVirtualDevice(const SalGraphics& rGraphics, tools::Long nDX, tools::Long nDY,
            DeviceFormat eFormat, std::unique_ptr<X11SalGraphics> pNewGraphics);
    X11SalVirtualDevice(const SalGraphics& rGraphics, tools::Long &nDX, tools::Long &nDY,
            DeviceFormat eFormat, const SystemGraphicsData& rData, std::unique_ptr<X11SalGraphics> pNewGraphics);

    virtual ~X11SalVirtualDevice() override;

    Display *GetXDisplay() const
    {
        return pDisplay_->GetDisplay();
    }
    SalDisplay *GetDisplay() const
    {
        return pDisplay_;
    }
    Pixmap          GetDrawable() const { return hDrawable_; }
    cairo_surface_t* GetSurface() const { return m_pSurface; }
    sal_uInt16      GetDepth() const { return nDepth_; }
    const SalX11Screen&     GetXScreenNumber() const { return m_nXScreen; }

    virtual SalGraphics*    AcquireGraphics() override;
    virtual void            ReleaseGraphics( SalGraphics* pGraphics ) override;

    /// Set new size, without saving the old contents
    virtual bool        SetSize( tools::Long nNewDX, tools::Long nNewDY, bool bAlphaMaskTransparent ) override;

    // SalGeometryProvider
    virtual tools::Long GetWidth() const override { return nDX_; }
    virtual tools::Long GetHeight() const override { return nDY_; }
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
