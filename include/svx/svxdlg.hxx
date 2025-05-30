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
#ifndef INCLUDED_SVX_SVXDLG_HXX
#define INCLUDED_SVX_SVXDLG_HXX

#include <sfx2/sfxdlg.hxx>
#include <svx/anchorid.hxx>
#include <svx/zoom_def.hxx>
#include <editeng/edtdlg.hxx>

#include <com/sun/star/container/XIndexContainer.hpp>
#include <com/sun/star/container/XNameReplace.hpp>
#include <svx/svxdllapi.h>
#include <vector>

namespace com::sun::star::linguistic2 {
    class XDictionary;
}
class SdrModel;
class SdrView;
enum class TransliterationFlags;
struct ExchangeData;
class GalleryTheme;
class SearchAttrItemList;
class Graphic;
class SdrObject;
struct FmSearchContext;

typedef const WhichRangesContainer & (*DialogGetRanges)();

typedef ::std::vector< OUString > TargetList;

namespace svx{ class SpellDialogChildWindow;}
namespace weld{
    class Dialog;
    class Window;
}

class AbstractFmShowColsDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractFmShowColsDialog() override = default;
public:
     virtual void SetColumns(const css::uno::Reference< css::container::XIndexContainer>& xCols)= 0;
};

class AbstractSvxZoomDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxZoomDialog() override = default;
public:
    virtual void    SetLimits( sal_uInt16 nMin, sal_uInt16 nMax ) = 0;
    virtual void    HideButton( ZoomButtonId nBtnId )= 0;
    virtual const SfxItemSet*   GetOutputItemSet() const = 0 ;
};

class AbstractSpellDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSpellDialog() override = default;
public:
    virtual void InvalidateDialog() = 0;
    virtual std::shared_ptr<SfxDialogController> GetController() = 0;
    virtual SfxBindings& GetBindings() = 0;
};

class AbstractTitleDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractTitleDialog() override = default;
public:
    virtual OUString  GetTitle() const =0;
};

class AbstractGalleryIdDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractGalleryIdDialog() override = default;
public:
    virtual sal_uInt32 GetId() const =0;
};

class AbstractURLDlg :public VclAbstractDialog
{
protected:
    virtual ~AbstractURLDlg() override = default;
public:
    virtual OUString      GetURL() const = 0;
    virtual OUString      GetAltText() const = 0;
    virtual OUString      GetDesc() const = 0;
    virtual OUString      GetTarget() const = 0;
    virtual OUString      GetName() const = 0;
};

class AbstractSvxSearchSimilarityDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxSearchSimilarityDialog() override = default;
public:
    virtual sal_uInt16  GetOther() =0 ;
    virtual sal_uInt16  GetShorter() =0 ;
    virtual sal_uInt16  GetLonger() =0 ;
    virtual bool        IsRelaxed() =0 ;
};

class AbstractSvxJSearchOptionsDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxJSearchOptionsDialog() override = default;
public:
    virtual TransliterationFlags GetTransliterationFlags() const = 0;
};

class AbstractFmInputRecordNoDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractFmInputRecordNoDialog() override = default;
public:
    virtual void SetValue(tools::Long dNew) = 0;
    virtual tools::Long GetValue() const = 0;
};

class AbstractSvxNewDictionaryDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxNewDictionaryDialog() override = default;
public:
    virtual css::uno::Reference< css::linguistic2::XDictionary >  GetNewDictionary() = 0;
};

class AbstractSvxNameDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxNameDialog() override = default;
public:
    virtual OUString GetName() = 0;
    virtual void    SetCheckNameHdl( const Link<AbstractSvxNameDialog&,bool>& rLink ) = 0;
    virtual void    SetCheckNameTooltipHdl( const Link<AbstractSvxNameDialog&,OUString>& rLink ) = 0;
    virtual void    SetEditHelpId(const OUString&) = 0;
    virtual void    SetText( const OUString& rStr ) = 0;
};

class AbstractSvxObjectNameDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxObjectNameDialog() override = default;
public:
    virtual OUString GetName() = 0;
    virtual void SetCheckNameHdl(const Link<AbstractSvxObjectNameDialog&,bool>& rLink) = 0;
};

class AbstractSvxObjectTitleDescDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxObjectTitleDescDialog() override = default;
public:
    virtual OUString GetTitle() = 0;
    virtual OUString GetDescription() = 0;
    virtual bool IsDecorative() = 0;
};

/// Abstract class provides the get information from the numbering and position dialog.
class AbstractSvxBulletAndPositionDlg :public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxBulletAndPositionDlg() override = default;
public:
    virtual const SfxItemSet*   GetOutputItemSet( SfxItemSet* pSet) const = 0 ;
    virtual bool IsApplyToMaster() = 0;
    virtual bool IsSlideScope() = 0;
};

class AbstractSvxMultiPathDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxMultiPathDialog() override = default;
public:
    virtual OUString        GetPath() const = 0;
    virtual void            SetPath( const OUString& rPath ) = 0;
    virtual void            SetTitle( const OUString& rNewTitle ) = 0;
};

class AbstractSvxHpLinkDlg : public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxHpLinkDlg() override = default;
public:
    virtual std::shared_ptr<SfxDialogController> GetController()  = 0;
    virtual bool       QueryClose() = 0;
};

struct FmFoundRecordInformation;
class AbstractFmSearchDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractFmSearchDialog() override = default;
public:
    virtual void SetFoundHandler(const Link<FmFoundRecordInformation&,void>& lnk) = 0;
    virtual void SetCanceledNotFoundHdl(const Link<FmFoundRecordInformation&,void>& lnk)=0;
    virtual void SetActiveField(const OUString& strField)=0;
};

class AbstractGraphicFilterDialog :public VclAbstractDialog
{
protected:
    virtual ~AbstractGraphicFilterDialog() override = default;
public:
    virtual Graphic GetFilteredGraphic( const Graphic& rGraphic, double fScaleX, double fScaleY ) = 0;
};

class AbstractSvxAreaTabDialog :public SfxAbstractTabDialog
{
protected:
    virtual ~AbstractSvxAreaTabDialog() override = default;
};

struct SvxSwFrameValidation;
class AbstractSvxTransformTabDialog : public SfxAbstractTabDialog
{
protected:
    virtual ~AbstractSvxTransformTabDialog() override = default;
public:
    virtual void SetValidateFramePosLink( const Link<SvxSwFrameValidation&,void>& rLink ) = 0;
};

class AbstractSvxCaptionDialog : public SfxAbstractTabDialog
{
protected:
    virtual ~AbstractSvxCaptionDialog() override = default;
public:
    virtual void SetValidateFramePosLink( const Link<SvxSwFrameValidation&,void>& rLink ) = 0;
};

class AbstractSvxPostItDialog : public VclAbstractDialog
{
protected:
    virtual ~AbstractSvxPostItDialog() override = default;
public:
    virtual void                SetText( const OUString& rStr ) = 0;
    virtual const SfxItemSet*   GetOutputItemSet() const = 0;
    virtual void                SetPrevHdl( const Link<AbstractSvxPostItDialog&,void>& rLink ) = 0;
    virtual void                SetNextHdl( const Link<AbstractSvxPostItDialog&,void>& rLink ) = 0;
    virtual void                EnableTravel(bool bNext, bool bPrev) = 0;
    virtual OUString            GetNote()  = 0;
    virtual void                SetNote(const OUString& rTxt)  = 0;
    virtual void                ShowLastAuthor(const OUString& rAuthor, const OUString& rDate) = 0;
    virtual void                DontChangeAuthor() = 0;
    virtual void                HideAuthor() = 0;
    virtual std::shared_ptr<weld::Dialog> GetDialog() = 0;
};

class SvxAbstractSplitTableDialog : public VclAbstractDialog
{
protected:
    virtual ~SvxAbstractSplitTableDialog() override = default;
public:
    virtual bool IsHorizontal() const = 0;
    virtual bool IsProportional() const = 0;
    virtual tools::Long GetCount() const = 0;
    virtual void SetSplitVerticalByDefault() = 0;
};

class SvxAbstractNewTableDialog
{
protected:
    virtual ~SvxAbstractNewTableDialog() = default;
public:
    virtual sal_Int32 getRows() const = 0;
    virtual sal_Int32 getColumns() const = 0;
    virtual std::shared_ptr<weld::DialogController> getDialogController() = 0;
};

class SvxAbstractInsRowColDlg : public VclAbstractDialog
{
protected:
    virtual ~SvxAbstractInsRowColDlg() override = default;
public:
    virtual bool isInsertBefore() const = 0;
    virtual sal_uInt16 getInsertCount() const = 0;
};

class SVXCORE_DLLPUBLIC SvxAbstractDialogFactory : public SfxAbstractDialogFactory, public EditAbstractDialogFactory
{
public:
    static SvxAbstractDialogFactory* Create();

    virtual                             ~SvxAbstractDialogFactory() override;

    virtual VclPtr<SfxAbstractTabDialog>      CreateTextTabDialog( weld::Window* pParent,
                                            const SfxItemSet* pAttrSet,
                                            SdrView* pView ) = 0 ;

    virtual VclPtr<AbstractSvxCaptionDialog> CreateCaptionDialog(weld::Window* pParent,
                                                                 const SdrView* pView,
                                                                 SvxAnchorIds nAnchorTypes = SvxAnchorIds::NONE) = 0;

    virtual VclPtr<AbstractFmShowColsDialog> CreateFmShowColsDialog(weld::Window* pParent) = 0;

    virtual VclPtr<AbstractSvxZoomDialog> CreateSvxZoomDialog(weld::Window* pParent, const SfxItemSet& rCoreSet) = 0;

    virtual VclPtr<AbstractSpellDialog>   CreateSvxSpellDialog(weld::Window* pParent,
                                            SfxBindings* pBindings,
                                            svx::SpellDialogChildWindow* pSpellChildWindow )=0;

    virtual VclPtr<VclAbstractDialog> CreateActualizeProgressDialog(weld::Widget* pParent,
                                            GalleryTheme* pThm) = 0;
    virtual VclPtr<AbstractTitleDialog> CreateTitleDialog(weld::Widget* pParent,
                                             const OUString& rOldText) = 0;
    virtual VclPtr<AbstractGalleryIdDialog> CreateGalleryIdDialog(weld::Widget* pParent,
                                            GalleryTheme* pThm) = 0;
    virtual VclPtr<VclAbstractDialog> CreateGalleryThemePropertiesDialog(weld::Widget* pParent,
                                            ExchangeData* pData,
                                            SfxItemSet* pItemSet ) = 0;

    virtual VclPtr<AbstractURLDlg> CreateURLDialog(weld::Widget* pParent,
                                            const OUString& rURL, const OUString& rAltText, const OUString& rDescription,
                                            const OUString& rTarget, const OUString& rName,
                                            TargetList& rTargetList ) = 0;

    virtual VclPtr<SfxAbstractTabDialog> CreateTabItemDialog(weld::Window* pParent, const SfxItemSet& rSet) = 0;
    virtual VclPtr<VclAbstractDialog>      CreateSvxSearchAttributeDialog(weld::Window* pParent,
                                            SearchAttrItemList& rLst,
                                            const WhichRangesContainer& pWhRanges)=0;
    virtual VclPtr<AbstractSvxSearchSimilarityDialog> CreateSvxSearchSimilarityDialog( weld::Window* pParent,
                                                            bool bRelax,
                                                            sal_uInt16 nOther,
                                                            sal_uInt16 nShorter,
                                                            sal_uInt16 nLonger ) = 0;
    // add for SvxBorderBackgroundDlg
    virtual VclPtr<SfxAbstractTabDialog> CreateSvxBorderBackgroundDlg(
        weld::Window* pParent,
        const SfxItemSet& rCoreSet,
        bool bEnableDrawingLayerFillStyles) = 0;

    virtual VclPtr<AbstractSvxTransformTabDialog> CreateSvxTransformTabDialog(weld::Window* pParent,
                                                                              const SfxItemSet* pAttr,
                                                                              const SdrView* pView,
                                                                              SvxAnchorIds nAnchorTypes = SvxAnchorIds::NONE) = 0;
    virtual VclPtr<SfxAbstractTabDialog> CreateSchTransformTabDialog(weld::Window* pParent,
                                                                     const SfxItemSet* pAttr,
                                                                     const SdrView* pSdrView,
                                                                     bool bSizeTabPage) = 0;
    virtual VclPtr<AbstractSvxJSearchOptionsDialog> CreateSvxJSearchOptionsDialog(weld::Window* pParent,
                                                                                  const SfxItemSet& rOptionsSet,
                                                                                  TransliterationFlags nInitialFlags) = 0;
    virtual VclPtr<AbstractFmInputRecordNoDialog> CreateFmInputRecordNoDialog(weld::Window* pParent) = 0;
    virtual VclPtr<AbstractSvxNewDictionaryDialog> CreateSvxNewDictionaryDialog(weld::Window* pParent) = 0;
    virtual VclPtr<VclAbstractDialog>     CreateSvxEditDictionaryDialog(weld::Window* pParent, const OUString& rName) = 0;
    virtual VclPtr<AbstractSvxNameDialog> CreateSvxNameDialog(weld::Window* pParent,
                                            const OUString& rName, const OUString& rDesc, const OUString& rTitle = u""_ustr ) = 0;

    // #i68101#
    virtual VclPtr<AbstractSvxObjectNameDialog> CreateSvxObjectNameDialog(weld::Window* pParent, const OUString& rName) = 0;
    virtual VclPtr<AbstractSvxObjectTitleDescDialog> CreateSvxObjectTitleDescDialog(weld::Window* pParent, const OUString& rTitle, const OUString& rDescription, bool isDecorative) = 0;
    virtual VclPtr<AbstractSvxMultiPathDialog>    CreateSvxMultiPathDialog(weld::Window* pParent) = 0 ;
    virtual VclPtr<AbstractSvxMultiPathDialog>    CreateSvxPathSelectDialog(weld::Window* pParent) = 0 ;
    virtual VclPtr<AbstractSvxHpLinkDlg>  CreateSvxHpLinkDlg(SfxChildWindow* pChild, SfxBindings* pBindings, weld::Window* pParent) = 0;
    virtual VclPtr<AbstractFmSearchDialog> CreateFmSearchDialog(weld::Window* pParent,
                                                        const OUString& strInitialText,
                                                        const ::std::vector< OUString >& _rContexts,
                                                        sal_Int16 nInitialContext,
                                                        const Link<FmSearchContext&,sal_uInt32>& lnkContextSupplier)=0;
    virtual VclPtr<AbstractGraphicFilterDialog>   CreateGraphicFilterEmboss(weld::Window* pParent,
                                                const Graphic& rGraphic)=0;
    virtual VclPtr<AbstractGraphicFilterDialog>   CreateGraphicFilterPoster(weld::Window* pParent,
                                                const Graphic& rGraphic)=0;
    virtual VclPtr<AbstractGraphicFilterDialog>   CreateGraphicFilterSepia(weld::Window* pParent,
                                                const Graphic& rGraphic)=0;
    virtual VclPtr<AbstractGraphicFilterDialog>   CreateGraphicFilterSmooth(weld::Window* pParent,
                                                const Graphic& rGraphic,
                                                double nRadius)=0;
    virtual VclPtr<AbstractGraphicFilterDialog>   CreateGraphicFilterSolarize(weld::Window* pParent,
                                                const Graphic& rGraphic)=0;
    virtual VclPtr<AbstractGraphicFilterDialog>   CreateGraphicFilterMosaic(weld::Window* pParent,
                                                const Graphic& rGraphic)=0;
    virtual VclPtr<AbstractSvxAreaTabDialog>       CreateSvxAreaTabDialog(weld::Window* pParent,
                                                                          const SfxItemSet* pAttr,
                                                                          SdrModel* pModel,
                                                                          bool bShadow,
                                                                          bool bSlideBackground) = 0 ;
    virtual VclPtr<SfxAbstractTabDialog>           CreateSvxLineTabDialog(weld::Window* pParent, const SfxItemSet* pAttr, //add forSvxLineTabDialog
                                                                 SdrModel* pModel,
                                                                 const SdrObject* pObj,
                                                                 bool bHasObj )=0;
    virtual VclPtr<SfxAbstractDialog>       CreateSfxDialog( weld::Window* pParent,
                                                                        const SfxItemSet& rAttr,
                                                                        const SdrView* pView,
                                                                        sal_uInt32 nResId )=0;
    virtual VclPtr<SfxAbstractDialog>       CreateCharMapDialog(weld::Window* pParent, const SfxItemSet& rAttr,
                                                                const css::uno::Reference<css::frame::XFrame>& rFrame) = 0;
    virtual VclPtr<SfxAbstractDialog>       CreateEventConfigDialog(weld::Widget* pParent, std::unique_ptr<const SfxItemSet> xAttr,
                                                                    const css::uno::Reference< css::frame::XFrame >& rFrame) = 0;
    virtual VclPtr<AbstractSvxPostItDialog>    CreateSvxPostItDialog(weld::Widget* pParent, const SfxItemSet& rCoreSet, bool bPrevNext = false) = 0;
    virtual VclPtr<VclAbstractDialog>          CreateSvxScriptOrgDialog(weld::Window* pParent, const OUString& rLanguage) override = 0;

    virtual DialogGetRanges                    GetDialogGetRangesFunc() = 0;

    virtual VclPtr<AbstractSecurityOptionsDialog> CreateSvxSecurityOptionsDialog(weld::Window* pParent) override = 0;

    virtual VclPtr<AbstractScriptSelectorDialog> CreateScriptSelectorDialog(weld::Window* pParent,
            const css::uno::Reference< css::frame::XFrame >& rxFrame) override = 0;

    virtual VclPtr<AbstractMacroManagerDialog> CreateMacroManagerDialog(weld::Window* pParent,
            const css::uno::Reference< css::frame::XFrame >& rxFrame) override = 0;

    virtual void ShowAsyncScriptErrorDialog(weld::Window* pParent, const css::uno::Any& rException) override = 0;

    virtual VclPtr<VclAbstractDialog> CreateSvxMacroAssignDlg(
                weld::Window* _pParent,
                const css::uno::Reference< css::frame::XFrame >& _rxDocumentFrame,
                const bool _bUnoDialogMode,
                const css::uno::Reference< css::container::XNameReplace >& _rxEvents,
                const sal_uInt16 _nInitiallySelectedEvent
            ) = 0;

    virtual VclPtr<SfxAbstractTabDialog> CreateSvxFormatCellsDialog(weld::Window* pParent, const SfxItemSet& rAttr, const SdrModel& rModel, bool bStyle)=0;

    virtual VclPtr<SvxAbstractSplitTableDialog> CreateSvxSplitTableDialog(weld::Window* pParent, bool bIsTableVertical, tools::Long nMaxVertical) = 0;

    virtual std::shared_ptr<SvxAbstractNewTableDialog> CreateSvxNewTableDialog(weld::Window* pParent) = 0;

    virtual VclPtr<SvxAbstractInsRowColDlg> CreateSvxInsRowColDlg(weld::Window* pParent, bool bCol, const OUString& rHelpId) = 0;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
