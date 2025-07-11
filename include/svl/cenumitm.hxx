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

#ifndef INCLUDED_SVL_CENUMITM_HXX
#define INCLUDED_SVL_CENUMITM_HXX

#include <svl/svldllapi.h>
#include <svl/poolitem.hxx>

class SVL_DLLPUBLIC SfxEnumItemInterface: public SfxPoolItem
{
protected:
    explicit SfxEnumItemInterface(sal_uInt16 which): SfxPoolItem(which) {}

    SfxEnumItemInterface(const SfxEnumItemInterface &) = default;

public:
    DECLARE_ITEM_TYPE_FUNCTION(SfxEnumItemInterface)

    virtual bool operator ==(const SfxPoolItem & rItem) const override;

    virtual bool GetPresentation(SfxItemPresentation, MapUnit, MapUnit,
                                 OUString & rText,
                                 const IntlWrapper&) const override;

    virtual bool QueryValue(css::uno::Any & rVal, sal_uInt8 = 0) const override;

    virtual bool PutValue(const css::uno::Any & rVal, sal_uInt8 ) override;

    virtual sal_uInt16 GetEnumValue() const = 0;

    virtual void SetEnumValue(sal_uInt16 nValue) = 0;

    virtual bool HasBoolValue() const;

    virtual bool GetBoolValue() const;

    virtual void SetBoolValue(bool bValue);
};

#endif // INCLUDED_SVL_CENUMITM_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
