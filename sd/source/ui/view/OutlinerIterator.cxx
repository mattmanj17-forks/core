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

#include <OutlinerIterator.hxx>
#include <OutlinerIteratorImpl.hxx>
#include <svx/svditer.hxx>
#include <tools/debug.hxx>
#include <osl/diagnose.h>
#include <Outliner.hxx>

#include <drawdoc.hxx>
#include <DrawViewShell.hxx>
#include <sdpage.hxx>
#include <utility>
#include <ViewShellBase.hxx>
#include <ViewShellManager.hxx>

namespace sd::outliner {

//===== IteratorPosition ======================================================

IteratorPosition::IteratorPosition()
: mnText(0)
, mnPageIndex(-1)
, mePageKind(PageKind::Standard)
, meEditMode(EditMode::Page)
{
}

bool IteratorPosition::operator== (const IteratorPosition& aPosition) const
{
    return mxObject.get() == aPosition.mxObject.get()
        && mnText == aPosition.mnText
        && mnPageIndex == aPosition.mnPageIndex
        && mePageKind == aPosition.mePageKind
        && meEditMode == aPosition.meEditMode;
}

//===== Iterator ==============================================================

Iterator::Iterator()
{
}

Iterator::Iterator (const Iterator& rIterator)
    : mxIterator(rIterator.mxIterator ? rIterator.mxIterator->Clone() : nullptr)
{
}

Iterator::Iterator(Iterator&& rIterator) noexcept
    : mxIterator(std::move(rIterator.mxIterator))
{
}

Iterator::Iterator (std::unique_ptr<IteratorImplBase> pObject)
    : mxIterator(std::move(pObject))
{
}

Iterator::~Iterator()
{
}

Iterator& Iterator::operator= (const Iterator& rIterator)
{
    if (this != &rIterator)
    {
        if (rIterator.mxIterator)
            mxIterator.reset(rIterator.mxIterator->Clone());
        else
            mxIterator.reset();
    }
    return *this;
}

Iterator& Iterator::operator=(Iterator&& rIterator) noexcept
{
    mxIterator = std::move(rIterator.mxIterator);
    return *this;
}

const IteratorPosition& Iterator::operator* () const
{
    DBG_ASSERT (mxIterator, "::sd::outliner::Iterator::operator* : missing implementation object");
    return mxIterator->GetPosition();
}

Iterator& Iterator::operator++ ()
{
    if (mxIterator)
        mxIterator->GotoNextText();
    return *this;
}

bool Iterator::operator== (const Iterator& rIterator) const
{
    if (!mxIterator || !rIterator.mxIterator)
        return mxIterator.get() == rIterator.mxIterator.get();
    else
        return *mxIterator == *rIterator.mxIterator;
}

bool Iterator::operator!= (const Iterator& rIterator) const
{
    return ! operator==(rIterator);
}

void Iterator::Reverse()
{
    if (mxIterator)
        mxIterator->Reverse();
}

//===== IteratorFactory =======================================================

OutlinerContainer::OutlinerContainer (SdOutliner* pOutliner)
: mpOutliner(pOutliner)
{
}

Iterator OutlinerContainer::begin()
{
    return CreateIterator (BEGIN);
}

Iterator OutlinerContainer::end()
{
    return CreateIterator (END);
}

Iterator OutlinerContainer::current()
{
    return CreateIterator (CURRENT);
}

Iterator OutlinerContainer::CreateIterator (IteratorLocation aLocation)
{
    std::shared_ptr<sd::ViewShell> pOverridingShell{};
    if (auto pBase = dynamic_cast<sd::ViewShellBase*>(SfxViewShell::Current()))
        if (auto pViewShellManager = pBase->GetViewShellManager())
            pOverridingShell = pViewShellManager->GetOverridingMainShell();

    // Decide on certain features of the outliner which kind of iterator to
    // use.
    if (mpOutliner->mbRestrictSearchToSelection)
        // There is a selection.  Search only in this.
        return CreateSelectionIterator (
            mpOutliner->maMarkListCopy,
            &mpOutliner->mrDrawDocument,
            pOverridingShell ? std::move(pOverridingShell) :
            mpOutliner->mpWeakViewShell.lock(),
            mpOutliner->mbDirectionIsForward,
            aLocation);
    else
        // Search in the whole document.
        return CreateDocumentIterator (
            &mpOutliner->mrDrawDocument,
            pOverridingShell ? std::move(pOverridingShell) :
            mpOutliner->mpWeakViewShell.lock(),
            mpOutliner->mbDirectionIsForward,
            aLocation);
}

Iterator OutlinerContainer::CreateSelectionIterator (
    const ::std::vector<::unotools::WeakReference<SdrObject>>& rObjectList,
    SdDrawDocument* pDocument,
    const std::shared_ptr<ViewShell>& rpViewShell,
    bool bDirectionIsForward,
    IteratorLocation aLocation)
{
    OSL_ASSERT(rpViewShell);

    sal_Int32 nObjectIndex;

    if (bDirectionIsForward)
        switch (aLocation)
        {
            case CURRENT:
            case BEGIN:
            default:
                nObjectIndex = 0;
                break;
            case END:
                nObjectIndex = rObjectList.size();
                break;
        }
    else
        switch (aLocation)
        {
            case CURRENT:
            case BEGIN:
            default:
                nObjectIndex = rObjectList.size()-1;
                break;
            case END:
                nObjectIndex = -1;
                break;
        }

    return Iterator (std::make_unique<SelectionIteratorImpl> (
        rObjectList, nObjectIndex, pDocument, rpViewShell, bDirectionIsForward));
}

Iterator OutlinerContainer::CreateDocumentIterator (
    SdDrawDocument* pDocument,
    const std::shared_ptr<ViewShell>& rpViewShell,
    bool bDirectionIsForward,
    IteratorLocation aLocation)
{
    OSL_ASSERT(rpViewShell);

    PageKind ePageKind;
    EditMode eEditMode;

    switch (aLocation)
    {
        case BEGIN:
        default:
            if (bDirectionIsForward)
            {
                ePageKind = PageKind::Standard;
                eEditMode = EditMode::Page;
            }
            else
            {
                ePageKind = PageKind::Handout;
                eEditMode = EditMode::MasterPage;
            }
            break;

        case END:
            if (bDirectionIsForward)
            {
                ePageKind = PageKind::Handout;
                eEditMode = EditMode::MasterPage;
            }
            else
            {
                ePageKind = PageKind::Standard;
                eEditMode = EditMode::Page;
            }
            break;

        case CURRENT:
            const std::shared_ptr<DrawViewShell> pDrawViewShell(
                std::dynamic_pointer_cast<DrawViewShell>(rpViewShell));
            if (pDrawViewShell)
            {
                ePageKind = pDrawViewShell->GetPageKind();
                eEditMode = pDrawViewShell->GetEditMode();
            }
            else
            {
                auto pPage = rpViewShell->getCurrentPage();
                ePageKind =  pPage ? pPage->GetPageKind() : PageKind::Standard;
                eEditMode = EditMode::Page;
            }
            break;
    }

    sal_Int32 nPageIndex = GetPageIndex (pDocument, rpViewShell,
        ePageKind, eEditMode, bDirectionIsForward, aLocation);

    return Iterator (
        std::make_unique<DocumentIteratorImpl> (nPageIndex, ePageKind, eEditMode,
            pDocument, rpViewShell, bDirectionIsForward));
}

sal_Int32 OutlinerContainer::GetPageIndex (
    SdDrawDocument const * pDocument,
    const std::shared_ptr<ViewShell>& rpViewShell,
    PageKind ePageKind,
    EditMode eEditMode,
    bool bDirectionIsForward,
    IteratorLocation aLocation)
{
    OSL_ASSERT(rpViewShell);

    sal_Int32 nPageIndex;
    sal_Int32 nPageCount;

    const std::shared_ptr<DrawViewShell> pDrawViewShell(
        std::dynamic_pointer_cast<DrawViewShell>(rpViewShell));

    switch (eEditMode)
    {
        case EditMode::Page:
            nPageCount = pDocument->GetSdPageCount (ePageKind);
            break;
        case EditMode::MasterPage:
            nPageCount = pDocument->GetMasterSdPageCount(ePageKind);
            break;
        default:
            nPageCount = 0;
    }

    switch (aLocation)
    {
        case CURRENT:
            if (pDrawViewShell)
                nPageIndex = pDrawViewShell->GetCurPagePos();
            else
            {
                const SdPage* pPage = rpViewShell->GetActualPage();
                if (pPage != nullptr)
                    nPageIndex = (pPage->GetPageNum()-1)/2;
                else
                    nPageIndex = 0;
            }
            break;

        case BEGIN:
        default:
            if (bDirectionIsForward)
                nPageIndex = 0;
            else
                nPageIndex = nPageCount-1;
            break;

        case END:
            if (bDirectionIsForward)
                nPageIndex = nPageCount;
            else
                nPageIndex = -1;
            break;
    }

    return nPageIndex;
}

//===== IteratorImplBase ====================================================

IteratorImplBase::IteratorImplBase(SdDrawDocument* pDocument,
    const std::weak_ptr<ViewShell>& rpViewShellWeak,
    bool bDirectionIsForward)
:   mpDocument (pDocument)
,   mpViewShellWeak (rpViewShellWeak)
,   mbDirectionIsForward (bDirectionIsForward)
{
    std::shared_ptr<DrawViewShell> pDrawViewShell;
    if ( ! mpViewShellWeak.expired())
        pDrawViewShell = std::dynamic_pointer_cast<DrawViewShell>(rpViewShellWeak.lock());

    if (pDrawViewShell)
    {
        maPosition.mePageKind = pDrawViewShell->GetPageKind();
        maPosition.meEditMode = pDrawViewShell->GetEditMode();
    }
    else
    {
        maPosition.mePageKind = PageKind::Standard;
        maPosition.meEditMode = EditMode::Page;
    }
}

IteratorImplBase::IteratorImplBase( SdDrawDocument* pDocument,
    std::weak_ptr<ViewShell> pViewShellWeak,
    bool bDirectionIsForward, PageKind ePageKind, EditMode eEditMode)
: mpDocument (pDocument)
, mpViewShellWeak (std::move(pViewShellWeak))
, mbDirectionIsForward (bDirectionIsForward)
{
    maPosition.mePageKind = ePageKind;
    maPosition.meEditMode = eEditMode;
}

IteratorImplBase::~IteratorImplBase()
{}

bool IteratorImplBase::operator== (const IteratorImplBase& rIterator) const
{
    return maPosition == rIterator.maPosition;
}

bool IteratorImplBase::IsEqualSelection(const IteratorImplBase& rIterator) const
{
    // When this method is executed instead of the ones from derived classes
    // then the argument is of another type then the object itself.  In this
    // just compare the position objects.
    return maPosition == rIterator.maPosition;
}

const IteratorPosition& IteratorImplBase::GetPosition()
{
    return maPosition;
}

IteratorImplBase* IteratorImplBase::Clone (IteratorImplBase* pObject) const
{
    if (pObject != nullptr)
    {
        pObject->maPosition = maPosition;
        pObject->mpDocument = mpDocument;
        pObject->mpViewShellWeak = mpViewShellWeak;
        pObject->mbDirectionIsForward = mbDirectionIsForward;
    }
    return pObject;
}

void IteratorImplBase::Reverse()
{
    mbDirectionIsForward = ! mbDirectionIsForward;
}

//===== SelectionIteratorImpl ===========================================

SelectionIteratorImpl::SelectionIteratorImpl (
    const ::std::vector<::unotools::WeakReference<SdrObject>>& rObjectList,
    sal_Int32 nObjectIndex,
    SdDrawDocument* pDocument,
    const std::weak_ptr<ViewShell>& rpViewShellWeak,
    bool bDirectionIsForward)
    : IteratorImplBase (pDocument, rpViewShellWeak, bDirectionIsForward),
      mrObjectList(rObjectList),
      mnObjectIndex(nObjectIndex)
{
}

SelectionIteratorImpl::~SelectionIteratorImpl()
{}

IteratorImplBase* SelectionIteratorImpl::Clone (IteratorImplBase* pObject) const
{
    SelectionIteratorImpl* pIterator = static_cast<SelectionIteratorImpl*>(pObject);
    if (pIterator == nullptr)
        pIterator = new SelectionIteratorImpl (
            mrObjectList, mnObjectIndex, mpDocument, mpViewShellWeak, mbDirectionIsForward);
    return pIterator;
}

void SelectionIteratorImpl::GotoNextText()
{
    SdrTextObj* pTextObj = DynCastSdrTextObj( mrObjectList.at(mnObjectIndex).get().get() );
    if (mbDirectionIsForward)
    {
        if( pTextObj )
        {
            ++maPosition.mnText;
            if( maPosition.mnText >= pTextObj->getTextCount() )
            {
                maPosition.mnText = 0;
                ++mnObjectIndex;
            }
        }
        else
        {
            ++mnObjectIndex;
        }
    }
    else
    {
        if( pTextObj )
        {
            --maPosition.mnText;
            if( maPosition.mnText < 0 )
            {
                maPosition.mnText = -1;
                --mnObjectIndex;
            }
        }
        else
        {
            --mnObjectIndex;
            maPosition.mnText = -1;
        }

        if( (maPosition.mnText == -1) && (mnObjectIndex >= 0) )
        {
            pTextObj = DynCastSdrTextObj( mrObjectList.at(mnObjectIndex).get().get() );
            if( pTextObj )
                maPosition.mnText = pTextObj->getTextCount() - 1;
        }

        if( maPosition.mnText == -1 )
            maPosition.mnText = 0;
    }
}

const IteratorPosition& SelectionIteratorImpl::GetPosition()
{
    maPosition.mxObject = mrObjectList.at(mnObjectIndex);

    return maPosition;
}

bool SelectionIteratorImpl::operator== (const IteratorImplBase& rIterator) const
{
    return rIterator.IsEqualSelection(*this);
}

bool SelectionIteratorImpl::IsEqualSelection(const IteratorImplBase& rIterator) const
{
    const SelectionIteratorImpl* pSelectionIterator =
        static_cast<const SelectionIteratorImpl*>(&rIterator);
    return mpDocument == pSelectionIterator->mpDocument
        && mnObjectIndex == pSelectionIterator->mnObjectIndex;
}

void DocumentIteratorImpl::SetPage(sal_Int32 nPageIndex)
{
    maPosition.mnPageIndex = nPageIndex;

    sal_Int32 nPageCount;
    if (maPosition.meEditMode == EditMode::Page)
        nPageCount = mpDocument->GetSdPageCount(maPosition.mePageKind);
    else
        nPageCount = mpDocument->GetMasterSdPageCount(
            maPosition.mePageKind);

    // Get page pointer.  Here we have three cases: regular pages,
    // master pages and invalid page indices.  The later ones are not
    // errors but the effect of the iterator advancing to the next page
    // and going past the last one.  This dropping of the rim at the far
    // side is detected here and has to be reacted to by the caller.
    if (nPageIndex>=0 && nPageIndex < nPageCount)
    {
        if (maPosition.meEditMode == EditMode::Page)
            mpPage = mpDocument->GetSdPage (
                static_cast<sal_uInt16>(nPageIndex),
                maPosition.mePageKind);
        else
            mpPage = mpDocument->GetMasterSdPage (
                static_cast<sal_uInt16>(nPageIndex),
                maPosition.mePageKind);
    }
    else
        mpPage = nullptr;

    // Set up object list iterator.
    if (mpPage != nullptr)
        moObjectIterator.emplace(mpPage, SdrIterMode::DeepNoGroups, ! mbDirectionIsForward);
    else
        moObjectIterator.reset();

    // Get object pointer.
    if (moObjectIterator && moObjectIterator->IsMore())
        maPosition.mxObject = moObjectIterator->Next();
    else
        maPosition.mxObject = nullptr;

    maPosition.mnText = 0;
    if( !mbDirectionIsForward && maPosition.mxObject.get().is() )
    {
        SdrTextObj* pTextObj = DynCastSdrTextObj( maPosition.mxObject.get().get() );
        if( pTextObj )
            maPosition.mnText = pTextObj->getTextCount() - 1;
    }

}

void DocumentIteratorImpl::Reverse()
{
    IteratorImplBase::Reverse ();

    // Create reversed object list iterator.
    if (mpPage != nullptr)
        moObjectIterator.emplace(mpPage, SdrIterMode::DeepNoGroups, ! mbDirectionIsForward);
    else
        moObjectIterator.reset();

    // Move iterator to the current object.
    ::unotools::WeakReference<SdrObject> xObject = std::move(maPosition.mxObject);
    maPosition.mxObject = nullptr;

    if (!moObjectIterator)
        return;

    while (moObjectIterator->IsMore() && maPosition.mxObject.get() != xObject.get())
        maPosition.mxObject = moObjectIterator->Next();
}

//===== DocumentIteratorImpl ============================================

DocumentIteratorImpl::DocumentIteratorImpl (
    sal_Int32 nPageIndex,
    PageKind ePageKind, EditMode eEditMode,
    SdDrawDocument* pDocument,
    const std::weak_ptr<ViewShell>& rpViewShellWeak,
    bool bDirectionIsForward)
    : IteratorImplBase (pDocument, rpViewShellWeak, bDirectionIsForward, ePageKind, eEditMode)
    , mpPage(nullptr)
{
    if (eEditMode == EditMode::Page)
        mnPageCount = pDocument->GetSdPageCount (ePageKind);
    else
        mnPageCount = pDocument->GetMasterSdPageCount(ePageKind);
    SetPage(nPageIndex);
}

DocumentIteratorImpl::~DocumentIteratorImpl()
{}

IteratorImplBase* DocumentIteratorImpl::Clone (IteratorImplBase* pObject) const
{
    DocumentIteratorImpl* pIterator = static_cast<DocumentIteratorImpl*>(pObject);
    if (pIterator == nullptr)
        pIterator = new DocumentIteratorImpl (
            maPosition.mnPageIndex, maPosition.mePageKind, maPosition.meEditMode,
            mpDocument, mpViewShellWeak, mbDirectionIsForward);
    // Finish the cloning.
    IteratorImplBase::Clone (pObject);

    if (moObjectIterator)
    {
        pIterator->moObjectIterator.emplace(mpPage, SdrIterMode::DeepNoGroups, !mbDirectionIsForward);

        // No direct way to set the object iterator to the current object.
        pIterator->maPosition.mxObject = nullptr;
        while (pIterator->moObjectIterator->IsMore() && pIterator->maPosition.mxObject.get()!=maPosition.mxObject.get())
            pIterator->maPosition.mxObject = pIterator->moObjectIterator->Next();
    }
    else
        pIterator->moObjectIterator.reset();

    return pIterator;
}

void DocumentIteratorImpl::GotoNextText()
{
    bool bSetToOnePastLastPage = false;
    bool bViewChanged = false;

    SdrTextObj* pTextObj = DynCastSdrTextObj( maPosition.mxObject.get().get() );
    if( pTextObj )
    {
        if (mbDirectionIsForward)
        {
            ++maPosition.mnText;
            if( maPosition.mnText < pTextObj->getTextCount() )
                return;
        }
        else
        {
            --maPosition.mnText;
            if( maPosition.mnText >= 0 )
                return;
       }
    }

    if (moObjectIterator && moObjectIterator->IsMore())
        maPosition.mxObject = moObjectIterator->Next();
    else
        maPosition.mxObject = nullptr;

    if (!maPosition.mxObject.get().is() )
    {
        if (maPosition.meEditMode == EditMode::Page
            && maPosition.mePageKind == PageKind::Standard)
        {
            maPosition.mePageKind = PageKind::Notes;
            if (mbDirectionIsForward)
                SetPage(maPosition.mnPageIndex);
            else
                SetPage(maPosition.mnPageIndex - 1);
        }
        else if (maPosition.meEditMode == EditMode::Page
                 && maPosition.mePageKind == PageKind::Notes)
        {
            maPosition.mePageKind = PageKind::Standard;
            if (mbDirectionIsForward)
                SetPage(maPosition.mnPageIndex + 1);
            else
                SetPage(maPosition.mnPageIndex);
        }
        else
        {
            if (mbDirectionIsForward)
                SetPage(maPosition.mnPageIndex + 1);
            else
                SetPage(maPosition.mnPageIndex - 1);
        }

        if (mpPage != nullptr)
            moObjectIterator.emplace( mpPage, SdrIterMode::DeepNoGroups, !mbDirectionIsForward );
        if (moObjectIterator && moObjectIterator->IsMore())
            maPosition.mxObject = moObjectIterator->Next();
        else
            maPosition.mxObject = nullptr;
    }

    maPosition.mnText = 0;
    if( !mbDirectionIsForward && maPosition.mxObject.get().is() )
    {
        pTextObj = DynCastSdrTextObj( maPosition.mxObject.get().get() );
        if( pTextObj )
            maPosition.mnText = pTextObj->getTextCount() - 1;
    }


    if (mbDirectionIsForward)
    {
        if (maPosition.mnPageIndex >= mnPageCount)
        {
            // Switch to master page.
            if (maPosition.meEditMode == EditMode::Page)
            {
                maPosition.meEditMode = EditMode::MasterPage;
                SetPage (0);
            }
            // Switch to next view mode.
            else
            {
                if (maPosition.mePageKind == PageKind::Handout)
                {
                    // search wrapped
                    bSetToOnePastLastPage = true;
                }
                else if (maPosition.mePageKind == PageKind::Standard)
                {
                    // switch to master notes
                    maPosition.mePageKind = PageKind::Notes;
                    SetPage(0);
                }
                else if (maPosition.mePageKind == PageKind::Notes)
                {
                    // switch to master handout
                    maPosition.mePageKind = PageKind::Handout;
                    SetPage(0);
                }
            }
            bViewChanged = true;
        }
    }
    else
    {
        if (maPosition.mnPageIndex < 0)
        {
            if (maPosition.meEditMode == EditMode::MasterPage)
            {
                if (maPosition.mePageKind == PageKind::Standard)
                {
                    maPosition.meEditMode = EditMode::Page;
                    maPosition.mePageKind = PageKind::Notes;
                    bSetToOnePastLastPage = true;
                }
                else if (maPosition.mePageKind == PageKind::Handout)
                {
                    maPosition.mePageKind = PageKind::Notes;
                    bSetToOnePastLastPage = true;
                }
                else if (maPosition.mePageKind == PageKind::Notes)
                {
                    maPosition.mePageKind = PageKind::Standard;
                    bSetToOnePastLastPage = true;
                }
            }
            else
            {
                // search wrapped
                SetPage(-1);
            }
        }
        bViewChanged = true;
    }

    if (!bViewChanged)
        return;

    // Get new page count;
    if (maPosition.meEditMode == EditMode::Page)
        mnPageCount = mpDocument->GetSdPageCount (maPosition.mePageKind);
    else
        mnPageCount = mpDocument->GetMasterSdPageCount(maPosition.mePageKind);

    // Now that we know the number of pages we can set the current page index.
    if (bSetToOnePastLastPage)
        SetPage (mnPageCount);
}

} // end of namespace ::sd::outliner

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
