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

#include <strings.hrc>
#include <helpids.h>
#include <iderid.hxx>

#include <accessibledialogwindow.hxx>
#include <baside3.hxx>
#include <basidesh.hxx>
#include <bastype2.hxx>
#include <basobj.hxx>
#include <dlged.hxx>
#include <dlgeddef.hxx>
#include <dlgedmod.hxx>
#include <dlgedview.hxx>
#include <iderdll.hxx>
#include <localizationmgr.hxx>
#include <managelang.hxx>

#include <com/sun/star/script/XLibraryContainer2.hpp>
#include <com/sun/star/resource/StringResourceWithLocation.hpp>
#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/XFilePicker3.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerControlAccess.hpp>
#include <comphelper/processfactory.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/filedlghelper.hxx>
#include <sfx2/request.hxx>
#include <sfx2/viewfrm.hxx>
#include <svl/visitem.hxx>
#include <svl/whiter.hxx>
#include <svx/svdundo.hxx>
#include <svx/svxids.hrc>
#include <comphelper/diagnose_ex.hxx>
#include <tools/urlobj.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/weld.hxx>
#include <vcl/settings.hxx>
#include <vcl/stdtext.hxx>
#include <vcl/svapp.hxx>
#include <xmlscript/xmldlg_imexp.hxx>

namespace basctl
{

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::io;
using namespace ::com::sun::star::resource;
using namespace ::com::sun::star::ui::dialogs;

#ifdef _WIN32
constexpr OUString FilterMask_All = u"*.*"_ustr;
#else
constexpr OUString FilterMask_All = u"*"_ustr;
#endif

DialogWindow::DialogWindow(DialogWindowLayout* pParent, ScriptDocument const& rDocument,
                           const OUString& aLibName, const OUString& aName,
                           css::uno::Reference<css::container::XNameContainer> const& xDialogModel)
    : BaseWindow(pParent, rDocument, aLibName, aName)
    ,m_rLayout(*pParent)
    ,m_pEditor(new DlgEditor(*this, m_rLayout, rDocument.isDocument()
                                            ? rDocument.getDocument()
                                            : Reference<frame::XModel>(), xDialogModel))
    ,m_pUndoMgr(new SfxUndoManager)
    ,m_nControlSlotId(SID_INSERT_SELECT)
{
    InitSettings();

    m_pEditor->GetModel().SetNotifyUndoActionHdl(
        &DialogWindow::NotifyUndoActionHdl
    );

    SetHelpId( HID_BASICIDE_DIALOGWINDOW );

    // set readonly mode for readonly libraries
    Reference< script::XLibraryContainer2 > xDlgLibContainer( GetDocument().getLibraryContainer( E_DIALOGS ) );
    if ( xDlgLibContainer.is() && xDlgLibContainer->hasByName( aLibName ) && xDlgLibContainer->isLibraryReadOnly( aLibName ) )
        SetReadOnly(true);

    if ( rDocument.isDocument() && rDocument.isReadOnly() )
        SetReadOnly(true);
}

void DialogWindow::dispose()
{
    m_pEditor.reset();
    BaseWindow::dispose();
}

void DialogWindow::LoseFocus()
{
    if ( IsModified() )
        StoreData();

    Window::LoseFocus();
}

void DialogWindow::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    m_pEditor->Paint(rRenderContext, rRect);
}

void DialogWindow::Resize()
{
    if (GetHScrollBar() && GetVScrollBar())
    {
        m_pEditor->SetScrollBars( GetHScrollBar(), GetVScrollBar() );
    }
}

void DialogWindow::MouseButtonDown( const MouseEvent& rMEvt )
{
    m_pEditor->MouseButtonDown( rMEvt );

    if (SfxBindings* pBindings = GetBindingsPtr())
        pBindings->Invalidate( SID_SHOW_PROPERTYBROWSER );
}

void DialogWindow::MouseButtonUp( const MouseEvent& rMEvt )
{
    m_pEditor->MouseButtonUp( rMEvt );
    if( (m_pEditor->GetMode() == DlgEditor::INSERT) && !m_pEditor->IsCreateOK() )
    {
        m_nControlSlotId = SID_INSERT_SELECT;
        m_pEditor->SetMode( DlgEditor::SELECT );
        Shell::InvalidateControlSlots();
    }
    if (SfxBindings* pBindings = GetBindingsPtr())
    {
        pBindings->Invalidate( SID_SHOW_PROPERTYBROWSER );
        pBindings->Invalidate( SID_DOC_MODIFIED );
        pBindings->Invalidate( SID_SAVEDOC );
        pBindings->Invalidate( SID_COPY );
        pBindings->Invalidate( SID_CUT );
    }
}

void DialogWindow::MouseMove( const MouseEvent& rMEvt )
{
    m_pEditor->MouseMove( rMEvt );
}

void DialogWindow::KeyInput( const KeyEvent& rKEvt )
{
    SfxBindings* pBindings = GetBindingsPtr();

    if( rKEvt.GetKeyCode() == KEY_BACKSPACE )
    {
        if (SfxDispatcher* pDispatcher = GetDispatcher())
            pDispatcher->Execute( SID_BACKSPACE );
    }
    else
    {
        if( pBindings && rKEvt.GetKeyCode() == KEY_TAB )
            pBindings->Invalidate( SID_SHOW_PROPERTYBROWSER );

        if( !m_pEditor->KeyInput( rKEvt ) )
        {
            if( !SfxViewShell::Current()->KeyInput( rKEvt ) )
                Window::KeyInput( rKEvt );
        }
    }

    // may be KEY_TAB, KEY_BACKSPACE, KEY_ESCAPE
    if( pBindings )
    {
        pBindings->Invalidate( SID_COPY );
        pBindings->Invalidate( SID_CUT );
    }
}

void DialogWindow::Command( const CommandEvent& rCEvt )
{
    if ( ( rCEvt.GetCommand() == CommandEventId::Wheel           ) ||
         ( rCEvt.GetCommand() == CommandEventId::StartAutoScroll ) ||
         ( rCEvt.GetCommand() == CommandEventId::AutoScroll      ) )
    {
        HandleScrollCommand( rCEvt, GetHScrollBar(), GetVScrollBar() );
    }
    else if ( rCEvt.GetCommand() == CommandEventId::ContextMenu )
    {
        if (GetDispatcher())
        {
            SdrView& rView = GetView();
            if( !rCEvt.IsMouseEvent() && rView.GetMarkedObjectList().GetMarkCount() != 0 )
            {
                tools::Rectangle aMarkedRect( rView.GetMarkedRect() );
                Point MarkedCenter( aMarkedRect.Center() );
                Point PosPixel( LogicToPixel( MarkedCenter ) );
                SfxDispatcher::ExecutePopup( this, &PosPixel );
            }
            else
            {
                SfxDispatcher::ExecutePopup();
            }

        }
    }
    else
        BaseWindow::Command( rCEvt );
}


void DialogWindow::NotifyUndoActionHdl( std::unique_ptr<SdrUndoAction> )
{
    // #i120515# pUndoAction needs to be deleted, this hand over is an ownership
    // change. As long as it does not get added to the undo manager, it needs at
    // least to be deleted.
}

void DialogWindow::DoInit()
{
    m_pEditor->SetScrollBars( GetHScrollBar(), GetVScrollBar() );
}

void DialogWindow::DoScroll( Scrollable*  )
{
    m_pEditor->DoScroll();
}

void DialogWindow::GetState( SfxItemSet& rSet )
{
    SfxWhichIter aIter(rSet);
    bool bIsCalc = false;
    if ( GetDocument().isDocument() )
    {
        Reference< frame::XModel > xModel= GetDocument().getDocument();
        if ( xModel.is() )
        {
            Reference< lang::XServiceInfo > xServiceInfo ( xModel, UNO_QUERY );
            if ( xServiceInfo.is() && xServiceInfo->supportsService( u"com.sun.star.sheet.SpreadsheetDocument"_ustr ) )
                bIsCalc = true;
        }
    }

    for ( sal_uInt16 nWh = aIter.FirstWhich(); nWh != 0; nWh = aIter.NextWhich() )
    {
        switch ( nWh )
        {
            case SID_PASTE:
            {
                if ( !IsPasteAllowed() )
                    rSet.DisableItem( nWh );

                if ( IsReadOnly() )
                    rSet.DisableItem( nWh );
            }
            break;
            case SID_COPY:
            {
                // any object selected?
                if ( m_pEditor->GetView().GetMarkedObjectList().GetMarkCount() == 0 )
                    rSet.DisableItem( nWh );
            }
            break;
            case SID_CUT:
            case SID_DELETE:
            case SID_BACKSPACE:
            {
                // any object selected?
                if ( m_pEditor->GetView().GetMarkedObjectList().GetMarkCount() == 0 )
                    rSet.DisableItem( nWh );

                if ( IsReadOnly() )
                    rSet.DisableItem( nWh );
            }
            break;
            case SID_REDO:
            {
                if ( !m_pUndoMgr->GetUndoActionCount() )
                    rSet.DisableItem( nWh );
            }
            break;

            case SID_DIALOG_TESTMODE:
            {
                // is the IDE still active?
                bool const bBool = GetShell()->GetFrame() &&
                    m_pEditor->GetMode() == DlgEditor::TEST;
                rSet.Put(SfxBoolItem(SID_DIALOG_TESTMODE, bBool));
            }
            break;

            case SID_CHOOSE_CONTROLS:
            {
                if ( IsReadOnly() )
                    rSet.DisableItem( nWh );
            }
            break;

            case SID_SHOW_PROPERTYBROWSER:
            {
                Shell* pShell = GetShell();
                SfxViewFrame* pViewFrame = pShell ? &pShell->GetViewFrame() : nullptr;
                if ( pViewFrame && !pViewFrame->HasChildWindow( SID_SHOW_PROPERTYBROWSER ) && m_pEditor->GetView().GetMarkedObjectList().GetMarkCount() == 0 )
                    rSet.DisableItem( nWh );

                if ( IsReadOnly() )
                    rSet.DisableItem( nWh );
            }
            break;
            case SID_INSERT_FORM_RADIO:
            case SID_INSERT_FORM_CHECK:
            case SID_INSERT_FORM_LIST:
            case SID_INSERT_FORM_COMBO:
            case SID_INSERT_FORM_VSCROLL:
            case SID_INSERT_FORM_HSCROLL:
            case SID_INSERT_FORM_SPIN:
            {
                if ( !bIsCalc || IsReadOnly() )
                    rSet.DisableItem( nWh );
                else
                    rSet.Put( SfxBoolItem( nWh, m_nControlSlotId == nWh ) );
            }
            break;

            case SID_INSERT_SELECT:
            case SID_INSERT_PUSHBUTTON:
            case SID_INSERT_RADIOBUTTON:
            case SID_INSERT_CHECKBOX:
            case SID_INSERT_LISTBOX:
            case SID_INSERT_COMBOBOX:
            case SID_INSERT_GROUPBOX:
            case SID_INSERT_EDIT:
            case SID_INSERT_FIXEDTEXT:
            case SID_INSERT_IMAGECONTROL:
            case SID_INSERT_PROGRESSBAR:
            case SID_INSERT_HSCROLLBAR:
            case SID_INSERT_VSCROLLBAR:
            case SID_INSERT_HFIXEDLINE:
            case SID_INSERT_VFIXEDLINE:
            case SID_INSERT_DATEFIELD:
            case SID_INSERT_TIMEFIELD:
            case SID_INSERT_NUMERICFIELD:
            case SID_INSERT_CURRENCYFIELD:
            case SID_INSERT_FORMATTEDFIELD:
            case SID_INSERT_PATTERNFIELD:
            case SID_INSERT_FILECONTROL:
            case SID_INSERT_SPINBUTTON:
            case SID_INSERT_GRIDCONTROL:
            case SID_INSERT_HYPERLINKCONTROL:
            case SID_INSERT_TREECONTROL:
            {
                if ( IsReadOnly() )
                    rSet.DisableItem( nWh );
                else
                    rSet.Put( SfxBoolItem( nWh, m_nControlSlotId == nWh ) );
            }
            break;
            case SID_SHOWLINES:
            {
                // if this is not a module window hide the
                // setting, doesn't make sense for example if the
                // dialog editor is open
                rSet.DisableItem(nWh);
                rSet.Put(SfxVisibilityItem(nWh, false));
                break;
            }
            case SID_SELECTALL:
            {
                rSet.DisableItem( nWh );
            }
            break;
        }
    }
}

void DialogWindow::ExecuteCommand( SfxRequest& rReq )
{
    const sal_uInt16 nSlotId(rReq.GetSlot());
    SdrObjKind nInsertObj(SdrObjKind::NONE);

    switch ( nSlotId )
    {
        case SID_CUT:
            if ( !IsReadOnly() )
            {
                GetEditor().Cut();
                if (SfxBindings* pBindings = GetBindingsPtr())
                    pBindings->Invalidate( SID_DOC_MODIFIED );
            }
            break;
        case SID_DELETE:
            if ( !IsReadOnly() )
            {
                GetEditor().Delete();
                if (SfxBindings* pBindings = GetBindingsPtr())
                    pBindings->Invalidate( SID_DOC_MODIFIED );
            }
            break;
        case SID_COPY:
            GetEditor().Copy();
            break;
        case SID_PASTE:
            if ( !IsReadOnly() )
            {
                GetEditor().Paste();
                if (SfxBindings* pBindings = GetBindingsPtr())
                    pBindings->Invalidate( SID_DOC_MODIFIED );
            }
            break;

        case SID_INSERT_FORM_RADIO:
            nInsertObj = SdrObjKind::BasicDialogFormRadio;
            break;
        case SID_INSERT_FORM_CHECK:
            nInsertObj = SdrObjKind::BasicDialogFormCheck;
            break;
        case SID_INSERT_FORM_LIST:
            nInsertObj = SdrObjKind::BasicDialogFormList;
            break;
        case SID_INSERT_FORM_COMBO:
            nInsertObj = SdrObjKind::BasicDialogFormCombo;
            break;
        case SID_INSERT_FORM_SPIN:
            nInsertObj = SdrObjKind::BasicDialogFormSpin;
            break;
        case SID_INSERT_FORM_VSCROLL:
            nInsertObj = SdrObjKind::BasicDialogFormVerticalScroll;
            break;
        case SID_INSERT_FORM_HSCROLL:
            nInsertObj = SdrObjKind::BasicDialogFormHorizontalScroll;
            break;
        case SID_INSERT_PUSHBUTTON:
            nInsertObj = SdrObjKind::BasicDialogPushButton;
            break;
        case SID_INSERT_RADIOBUTTON:
            nInsertObj = SdrObjKind::BasicDialogRadioButton;
            break;
        case SID_INSERT_CHECKBOX:
            nInsertObj = SdrObjKind::BasicDialogCheckbox;
            break;
        case SID_INSERT_LISTBOX:
            nInsertObj = SdrObjKind::BasicDialogListbox;
            break;
        case SID_INSERT_COMBOBOX:
            nInsertObj = SdrObjKind::BasicDialogCombobox;
            break;
        case SID_INSERT_GROUPBOX:
            nInsertObj = SdrObjKind::BasicDialogGroupBox;
            break;
        case SID_INSERT_EDIT:
            nInsertObj = SdrObjKind::BasicDialogEdit;
            break;
        case SID_INSERT_FIXEDTEXT:
            nInsertObj = SdrObjKind::BasicDialogFixedText;
            break;
        case SID_INSERT_IMAGECONTROL:
            nInsertObj = SdrObjKind::BasicDialogImageControl;
            break;
        case SID_INSERT_PROGRESSBAR:
            nInsertObj = SdrObjKind::BasicDialogProgressbar;
            break;
        case SID_INSERT_HSCROLLBAR:
            nInsertObj = SdrObjKind::BasicDialogHorizontalScrollbar;
            break;
        case SID_INSERT_VSCROLLBAR:
            nInsertObj = SdrObjKind::BasicDialogVerticalScrollbar;
            break;
        case SID_INSERT_HFIXEDLINE:
            nInsertObj = SdrObjKind::BasicDialogHorizontalFixedLine;
            break;
        case SID_INSERT_VFIXEDLINE:
            nInsertObj = SdrObjKind::BasicDialogVerticalFixedLine;
            break;
        case SID_INSERT_DATEFIELD:
            nInsertObj = SdrObjKind::BasicDialogDateField;
            break;
        case SID_INSERT_TIMEFIELD:
            nInsertObj = SdrObjKind::BasicDialogTimeField;
            break;
        case SID_INSERT_NUMERICFIELD:
            nInsertObj = SdrObjKind::BasicDialogNumericField;
            break;
        case SID_INSERT_CURRENCYFIELD:
            nInsertObj = SdrObjKind::BasicDialogCurencyField;
            break;
        case SID_INSERT_FORMATTEDFIELD:
            nInsertObj = SdrObjKind::BasicDialogFormattedField;
            break;
        case SID_INSERT_PATTERNFIELD:
            nInsertObj = SdrObjKind::BasicDialogPatternField;
            break;
        case SID_INSERT_FILECONTROL:
            nInsertObj = SdrObjKind::BasicDialogFileControl;
            break;
        case SID_INSERT_SPINBUTTON:
            nInsertObj = SdrObjKind::BasicDialogSpinButton;
            break;
        case SID_INSERT_GRIDCONTROL:
            nInsertObj = SdrObjKind::BasicDialogGridControl;
            break;
        case SID_INSERT_HYPERLINKCONTROL:
            nInsertObj = SdrObjKind::BasicDialogHyperlinkControl;
            break;
        case SID_INSERT_TREECONTROL:
            nInsertObj = SdrObjKind::BasicDialogTreeControl;
            break;
        case SID_INSERT_SELECT:
            m_nControlSlotId = nSlotId;
            GetEditor().SetMode( DlgEditor::SELECT );
            Shell::InvalidateControlSlots();
            break;

        case SID_DIALOG_TESTMODE:
        {
            DlgEditor::Mode eOldMode = GetEditor().GetMode();
            GetEditor().SetMode( DlgEditor::TEST );
            GetEditor().SetMode( eOldMode );
            rReq.Done();
            if (SfxBindings* pBindings = GetBindingsPtr())
                pBindings->Invalidate( SID_DIALOG_TESTMODE );
            return;
        }
        case SID_EXPORT_DIALOG:
            SaveDialog();
            break;

        case SID_IMPORT_DIALOG:
            ImportDialog();
            break;

        case SID_BASICIDE_DELETECURRENT:
            if (QueryDelDialog(m_aName, GetFrameWeld()))
            {
                if (RemoveDialog(m_aDocument, m_aLibName, m_aName))
                {
                    MarkDocumentModified(m_aDocument);
                    GetShell()->RemoveWindow(this, true);
                }
            }
            break;
    }

    if ( nInsertObj != SdrObjKind::NONE )
    {
        m_nControlSlotId = nSlotId;
        GetEditor().SetMode( DlgEditor::INSERT );
        GetEditor().SetInsertObj( nInsertObj );

        if ( rReq.GetModifier() & KEY_MOD1 )
        {
            GetEditor().CreateDefaultObject();
            if (SfxBindings* pBindings = GetBindingsPtr())
                pBindings->Invalidate( SID_DOC_MODIFIED );
        }

        Shell::InvalidateControlSlots();
    }

    rReq.Done();
}

Reference< container::XNameContainer > const & DialogWindow::GetDialog() const
{
    return m_pEditor->GetDialog();
}

bool DialogWindow::RenameDialog( const OUString& rNewName )
{
    if (!basctl::RenameDialog(GetFrameWeld(), GetDocument(), GetLibName(), GetName(), rNewName))
        return false;

    if (SfxBindings* pBindings = GetBindingsPtr())
        pBindings->Invalidate( SID_DOC_MODIFIED );

    return true;
}

void DialogWindow::DisableBrowser()
{
    m_rLayout.DisablePropertyBrowser();
}

void DialogWindow::UpdateBrowser()
{
    m_rLayout.UpdatePropertyBrowser();
}

void DialogWindow::SaveDialog()
{
    sfx2::FileDialogHelper aDlg(ui::dialogs::TemplateDescription::FILESAVE_AUTOEXTENSION,
                                FileDialogFlags::NONE, this->GetFrameWeld());
    aDlg.SetContext(sfx2::FileDialogHelper::BasicExportDialog);
    Reference<XFilePicker3> xFP = aDlg.GetFilePicker();

    xFP.queryThrow<XFilePickerControlAccess>()->setValue(ExtendedFilePickerElementIds::CHECKBOX_AUTOEXTENSION, 0, Any(true));
    xFP->setDefaultName( GetName() );

    OUString aDialogStr(IDEResId(RID_STR_STDDIALOGNAME));
    xFP->appendFilter( aDialogStr, u"*.xdl"_ustr );
    xFP->appendFilter( IDEResId(RID_STR_FILTER_ALLFILES), FilterMask_All );
    xFP->setCurrentFilter( aDialogStr );

    if( aDlg.Execute() != ERRCODE_NONE )
        return;

    OUString aSelectedFileURL = xFP->getSelectedFiles()[0];

    const Reference<uno::XComponentContext>& xContext(comphelper::getProcessComponentContext());
    Reference< XSimpleFileAccess3 > xSFI( SimpleFileAccess::create(xContext) );

    Reference< XOutputStream > xOutput;
    try
    {
        if( xSFI->exists(aSelectedFileURL) )
            xSFI->kill(aSelectedFileURL);
        xOutput = xSFI->openFileWrite(aSelectedFileURL);
    }
    catch(const Exception& )
    {}

    if (!xOutput)
    {
        std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(GetFrameWeld(),
                                                  VclMessageType::Warning, VclButtonsType::Ok, IDEResId(RID_STR_COULDNTWRITE)));
        xBox->run();
        return;
    }

    // export dialog model to xml
    auto xInput(xmlscript::exportDialogModel(GetDialog(), xContext, GetDocument().getDocumentOrNull())->createInputStream());

    for (Sequence<sal_Int8> bytes; xInput->readBytes(bytes, xInput->available());)
        xOutput->writeBytes(bytes);

    // With resource?
    Reference< resource::XStringResourceResolver > xStringResourceResolver;
    if (auto xDialogModelPropSet = GetDialog().query<beans::XPropertySet>())
    {
        try
        {
            Any aResourceResolver = xDialogModelPropSet->getPropertyValue( u"ResourceResolver"_ustr );
            aResourceResolver >>= xStringResourceResolver;
        }
        catch(const beans::UnknownPropertyException& )
        {}
    }

    Sequence<lang::Locale> aLocaleSeq;
    if (xStringResourceResolver)
        aLocaleSeq = xStringResourceResolver->getLocales();
    if (aLocaleSeq.hasElements())
    {
        INetURLObject aURLObj(aSelectedFileURL);
        aURLObj.removeExtension();
        OUString aDialogName( aURLObj.getName() );
        aURLObj.removeSegment();
        OUString aURL( aURLObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ) );
        OUString aComment = "# " + aDialogName + " strings" ;
        Reference< task::XInteractionHandler > xDummyHandler;

        // Remove old properties files in case of overwriting Dialog files
        if( xSFI->isFolder( aURL ) )
        {
            Sequence< OUString > aContentSeq = xSFI->getFolderContents( aURL, false );

            OUString aDialogName_ = aDialogName + "_" ;
            for( const OUString& rCompleteName : aContentSeq )
            {
                OUString aPureName;
                OUString aExtension;
                sal_Int32 iDot = rCompleteName.lastIndexOf( '.' );
                if( iDot != -1 )
                {
                    sal_Int32 iSlash = rCompleteName.lastIndexOf( '/' );
                    sal_Int32 iCopyFrom = (iSlash != -1) ? iSlash + 1 : 0;
                    aPureName = rCompleteName.copy( iCopyFrom, iDot-iCopyFrom );
                    aExtension = rCompleteName.copy( iDot + 1 );
                }

                if( aExtension == "properties" || aExtension == "default" )
                {
                    if( aPureName.startsWith( aDialogName_ ) )
                    {
                        try
                        {
                            xSFI->kill( rCompleteName );
                        }
                        catch(const uno::Exception& )
                        {}
                    }
                }
            }
        }

        Reference< XStringResourceWithLocation > xStringResourceWithLocation =
            StringResourceWithLocation::create( xContext, aURL, false/*bReadOnly*/,
                xStringResourceResolver->getDefaultLocale(), aDialogName, aComment, xDummyHandler );

        // Add locales
        for( const lang::Locale& rLocale : aLocaleSeq )
        {
            xStringResourceWithLocation->newLocale( rLocale );
        }

        LocalizationMgr::copyResourceForDialog( GetDialog(),
            xStringResourceResolver, xStringResourceWithLocation );

        xStringResourceWithLocation->store();
    }
}

static std::vector< lang::Locale > implGetLanguagesOnlyContainedInFirstSeq
    ( const Sequence< lang::Locale >& aFirstSeq, const Sequence< lang::Locale >& aSecondSeq )
{
    std::vector< lang::Locale > avRet;

    std::copy_if(aFirstSeq.begin(), aFirstSeq.end(),
                std::back_inserter(avRet),
                [&aSecondSeq](const lang::Locale& rFirstLocale) {
                    return std::none_of(
                        aSecondSeq.begin(), aSecondSeq.end(),
                        [&rFirstLocale](const lang::Locale& rSecondLocale) {
                            return localesAreEqual(rFirstLocale, rSecondLocale);
                        });
                });
    return avRet;
}

namespace {

class NameClashQueryBox
{
private:
    std::unique_ptr<weld::MessageDialog> m_xQueryBox;
public:
    NameClashQueryBox(weld::Window* pParent, const OUString& rTitle, const OUString& rMessage)
        : m_xQueryBox(Application::CreateMessageDialog(pParent, VclMessageType::Question, VclButtonsType::NONE, rMessage))
    {
        if (!rTitle.isEmpty())
            m_xQueryBox->set_title(rTitle);
        m_xQueryBox->add_button(IDEResId(RID_STR_DLGIMP_CLASH_RENAME), RET_YES);
        m_xQueryBox->add_button(IDEResId(RID_STR_DLGIMP_CLASH_REPLACE), RET_NO);
        m_xQueryBox->add_button(GetStandardText(StandardButtonType::Cancel), RET_CANCEL);
        m_xQueryBox->set_default_response(RET_YES);
    }
    short run() { return m_xQueryBox->run(); }
};

class LanguageMismatchQueryBox
{
private:
    std::unique_ptr<weld::MessageDialog> m_xQueryBox;
public:
    LanguageMismatchQueryBox(weld::Window* pParent, const OUString& rTitle, const OUString& rMessage)
        : m_xQueryBox(Application::CreateMessageDialog(pParent, VclMessageType::Question, VclButtonsType::NONE, rMessage))
    {
        if (!rTitle.isEmpty())
            m_xQueryBox->set_title(rTitle);
        m_xQueryBox->add_button(IDEResId(RID_STR_DLGIMP_MISMATCH_ADD), RET_YES);
        m_xQueryBox->add_button(IDEResId(RID_STR_DLGIMP_MISMATCH_OMIT), RET_NO);
        m_xQueryBox->add_button(GetStandardText(StandardButtonType::Cancel), RET_CANCEL);
        m_xQueryBox->add_button(GetStandardText(StandardButtonType::Help), RET_HELP);
        m_xQueryBox->set_default_response(RET_YES);
    }
    short run() { return m_xQueryBox->run(); }
};

}

bool implImportDialog(weld::Window* pWin, const ScriptDocument& rDocument, const OUString& aLibName)
{
    bool bDone = false;

    const Reference<uno::XComponentContext>& xContext(::comphelper::getProcessComponentContext());
    sfx2::FileDialogHelper aDlg(ui::dialogs::TemplateDescription::FILEOPEN_SIMPLE,
                                FileDialogFlags::NONE, pWin);
    aDlg.SetContext(sfx2::FileDialogHelper::BasicImportDialog);
    Reference<XFilePicker3> xFP = aDlg.GetFilePicker();

    OUString aDialogStr(IDEResId(RID_STR_STDDIALOGNAME));
    xFP->appendFilter( aDialogStr, u"*.xdl"_ustr );
    xFP->appendFilter( IDEResId(RID_STR_FILTER_ALLFILES), FilterMask_All );
    xFP->setCurrentFilter( aDialogStr );

    if( aDlg.Execute() == ERRCODE_NONE )
    {
        Sequence< OUString > aPaths = xFP->getSelectedFiles();

        OUString aBasePath;
        const OUString& rOUCurPath( aPaths[0] );
        sal_Int32 iSlash = rOUCurPath.lastIndexOf( '/' );
        if( iSlash != -1 )
            aBasePath = rOUCurPath.copy( 0, iSlash + 1 );

        try
        {
            // create dialog model
            Reference< container::XNameContainer > xDialogModel(
                xContext->getServiceManager()->createInstanceWithContext(u"com.sun.star.awt.UnoControlDialogModel"_ustr, xContext),
                UNO_QUERY_THROW );

            Reference< XSimpleFileAccess3 > xSFI( SimpleFileAccess::create(xContext) );

            Reference< XInputStream > xInput;
            if( xSFI->exists( rOUCurPath ) )
                xInput = xSFI->openFileRead( rOUCurPath );

            ::xmlscript::importDialogModel( xInput, xDialogModel, xContext, rDocument.isDocument() ? rDocument.getDocument() : Reference< frame::XModel >() );

            OUString aXmlDlgName;
            Reference< beans::XPropertySet > xDialogModelPropSet( xDialogModel, UNO_QUERY );
            assert(xDialogModelPropSet.is());
            try
            {
                Any aXmlDialogNameAny = xDialogModelPropSet->getPropertyValue( DLGED_PROP_NAME );
                aXmlDialogNameAny >>= aXmlDlgName;
            }
            catch(const beans::UnknownPropertyException& )
            {
                TOOLS_WARN_EXCEPTION("basctl", "");
            }
            assert( !aXmlDlgName.isEmpty() );

            bool bDialogAlreadyExists = rDocument.hasDialog( aLibName, aXmlDlgName );

            OUString aNewDlgName = aXmlDlgName;
            enum NameClashMode
            {
                NO_CLASH,
                CLASH_OVERWRITE_DIALOG,
                CLASH_RENAME_DIALOG,
            };
            NameClashMode eNameClashMode = NO_CLASH;
            if( bDialogAlreadyExists )
            {
                OUString aQueryBoxTitle(IDEResId(RID_STR_DLGIMP_CLASH_TITLE));
                OUString aQueryBoxText(IDEResId(RID_STR_DLGIMP_CLASH_TEXT));
                aQueryBoxText = aQueryBoxText.replaceAll("$(ARG1)", aXmlDlgName);

                NameClashQueryBox aQueryBox(pWin, aQueryBoxTitle, aQueryBoxText);
                sal_uInt16 nRet = aQueryBox.run();
                if( nRet == RET_YES )
                {
                    // RET_YES == Rename, see NameClashQueryBox::NameClashQueryBox
                    eNameClashMode = CLASH_RENAME_DIALOG;

                    aNewDlgName = rDocument.createObjectName( E_DIALOGS, aLibName );
                }
                else if( nRet == RET_NO )
                {
                    // RET_NO == Replace, see NameClashQueryBox::NameClashQueryBox
                    eNameClashMode = CLASH_OVERWRITE_DIALOG;
                }
                else if( nRet == RET_CANCEL )
                {
                    return bDone;
                }
            }

            Shell* pShell = GetShell();
            assert(pShell);

            // Resource?
            css::lang::Locale aLocale = Application::GetSettings().GetUILanguageTag().getLocale();
            Reference< task::XInteractionHandler > xDummyHandler;
            Reference< XStringResourceWithLocation > xImportStringResource =
                StringResourceWithLocation::create( xContext, aBasePath, true/*bReadOnly*/,
                aLocale, aXmlDlgName, OUString(), xDummyHandler );

            Sequence< lang::Locale > aImportLocaleSeq = xImportStringResource->getLocales();
            sal_Int32 nImportLocaleCount = aImportLocaleSeq.getLength();

            Reference< container::XNameContainer > xDialogLib( rDocument.getLibrary( E_DIALOGS, aLibName, true ) );
            Reference< resource::XStringResourceManager > xLibStringResourceManager = LocalizationMgr::getStringResourceFromDialogLibrary( xDialogLib );
            sal_Int32 nLibLocaleCount = 0;
            Sequence< lang::Locale > aLibLocaleSeq;
            if( xLibStringResourceManager.is() )
            {
                aLibLocaleSeq = xLibStringResourceManager->getLocales();
                nLibLocaleCount = aLibLocaleSeq.getLength();
            }

            // Check language matches
            std::vector< lang::Locale > aOnlyInImportLanguages =
                implGetLanguagesOnlyContainedInFirstSeq( aImportLocaleSeq, aLibLocaleSeq );
            int nOnlyInImportLanguageCount = aOnlyInImportLanguages.size();

            // For now: Keep languages from lib
            bool bLibLocalized = (nLibLocaleCount > 0);
            bool bImportLocalized = (nImportLocaleCount > 0);

            bool bAddDialogLanguagesToLib = false;
            if( nOnlyInImportLanguageCount > 0 )
            {
                OUString aQueryBoxTitle(IDEResId(RID_STR_DLGIMP_MISMATCH_TITLE));
                OUString aQueryBoxText(IDEResId(RID_STR_DLGIMP_MISMATCH_TEXT));
                LanguageMismatchQueryBox aQueryBox(pWin, aQueryBoxTitle, aQueryBoxText);
                sal_uInt16 nRet = aQueryBox.run();
                if( nRet == RET_YES )
                {
                    // RET_YES == Add, see LanguageMismatchQueryBox::LanguageMismatchQueryBox
                    bAddDialogLanguagesToLib = true;
                }
                // RET_NO == Omit, see LanguageMismatchQueryBox::LanguageMismatchQueryBox
                // -> nothing to do here
                //else if( RET_NO == nRet )
                //{
                //}
                else if( nRet == RET_CANCEL )
                {
                    return bDone;
                }
            }

            if( bImportLocalized )
            {
                bool bCopyResourcesForDialog = true;
                if( bAddDialogLanguagesToLib )
                {
                    const std::shared_ptr<LocalizationMgr>& pCurMgr = pShell->GetCurLocalizationMgr();

                    lang::Locale aFirstLocale = aOnlyInImportLanguages[0];
                    if( nOnlyInImportLanguageCount > 1 )
                    {
                        // Check if import default belongs to only import languages and use it then
                        lang::Locale aImportDefaultLocale = xImportStringResource->getDefaultLocale();

                        if (std::any_of(aOnlyInImportLanguages.begin(), aOnlyInImportLanguages.end(),
                                        [&aImportDefaultLocale](const lang::Locale& aTmpLocale) {
                                            return localesAreEqual(aImportDefaultLocale, aTmpLocale);
                                        }))
                        {
                            aFirstLocale = std::move(aImportDefaultLocale);
                        }
                    }

                    pCurMgr->handleAddLocales( {aFirstLocale} );

                    if( nOnlyInImportLanguageCount > 1 )
                    {
                        Sequence< lang::Locale > aRemainingLocaleSeq( nOnlyInImportLanguageCount - 1 );
                        auto pRemainingLocaleSeq = aRemainingLocaleSeq.getArray();
                        int iSeq = 0;
                        for( const lang::Locale& rLocale : aOnlyInImportLanguages )
                        {
                            if( !localesAreEqual( aFirstLocale, rLocale ) )
                                pRemainingLocaleSeq[iSeq++] = rLocale;
                        }
                        pCurMgr->handleAddLocales( aRemainingLocaleSeq );
                    }
                }
                else if( !bLibLocalized )
                {
                    LocalizationMgr::resetResourceForDialog( xDialogModel, xImportStringResource );
                    bCopyResourcesForDialog = false;
                }

                if( bCopyResourcesForDialog )
                {
                    LocalizationMgr::copyResourceForDroppedDialog( xDialogModel, aXmlDlgName,
                        xLibStringResourceManager, xImportStringResource );
                }
            }
            else if( bLibLocalized )
            {
                LocalizationMgr::setResourceIDsForDialog( xDialogModel, xLibStringResourceManager );
            }


            LocalizationMgr::setStringResourceAtDialog( rDocument, aLibName, aNewDlgName, xDialogModel );

            if( eNameClashMode == CLASH_OVERWRITE_DIALOG )
            {
                if (basctl::RemoveDialog( rDocument, aLibName, aNewDlgName ) )
                {
                    BaseWindow* pDlgWin = pShell->FindDlgWin( rDocument, aLibName, aNewDlgName, false, true );
                    if( pDlgWin != nullptr )
                        pShell->RemoveWindow( pDlgWin, false );
                    MarkDocumentModified( rDocument );
                }
                else
                {
                    // TODO: Assertion?
                    return bDone;
                }
            }

            if( eNameClashMode == CLASH_RENAME_DIALOG )
            {
                bool bRenamed = false;
                if( xDialogModelPropSet.is() )
                {
                    try
                    {
                        xDialogModelPropSet->setPropertyValue( DLGED_PROP_NAME, Any(aNewDlgName) );
                        bRenamed = true;
                    }
                    catch(const beans::UnknownPropertyException& )
                    {}
                }


                if( bRenamed )
                {
                    LocalizationMgr::renameStringResourceIDs( rDocument, aLibName, aNewDlgName, xDialogModel );
                }
                else
                {
                    // TODO: Assertion?
                    return bDone;
                }
            }

            Reference< XInputStreamProvider > xISP = ::xmlscript::exportDialogModel( xDialogModel, xContext, rDocument.isDocument() ? rDocument.getDocument() : Reference< frame::XModel >() );
            bool bSuccess = rDocument.insertDialog( aLibName, aNewDlgName, xISP );
            if( bSuccess )
            {
                VclPtr<DialogWindow> pNewDlgWin = pShell->CreateDlgWin( rDocument, aLibName, aNewDlgName );
                pShell->SetCurWindow( pNewDlgWin, true );
            }

            bDone = true;
        }
        catch(const Exception& )
        {}
    }

    return bDone;
}

void DialogWindow::ImportDialog()
{
    const ScriptDocument& rDocument = GetDocument();
    OUString aLibName = GetLibName();
    implImportDialog(GetFrameWeld(), rDocument, aLibName);
}

DlgEdModel& DialogWindow::GetModel() const
{
    return m_pEditor->GetModel();
}

DlgEdPage& DialogWindow::GetPage() const
{
    return m_pEditor->GetPage();
}

DlgEdView& DialogWindow::GetView() const
{
    return m_pEditor->GetView();
}

bool DialogWindow::IsModified()
{
    return m_pEditor->IsModified();
}

SfxUndoManager* DialogWindow::GetUndoManager()
{
    return m_pUndoMgr.get();
}

OUString DialogWindow::GetTitle()
{
    return GetName();
}

EntryDescriptor DialogWindow::CreateEntryDescriptor()
{
    ScriptDocument aDocument( GetDocument() );
    OUString aLibName( GetLibName() );
    LibraryLocation eLocation = aDocument.getLibraryLocation( aLibName );
    return EntryDescriptor( std::move(aDocument), eLocation, aLibName, OUString(), GetName(), OBJ_TYPE_DIALOG );
}

void DialogWindow::SetReadOnly (bool bReadOnly)
{
    m_pEditor->SetMode(bReadOnly ? DlgEditor::READONLY : DlgEditor::SELECT);
}

bool DialogWindow::IsReadOnly ()
{
    return m_pEditor->GetMode() == DlgEditor::READONLY;
}

bool DialogWindow::IsPasteAllowed()
{
    return m_pEditor->IsPasteAllowed();
}

void DialogWindow::StoreData()
{
    if ( !IsModified() )
        return;

    try
    {
        Reference< container::XNameContainer > xLib = GetDocument().getLibrary( E_DIALOGS, GetLibName(), true );

        if( xLib.is() )
        {
            Reference< container::XNameContainer > xDialogModel = m_pEditor->GetDialog();

            if( xDialogModel.is() )
            {
                const Reference< XComponentContext >& xContext(
                    comphelper::getProcessComponentContext() );
                Reference< XInputStreamProvider > xISP = ::xmlscript::exportDialogModel( xDialogModel, xContext, GetDocument().isDocument() ? GetDocument().getDocument() : Reference< frame::XModel >() );
                xLib->replaceByName( GetName(), Any( xISP ) );
            }
        }
    }
    catch (const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION("basctl.basicide");
    }
    MarkDocumentModified( GetDocument() );
    m_pEditor->ClearModifyFlag();
}

void DialogWindow::Activating ()
{
    UpdateBrowser();
    Show();
}

void DialogWindow::Deactivating()
{
    Hide();
    if ( IsModified() )
        MarkDocumentModified( GetDocument() );
    DisableBrowser();
}

sal_Int32 DialogWindow::countPages( Printer* )
{
    return 1;
}

void DialogWindow::printPage( sal_Int32 nPage, Printer* pPrinter )
{
    DlgEditor::printPage( nPage, pPrinter, CreateQualifiedName() );
}

void DialogWindow::DataChanged( const DataChangedEvent& rDCEvt )
{
    if( (rDCEvt.GetType()==DataChangedEventType::SETTINGS) && (rDCEvt.GetFlags() & AllSettingsFlags::STYLE) )
    {
        InitSettings();
        Invalidate();
    }
    else
        BaseWindow::DataChanged( rDCEvt );
}

void DialogWindow::InitSettings()
{
    // FIXME RenderContext
    const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
    vcl::Font aFont = rStyleSettings.GetFieldFont();
    SetPointFont(*GetOutDev(), aFont);

    SetTextColor( rStyleSettings.GetFieldTextColor() );
    SetTextFillColor();

    SetBackground(rStyleSettings.GetFaceColor());
}

rtl::Reference<comphelper::OAccessible> DialogWindow::CreateAccessible()
{
    return new AccessibleDialogWindow(this);
}

OUString DialogWindow::GetHid () const
{
    return HID_BASICIDE_DIALOGWINDOW;
}

SbxItemType DialogWindow::GetSbxType () const
{
    return SBX_TYPE_DIALOG;
}


// DialogWindowLayout


DialogWindowLayout::DialogWindowLayout (vcl::Window* pParent, ObjectCatalog& rObjectCatalog_) :
    Layout(pParent),
    rObjectCatalog(rObjectCatalog_),
    pPropertyBrowser(nullptr)
{
    ShowPropertyBrowser();
}

DialogWindowLayout::~DialogWindowLayout()
{
    disposeOnce();
}

void DialogWindowLayout::dispose()
{
    if (pPropertyBrowser)
        Remove(pPropertyBrowser);
    pPropertyBrowser.disposeAndClear();
    Layout::dispose();
}

// shows the property browser (and creates if necessary)
void DialogWindowLayout::ShowPropertyBrowser ()
{
    // not exists?
    if (!pPropertyBrowser)
    {
        // creating
        pPropertyBrowser = VclPtr<PropBrw>::Create(*this);
        pPropertyBrowser->Show();
        // after OnFirstSize():
        if (HasSize())
            AddPropertyBrowser();
        // updating if necessary
        UpdatePropertyBrowser();
    }
    else
        pPropertyBrowser->Show();
    // refreshing the button state
    if (SfxBindings* pBindings = GetBindingsPtr())
        pBindings->Invalidate(SID_SHOW_PROPERTYBROWSER);
}

// disables the property browser
void DialogWindowLayout::DisablePropertyBrowser ()
{
    if (pPropertyBrowser)
        pPropertyBrowser->Update(nullptr);
}

// updates the property browser
void DialogWindowLayout::UpdatePropertyBrowser ()
{
    if (pPropertyBrowser)
        pPropertyBrowser->Update(GetShell());
}

void DialogWindowLayout::Activating (BaseWindow& rChild)
{
    assert(dynamic_cast<DialogWindow*>(&rChild));
    rObjectCatalog.SetLayoutWindow(this);
    rObjectCatalog.UpdateEntries();
    rObjectCatalog.Show();
    if (pPropertyBrowser)
        pPropertyBrowser->Show();
    Layout::Activating(rChild);
}

void DialogWindowLayout::Deactivating ()
{
    Layout::Deactivating();
    rObjectCatalog.Hide();
    if (pPropertyBrowser)
        pPropertyBrowser->Hide();
}

void DialogWindowLayout::ExecuteGlobal (SfxRequest& rReq)
{
    switch (rReq.GetSlot())
    {
        case SID_SHOW_PROPERTYBROWSER:
            // toggling property browser
            if (pPropertyBrowser && pPropertyBrowser->IsVisible())
                pPropertyBrowser->Hide();
            else
                ShowPropertyBrowser();
            ArrangeWindows();
            // refreshing the button state
            if (SfxBindings* pBindings = GetBindingsPtr())
                pBindings->Invalidate(SID_SHOW_PROPERTYBROWSER);
            break;
    }
}

void DialogWindowLayout::GetState (SfxItemSet& rSet, unsigned nWhich)
{
    switch (nWhich)
    {
        case SID_SHOW_PROPERTYBROWSER:
            rSet.Put(SfxBoolItem(nWhich, pPropertyBrowser && pPropertyBrowser->IsVisible()));
            break;

        case SID_BASICIDE_CHOOSEMACRO:
            rSet.Put(SfxVisibilityItem(nWhich, false));
            break;
    }
}

void DialogWindowLayout::OnFirstSize (tools::Long const nWidth, tools::Long const nHeight)
{
    AddToLeft(&rObjectCatalog, Size(nWidth * 0.25, nHeight * 0.35));
    if (pPropertyBrowser)
        AddPropertyBrowser();
}

void DialogWindowLayout::AddPropertyBrowser () {
    Size const aSize = GetOutputSizePixel();
    AddToLeft(pPropertyBrowser, Size(aSize.Width() * 0.25, aSize.Height() * 0.65));
}


} // namespace basctl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
