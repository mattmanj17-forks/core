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

#include <svx/svdmodel.hxx>
#include <svx/svdoutl.hxx>
#include <svx/svdetc.hxx>

#include <futext.hxx>
#include <tabvwsh.hxx>

std::unique_ptr<SdrOutliner> FuText::MakeOutliner()
{
    ScViewData& rViewData = rViewShell.GetViewData();
    std::unique_ptr<SdrOutliner> pOutl = SdrMakeOutliner(OutlinerMode::OutlineObject, rDrDoc);

    rViewData.UpdateOutlinerFlags(*pOutl);

    //  The EditEngine uses during RTF export (Clipboard / Drag&Drop)
    //  the MapMode of RefDevice to set the font size

    //  #i10426# The ref device isn't set to the EditEngine before SdrBeginTextEdit now,
    //  so the device must be taken from the model here.
    OutputDevice* pRef = rDrDoc.GetRefDevice();
    if (pRef && pRef != pWindow->GetOutDev())
        pRef->SetMapMode(MapMode(MapUnit::Map100thMM));

    return pOutl;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
