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

#include <memory>
#include <viscrs.hxx>
#include <o3tl/any.hxx>
#include <sfx2/printer.hxx>
#include <sfx2/viewfrm.hxx>
#include <cmdid.h>
#include <docsh.hxx>
#include <rubylist.hxx>
#include <doc.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <unotxvw.hxx>
#include <unodispatch.hxx>
#include <unomap.hxx>
#include <unoprnms.hxx>
#include <view.hxx>
#include <viewopt.hxx>
#include <unomod.hxx>
#include <unoframe.hxx>
#include <unocrsr.hxx>
#include <wrtsh.hxx>
#include <unotbl.hxx>
#include <svx/fmshell.hxx>
#include <svx/svdview.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdouno.hxx>
#include <editeng/pbinitem.hxx>
#include <pagedesc.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/request.hxx>
#include <frmatr.hxx>
#include <IMark.hxx>
#include <unodraw.hxx>
#include <svx/svdpagv.hxx>
#include <ndtxt.hxx>
#include <SwStyleNameMapper.hxx>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/drawing/ShapeCollection.hpp>
#include <editeng/outliner.hxx>
#include <editeng/editview.hxx>
#include <unoparagraph.hxx>
#include <unocrsrhelper.hxx>
#include <unotextrange.hxx>
#include <sfx2/docfile.hxx>
#include <swdtflvr.hxx>
#include <rootfrm.hxx>
#include <edtwin.hxx>
#include <vcl/svapp.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/profilezone.hxx>
#include <comphelper/servicehelper.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/typeprovider.hxx>
#include <tools/UnitConversion.hxx>
#include <comphelper/dumpxmltostring.hxx>
#include <fmtanchr.hxx>
#include <names.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::view;
using namespace ::com::sun::star::frame;

using ::com::sun::star::util::URL;

SwXTextView::SwXTextView(SwView* pSwView) :
    SwXTextView_Base(pSwView),
    m_SelChangedListeners(m_aMutex),
    m_pView(pSwView),
    m_pPropSet( aSwMapProvider.GetPropertySet( PROPERTY_MAP_TEXT_VIEW ) )
{

}

SwXTextView::~SwXTextView()
{
    Invalidate();
}

void SwXTextView::Invalidate()
{
    if(mxViewSettings.is())
    {
        mxViewSettings->Invalidate();
        mxViewSettings.clear();
    }
    if(mxTextViewCursor.is())
    {
        mxTextViewCursor->Invalidate();
        mxTextViewCursor.clear();
    }

    osl_atomic_increment(&m_refCount); //prevent second d'tor call

    {
        uno::Reference<uno::XInterface> const xInt(static_cast<
                cppu::OWeakObject*>(static_cast<SfxBaseController*>(this)));
        lang::EventObject aEvent(xInt);
        m_SelChangedListeners.disposeAndClear(aEvent);
    }

    osl_atomic_decrement(&m_refCount);
    m_pView = nullptr;
}

sal_Bool SwXTextView::select(const uno::Any& aInterface)
{
    SolarMutexGuard aGuard;

    uno::Reference< uno::XInterface >  xInterface;
    if (!GetView() || !(aInterface >>= xInterface))
    {
        return false;
    }

    SwWrtShell& rSh = GetView()->GetWrtShell();
    SwDoc* pDoc = GetView()->GetDocShell()->GetDoc();
    std::vector<SdrObject *> sdrObjects;
    uno::Reference<awt::XControlModel> const xCtrlModel(xInterface,
            UNO_QUERY);
    if (xCtrlModel.is())
    {
        uno::Reference<awt::XControl> xControl;
        SdrObject *const pSdrObject = GetControl(xCtrlModel, xControl);
        if (pSdrObject) // hmm... needs view to verify it's in right doc...
        {
            sdrObjects.push_back(pSdrObject);
        }
    }
    else
    {
        std::optional<SwPaM> pPaM;
        std::pair<UIName, FlyCntType> frame;
        UIName tableName;
        SwUnoTableCursor const* pTableCursor(nullptr);
        ::sw::mark::MarkBase const* pMark(nullptr);
        SwUnoCursorHelper::GetSelectableFromAny(xInterface, *pDoc,
                pPaM, frame, tableName, pTableCursor, pMark, sdrObjects);
        if (pPaM)
        {
            rSh.EnterStdMode();
            rSh.SetSelection(*pPaM);
            // the pPaM has been copied - delete it
            while (pPaM->GetNext() != &*pPaM)
            {
                // coverity[deref_arg] - the SwPaM delete moves a new entry into GetNext()
                delete pPaM->GetNext();
            }
            pPaM.reset();
            return true;
        }
        else if (!frame.first.isEmpty())
        {
            bool const bSuccess(rSh.GotoFly(frame.first, frame.second));
            if (bSuccess)
            {
                rSh.HideCursor();
                rSh.EnterSelFrameMode();
            }
            return true;
        }
        else if (!tableName.isEmpty())
        {
            rSh.EnterStdMode();
            rSh.GotoTable(tableName);
            return true;
        }
        else if (pTableCursor)
        {
            UnoActionRemoveContext const aContext(*pTableCursor);
            rSh.EnterStdMode();
            rSh.SetSelection(*pTableCursor);
            return true;
        }
        else if (pMark)
        {
            rSh.EnterStdMode();
            rSh.GotoMark(pMark);
            return true;
        }
        // sdrObjects handled below
    }
    bool bRet(false);
    if (!sdrObjects.empty())
    {

        SdrView *const pDrawView = rSh.GetDrawView();
        SdrPageView *const pPV = pDrawView->GetSdrPageView();

        pDrawView->SdrEndTextEdit();
        pDrawView->UnmarkAll();

        for (SdrObject* pSdrObject : sdrObjects)
        {
            // GetSelectableFromAny did not check pSdrObject is in right doc!
            if (pPV && pSdrObject->getSdrPageFromSdrObject() == pPV->GetPage())
            {
                pDrawView->MarkObj(pSdrObject, pPV);
                bRet = true;
            }
        }

        // tdf#112696 if we selected every individual element of a group, then
        // select that group instead
        const SdrMarkList &rMrkList = pDrawView->GetMarkedObjectList();
        size_t nMarkCount = rMrkList.GetMarkCount();
        if (nMarkCount > 1)
        {
            SdrObject* pObject = rMrkList.GetMark(0)->GetMarkedSdrObj();
            SdrObject* pGroupParent = pObject->getParentSdrObjectFromSdrObject();
            for (size_t i = 1; i < nMarkCount; ++i)
            {
                pObject = rMrkList.GetMark(i)->GetMarkedSdrObj();
                SdrObject* pParent = pObject->getParentSdrObjectFromSdrObject();
                if (pParent != pGroupParent)
                {
                    pGroupParent = nullptr;
                    break;
                }
            }

            if (pGroupParent && pGroupParent->IsGroupObject() &&
                pGroupParent->getChildrenOfSdrObject()->GetObjCount() == nMarkCount)
            {
                pDrawView->UnmarkAll();
                pDrawView->MarkObj(pGroupParent, pPV);
            }
        }
    }
    return bRet;
}

uno::Any SwXTextView::getSelection()
{
    SolarMutexGuard aGuard;
    uno::Reference< uno::XInterface >  aRef;
    if(GetView())
    {
        //force immediate shell update
        m_pView->StopShellTimer();
        //Generating an interface from the current selection.
        SwWrtShell& rSh = m_pView->GetWrtShell();
        ShellMode eSelMode = m_pView->GetShellMode();
        switch(eSelMode)
        {
            case ShellMode::TableText      :
            {
                if(rSh.GetTableCursor())
                {
                    OSL_ENSURE(rSh.GetTableFormat(), "not a table format?");
                    uno::Reference< text::XTextTableCursor >  xCursor = new SwXTextTableCursor(*rSh.GetTableFormat(),
                                                    rSh.GetTableCursor());
                    aRef.set(xCursor, uno::UNO_QUERY);
                    break;
                }
                [[fallthrough]];
                    // without a table selection the text will be delivered
            }
            case ShellMode::ListText       :
            case ShellMode::TableListText:
            case ShellMode::Text            :
            {
                rtl::Reference< SwXTextRanges > xPos = SwXTextRanges::Create(rSh.GetCursor());
                aRef.set(uno::Reference< container::XIndexAccess >(xPos), uno::UNO_QUERY);
            }
            break;
            case ShellMode::Frame           :
            {
                SwFrameFormat *const pFormat = rSh.GetFlyFrameFormat();
                if (pFormat)
                {
                    aRef = cppu::getXWeak(SwXTextFrame::CreateXTextFrame(
                            pFormat->GetDoc(), pFormat).get());
                }
            }
            break;
            case ShellMode::Graphic         :
            {
                SwFrameFormat *const pFormat = rSh.GetFlyFrameFormat();
                if (pFormat)
                {
                    aRef = cppu::getXWeak(SwXTextGraphicObject::CreateXTextGraphicObject(
                            pFormat->GetDoc(), pFormat).get());
                }
            }
            break;
            case ShellMode::Object          :
            {
                SwFrameFormat *const pFormat = rSh.GetFlyFrameFormat();
                if (pFormat)
                {
                    aRef = cppu::getXWeak(SwXTextEmbeddedObject::CreateXTextEmbeddedObject(
                            pFormat->GetDoc(), pFormat).get());
                }
            }
            break;
            case ShellMode::Draw            :
            case ShellMode::DrawForm        :
            case ShellMode::DrawText        :
            case ShellMode::Bezier          :
            {
                uno::Reference< drawing::XShapes >  xShCol = drawing::ShapeCollection::create(
                        comphelper::getProcessComponentContext());

                const SdrMarkList& rMarkList = rSh.GetDrawView()->GetMarkedObjectList();
                for(size_t i = 0; i < rMarkList.GetMarkCount(); ++i)
                {
                    SdrObject* pObj = rMarkList.GetMark(i)->GetMarkedSdrObj();
                    uno::Reference<drawing::XShape> xShape = SwFmDrawPage::GetShape( pObj );
                    xShCol->add(xShape);
                }
                aRef.set(xShCol, uno::UNO_QUERY);
            }
            break;
            default:;//prevent warning
        }
    }
    uno::Any aRet(&aRef, cppu::UnoType<uno::XInterface>::get());
    return aRet;
}

void SwXTextView::addSelectionChangeListener(
                                    const uno::Reference< view::XSelectionChangeListener > & rxListener)
{
    SolarMutexGuard aGuard;
    m_SelChangedListeners.addInterface(rxListener);
}

void SwXTextView::removeSelectionChangeListener(
                                        const uno::Reference< view::XSelectionChangeListener > & rxListener)
{
    SolarMutexGuard aGuard;
    m_SelChangedListeners.removeInterface(rxListener);
}

SdrObject* SwXTextView::GetControl(
        const uno::Reference< awt::XControlModel > & xModel,
        uno::Reference< awt::XControl >& xToFill  )
{
    SwView* pView2 = GetView();
    FmFormShell* pFormShell = pView2 ? pView2->GetFormShell() : nullptr;
    SdrView* pDrawView = pView2 ? pView2->GetDrawView() : nullptr;
    vcl::Window* pWindow = pView2 ? pView2->GetWrtShell().GetWin() : nullptr;

    OSL_ENSURE( pFormShell && pDrawView && pWindow, "SwXTextView::GetControl: how could I?" );

    SdrObject* pControl = nullptr;
    if ( pFormShell && pDrawView && pWindow )
        pControl = pFormShell->GetFormControl( xModel, *pDrawView, *pWindow->GetOutDev(), xToFill );
    return pControl;
}

uno::Reference< awt::XControl >  SwXTextView::getControl(const uno::Reference< awt::XControlModel > & xModel)
{
    SolarMutexGuard aGuard;
    uno::Reference< awt::XControl >  xRet;
    GetControl(xModel, xRet);
    return xRet;
}

uno::Reference< form::runtime::XFormController > SAL_CALL SwXTextView::getFormController( const uno::Reference< form::XForm >& Form )
{
    SolarMutexGuard aGuard;

    SwView* pView2 = GetView();
    FmFormShell* pFormShell = pView2 ? pView2->GetFormShell() : nullptr;
    SdrView* pDrawView = pView2 ? pView2->GetDrawView() : nullptr;
    vcl::Window* pWindow = pView2 ? pView2->GetWrtShell().GetWin() : nullptr;
    OSL_ENSURE( pFormShell && pDrawView && pWindow, "SwXTextView::getFormController: how could I?" );

    uno::Reference< form::runtime::XFormController > xController;
    if ( pFormShell && pDrawView && pWindow )
        xController = FmFormShell::GetFormController( Form, *pDrawView, *pWindow->GetOutDev() );
    return xController;
}

sal_Bool SAL_CALL SwXTextView::isFormDesignMode(  )
{
    SolarMutexGuard aGuard;
    SwView* pView2 = GetView();
    FmFormShell* pFormShell = pView2 ? pView2->GetFormShell() : nullptr;
    return !pFormShell || pFormShell->IsDesignMode();
}

void SAL_CALL SwXTextView::setFormDesignMode( sal_Bool DesignMode )
{
    SolarMutexGuard aGuard;
    SwView* pView2 = GetView();
    FmFormShell* pFormShell = pView2 ? pView2->GetFormShell() : nullptr;
    if ( pFormShell )
        pFormShell->SetDesignMode( DesignMode );
}

uno::Reference< text::XTextViewCursor >  SwXTextView::getViewCursor()
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("getViewCursor");
    if(!GetView())
        throw uno::RuntimeException();

    if(!mxTextViewCursor.is())
    {
        mxTextViewCursor = new SwXTextViewCursor(GetView());
    }
    return mxTextViewCursor;
}

uno::Reference<text::XTextRange>
SwXTextView::createTextRangeByPixelPosition(const awt::Point& rPixelPosition)
{
    SolarMutexGuard aGuard;

    Point aPixelPoint(rPixelPosition.X, rPixelPosition.Y);
    if (!m_pView)
        throw RuntimeException();

    Point aLogicPoint = m_pView->GetEditWin().PixelToLogic(aPixelPoint);
    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPosition aPosition(*rSh.GetCurrentShellCursor().GetPoint());
    rSh.GetLayout()->GetModelPositionForViewPoint(&aPosition, aLogicPoint);

    if (aPosition.GetNode().IsGrfNode())
    {
        // The point is closest to a graphic node, look up its format.
        const SwFrameFormat* pGraphicFormat = aPosition.GetNode().GetFlyFormat();
        if (pGraphicFormat)
        {
            // Get the anchor of this format.
            const SwFormatAnchor& rAnchor = pGraphicFormat->GetAnchor();
            const SwPosition* pAnchor = rAnchor.GetContentAnchor();
            if (pAnchor)
            {
                aPosition = *pAnchor;
            }
            else
            {
                // Page-anchored graphics have no anchor.
                return {};
            }
        }
    }

    rtl::Reference<SwXTextRange> xRet
        = SwXTextRange::CreateXTextRange(*rSh.GetDoc(), aPosition, /*pMark=*/nullptr);

    return xRet;
}

uno::Reference< beans::XPropertySet >  SwXTextView::getViewSettings()
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if(!mxViewSettings.is())
    {
        mxViewSettings = new SwXViewSettings( m_pView );
    }

    return mxViewSettings;
}

Sequence< Sequence< PropertyValue > > SwXTextView::getRubyList( sal_Bool /*bAutomatic*/ )
{
    SolarMutexGuard aGuard;

    if(!GetView())
        throw RuntimeException();
    SwWrtShell& rSh = m_pView->GetWrtShell();
    ShellMode  eSelMode = m_pView->GetShellMode();
    if (eSelMode != ShellMode::ListText      &&
        eSelMode != ShellMode::TableListText &&
        eSelMode != ShellMode::TableText      &&
        eSelMode != ShellMode::Text           )
        return Sequence< Sequence< PropertyValue > > ();

    SwRubyList aList;

    const sal_uInt16 nCount = SwDoc::FillRubyList( *rSh.GetCursor(), aList );
    Sequence< Sequence< PropertyValue > > aRet(nCount);
    Sequence< PropertyValue >* pRet = aRet.getArray();
    ProgName aString;
    for(sal_uInt16 n = 0; n < nCount; n++)
    {
        const SwRubyListEntry* pEntry = aList[n].get();

        const OUString& rEntryText = pEntry->GetText();
        const SwFormatRuby& rAttr = pEntry->GetRubyAttr();

        pRet[n].realloc(6);
        PropertyValue* pValues = pRet[n].getArray();
        pValues[0].Name = UNO_NAME_RUBY_BASE_TEXT;
        pValues[0].Value <<= rEntryText;
        pValues[1].Name = UNO_NAME_RUBY_TEXT;
        pValues[1].Value <<= rAttr.GetText();
        pValues[2].Name = UNO_NAME_RUBY_CHAR_STYLE_NAME;
        SwStyleNameMapper::FillProgName(rAttr.GetCharFormatName(), aString, SwGetPoolIdFromName::ChrFmt );
        pValues[2].Value <<= aString.toString();
        pValues[3].Name = UNO_NAME_RUBY_ADJUST;
        pValues[3].Value <<= static_cast<sal_Int16>(rAttr.GetAdjustment());
        pValues[4].Name = UNO_NAME_RUBY_IS_ABOVE;
        pValues[4].Value <<= !rAttr.GetPosition();
        pValues[5].Name = UNO_NAME_RUBY_POSITION;
        pValues[5].Value <<= rAttr.GetPosition();
    }
    return aRet;
}

void SAL_CALL SwXTextView::setRubyList(
    const Sequence< Sequence< PropertyValue > >& rRubyList, sal_Bool /*bAutomatic*/ )
{
    SolarMutexGuard aGuard;

    if(!GetView() || !rRubyList.hasElements())
        throw RuntimeException();
    SwWrtShell& rSh = m_pView->GetWrtShell();
    ShellMode eSelMode = m_pView->GetShellMode();
    if (eSelMode != ShellMode::ListText      &&
        eSelMode != ShellMode::TableListText &&
        eSelMode != ShellMode::TableText      &&
        eSelMode != ShellMode::Text           )
        throw RuntimeException();

    SwRubyList aList;

    for(const Sequence<PropertyValue>& rPropList : rRubyList)
    {
        std::unique_ptr<SwRubyListEntry> pEntry(new SwRubyListEntry);
        OUString sTmp;
        for(const PropertyValue& rProperty : rPropList)
        {
            if(rProperty.Name == UNO_NAME_RUBY_BASE_TEXT)
            {
                rProperty.Value >>= sTmp;
                pEntry->SetText(sTmp);
            }
            else if(rProperty.Name == UNO_NAME_RUBY_TEXT)
            {
                rProperty.Value >>= sTmp;
                pEntry->GetRubyAttr().SetText(sTmp);
            }
            else if(rProperty.Name == UNO_NAME_RUBY_CHAR_STYLE_NAME)
            {
                if(rProperty.Value >>= sTmp)
                {
                    UIName sName;
                    SwStyleNameMapper::FillUIName(ProgName(sTmp), sName, SwGetPoolIdFromName::ChrFmt );
                    const sal_uInt16 nPoolId = sName.isEmpty() ? 0
                        : SwStyleNameMapper::GetPoolIdFromUIName(sName,
                                SwGetPoolIdFromName::ChrFmt );

                    pEntry->GetRubyAttr().SetCharFormatName( sName );
                    pEntry->GetRubyAttr().SetCharFormatId( nPoolId );
                }
            }
            else if(rProperty.Name == UNO_NAME_RUBY_ADJUST)
            {
                sal_Int16 nTmp = 0;
                if(rProperty.Value >>= nTmp)
                    pEntry->GetRubyAttr().SetAdjustment(static_cast<css::text::RubyAdjust>(nTmp));
            }
            else if(rProperty.Name == UNO_NAME_RUBY_IS_ABOVE)
            {
                bool bValue = !rProperty.Value.hasValue() ||
                    *o3tl::doAccess<bool>(rProperty.Value);
                pEntry->GetRubyAttr().SetPosition(bValue ? 0 : 1);
            }
            else if(rProperty.Name == UNO_NAME_RUBY_POSITION)
            {
                sal_Int16 nTmp = 0;
                if(rProperty.Value >>= nTmp)
                    pEntry->GetRubyAttr().SetPosition( nTmp );
            }
        }
        aList.push_back(std::move(pEntry));
    }
    SwDoc* pDoc = m_pView->GetDocShell()->GetDoc();
    pDoc->SetRubyList( *rSh.GetCursor(), aList );
}

SfxObjectShellLock SwXTextView::BuildTmpSelectionDoc()
{
    SwWrtShell& rOldSh = m_pView->GetWrtShell();
    SfxPrinter *pPrt = rOldSh.getIDocumentDeviceAccess().getPrinter( false );
    rtl::Reference<SwDocShell> pDocSh = new SwDocShell(SfxObjectCreateMode::STANDARD);
    SfxObjectShellLock xDocSh(pDocSh.get());
    xDocSh->DoInitNew();
    SwDoc *const pTempDoc( pDocSh->GetDoc() );
    // #i103634#, #i112425#: do not expand numbering and fields on PDF export
    pTempDoc->SetClipBoard(true);
    rOldSh.FillPrtDoc(*pTempDoc,  pPrt);
    SfxViewFrame* pDocFrame = SfxViewFrame::LoadHiddenDocument( *xDocSh, SFX_INTERFACE_NONE );
    SwView* pDocView = static_cast<SwView*>( pDocFrame->GetViewShell() );
    pDocView->AttrChangedNotify(nullptr);//So that SelectShell is called.
    if (SwWrtShell* pSh = pDocView->GetWrtShellPtr())
    {
        IDocumentDeviceAccess& rIDDA = pSh->getIDocumentDeviceAccess();
        SfxPrinter* pTempPrinter = rIDDA.getPrinter( true );

        const SwPageDesc& rCurPageDesc = rOldSh.GetPageDesc(rOldSh.GetCurPageDesc());

        IDocumentDeviceAccess& rIDDA_old = rOldSh.getIDocumentDeviceAccess();

        if( rIDDA_old.getPrinter( false ) )
        {
            rIDDA.setJobsetup( *rIDDA_old.getJobsetup() );
            //#69563# if it isn't the same printer then the pointer has been invalidated!
            pTempPrinter = rIDDA.getPrinter( true );
        }

        pTempPrinter->SetPaperBin(rCurPageDesc.GetMaster().GetPaperBin().GetValue());
    }

    return xDocSh;
}

void SwXTextView::NotifySelChanged()
{
    OSL_ENSURE( m_pView, "view is missing" );

    lang::EventObject const aEvent(getXWeak());
    m_SelChangedListeners.notifyEach(
            &view::XSelectionChangeListener::selectionChanged, aEvent);
}

void SwXTextView::NotifyDBChanged()
{
    URL aURL;
    aURL.Complete = OUString::createFromAscii(SwXDispatch::GetDBChangeURL());

    m_SelChangedListeners.forEach(
        [&aURL] (const uno::Reference<XSelectionChangeListener>& xListener)
        {
            uno::Reference<XDispatch> xDispatch(xListener, UNO_QUERY);
            if (xDispatch)
                xDispatch->dispatch(aURL, {});
        });
}

uno::Reference< beans::XPropertySetInfo > SAL_CALL SwXTextView::getPropertySetInfo(  )
{
    SolarMutexGuard aGuard;
    static uno::Reference< XPropertySetInfo > aRef = m_pPropSet->getPropertySetInfo();
    return aRef;
}

void SAL_CALL SwXTextView::setPropertyValue(
        const OUString& rPropertyName, const uno::Any& rValue )
{
    SolarMutexGuard aGuard;

    if (rPropertyName == "PDFExport_ShowChanges")
    {
        // hack used in PDF export code to turn off (or on) exporting showing tracked changes
        // logic is simplified from FN_REDLINE_SHOW path in SwView
        bool bShow = false;
        rValue >>= bShow;
        SwWrtShell& rSh = m_pView->GetWrtShell();
        rSh.StartAllAction();
        rSh.GetLayout()->SetHideRedlines(!bShow);
        rSh.EndAllAction();
        return;
    }

    const SfxItemPropertyMapEntry* pEntry = m_pPropSet->getPropertyMap().getByName( rPropertyName );
    if (!pEntry)
        throw UnknownPropertyException(rPropertyName);
    else if (pEntry->nFlags & PropertyAttribute::READONLY)
        throw PropertyVetoException();
    else
    {
        switch (pEntry->nWID)
        {
            case WID_IS_HIDE_SPELL_MARKS :
                // deprecated #i91949
            break;
            case WID_IS_CONSTANT_SPELLCHECK :
            {
                bool bVal = false;
                const SwViewOption *pOpt = m_pView->GetWrtShell().GetViewOptions();
                if (!pOpt || !(rValue >>= bVal))
                    throw RuntimeException();
                SwViewOption aNewOpt( *pOpt );
                if (pEntry->nWID == WID_IS_CONSTANT_SPELLCHECK)
                    aNewOpt.SetOnlineSpell(bVal);
                m_pView->GetWrtShell().ApplyViewOptions( aNewOpt );
            }
            break;
            default :
                OSL_FAIL("unknown WID");
        }
    }
}

uno::Any SAL_CALL SwXTextView::getPropertyValue(
        const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;

    Any aRet;

    if (rPropertyName == "PDFExport_ShowChanges")
    {
        const bool bShow = !m_pView->GetWrtShell().GetLayout()->IsHideRedlines();
        aRet <<= bShow;
        return aRet;
    }

    const SfxItemPropertyMapEntry* pEntry = m_pPropSet->getPropertyMap().getByName( rPropertyName );
    if (!pEntry)
        throw UnknownPropertyException(rPropertyName);

    sal_Int16 nWID = pEntry->nWID;
    switch (nWID)
    {
        case WID_PAGE_COUNT :
        case WID_LINE_COUNT :
        {
            // format document completely in order to get meaningful
            // values for page count and line count
            m_pView->GetWrtShell().CalcLayout();

            sal_Int32 nCount = -1;
            if (nWID == WID_PAGE_COUNT)
                nCount = m_pView->GetWrtShell().GetPageCount();
            else // WID_LINE_COUNT
                nCount = m_pView->GetWrtShell().GetLineCount();
            aRet <<= nCount;
        }
        break;
        case WID_IS_HIDE_SPELL_MARKS :
            // deprecated #i91949
        break;
        case WID_IS_CONSTANT_SPELLCHECK :
        {
            const SwViewOption *pOpt = m_pView->GetWrtShell().GetViewOptions();
            if (!pOpt)
                throw RuntimeException();
            aRet <<= pOpt->IsOnlineSpell();
        }
        break;
        default :
            OSL_FAIL("unknown WID");
    }

    return aRet;
}

void SAL_CALL SwXTextView::addPropertyChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XPropertyChangeListener >& /*rxListener*/ )
{
    OSL_FAIL("not implemented");
}

void SAL_CALL SwXTextView::removePropertyChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XPropertyChangeListener >& /*rxListener*/ )
{
    OSL_FAIL("not implemented");
}

void SAL_CALL SwXTextView::addVetoableChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XVetoableChangeListener >& /*rxListener*/ )
{
    OSL_FAIL("not implemented");
}

void SAL_CALL SwXTextView::removeVetoableChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XVetoableChangeListener >& /*rxListener*/ )
{
    OSL_FAIL("not implemented");
}

OUString SwXTextView::getImplementationName()
{
    return u"SwXTextView"_ustr;
}

sal_Bool SwXTextView::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXTextView::getSupportedServiceNames()
{
    return { u"com.sun.star.text.TextDocumentView"_ustr, u"com.sun.star.view.OfficeDocumentView"_ustr };
}

SwXTextViewCursor::SwXTextViewCursor(SwView* pVw) :
    m_pView(pVw),
    m_pPropSet(aSwMapProvider.GetPropertySet(PROPERTY_MAP_TEXT_CURSOR))
{
}

SwXTextViewCursor::~SwXTextViewCursor()
{
}

// used to determine if there is a text selection or not.
// If there is no text selection the functions that need a working
// cursor will be disabled (throw RuntimeException). This will be the case
// for the following interfaces:
// - XViewCursor
// - XTextCursor
// - XTextRange
// - XLineCursor
bool SwXTextViewCursor::IsTextSelection( bool bAllowTables ) const
{

    bool bRes = false;
    OSL_ENSURE(m_pView, "m_pView is NULL ???");
    if(m_pView)
    {
        //! m_pView->GetShellMode() will only work after the shell
        //! has already changed and thus can not be used here!
        SelectionType eSelType = m_pView->GetWrtShell().GetSelectionType();
        bRes =  ( (SelectionType::Text & eSelType) ||
                  (SelectionType::NumberList & eSelType) )  &&
                (!(SelectionType::TableCell & eSelType) || bAllowTables);
    }
    return bRes;
}

sal_Bool SwXTextViewCursor::isVisible()
{
    OSL_FAIL("not implemented");
    return true;
}

void SwXTextViewCursor::setVisible(sal_Bool /*bVisible*/)
{
    OSL_FAIL("not implemented");
}

awt::Point SwXTextViewCursor::getPosition()
{
    SolarMutexGuard aGuard;
    awt::Point aRet;
    if(!m_pView)
        throw uno::RuntimeException();

    const SwWrtShell& rSh = m_pView->GetWrtShell();
    const SwRect& aCharRect(rSh.GetCharRect());

    const SwFrameFormat& rMaster = rSh.GetPageDesc( rSh.GetCurPageDesc() ).GetMaster();

    const SvxULSpaceItem& rUL = rMaster.GetULSpace();
    const tools::Long nY = aCharRect.Top() - (rUL.GetUpper() + DOCUMENTBORDER);
    aRet.Y = convertTwipToMm100(nY);

    const SvxLRSpaceItem& rLR = rMaster.GetLRSpace();
    const tools::Long nX = aCharRect.Left() - (rLR.ResolveLeft({}) + DOCUMENTBORDER);
    aRet.X = convertTwipToMm100(nX);

    return aRet;
}

void SwXTextViewCursor::collapseToStart()
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    SwWrtShell& rSh = m_pView->GetWrtShell();
    if(rSh.HasSelection())
    {
        SwPaM* pShellCursor = rSh.GetCursor();
        if(*pShellCursor->GetPoint() > *pShellCursor->GetMark())
            pShellCursor->Exchange();
        pShellCursor->DeleteMark();
        rSh.EnterStdMode();
        rSh.SetSelection(*pShellCursor);
    }

}

void SwXTextViewCursor::collapseToEnd()
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    SwWrtShell& rSh = m_pView->GetWrtShell();
    if(rSh.HasSelection())
    {
        SwPaM* pShellCursor = rSh.GetCursor();
        if(*pShellCursor->GetPoint() < *pShellCursor->GetMark())
            pShellCursor->Exchange();
        pShellCursor->DeleteMark();
        rSh.EnterStdMode();
        rSh.SetSelection(*pShellCursor);
    }

}

sal_Bool SwXTextViewCursor::isCollapsed()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    const SwWrtShell& rSh = m_pView->GetWrtShell();
    bRet = !rSh.HasSelection();

    return bRet;

}

sal_Bool SwXTextViewCursor::goLeft(sal_Int16 nCount, sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    bRet = m_pView->GetWrtShell().Left( SwCursorSkipMode::Chars, bExpand, nCount, true );

    return bRet;
}

sal_Bool SwXTextViewCursor::goRight(sal_Int16 nCount, sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    bRet = m_pView->GetWrtShell().Right( SwCursorSkipMode::Chars, bExpand, nCount, true );

    return bRet;

}

void SwXTextViewCursor::gotoRange(
    const uno::Reference< text::XTextRange > & xRange,
    sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    if(!(m_pView && xRange.is()))
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    SwUnoInternalPaM rDestPam(*m_pView->GetDocShell()->GetDoc());
    if (!::sw::XTextRangeToSwPaM(rDestPam, xRange))
    {
        throw uno::RuntimeException();
    }

    ShellMode   eSelMode = m_pView->GetShellMode();
    SwWrtShell& rSh = m_pView->GetWrtShell();
    // call EnterStdMode in non-text selections only
    if(!bExpand ||
       (eSelMode != ShellMode::TableText &&
        eSelMode != ShellMode::ListText &&
        eSelMode != ShellMode::TableListText &&
        eSelMode != ShellMode::Text ))
            rSh.EnterStdMode();
    SwPaM* pShellCursor = rSh.GetCursor();
    SwPaM aOwnPaM(*pShellCursor->GetPoint());
    if(pShellCursor->HasMark())
    {
        aOwnPaM.SetMark();
        *aOwnPaM.GetMark() = *pShellCursor->GetMark();
    }

    SwXTextRange* pRange = dynamic_cast<SwXTextRange*>(xRange.get());
    SwXParagraph* pPara = dynamic_cast<SwXParagraph*>(xRange.get());
    OTextCursorHelper* pCursor = dynamic_cast<OTextCursorHelper*>(xRange.get());

    const FrameTypeFlags nFrameType = rSh.GetFrameType(nullptr,true);

    SwStartNodeType eSearchNodeType = SwNormalStartNode;
    if(nFrameType & FrameTypeFlags::FLY_ANY)
        eSearchNodeType = SwFlyStartNode;
    else if(nFrameType &FrameTypeFlags::HEADER)
        eSearchNodeType = SwHeaderStartNode;
    else if(nFrameType & FrameTypeFlags::FOOTER)
        eSearchNodeType = SwFooterStartNode;
    else if(nFrameType & FrameTypeFlags::TABLE)
        eSearchNodeType = SwTableBoxStartNode;
    else if(nFrameType & FrameTypeFlags::FOOTNOTE)
        eSearchNodeType = SwFootnoteStartNode;

    const SwStartNode* pOwnStartNode = aOwnPaM.GetPointNode().
                                            FindStartNodeByType(eSearchNodeType);

    const SwNode* pSrcNode = nullptr;
    if(pCursor && pCursor->GetPaM())
    {
        pSrcNode = &pCursor->GetPaM()->GetPointNode();
    }
    else if (pRange)
    {
        SwPaM aPam(pRange->GetDoc().GetNodes());
        if (pRange->GetPositions(aPam))
        {
            pSrcNode = &aPam.GetPointNode();
        }
    }
    else if (pPara && pPara->GetTextNode())
    {
        pSrcNode = pPara->GetTextNode();
    }
    const SwStartNode* pTmp = pSrcNode ? pSrcNode->FindStartNodeByType(eSearchNodeType) : nullptr;

    //Skip SectionNodes
    while(pTmp && pTmp->IsSectionNode())
    {
        pTmp = pTmp->StartOfSectionNode();
    }
    while(pOwnStartNode && pOwnStartNode->IsSectionNode())
    {
        pOwnStartNode = pOwnStartNode->StartOfSectionNode();
    }
    //Without Expand it is allowed to jump out with the ViewCursor everywhere,
    //with Expand only in the same environment
    if(bExpand &&
        (pOwnStartNode != pTmp ||
        (eSelMode != ShellMode::TableText &&
            eSelMode != ShellMode::ListText &&
            eSelMode != ShellMode::TableListText &&
            eSelMode != ShellMode::Text)))
        throw uno::RuntimeException();

    //Now, the selection must be expanded.
    if(bExpand)
    {
        // The cursor should include everything that has been included
        // by him and the transferred Range.
        SwPosition aOwnLeft(*aOwnPaM.Start());
        SwPosition aOwnRight(*aOwnPaM.End());
        auto [pParamLeft, pParamRight] = rDestPam.StartEnd(); // SwPosition*
        // Now four SwPositions are there, two of them are needed, but which?
        if(aOwnRight > *pParamRight)
            *aOwnPaM.GetPoint() = std::move(aOwnRight);
        else
            *aOwnPaM.GetPoint() = *pParamRight;
        aOwnPaM.SetMark();
        if(aOwnLeft < *pParamLeft)
            *aOwnPaM.GetMark() = std::move(aOwnLeft);
        else
            *aOwnPaM.GetMark() = *pParamLeft;
    }
    else
    {
        //The cursor shall match the passed range.
        *aOwnPaM.GetPoint() = *rDestPam.GetPoint();
        if(rDestPam.HasMark())
        {
            aOwnPaM.SetMark();
            *aOwnPaM.GetMark() = *rDestPam.GetMark();
        }
        else
            aOwnPaM.DeleteMark();
    }
    rSh.SetSelection(aOwnPaM);


}

void SwXTextViewCursor::gotoStart(sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("SwXTextViewCursor::gotoStart");
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    m_pView->GetWrtShell().StartOfSection( bExpand );

}

void SwXTextViewCursor::gotoEnd(sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("SwXTextViewCursor::gotoEnd");
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    m_pView->GetWrtShell().EndOfSection( bExpand );

}

sal_Bool SwXTextViewCursor::jumpToFirstPage()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    SwWrtShell& rSh = m_pView->GetWrtShell();
    if (rSh.IsSelFrameMode())
    {
        rSh.UnSelectFrame();
        rSh.LeaveSelFrameMode();
    }
    rSh.EnterStdMode();
    bRet = rSh.SttEndDoc(true);

    return bRet;
}

sal_Bool SwXTextViewCursor::jumpToLastPage()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    SwWrtShell& rSh = m_pView->GetWrtShell();
    if (rSh.IsSelFrameMode())
    {
        rSh.UnSelectFrame();
        rSh.LeaveSelFrameMode();
    }
    rSh.EnterStdMode();
    bRet = rSh.SttEndDoc(false);
    rSh.SttPg();

    return bRet;
}

sal_Bool SwXTextViewCursor::jumpToPage(sal_Int16 nPage)
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    bRet = m_pView->GetWrtShell().GotoPage(nPage, true);

    return bRet;
}

sal_Bool SwXTextViewCursor::jumpToNextPage()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    bRet = m_pView->GetWrtShell().SttNxtPg();

    return bRet;
}

sal_Bool SwXTextViewCursor::jumpToPreviousPage()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    bRet = m_pView->GetWrtShell().EndPrvPg();

    return bRet;
}

sal_Bool SwXTextViewCursor::jumpToEndOfPage()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    bRet = m_pView->GetWrtShell().EndPg();

    return bRet;
}

sal_Bool SwXTextViewCursor::jumpToStartOfPage()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    bRet = m_pView->GetWrtShell().SttPg();

    return bRet;
}

sal_Int16 SwXTextViewCursor::getPage()
{
    SolarMutexGuard aGuard;
    sal_Int16 nRet = 0;
    if(!m_pView)
        throw uno::RuntimeException();

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    nRet = static_cast<sal_Int16>(pShellCursor->GetPageNum());

    return nRet;
}

sal_Bool SwXTextViewCursor::screenDown()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    SfxRequest aReq(FN_PAGEDOWN, SfxCallMode::SLOT, m_pView->GetPool());
    m_pView->Execute(aReq);
    const SfxPoolItemHolder& rResult(aReq.GetReturnValue());
    bRet = rResult && static_cast<const SfxBoolItem*>(rResult.getItem())->GetValue();

    return bRet;
}

sal_Bool SwXTextViewCursor::screenUp()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    SfxRequest aReq(FN_PAGEUP, SfxCallMode::SLOT, m_pView->GetPool());
    m_pView->Execute(aReq);
    const SfxPoolItemHolder& rResult(aReq.GetReturnValue());
    bRet = rResult && static_cast<const SfxBoolItem*>(rResult.getItem())->GetValue();

    return bRet;
}

uno::Reference< text::XText >  SwXTextViewCursor::getText()
{
    SolarMutexGuard aGuard;
    uno::Reference< text::XText >  xRet;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection( false ))
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    SwDoc* pDoc = m_pView->GetDocShell()->GetDoc();
    xRet = ::sw::CreateParentXText(*pDoc, *pShellCursor->Start());

    return xRet;
}

uno::Reference< text::XTextRange > SwXTextViewCursor::getStart()
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    SwDoc* pDoc = m_pView->GetDocShell()->GetDoc();

    return SwXTextRange::CreateXTextRange(*pDoc, *pShellCursor->Start(), nullptr);
}

uno::Reference< text::XTextRange > SwXTextViewCursor::getEnd()
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    SwDoc* pDoc = m_pView->GetDocShell()->GetDoc();

    return SwXTextRange::CreateXTextRange(*pDoc, *pShellCursor->End(), nullptr);
}

OUString SwXTextViewCursor::getString()
{
    SolarMutexGuard aGuard;
    OUString uRet;
    if(m_pView)
    {
        if (!IsTextSelection( false ))
        {
            SAL_WARN("sw.uno", "no text selection in getString() " << getXWeak());
            return uRet;
        }

        ShellMode eSelMode = m_pView->GetShellMode();
        switch(eSelMode)
        {
            //! since setString for SEL_TABLE_TEXT (with possible
            //! multi selection of cells) would not work properly we
            //! will ignore this case for both
            //! functions (setString AND getString) because of symmetrie.

            case ShellMode::ListText       :
            case ShellMode::TableListText:
            case ShellMode::Text            :
            {
                SwWrtShell& rSh = m_pView->GetWrtShell();
                SwPaM* pShellCursor = rSh.GetCursor();
                SwUnoCursorHelper::GetTextFromPam(*pShellCursor, uRet,
                        rSh.GetLayout());
                break;
            }
            default:;//prevent warning
        }
    }
    return uRet;
}

void SwXTextViewCursor::setString(const OUString& aString)
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        return;

    if (!IsTextSelection( false ))
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    ShellMode eSelMode = m_pView->GetShellMode();
    switch(eSelMode)
    {
        //! since setString for SEL_TABLE_TEXT (with possible
        //! multi selection of cells) would not work properly we
        //! will ignore this case for both
        //! functions (setString AND getString) because of symmetrie.

        case ShellMode::ListText       :
        case ShellMode::TableListText :
        case ShellMode::Text            :
        {
            SwWrtShell& rSh = m_pView->GetWrtShell();
            SwCursor* pShellCursor = rSh.GetCursor();
            SwUnoCursorHelper::SetString(*pShellCursor, aString);
            break;
        }
        default:;//prevent warning
    }
}

uno::Reference< XPropertySetInfo >  SwXTextViewCursor::getPropertySetInfo(  )
{
    static uno::Reference< XPropertySetInfo >  xRef = m_pPropSet->getPropertySetInfo();
    return xRef;
}

void  SwXTextViewCursor::setPropertyValue( const OUString& rPropertyName, const Any& aValue )
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw RuntimeException();

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    SwNode& rNode = pShellCursor->GetPointNode();
    if (!rNode.IsTextNode())
        throw RuntimeException();

    SwUnoCursorHelper::SetPropertyValue(
        *pShellCursor, *m_pPropSet, rPropertyName, aValue );


}

Any  SwXTextViewCursor::getPropertyValue( const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;
    Any aRet;
    if(!m_pView)
        throw RuntimeException();

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    aRet = SwUnoCursorHelper::GetPropertyValue(
            *pShellCursor, *m_pPropSet, rPropertyName);

    return aRet;
}

void  SwXTextViewCursor::addPropertyChangeListener(
    const OUString& /*aPropertyName*/, const uno::Reference< XPropertyChangeListener >& /*xListener*/ )
{
}

void  SwXTextViewCursor::removePropertyChangeListener(
    const OUString& /*aPropertyName*/, const uno::Reference< XPropertyChangeListener >& /*aListener*/ )
{
}

void  SwXTextViewCursor::addVetoableChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< XVetoableChangeListener >& /*aListener*/ )
{
}

void  SwXTextViewCursor::removeVetoableChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< XVetoableChangeListener >& /*aListener*/ )
{
}

PropertyState  SwXTextViewCursor::getPropertyState( const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;
    PropertyState eState;
    if(!m_pView)
        throw RuntimeException();

    SwWrtShell& rSh = m_pView->GetWrtShell();
    SwPaM* pShellCursor = rSh.GetCursor();
    eState = SwUnoCursorHelper::GetPropertyState(
            *pShellCursor, *m_pPropSet, rPropertyName);

    return eState;
}

Sequence< PropertyState >  SwXTextViewCursor::getPropertyStates(
    const Sequence< OUString >& rPropertyNames )
{
    SolarMutexGuard aGuard;
    Sequence< PropertyState >  aRet;
    if(m_pView)
    {
        SwWrtShell& rSh = m_pView->GetWrtShell();
        SwPaM* pShellCursor = rSh.GetCursor();
        aRet = SwUnoCursorHelper::GetPropertyStates(
                *pShellCursor, *m_pPropSet,  rPropertyNames);
    }
    return aRet;
}

void  SwXTextViewCursor::setPropertyToDefault( const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;
    if(m_pView)
    {
        SwWrtShell& rSh = m_pView->GetWrtShell();
        SwPaM* pShellCursor = rSh.GetCursor();
        SwUnoCursorHelper::SetPropertyToDefault(
                *pShellCursor, *m_pPropSet, rPropertyName);
    }
}

Any  SwXTextViewCursor::getPropertyDefault( const OUString& rPropertyName )
{
    Any aRet;
    SolarMutexGuard aGuard;
    if(m_pView)
    {
        SwWrtShell& rSh = m_pView->GetWrtShell();
        SwPaM* pShellCursor = rSh.GetCursor();
        aRet = SwUnoCursorHelper::GetPropertyDefault(
                *pShellCursor, *m_pPropSet, rPropertyName);
    }
    return aRet;
}

sal_Bool SwXTextViewCursor::goDown(sal_Int16 nCount, sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("SwXTextViewCursor::goDown");
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    bRet = m_pView->GetWrtShell().Down( bExpand, nCount, true );

    return bRet;
}

sal_Bool SwXTextViewCursor::goUp(sal_Int16 nCount, sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    comphelper::ProfileZone aZone("SwXTextViewCursor::goUp");
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection())
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    bRet = m_pView->GetWrtShell().Up( bExpand, nCount, true );

    return bRet;
}

sal_Bool SwXTextViewCursor::isAtStartOfLine()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection( false ))
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    bRet = m_pView->GetWrtShell().IsAtLeftMargin();

    return bRet;
}

sal_Bool SwXTextViewCursor::isAtEndOfLine()
{
    SolarMutexGuard aGuard;
    bool bRet = false;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection( false ))
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    bRet = m_pView->GetWrtShell().IsAtRightMargin();

    return bRet;
}

void SwXTextViewCursor::gotoEndOfLine(sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection( false ))
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    m_pView->GetWrtShell().RightMargin(bExpand, true);

}

void SwXTextViewCursor::gotoStartOfLine(sal_Bool bExpand)
{
    SolarMutexGuard aGuard;
    if(!m_pView)
        throw uno::RuntimeException();

    if (!IsTextSelection( false ))
        throw  uno::RuntimeException(u"no text selection"_ustr, getXWeak() );

    m_pView->GetWrtShell().LeftMargin(bExpand, true);

}

OUString SwXTextViewCursor::getImplementationName()
{
    return u"SwXTextViewCursor"_ustr;
}

sal_Bool SwXTextViewCursor::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SwXTextViewCursor::getSupportedServiceNames()
{
    return { u"com.sun.star.text.TextViewCursor"_ustr,
             u"com.sun.star.style.CharacterProperties"_ustr,
             u"com.sun.star.style.CharacterPropertiesAsian"_ustr,
             u"com.sun.star.style.CharacterPropertiesComplex"_ustr,
             u"com.sun.star.style.ParagraphProperties"_ustr,
             u"com.sun.star.style.ParagraphPropertiesAsian"_ustr,
             u"com.sun.star.style.ParagraphPropertiesComplex"_ustr };
}

const SwDoc*        SwXTextViewCursor::GetDoc() const
{
    SwWrtShell& rSh = m_pView->GetWrtShell();
    return   rSh.GetCursor() ? &rSh.GetCursor()->GetDoc() : nullptr;
}

SwDoc*  SwXTextViewCursor::GetDoc()
{
    SwWrtShell& rSh = m_pView->GetWrtShell();
    return   rSh.GetCursor() ? &rSh.GetCursor()->GetDoc() : nullptr;
}

const SwPaM*    SwXTextViewCursor::GetPaM() const
{
    SwWrtShell& rSh = m_pView->GetWrtShell();
    return rSh.GetCursor();
}

SwPaM*  SwXTextViewCursor::GetPaM()
{
    SwWrtShell& rSh = m_pView->GetWrtShell();
    return rSh.GetCursor();
}

uno::Reference<datatransfer::XTransferable> SAL_CALL
SwXTextView::getTransferableForTextRange(uno::Reference<text::XTextRange> const& xTextRange)
{
    SolarMutexGuard aGuard;

    // the point is we can copy PaM that wouldn't be legal as shell cursor
    SwUnoInternalPaM aPam(*m_pView->GetDocShell()->GetDoc());
    if (!::sw::XTextRangeToSwPaM(aPam, xTextRange, ::sw::TextRangeMode::AllowNonTextNode))
    {
        throw uno::RuntimeException(u"invalid text range"_ustr);
    }

    //force immediate shell update
    GetView()->StopShellTimer();
    SwWrtShell& rSh = GetView()->GetWrtShell();
    rtl::Reference<SwTransferable> pTransfer = new SwTransferable(rSh);
    const bool bLockedView = rSh.IsViewLocked();
    rSh.LockView( true );
    pTransfer->PrepareForCopyTextRange(aPam);
    rSh.LockView( bLockedView );
    return pTransfer;
}

OUString SAL_CALL SwXTextView::dump(const OUString& rKind)
{
    if (rKind == "layout")
    {
        SwRootFrame* pLayout = GetView()->GetWrtShell().GetLayout();
        return comphelper::dumpXmlToString([pLayout](xmlTextWriterPtr pWriter)
                                           { pLayout->dumpAsXml(pWriter); });
    }

    return OUString();
}

uno::Reference< datatransfer::XTransferable > SAL_CALL SwXTextView::getTransferable()
{
    SolarMutexGuard aGuard;

    //force immediate shell update
    GetView()->StopShellTimer();
    SwWrtShell& rSh = GetView()->GetWrtShell();
    if ( GetView()->GetShellMode() == ShellMode::DrawText )
    {
        SdrView *pSdrView = rSh.GetDrawView();
        OutlinerView* pOLV = pSdrView->GetTextEditOutlinerView();
        return pOLV->GetEditView().GetTransferable();
    }
    else
    {
        rtl::Reference<SwTransferable> pTransfer = new SwTransferable( rSh );
        const bool bLockedView = rSh.IsViewLocked();
        rSh.LockView( true );    //lock visible section
        pTransfer->PrepareForCopy();
        rSh.LockView( bLockedView );
        return pTransfer;
    }
}

void SAL_CALL SwXTextView::insertTransferable( const uno::Reference< datatransfer::XTransferable >& xTrans )
{
    SolarMutexGuard aGuard;

    //force immediate shell update
    GetView()->StopShellTimer();
    SwWrtShell& rSh = GetView()->GetWrtShell();
    if ( GetView()->GetShellMode() == ShellMode::DrawText )
    {
        SdrView *pSdrView = rSh.GetDrawView();
        OutlinerView* pOLV = pSdrView->GetTextEditOutlinerView();
        pOLV->GetEditView().InsertText( xTrans, GetView()->GetDocShell()->GetMedium()->GetBaseURL(), false );
    }
    else
    {
        TransferableDataHelper aDataHelper( xTrans );
        if ( SwTransferable::IsPaste( rSh, aDataHelper ) )
        {
            SwTransferable::Paste( rSh, aDataHelper );
            if( rSh.IsFrameSelected() || rSh.GetSelectedObjCount() )
                rSh.EnterSelFrameMode();
            GetView()->AttrChangedNotify(nullptr);
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
