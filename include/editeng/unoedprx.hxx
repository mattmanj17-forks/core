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

#ifndef INCLUDED_EDITENG_UNOEDPRX_HXX
#define INCLUDED_EDITENG_UNOEDPRX_HXX

#include <config_options.h>
#include <memory>
#include <svl/SfxBroadcaster.hxx>
#include <tools/fontenum.hxx>
#include <editeng/unoedsrc.hxx>

#include <editeng/editdata.hxx>
#include <editeng/editengdllapi.h>

class SvxAccessibleTextAdapter final : public SvxTextForwarder
{
public:
    SvxAccessibleTextAdapter();
    virtual ~SvxAccessibleTextAdapter() override;

    virtual sal_Int32       GetParagraphCount() const override;
    virtual sal_Int32       GetTextLen( sal_Int32 nParagraph ) const override;
    virtual OUString        GetText( const ESelection& rSel ) const override;
    virtual SfxItemSet      GetAttribs( const ESelection& rSel, EditEngineAttribs nOnlyHardAttrib = EditEngineAttribs::All ) const override;
    virtual SfxItemSet      GetParaAttribs( sal_Int32 nPara ) const override;
    virtual void            SetParaAttribs( sal_Int32 nPara, const SfxItemSet& rSet ) override;
    virtual void            RemoveAttribs( const ESelection& rSelection ) override;
    virtual void            GetPortions( sal_Int32 nPara, std::vector<sal_Int32>& rList ) const override;

    sal_Int32               CalcEditEngineIndex( sal_Int32 nPara, sal_Int32 nLogicalIndex );

    virtual OUString        GetStyleSheet(sal_Int32 nPara) const override;
    virtual void            SetStyleSheet(sal_Int32 nPara, const OUString& rStyleName) override;

    virtual SfxItemState    GetItemState( const ESelection& rSel, sal_uInt16 nWhich ) const override;
    virtual SfxItemState    GetItemState( sal_Int32 nPara, sal_uInt16 nWhich ) const override;

    virtual void            QuickInsertText( const OUString& rText, const ESelection& rSel ) override;
    virtual void            QuickInsertField( const SvxFieldItem& rFld, const ESelection& rSel ) override;
    virtual void            QuickSetAttribs( const SfxItemSet& rSet, const ESelection& rSel ) override;
    virtual void            QuickInsertLineBreak( const ESelection& rSel ) override;

    virtual SfxItemPool*    GetPool() const override;

    virtual OUString        CalcFieldValue( const SvxFieldItem& rField, sal_Int32 nPara, sal_Int32 nPos, std::optional<Color>& rpTxtColor, std::optional<Color>& rpFldColor, std::optional<FontLineStyle>& rpFldLineStyle ) override;
    virtual void            FieldClicked( const SvxFieldItem& rField ) override;

    virtual bool            IsValid() const override;

    virtual LanguageType    GetLanguage( sal_Int32, sal_Int32 ) const override;
    virtual std::vector<EFieldInfo> GetFieldInfo( sal_Int32 nPara ) const override;
    virtual EBulletInfo     GetBulletInfo( sal_Int32 nPara ) const override;
    virtual tools::Rectangle       GetCharBounds( sal_Int32 nPara, sal_Int32 nIndex ) const override;
    virtual tools::Rectangle       GetParaBounds( sal_Int32 nPara ) const override;
    virtual MapMode         GetMapMode() const override;
    virtual OutputDevice*   GetRefDevice() const override;
    virtual bool            GetIndexAtPoint( const Point&, sal_Int32& nPara, sal_Int32& nIndex ) const override;
    virtual bool            GetWordIndices( sal_Int32 nPara, sal_Int32 nIndex, sal_Int32& nStart, sal_Int32& nEnd ) const override;
    virtual bool            GetAttributeRun( sal_Int32& nStartIndex, sal_Int32& nEndIndex, sal_Int32 nPara, sal_Int32 nIndex, bool bInCell = false ) const override;
    virtual sal_Int32       GetLineCount( sal_Int32 nPara ) const override;
    virtual sal_Int32       GetLineLen( sal_Int32 nPara, sal_Int32 nLine ) const override;
    virtual void            GetLineBoundaries( /*out*/sal_Int32 &rStart, /*out*/sal_Int32 &rEnd, sal_Int32 nParagraph, sal_Int32 nLine ) const override;
    virtual sal_Int32       GetLineNumberAtIndex( sal_Int32 nPara, sal_Int32 nIndex ) const override;

    virtual bool            Delete( const ESelection& ) override;
    virtual bool            InsertText( const OUString&, const ESelection& ) override;
    virtual bool            QuickFormatDoc( bool bFull = false ) override;
    virtual bool SupportsOutlineDepth() const override;
    virtual sal_Int16       GetDepth( sal_Int32 nPara ) const override;
    virtual bool            SetDepth( sal_Int32 nPara, sal_Int16 nNewDepth ) override;

    virtual const SfxItemSet*   GetEmptyItemSetPtr() override;

    // implementation functions for XParagraphAppend and XTextPortionAppend
    // (not needed for accessibility, only for new import API)
    virtual void        AppendParagraph() override;
    virtual sal_Int32   AppendTextPortion( sal_Int32 nPara, const OUString &rText, const SfxItemSet &rSet ) override;

    //XTextCopy
    virtual void        CopyText(const SvxTextForwarder& rSource) override;

    void                SetForwarder( SvxTextForwarder& );
    bool                HaveImageBullet( sal_Int32 nPara ) const;
    bool                HaveTextBullet( sal_Int32 nPara ) const;

    /** Query whether all text in given selection is editable

        @return sal_True if every character in the given selection can
        be changed, and sal_False if e.g. a field or a bullet is
        contained therein.
     */
    bool                IsEditable( const ESelection& rSelection ) const;

private:
    SvxTextForwarder* mpTextForwarder;
};

class SvxAccessibleTextEditViewAdapter final : public SvxEditViewForwarder
{
public:

                        SvxAccessibleTextEditViewAdapter();
    virtual             ~SvxAccessibleTextEditViewAdapter() override;

    // SvxViewForwarder interface
    virtual bool        IsValid() const override;
    virtual Point       LogicToPixel( const Point& rPoint, const MapMode& rMapMode ) const override;
    virtual Point       PixelToLogic( const Point& rPoint, const MapMode& rMapMode ) const override;

    // SvxEditViewForwarder interface
    virtual bool        GetSelection( ESelection& rSelection ) const override;
    virtual bool        SetSelection( const ESelection& rSelection ) override;
    virtual bool        Copy() override;
    virtual bool        Cut() override;
    virtual bool        Paste() override;

    void                SetForwarder( SvxEditViewForwarder&, SvxAccessibleTextAdapter& );

private:
    SvxEditViewForwarder*       mpViewForwarder;
    SvxAccessibleTextAdapter*   mpTextForwarder;
};

class UNLESS_MERGELIBS(EDITENG_DLLPUBLIC) SvxEditSourceAdapter final : public SvxEditSource
{
public:
    SvxEditSourceAdapter();
    virtual ~SvxEditSourceAdapter() override;

    virtual std::unique_ptr<SvxEditSource>      Clone() const override;
    virtual SvxTextForwarder*                   GetTextForwarder() override;
    SvxAccessibleTextAdapter*                   GetTextForwarderAdapter(); // covariant return types don't work on MSVC
    virtual SvxViewForwarder*                   GetViewForwarder() override;
    virtual SvxEditViewForwarder*               GetEditViewForwarder( bool bCreate = false ) override;
    SvxAccessibleTextEditViewAdapter*           GetEditViewForwarderAdapter( bool bCreate ); // covariant return types don't work on MSVC
    virtual void                                UpdateData() override;
    virtual SfxBroadcaster&                     GetBroadcaster() const override;

    void        SetEditSource( ::std::unique_ptr< SvxEditSource > && pAdaptee );

    bool        IsValid() const { return mbEditSourceValid;}

private:
    SvxEditSourceAdapter( const SvxEditSourceAdapter& ) = delete;
    SvxEditSourceAdapter& operator= ( const SvxEditSourceAdapter& ) = delete;

    ::std::unique_ptr< SvxEditSource >    mpAdaptee;

    SvxAccessibleTextAdapter            maTextAdapter;
    SvxAccessibleTextEditViewAdapter    maEditViewAdapter;

    mutable SfxBroadcaster              maDummyBroadcaster;

    bool                                mbEditSourceValid;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
