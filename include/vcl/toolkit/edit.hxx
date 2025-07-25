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

#if !defined(VCL_DLLIMPLEMENTATION) && !defined(TOOLKIT_DLLIMPLEMENTATION) && !defined(VCL_INTERNALS)
#error "don't use this in new code"
#endif

#include <vcl/ctrl.hxx>
#include <vcl/textfilter.hxx>

#include <memory>

#include <rtl/ustrbuf.hxx>
#include <o3tl/deleter.hxx>
#include <vcl/dllapi.h>
#include <vcl/dndhelp.hxx>
#include <vcl/vclptr.hxx>
#include <com/sun/star/uno/Reference.h>

namespace com::sun::star::i18n {
    class XBreakIterator;
    class XExtendedInputSequenceChecker;
}
namespace weld {
    class Widget;
}

class PopupMenu;
class VclBuilder;
struct DragDropInfo;
struct Impl_IMEInfos;

#define EDIT_NOLIMIT                SAL_MAX_INT32

class Timer;

class VCL_DLLPUBLIC Edit : public Control, public vcl::unohelper::DragAndDropClient
{
private:
    VclPtr<Edit>        mpSubEdit;
    TextFilter*         mpFilterText;
    std::unique_ptr<DragDropInfo, o3tl::default_delete<DragDropInfo>> mpDDInfo;
    std::unique_ptr<Impl_IMEInfos> mpIMEInfos;
    OUStringBuffer      maText;
    OUString            maPlaceholderText;
    OUString            maSaveValue;
    OUString            maUndoText;
    tools::Long                mnXOffset;
    Selection           maSelection;
    sal_uInt16          mnAlign;
    sal_Int32           mnMaxTextLen;
    sal_Int32           mnWidthInChars;
    sal_Int32           mnMaxWidthChars;
    sal_Unicode         mcEchoChar;
    bool                mbInternModified:1,
                        mbReadOnly:1,
                        mbInsertMode:1,
                        mbClickedInSelection:1,
                        mbIsSubEdit:1,
                        mbActivePopup:1,
                        mbForceControlBackground:1,
                        mbPassword;
    Link<Edit&,void>    maModifyHdl;
    Link<Edit&,void>    maAutocompleteHdl;
    Link<Edit&,bool>    maActivateHdl;
    std::unique_ptr<VclBuilder> mpUIBuilder;

    css::uno::Reference<css::i18n::XBreakIterator> mxBreakIterator;
    css::uno::Reference<css::i18n::XExtendedInputSequenceChecker> mxISC;
    rtl::Reference<vcl::unohelper::DragAndDropWrapper> mxDnDListener;

    SAL_DLLPRIVATE bool        ImplTruncateToMaxLen( OUString&, sal_Int32 nSelectionLen ) const;
    SAL_DLLPRIVATE void        ImplInitEditData();
    SAL_DLLPRIVATE OUString    ImplGetText() const;
    SAL_DLLPRIVATE void        ImplRepaint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRectangle);
    SAL_DLLPRIVATE void        ImplInvalidateOrRepaint();
    SAL_DLLPRIVATE void        ImplDelete( const Selection& rSelection, sal_uInt8 nDirection, sal_uInt8 nMode );
    SAL_DLLPRIVATE void        ImplSetText( const OUString& rStr, const Selection* pNewSelection );
    SAL_DLLPRIVATE void        ImplInsertText( const OUString& rStr, const Selection* pNewSelection = nullptr, bool bIsUserInput = false );
    SAL_DLLPRIVATE static OUString ImplGetValidString( const OUString& rString );
    SAL_DLLPRIVATE void        ImplClearBackground(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRectangle, tools::Long nXStart, tools::Long nXEnd);
    SAL_DLLPRIVATE void        ImplPaintBorder(vcl::RenderContext const & rRenderContext);
    SAL_DLLPRIVATE void        ImplShowCursor( bool bOnlyIfVisible = true );
    SAL_DLLPRIVATE void        ImplAlign();
    SAL_DLLPRIVATE void        ImplAlignAndPaint();
    SAL_DLLPRIVATE sal_Int32   ImplGetCharPos( const Point& rWindowPos ) const;
    SAL_DLLPRIVATE void        ImplSetCursorPos( sal_Int32 nChar, bool bSelect );
    SAL_DLLPRIVATE void        ImplShowDDCursor();
    SAL_DLLPRIVATE void        ImplHideDDCursor();
    SAL_DLLPRIVATE bool        ImplHandleKeyEvent( const KeyEvent& rKEvt );
    SAL_DLLPRIVATE void        ImplCopyToSelectionClipboard();
    SAL_DLLPRIVATE void        ImplCopy(css::uno::Reference<css::datatransfer::clipboard::XClipboard> const & rxClipboard);
    SAL_DLLPRIVATE void        ImplPaste(css::uno::Reference<css::datatransfer::clipboard::XClipboard> const & rxClipboard);
    SAL_DLLPRIVATE tools::Long        ImplGetTextYPosition() const;
    SAL_DLLPRIVATE css::uno::Reference<css::i18n::XExtendedInputSequenceChecker> const& ImplGetInputSequenceChecker();
    SAL_DLLPRIVATE css::uno::Reference<css::i18n::XBreakIterator> const& ImplGetBreakIterator();
    SAL_DLLPRIVATE void        filterText();

protected:
    using Control::ImplInitSettings;
    using Window::ImplInit;
    SAL_DLLPRIVATE void        ImplInit( vcl::Window* pParent, WinBits nStyle );
    SAL_DLLPRIVATE static WinBits ImplInitStyle( WinBits nStyle );
    SAL_DLLPRIVATE void        ImplSetSelection( const Selection& rSelection, bool bPaint = true );
    SAL_DLLPRIVATE ControlType ImplGetNativeControlType() const;
    SAL_DLLPRIVATE tools::Long        ImplGetExtraXOffset() const;
    SAL_DLLPRIVATE tools::Long        ImplGetExtraYOffset() const;
    static SAL_DLLPRIVATE void ImplInvalidateOutermostBorder( vcl::Window* pWin );

    // DragAndDropClient
    using vcl::unohelper::DragAndDropClient::dragEnter;
    using vcl::unohelper::DragAndDropClient::dragExit;
    using vcl::unohelper::DragAndDropClient::dragOver;
    virtual void dragGestureRecognized(const css::datatransfer::dnd::DragGestureEvent& dge) override;
    virtual void dragDropEnd(const css::datatransfer::dnd::DragSourceDropEvent& dsde) override;
    virtual void drop(const css::datatransfer::dnd::DropTargetDropEvent& dtde) override;
    virtual void dragEnter(const css::datatransfer::dnd::DropTargetDragEnterEvent& dtdee) override;
    virtual void dragExit(const css::datatransfer::dnd::DropTargetEvent& dte) override;
    virtual void dragOver(const css::datatransfer::dnd::DropTargetDragEvent& dtde) override;

protected:
    SAL_DLLPRIVATE Edit(WindowType eType);
    virtual void FillLayoutData() const override;
    virtual void ApplySettings(vcl::RenderContext& rRenderContext) override;
public:
    // public because needed in button.cxx
    SAL_DLLPRIVATE bool ImplUseNativeBorder(vcl::RenderContext const & rRenderContext, WinBits nStyle) const;

    Edit( vcl::Window* pParent, WinBits nStyle = WB_BORDER );
    virtual ~Edit() override;
    virtual void dispose() override;

    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible() override;

    virtual void        MouseButtonDown( const MouseEvent& rMEvt ) override;
    virtual void        MouseButtonUp( const MouseEvent& rMEvt ) override;
    virtual void        KeyInput( const KeyEvent& rKEvt ) override;
    virtual void        Paint( vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect ) override;
    virtual void        Resize() override;
    virtual void        Draw( OutputDevice* pDev, const Point& rPos, SystemTextColorFlags nFlags ) override;
    virtual void        GetFocus() override;
    virtual void        LoseFocus() override;
    virtual void        Tracking( const TrackingEvent& rTEvt ) override;
    virtual void        Command( const CommandEvent& rCEvt ) override;
    virtual void        StateChanged( StateChangedType nType ) override;
    virtual void        DataChanged( const DataChangedEvent& rDCEvt ) override;
    virtual bool        PreNotify(NotifyEvent& rNEvt) override;

    virtual void        Modify();

    SAL_DLLPRIVATE static bool IsCharInput( const KeyEvent& rKEvt );

    virtual void        SetModifyFlag();

    void                SetEchoChar( sal_Unicode c );
    sal_Unicode         GetEchoChar() const { return mcEchoChar; }

    virtual void        SetReadOnly( bool bReadOnly = true );
    virtual bool        IsReadOnly() const { return mbReadOnly; }

    SAL_DLLPRIVATE void SetInsertMode( bool bInsert );
    SAL_DLLPRIVATE bool IsInsertMode() const;

    virtual void        SetMaxTextLen( sal_Int32 nMaxLen );
    virtual sal_Int32   GetMaxTextLen() const { return mnMaxTextLen; }

    SAL_DLLPRIVATE void SetWidthInChars(sal_Int32 nWidthInChars);
    sal_Int32           GetWidthInChars() const { return mnWidthInChars; }

    SAL_DLLPRIVATE void setMaxWidthChars(sal_Int32 nWidth);

    virtual void        SetSelection( const Selection& rSelection );
    virtual const Selection&    GetSelection() const;

    virtual void        ReplaceSelected( const OUString& rStr );
    virtual void        DeleteSelected();
    virtual OUString    GetSelected() const;

    virtual void        Cut();
    virtual void        Copy();
    virtual void        Paste();
    SAL_DLLPRIVATE void Undo();

    virtual void        SetText( const OUString& rStr ) override;
    virtual void        SetText( const OUString& rStr, const Selection& rNewSelection );
    virtual OUString    GetText() const override;

    SAL_DLLPRIVATE void SetCursorAtLast();

    SAL_DLLPRIVATE void SetPlaceholderText( const OUString& rStr );

    void                SaveValue() { maSaveValue = GetText(); }
    const OUString&     GetSavedValue() const { return maSaveValue; }

    virtual void        SetModifyHdl( const Link<Edit&,void>& rLink ) { maModifyHdl = rLink; }
    virtual const Link<Edit&,void>& GetModifyHdl() const { return maModifyHdl; }

    void                SetActivateHdl(const Link<Edit&,bool>& rLink) { maActivateHdl = rLink; }

    SAL_DLLPRIVATE void SetSubEdit( Edit* pEdit );
    Edit*               GetSubEdit() const { return mpSubEdit; }

    void                SetAutocompleteHdl( const Link<Edit&,void>& rLink ) { maAutocompleteHdl = rLink; }
    const Link<Edit&,void>& GetAutocompleteHdl() const { return maAutocompleteHdl; }

    virtual Size        CalcMinimumSize() const;
    virtual Size        CalcMinimumSizeForText(const OUString &rString) const;
    virtual Size        GetOptimalSize() const override;
    virtual Size        CalcSize(sal_Int32 nChars) const;
    sal_Int32           GetMaxVisChars() const;

    // shows a warning box saying "text too long, truncated"
    SAL_DLLPRIVATE static void ShowTruncationWarning(weld::Widget* pParent);

    SAL_DLLPRIVATE VclPtr<PopupMenu>           CreatePopupMenu();

    virtual OUString GetSurroundingText() const override;
    virtual Selection GetSurroundingTextSelection() const override;
    virtual bool DeleteSurroundingText(const Selection& rSelection) override;
    virtual bool set_property(const OUString &rKey, const OUString &rValue) override;

    void SetTextFilter(TextFilter* pFilter) { mpFilterText = pFilter; }

    virtual FactoryFunction GetUITestFactory() const override;

    void SetForceControlBackground(bool b) { mbForceControlBackground = b; }

    bool IsPassword() const { return mbPassword; }

    bool IsActivePopup() const { return mbActivePopup; }

    virtual void DumpAsPropertyTree(tools::JsonWriter& rJsonWriter) override;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
