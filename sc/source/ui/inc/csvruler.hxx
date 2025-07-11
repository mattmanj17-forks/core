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

#include "csvcontrol.hxx"
#include "csvsplits.hxx"

#include <vcl/virdev.hxx>

class ScCsvTableBox;

/** A ruler control for the CSV import dialog. Supports setting and moving
    splits (which divide lines of data into several columns). */
class ScCsvRuler : public ScCsvControl
{
private:
    ScCsvTableBox*              mpTableBox;         /// Grid Parent

    ScopedVclPtrInstance<VirtualDevice> maBackgrDev;/// Ruler background, scaling.
    ScopedVclPtrInstance<VirtualDevice> maRulerDev; /// Ruler with splits and cursor.

    Color                       maBackColor;        /// Background color.
    Color                       maActiveColor;      /// Color for active part of ruler.
    Color                       maTextColor;        /// Text and scale color.
    Color                       maSplitColor;       /// Split area color.

    ScCsvSplits                 maSplits;           /// Vector with split positions.
    ScCsvSplits                 maOldSplits;        /// Old state for cancellation.

    sal_Int32                   mnPosCursorLast;    /// Last valid position of cursor.
    sal_Int32                   mnPosMTStart;       /// Start position of mouse tracking.
    sal_Int32                   mnPosMTCurr;        /// Current position of mouse tracking.
    bool                        mbPosMTMoved;       /// Tracking: Anytime moved to another position?

    Size                        maWinSize;          /// Size of the control.
    tools::Rectangle            maActiveRect;       /// The active area of the ruler.
    sal_Int32                   mnSplitSize;        /// Size of a split circle.
    bool                        mbTracking;         /// If currently mouse tracking

public:
    explicit ScCsvRuler(const ScCsvLayoutData& rData, ScCsvTableBox* pTableBox);
    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override;
    ScCsvTableBox* GetTableBox() { return mpTableBox; }
    virtual ~ScCsvRuler() override;

    // common ruler handling --------------------------------------------------
public:
    /** Apply current layout data to the ruler. */
    void                        ApplyLayout( const ScCsvLayoutData& rOldData );

private:
    /** Reads colors from system settings. */
    void                        InitColors();
    /** Initializes all data dependent from the control's size. */
    void                        InitSizeData();

    /** Moves cursor to a new position.
        @param bScroll  sal_True = The method may scroll the ruler. */
    void                        MoveCursor( sal_Int32 nPos, bool bScroll = true );
    /** Moves cursor to the given direction. */
    void                        MoveCursorRel( ScMoveMode eDir );
    /** Sets cursor to an existing split, according to eDir. */
    void                        MoveCursorToSplit( ScMoveMode eDir );
    /** Scrolls data grid vertically. */
    void                        ScrollVertRel( ScMoveMode eDir );

    // split handling ---------------------------------------------------------
public:
    /** Returns the split array. */
    const ScCsvSplits&   GetSplits() const { return maSplits; }
    /** Returns the number of splits. */
    sal_uInt32           GetSplitCount() const
                                    { return maSplits.Count(); }
    /** Returns the position of the specified split. */
    sal_Int32            GetSplitPos( sal_uInt32 nIndex ) const
                                    { return maSplits[ nIndex ]; }
    /** Finds a position nearest to nPos which does not cause scrolling the visible area. */
    sal_Int32                   GetNoScrollPos( sal_Int32 nPos ) const;

    /** Returns true if at position nPos is a split. */
    bool                 HasSplit( sal_Int32 nPos ) const { return maSplits.HasSplit( nPos ); }
    /** Inserts a split. */
    void                        InsertSplit( sal_Int32 nPos );
    /** Removes a split. */
    void                        RemoveSplit( sal_Int32 nPos );
    /** Moves a split from nPos to nNewPos. */
    void                        MoveSplit( sal_Int32 nPos, sal_Int32 nNewPos );
    /** Removes all splits of the ruler. */
    void                        RemoveAllSplits();

private:
    /** Finds next position without a split. */
    sal_Int32                   FindEmptyPos( sal_Int32 nPos, ScMoveMode eDir ) const;

    /** Moves split and cursor to nNewPos and commits event. */
    void                        MoveCurrSplit( sal_Int32 nNewPos );
    /** Moves split and cursor to the given direction and commits event. */
    void                        MoveCurrSplitRel( ScMoveMode eDir );

    // event handling ---------------------------------------------------------
protected:
    virtual void                Resize() override;
    virtual void                GetFocus() override;
    virtual void                LoseFocus() override;
    virtual void                StyleUpdated() override;

    virtual bool                MouseButtonDown( const MouseEvent& rMEvt ) override;
    virtual bool                MouseMove( const MouseEvent& rMEvt ) override;
    virtual bool                MouseButtonUp( const MouseEvent& rMEvt ) override;

    virtual bool                KeyInput( const KeyEvent& rKEvt ) override;

    virtual tools::Rectangle    GetFocusRect() override;

private:
    /** Starts tracking at the specified position. */
    void                        StartMouseTracking( sal_Int32 nPos );
    /** Moves tracking to a new position. */
    void                        MoveMouseTracking( sal_Int32 nPos );
    /** Applies tracking action for the current tracking position */
    void                        EndMouseTracking();

    // painting ---------------------------------------------------------------
protected:
    virtual void                Paint( vcl::RenderContext& rRenderContext, const tools::Rectangle& ) override;

public:
    /** Redraws the entire ruler. */
    void                        ImplRedraw(vcl::RenderContext& rRenderContext);

private:
    /** Returns the width of the control. */
    sal_Int32            GetWidth() const { return maWinSize.Width(); }
    /** Returns the height of the control. */
    sal_Int32            GetHeight() const { return maWinSize.Height(); }
    /** Update the split size depending on the last width set by CSVCMD_SETCHARWIDTH */
    void UpdateSplitSize();

    /** Draws the background and active area to maBackgrDev (only the given X range). */
    void                        ImplDrawArea( sal_Int32 nPosX, sal_Int32 nWidth );
    /** Draws the entire ruler background with scaling to maBackgrDev. */
    void                        ImplDrawBackgrDev();

    /** Draws a split to maRulerDev. */
    void                        ImplDrawSplit( sal_Int32 nPos );
    /** Erases a split from maRulerDev. */
    void                        ImplEraseSplit( sal_Int32 nPos );
    /** Draws the ruler background, all splits and the cursor to maRulerDev. */
    void                        ImplDrawRulerDev();

    /** Inverts the cursor bar at the specified position in maRulerDev. */
    void                        ImplInvertCursor( sal_Int32 nPos );

    /** Sets arrow or horizontal split pointer. */
    void                        ImplSetMousePointer( sal_Int32 nPos );

    // accessibility ----------------------------------------------------------
protected:
    /** Creates a new accessible object. */
    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible() override;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
