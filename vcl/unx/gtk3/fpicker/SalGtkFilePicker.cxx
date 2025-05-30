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

#include <config_gio.h>
#include <config_gpgme.h>

#include <com/sun/star/awt/SystemDependentXWindow.hpp>
#include <com/sun/star/awt/Toolkit.hpp>
#include <com/sun/star/awt/XSystemDependentWindowPeer.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/SystemDependent.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <com/sun/star/ui/dialogs/CommonFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include <osl/diagnose.h>
#include <rtl/process.h>
#include <sal/log.hxx>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/ControlActions.hpp>
#include <com/sun/star/uno/Any.hxx>
#include <unx/gtk/gtkdata.hxx>
#include <unx/gtk/gtkinst.hxx>

#include <utility>
#include <vcl/svapp.hxx>

#include <o3tl/string_view.hxx>
#include <tools/urlobj.hxx>
#include <unotools/ucbhelper.hxx>

#include <algorithm>
#include <set>
#include <string.h>
#include <string_view>

#include "SalGtkFilePicker.hxx"

using namespace ::com::sun::star;
using namespace ::com::sun::star::ui::dialogs;
using namespace ::com::sun::star::ui::dialogs::TemplateDescription;
using namespace ::com::sun::star::ui::dialogs::ExtendedFilePickerElementIds;
using namespace ::com::sun::star::ui::dialogs::CommonFilePickerElementIds;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::uno;

struct FilterEntry
{
protected:
    OUString     m_sTitle;
    OUString     m_sFilter;

    css::uno::Sequence< css::beans::StringPair >       m_aSubFilters;

public:
    FilterEntry( OUString _aTitle, OUString _aFilter )
        :m_sTitle(std::move( _aTitle ))
        ,m_sFilter(std::move( _aFilter ))
    {
    }

    const OUString& getTitle() const { return m_sTitle; }
    const OUString& getFilter() const { return m_sFilter; }

    /// determines if the filter has sub filter (i.e., the filter is a filter group in real)
    bool        hasSubFilters( ) const;

    /** retrieves the filters belonging to the entry
    */
    void       getSubFilters( css::uno::Sequence< css::beans::StringPair >& _rSubFilterList );

    // helpers for iterating the sub filters
    const css::beans::StringPair*   beginSubFilters() const { return m_aSubFilters.begin(); }
    const css::beans::StringPair*   endSubFilters() const { return m_aSubFilters.end(); }
};

bool FilterEntry::hasSubFilters() const
{
    return m_aSubFilters.hasElements();
}

void FilterEntry::getSubFilters( css::uno::Sequence< css::beans::StringPair >& _rSubFilterList )
{
    _rSubFilterList = m_aSubFilters;
}

void SalGtkFilePicker::dialog_mapped_cb(GtkWidget *, SalGtkFilePicker *pobjFP)
{
    pobjFP->InitialMapping();
}

void SalGtkFilePicker::InitialMapping()
{
    if (!mbPreviewState )
    {
        gtk_widget_set_visible(m_pPreview, false);
#if !GTK_CHECK_VERSION(4, 0, 0)
        gtk_file_chooser_set_preview_widget_active( GTK_FILE_CHOOSER( m_pDialog ), false);
#endif
    }
    gtk_widget_set_size_request (m_pPreview, -1, -1);
}

SalGtkFilePicker::SalGtkFilePicker( const uno::Reference< uno::XComponentContext >& xContext ) :
    SalGtkPicker( xContext ),
    SalGtkFilePicker_Base( m_rbHelperMtx ),
    m_pVBox ( nullptr ),
    mnHID_FolderChange( 0 ),
    mnHID_SelectionChange( 0 ),
    bVersionWidthUnset( false ),
    mbPreviewState( false ),
    mbInitialized(false),
    mHID_Preview( 0 ),
    m_pPreview( nullptr ),
    m_pPseudoFilter( nullptr )
{
    int i;

    for( i = 0; i < TOGGLE_LAST; i++ )
    {
        m_pToggles[i] = nullptr;
        mbToggleVisibility[i] = false;
    }

    for( i = 0; i < BUTTON_LAST; i++ )
    {
        m_pButtons[i] = nullptr;
        mbButtonVisibility[i] = false;
    }

    for( i = 0; i < LIST_LAST; i++ )
    {
        m_pHBoxs[i] = nullptr;
        m_pLists[i] = nullptr;
        m_pListLabels[i] = nullptr;
        mbListVisibility[i] = false;
    }

    OUString aFilePickerTitle = getResString( FILE_PICKER_TITLE_OPEN );

    m_pDialog = GTK_WIDGET(g_object_new(GTK_TYPE_FILE_CHOOSER_DIALOG,
                                        "title", OUStringToOString(aFilePickerTitle, RTL_TEXTENCODING_UTF8).getStr(),
                                        "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                                        nullptr));
    gtk_window_set_modal(GTK_WINDOW(m_pDialog), true);
    gtk_dialog_set_default_response( GTK_DIALOG (m_pDialog), GTK_RESPONSE_ACCEPT );

#if !GTK_CHECK_VERSION(4, 0, 0)
#if ENABLE_GIO
    gtk_file_chooser_set_local_only( GTK_FILE_CHOOSER( m_pDialog ), false );
#endif
#endif

    gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER( m_pDialog ), false );

    m_pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // We don't want clickable items to have a huge hit-area
    GtkWidget *pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *pThinVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_box_pack_end (GTK_BOX( m_pVBox ), pHBox, false, false, 0);
    gtk_box_pack_start (GTK_BOX( pHBox ), pThinVBox, false, false, 0);
#else
    gtk_box_append(GTK_BOX(m_pVBox), pHBox);
    gtk_box_prepend(GTK_BOX(m_pVBox), pThinVBox);
#endif
    gtk_widget_set_visible(pHBox, true);
    gtk_widget_set_visible(pThinVBox, true);

    OUString aLabel;

    for( i = 0; i < TOGGLE_LAST; i++ )
    {
        m_pToggles[i] = gtk_check_button_new();
        gtk_widget_set_visible(m_pToggles[i], false);

#define LABEL_TOGGLE( elem ) \
        case elem : \
            aLabel = getResString( CHECKBOX_##elem ); \
            setLabel( CHECKBOX_##elem, aLabel ); \
            break

        switch( i ) {
        LABEL_TOGGLE( AUTOEXTENSION );
        LABEL_TOGGLE( PASSWORD );
        LABEL_TOGGLE( GPGENCRYPTION );
        LABEL_TOGGLE( GPGSIGN );
        LABEL_TOGGLE( FILTEROPTIONS );
        LABEL_TOGGLE( READONLY );
        LABEL_TOGGLE( LINK );
        LABEL_TOGGLE( PREVIEW );
        LABEL_TOGGLE( SELECTION );
        default:
            SAL_WARN( "vcl.gtk", "Handle unknown control " << i);
            break;
        }

#if !GTK_CHECK_VERSION(4, 0, 0)
        gtk_box_pack_end( GTK_BOX( pThinVBox ), m_pToggles[i], false, false, 0 );
#else
        gtk_box_append(GTK_BOX(pThinVBox), m_pToggles[i]);
#endif
    }

    for( i = 0; i < LIST_LAST; i++ )
    {
        m_pHBoxs[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_visible(m_pHBoxs[i], false);

        GtkListStore *pListStores[ LIST_LAST ];
        pListStores[i] = gtk_list_store_new (1, G_TYPE_STRING);
        m_pLists[i] = gtk_combo_box_new_with_model(GTK_TREE_MODEL(pListStores[i]));
        gtk_widget_set_visible(m_pLists[i], false);
        g_object_unref (pListStores[i]); // owned by the widget.
        GtkCellRenderer *pCell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start(
                GTK_CELL_LAYOUT(m_pLists[i]), pCell, true);
        gtk_cell_layout_set_attributes(
            GTK_CELL_LAYOUT (m_pLists[i]), pCell, "text", 0, nullptr);

        m_pListLabels[i] = gtk_label_new( "" );
        gtk_widget_set_visible(m_pListLabels[i], false);

#define LABEL_LIST( elem ) \
        case elem : \
            aLabel = getResString( LISTBOX_##elem##_LABEL ); \
            setLabel( LISTBOX_##elem##_LABEL, aLabel ); \
            break

        switch( i )
        {
            LABEL_LIST( VERSION );
            LABEL_LIST( TEMPLATE );
            LABEL_LIST( IMAGE_TEMPLATE );
            LABEL_LIST( IMAGE_ANCHOR );
            default:
                SAL_WARN( "vcl.gtk", "Handle unknown control " << i);
                break;
        }

#if !GTK_CHECK_VERSION(4, 0, 0)
        gtk_box_pack_end( GTK_BOX( m_pHBoxs[i] ), m_pLists[i], false, false, 0 );
        gtk_box_pack_end( GTK_BOX( m_pHBoxs[i] ), m_pListLabels[i], false, false, 0 );
#else
        gtk_box_append(GTK_BOX(m_pHBoxs[i]), m_pListLabels[i]);
        gtk_box_append(GTK_BOX(m_pHBoxs[i]), m_pLists[i]);
#endif
        gtk_label_set_mnemonic_widget( GTK_LABEL(m_pListLabels[i]), m_pLists[i] );
        gtk_box_set_spacing( GTK_BOX( m_pHBoxs[i] ), 12 );

#if !GTK_CHECK_VERSION(4, 0, 0)
        gtk_box_pack_end( GTK_BOX( m_pVBox ), m_pHBoxs[i], false, false, 0 );
#else
        gtk_box_append(GTK_BOX(m_pVBox), m_pHBoxs[i]);
#endif
    }

    aLabel = getResString( FILE_PICKER_FILE_TYPE );
    m_pFilterExpander = gtk_expander_new_with_mnemonic(
        OUStringToOString( aLabel, RTL_TEXTENCODING_UTF8 ).getStr());

#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_box_pack_end( GTK_BOX( m_pVBox ), m_pFilterExpander, false, true, 0 );
#else
    gtk_box_append(GTK_BOX(m_pVBox), m_pFilterExpander);
#endif

#if !GTK_CHECK_VERSION(4, 0, 0)
    GtkWidget *scrolled_window = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
        GTK_SHADOW_IN);
#else
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scrolled_window), true);
#endif
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_container_add (GTK_CONTAINER (m_pFilterExpander), scrolled_window);
#else
    gtk_expander_set_child(GTK_EXPANDER(m_pFilterExpander), scrolled_window);
#endif
    gtk_widget_set_visible(scrolled_window, true);

    m_pFilterStore = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING);
    m_pFilterView = gtk_tree_view_new_with_model (GTK_TREE_MODEL(m_pFilterStore));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(m_pFilterView), false);

    GtkCellRenderer *cell = nullptr;

    for (i = 0; i < 2; ++i)
    {
        GtkTreeViewColumn *column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_set_expand (column, true);
        gtk_tree_view_column_pack_start (column, cell, false);
        gtk_tree_view_column_set_attributes (column, cell, "text", i, nullptr);
        gtk_tree_view_append_column (GTK_TREE_VIEW(m_pFilterView), column);
    }

#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_container_add (GTK_CONTAINER (scrolled_window), m_pFilterView);
#else
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), m_pFilterView);
#endif
    gtk_widget_set_visible(m_pFilterView, true);

#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER( m_pDialog ), m_pVBox );
#else
    GtkBox* pContentBox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(m_pDialog)));
    gtk_box_append(pContentBox, m_pVBox);
#endif

    m_pPreview = gtk_image_new();
#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_file_chooser_set_preview_widget( GTK_FILE_CHOOSER( m_pDialog ), m_pPreview );
#endif

    g_signal_connect( G_OBJECT( m_pToggles[PREVIEW] ), "toggled",
                      G_CALLBACK( preview_toggled_cb ), this );
    g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW(m_pFilterView)), "changed",
                      G_CALLBACK ( type_changed_cb ), this);
    g_signal_connect( G_OBJECT( m_pDialog ), "notify::filter",
                      G_CALLBACK( filter_changed_cb ), this);
    g_signal_connect( G_OBJECT( m_pFilterExpander ), "activate",
                      G_CALLBACK( expander_changed_cb ), this);
    g_signal_connect (G_OBJECT( m_pDialog ), "map",
                      G_CALLBACK (dialog_mapped_cb), this);

    gtk_widget_set_visible(m_pVBox, true);

    PangoLayout  *layout = gtk_widget_create_pango_layout (m_pFilterView, nullptr);
    guint ypad;
    PangoRectangle row_height;
    pango_layout_set_markup (layout, "All Files", -1);
    pango_layout_get_pixel_extents (layout, nullptr, &row_height);
    g_object_unref (layout);

    g_object_get (cell, "ypad", &ypad, nullptr);
    guint height = (row_height.height + 2*ypad) * 5;
    gtk_widget_set_size_request (m_pFilterView, -1, height);
    gtk_widget_set_size_request (m_pPreview, 1, height);

#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_file_chooser_set_preview_widget_active( GTK_FILE_CHOOSER( m_pDialog ), true);
#endif
}

// XFilePickerNotifier

void SAL_CALL SalGtkFilePicker::addFilePickerListener( const uno::Reference<XFilePickerListener>& xListener )
{
    SolarMutexGuard g;

    OSL_ENSURE(!m_xListener.is(),
            "SalGtkFilePicker only talks with one listener at a time...");
    m_xListener = xListener;
}

void SAL_CALL SalGtkFilePicker::removeFilePickerListener( const uno::Reference<XFilePickerListener>& )
{
    SolarMutexGuard g;

    m_xListener.clear();
}

// FilePicker Event functions

void SalGtkFilePicker::impl_fileSelectionChanged( const FilePickerEvent& aEvent )
{
    if (m_xListener.is()) m_xListener->fileSelectionChanged( aEvent );
}

void SalGtkFilePicker::impl_directoryChanged( const FilePickerEvent& aEvent )
{
    if (m_xListener.is()) m_xListener->directoryChanged( aEvent );
}

void SalGtkFilePicker::impl_controlStateChanged( const FilePickerEvent& aEvent )
{
    if (m_xListener.is()) m_xListener->controlStateChanged( aEvent );
}

static bool
isFilterString( std::u16string_view rFilterString, const char *pMatch )
{
        sal_Int32 nIndex = 0;
        bool bIsFilter = true;

        OUString aMatch(OUString::createFromAscii(pMatch));

        do
        {
            std::u16string_view aToken = o3tl::getToken(rFilterString, 0, ';', nIndex );
            if( !o3tl::starts_with(aToken, aMatch) )
            {
                bIsFilter = false;
                break;
            }
        }
        while( nIndex >= 0 );

        return bIsFilter;
}

static OUString
shrinkFilterName( const OUString &rFilterName, bool bAllowNoStar = false )
{
    int i;
    int nBracketLen = -1;
    int nBracketEnd = -1;
    const sal_Unicode *pStr = rFilterName.getStr();
    OUString aRealName = rFilterName;

    for( i = aRealName.getLength() - 1; i > 0; i-- )
    {
        if( pStr[i] == ')' )
            nBracketEnd = i;
        else if( pStr[i] == '(' )
        {
            nBracketLen = nBracketEnd - i;
            if( nBracketEnd <= 0 )
                continue;
            if( isFilterString( rFilterName.subView( i + 1, nBracketLen - 1 ), "*." ) )
                aRealName = aRealName.replaceAt( i, nBracketLen + 1, u"" );
            else if (bAllowNoStar)
            {
                if( isFilterString( rFilterName.subView( i + 1, nBracketLen - 1 ), ".") )
                    aRealName = aRealName.replaceAt( i, nBracketLen + 1, u"" );
            }
        }
    }

    return aRealName;
}

namespace {

    struct FilterTitleMatch
    {
    protected:
        const OUString& rTitle;

    public:
        explicit FilterTitleMatch( const OUString& _rTitle ) : rTitle( _rTitle ) { }

        bool operator () ( const FilterEntry& _rEntry )
        {
            bool bMatch;
            if( !_rEntry.hasSubFilters() )
                // a real filter
                bMatch = (_rEntry.getTitle() == rTitle)
                      || (shrinkFilterName(_rEntry.getTitle()) == rTitle);
            else
                // a filter group -> search the sub filters
                bMatch =
                    ::std::any_of(
                        _rEntry.beginSubFilters(),
                        _rEntry.endSubFilters(),
                        *this
                    );

            return bMatch;
        }
        bool operator () ( const css::beans::StringPair& _rEntry )
        {
            OUString aShrunkName = shrinkFilterName( _rEntry.First );
            return aShrunkName == rTitle;
        }
    };
}

bool SalGtkFilePicker::FilterNameExists( const OUString& rTitle )
{
    bool bRet = false;

    if( m_pFilterVector )
        bRet =
            ::std::any_of(
                m_pFilterVector->begin(),
                m_pFilterVector->end(),
                FilterTitleMatch( rTitle )
            );

    return bRet;
}

bool SalGtkFilePicker::FilterNameExists( const css::uno::Sequence< css::beans::StringPair >& _rGroupedFilters )
{
    bool bRet = false;

    if( m_pFilterVector )
    {
        bRet = std::any_of(_rGroupedFilters.begin(), _rGroupedFilters.end(),
            [&](const css::beans::StringPair& rFilter) {
                return ::std::any_of( m_pFilterVector->begin(), m_pFilterVector->end(), FilterTitleMatch( rFilter.First ) ); });
    }

    return bRet;
}

void SalGtkFilePicker::ensureFilterVector( const OUString& _rInitialCurrentFilter )
{
    if( !m_pFilterVector )
    {
        m_pFilterVector.reset( new std::vector<FilterEntry> );

        // set the first filter to the current filter
        if ( m_aCurrentFilter.isEmpty() )
            m_aCurrentFilter = _rInitialCurrentFilter;
    }
}

void SAL_CALL SalGtkFilePicker::appendFilter( const OUString& aTitle, const OUString& aFilter )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    if( FilterNameExists( aTitle ) )
            throw IllegalArgumentException();

    // ensure that we have a filter list
    ensureFilterVector( aTitle );

    // append the filter
    m_pFilterVector->insert( m_pFilterVector->end(), FilterEntry( aTitle, aFilter ) );
}

void SAL_CALL SalGtkFilePicker::setCurrentFilter( const OUString& aTitle )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    if( aTitle != m_aCurrentFilter )
    {
        m_aCurrentFilter = aTitle;
        SetCurFilter( m_aCurrentFilter );
    }

    // TODO m_pImpl->setCurrentFilter( aTitle );
}

void SalGtkFilePicker::updateCurrentFilterFromName(const gchar* filtername)
{
    OUString aFilterName(filtername, strlen(filtername), RTL_TEXTENCODING_UTF8);
    if (m_pFilterVector)
    {
        for (auto const& filter : *m_pFilterVector)
        {
            if (aFilterName == shrinkFilterName(filter.getTitle()))
            {
                m_aCurrentFilter = filter.getTitle();
                break;
            }
        }
    }
}

void SalGtkFilePicker::UpdateFilterfromUI()
{
    // Update the filtername from the users selection if they have had a chance to do so.
    // If the user explicitly sets a type then use that, if not then take the implicit type
    // from the filter of the files glob on which he is currently searching
    if (!mnHID_FolderChange || !mnHID_SelectionChange)
        return;

    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_pFilterView));
    GtkTreeIter iter;
    GtkTreeModel *model;
    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gchar *title;
        gtk_tree_model_get (model, &iter, 2, &title, -1);
        updateCurrentFilterFromName(title);
        g_free (title);
    }
    else if( GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(m_pDialog)))
    {
        if (m_pPseudoFilter != filter)
            updateCurrentFilterFromName(gtk_file_filter_get_name( filter ));
        else
            updateCurrentFilterFromName(OUStringToOString( m_aInitialFilter, RTL_TEXTENCODING_UTF8 ).getStr());
    }
}

OUString SAL_CALL SalGtkFilePicker::getCurrentFilter()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    UpdateFilterfromUI();

    return m_aCurrentFilter;
}

// XFilterGroupManager functions

void SAL_CALL SalGtkFilePicker::appendFilterGroup( const OUString& /*sGroupTitle*/, const uno::Sequence<beans::StringPair>& aFilters )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    // TODO m_pImpl->appendFilterGroup( sGroupTitle, aFilters );
    // check the names
    if( FilterNameExists( aFilters ) )
        // TODO: a more precise exception message
            throw IllegalArgumentException();

    // ensure that we have a filter list
    OUString sInitialCurrentFilter;
    if( aFilters.hasElements() )
        sInitialCurrentFilter = aFilters[0].First;

    ensureFilterVector( sInitialCurrentFilter );

    // append the filter
    for( const auto& rSubFilter : aFilters )
        m_pFilterVector->insert( m_pFilterVector->end(), FilterEntry( rSubFilter.First, rSubFilter.Second ) );

}

// XFilePicker functions

void SAL_CALL SalGtkFilePicker::setMultiSelectionMode( sal_Bool bMode )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER(m_pDialog), bMode );
}

void SAL_CALL SalGtkFilePicker::setDefaultName( const OUString& aName )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    OString aStr = OUStringToOString( aName, RTL_TEXTENCODING_UTF8 );
    GtkFileChooserAction eAction = gtk_file_chooser_get_action( GTK_FILE_CHOOSER( m_pDialog ) );

    // set_current_name launches a Gtk critical error if called for other than save
    if( GTK_FILE_CHOOSER_ACTION_SAVE == eAction )
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( m_pDialog ), aStr.getStr() );
}

void SAL_CALL SalGtkFilePicker::setDisplayDirectory( const OUString& rDirectory )
{
    SolarMutexGuard g;

    implsetDisplayDirectory(rDirectory);
}

OUString SAL_CALL SalGtkFilePicker::getDisplayDirectory()
{
    SolarMutexGuard g;

    return implgetDisplayDirectory();
}

uno::Sequence<OUString> SAL_CALL SalGtkFilePicker::getFiles()
{
    // no member access => no mutex needed

    uno::Sequence< OUString > aFiles = getSelectedFiles();
    /*
      The previous multiselection API design was completely broken
      and unimplementable for some heterogeneous pseudo-URIs eg. search:
      Thus crop unconditionally to a single selection.
    */
    if (aFiles.getLength() > 1)
        aFiles.realloc(1);
    return aFiles;
}

namespace
{

bool lcl_matchFilter( std::u16string_view rFilter, std::u16string_view rExt )
{
    const sal_Unicode cSep {';'};
    size_t nIdx {0};

    for (;;)
    {
        const size_t nBegin = rFilter.find(rExt, nIdx);

        if (nBegin == std::u16string_view::npos) // not found
            break;

        // Let nIdx point to end of matched string, useful in order to
        // check string boundaries and also for a possible next iteration
        nIdx = nBegin + rExt.size();

        // Check if the found occurrence is an exact match: right side
        if (nIdx < rFilter.size() && rFilter[nIdx]!=cSep)
            continue;

        // Check if the found occurrence is an exact match: left side
        if (nBegin>0 && rFilter[nBegin-1]!=cSep)
            continue;

        return true;
    }

    return false;
}

}

uno::Sequence<OUString> SAL_CALL SalGtkFilePicker::getSelectedFiles()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

#if !GTK_CHECK_VERSION(4, 0, 0)
    GSList* pPathList = gtk_file_chooser_get_uris( GTK_FILE_CHOOSER(m_pDialog) );
    int nCount = g_slist_length( pPathList );
#else
    GListModel* pPathList = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(m_pDialog));
    int nCount = g_list_model_get_n_items(pPathList);
#endif

    int nIndex = 0;

    // get the current action setting
    GtkFileChooserAction eAction = gtk_file_chooser_get_action(
        GTK_FILE_CHOOSER( m_pDialog ));

    uno::Sequence< OUString > aSelectedFiles(nCount);
    auto aSelectedFilesRange = asNonConstRange(aSelectedFiles);

    // Convert to OOo
#if !GTK_CHECK_VERSION(4, 0, 0)
    for( GSList *pElem = pPathList; pElem; pElem = pElem->next)
    {
        gchar *pURI = static_cast<gchar*>(pElem->data);
#else
    while (gpointer pElem = g_list_model_get_item(pPathList, nIndex))
    {
        gchar *pURI = g_file_get_uri(static_cast<GFile*>(pElem));
#endif

        aSelectedFilesRange[ nIndex ] = uritounicode(pURI);

        if( GTK_FILE_CHOOSER_ACTION_SAVE == eAction )
        {
            OUString sFilterName;
            sal_Int32 nTokenIndex = 0;
            bool bExtensionTypedIn = false;

            GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_pFilterView));
            GtkTreeIter iter;
            GtkTreeModel *model;
            if (gtk_tree_selection_get_selected (selection, &model, &iter))
            {
                gchar *title = nullptr;
                gtk_tree_model_get (model, &iter, 2, &title, -1);
                if (title)
                    sFilterName = OUString( title, strlen( title ), RTL_TEXTENCODING_UTF8 );
                else
                    sFilterName = OUString();
                g_free (title);
            }
            else
            {
                if( aSelectedFiles[nIndex].indexOf('.') > 0 )
                {
                    std::u16string_view sExtension;
                    nTokenIndex = 0;
                    do
                        sExtension = o3tl::getToken(aSelectedFiles[nIndex], 0, '.', nTokenIndex );
                    while( nTokenIndex >= 0 );

                    if( sExtension.size() >= 3 ) // 3 = typical/minimum extension length
                    {
                        OUString aNewFilter;
                        OUString aOldFilter = getCurrentFilter();
                        bool bChangeFilter = true;
                        if ( m_pFilterVector)
                            for (auto const& filter : *m_pFilterVector)
                            {
                                if( lcl_matchFilter( filter.getFilter(), Concat2View(OUString::Concat("*.") + sExtension) ) )
                                {
                                    if( aNewFilter.isEmpty() )
                                        aNewFilter = filter.getTitle();

                                    if( aOldFilter == filter.getTitle() )
                                        bChangeFilter = false;

                                    bExtensionTypedIn = true;
                                }
                            }
                        if( bChangeFilter && bExtensionTypedIn )
                        {
                            gchar* pCurrentName = gtk_file_chooser_get_current_name(GTK_FILE_CHOOSER(m_pDialog));
                            setCurrentFilter( aNewFilter );
                            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(m_pDialog), pCurrentName);
                            g_free(pCurrentName);
                        }
                    }
                }

                GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(m_pDialog));
                if (m_pPseudoFilter != filter)
                {
                    const gchar* filtername = filter ? gtk_file_filter_get_name(filter) : nullptr;
                    if (filtername)
                        sFilterName = OUString(filtername, strlen( filtername ), RTL_TEXTENCODING_UTF8);
                    else
                        sFilterName.clear();
                }
                else
                    sFilterName = m_aInitialFilter;
            }

            if (m_pFilterVector)
            {
                auto aVectorIter = ::std::find_if(
                    m_pFilterVector->begin(), m_pFilterVector->end(), FilterTitleMatch(sFilterName) );

                OUString aFilter;
                if (aVectorIter != m_pFilterVector->end())
                    aFilter = aVectorIter->getFilter();

                nTokenIndex = 0;
                OUString sToken;
                do
                {
                    sToken = aFilter.getToken( 0, '.', nTokenIndex );

                    if ( sToken.lastIndexOf( ';' ) != -1 )
                    {
                        sToken = sToken.getToken(0, ';');
                        break;
                    }
                }
                while( nTokenIndex >= 0 );

                if( !bExtensionTypedIn && ( sToken != "*" ) )
                {
                    //if the filename does not already have the auto extension, stick it on
                    OUString sExtension = "." + sToken;
                    OUString &rBase = aSelectedFilesRange[nIndex];
                    sal_Int32 nExtensionIdx = rBase.getLength() - sExtension.getLength();
                    SAL_INFO(
                        "vcl.gtk",
                        "idx are " << rBase.lastIndexOf(sExtension) << " "
                        << nExtensionIdx);

                    if( rBase.lastIndexOf( sExtension ) != nExtensionIdx )
                        rBase += sExtension;
                }
            }

        }

        nIndex++;
        g_free( pURI );
    }

#if !GTK_CHECK_VERSION(4, 0, 0)
    g_slist_free( pPathList );
#else
    g_object_unref(pPathList);
#endif

    return aSelectedFiles;
}

// XExecutableDialog functions

void SAL_CALL SalGtkFilePicker::setTitle( const OUString& rTitle )
{
    SolarMutexGuard g;

    implsetTitle(rTitle);
}

#if GTK_CHECK_VERSION(4, 0, 0)
namespace
{

GtkColumnView* lcl_findColumnView(GtkWidget* pWidget)
{
    if (GTK_IS_COLUMN_VIEW(pWidget))
        return GTK_COLUMN_VIEW(pWidget);

    GtkWidget* pChild = gtk_widget_get_first_child(GTK_WIDGET(pWidget));
    while (pChild)
    {
        if (GtkColumnView* pColumnView = lcl_findColumnView(pChild))
            return pColumnView;
        pChild = gtk_widget_get_next_sibling(pChild);
    }

    return nullptr;
}

}
#endif

sal_Int16 SAL_CALL SalGtkFilePicker::execute()
{
    SolarMutexGuard g;

    if (!mbInitialized)
    {
        // tdf#144084 if not initialized default to FILEOPEN_SIMPLE
        impl_initialize(nullptr, FILEOPEN_SIMPLE);
        assert(mbInitialized);
    }

    OSL_ASSERT( m_pDialog != nullptr );

    sal_Int16 retVal = 0;

    SetFilters();

    // tdf#84431 - set the filter after the corresponding widget is created
    if ( !m_aCurrentFilter.isEmpty() )
        SetCurFilter(m_aCurrentFilter);

#if !GTK_CHECK_VERSION(4, 0, 0)
    mnHID_FolderChange =
        g_signal_connect( GTK_FILE_CHOOSER( m_pDialog ), "current-folder-changed",
            G_CALLBACK( folder_changed_cb ), static_cast<gpointer>(this) );
#else
    // no replacement in 4-0 that I can see :-(
    mnHID_FolderChange = 0;
#endif

#if !GTK_CHECK_VERSION(4, 0, 0)
    mnHID_SelectionChange =
        g_signal_connect( GTK_FILE_CHOOSER( m_pDialog ), "selection-changed",
            G_CALLBACK( selection_changed_cb ), static_cast<gpointer>(this) );
#else
    // GtkFileChooser::selection-changed was dropped in GTK 4
    // Make use of GTK implementation details to connect to
    // GtkSelectionModel::selection-changed signal of the underlying model instead
    GtkColumnView* pColumnView = lcl_findColumnView(m_pDialog);
    assert(pColumnView && "Couldn't find the file dialog's column view");
    GtkSelectionModel* pSelectionModel = gtk_column_view_get_model(pColumnView);
    assert(pSelectionModel);
    mnHID_SelectionChange
        = g_signal_connect(pSelectionModel, "selection-changed", G_CALLBACK(selection_changed_cb),
                           static_cast<gpointer>(this));
#endif

    int btn = GTK_RESPONSE_NO;

    uno::Reference< awt::XExtendedToolkit > xToolkit(
        awt::Toolkit::create(m_xContext),
        UNO_QUERY_THROW );

    uno::Reference< frame::XDesktop > xDesktop(
        frame::Desktop::create(m_xContext),
        UNO_QUERY_THROW );

    GtkWindow *pParent = GTK_WINDOW(m_pParentWidget);
    if (!pParent)
    {
        SAL_WARN( "vcl.gtk", "no parent widget set");
        pParent = RunDialog::GetTransientFor();
    }
    if (pParent)
        gtk_window_set_transient_for(GTK_WINDOW(m_pDialog), pParent);
    rtl::Reference<RunDialog> pRunDialog = new RunDialog(m_pDialog, xToolkit, xDesktop);
    while( GTK_RESPONSE_NO == btn )
    {
        btn = GTK_RESPONSE_YES; // we don't want to repeat unless user clicks NO for file save.

        gint nStatus = pRunDialog->run();
        switch( nStatus )
        {
            case GTK_RESPONSE_ACCEPT:
                if( GTK_FILE_CHOOSER_ACTION_SAVE == gtk_file_chooser_get_action( GTK_FILE_CHOOSER( m_pDialog ) ) )
                {
                    Sequence < OUString > aPathSeq = getFiles();
                    if( aPathSeq.getLength() == 1 )
                    {
                        const OUString& sFileName = aPathSeq[0];
                        if (::utl::UCBContentHelper::Exists(sFileName))
                        {
                            INetURLObject aFileObj(sFileName);

                            OString baseName(
                              OUStringToOString(
                                aFileObj.getName(
                                  INetURLObject::LAST_SEGMENT,
                                  true,
                                  INetURLObject::DecodeMechanism::WithCharset
                                ),
                                RTL_TEXTENCODING_UTF8
                              )
                            );
                            OString aMsg(
                              OUStringToOString(
                                getResString( FILE_PICKER_OVERWRITE_PRIMARY ),
                                RTL_TEXTENCODING_UTF8
                              )
                            );
                            OString toReplace("$filename$"_ostr);

                            aMsg = aMsg.replaceAt(
                              aMsg.indexOf( toReplace ),
                              toReplace.getLength(),
                              baseName
                            );

                            GtkWidget *dlg = gtk_message_dialog_new( nullptr,
                                GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_YES_NO,
                                "%s",
                                aMsg.getStr()
                            );

                            GtkWidget* pOkButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES);
                            GtkStyleContext* pStyleContext = gtk_widget_get_style_context(pOkButton);
                            gtk_style_context_add_class(pStyleContext, "destructive-action");

                            sal_Int32 nSegmentCount = aFileObj.getSegmentCount();
                            if (nSegmentCount >= 2)
                            {
                                OString dirName(
                                  OUStringToOString(
                                    aFileObj.getName(
                                      nSegmentCount-2,
                                      true,
                                      INetURLObject::DecodeMechanism::WithCharset
                                    ),
                                    RTL_TEXTENCODING_UTF8
                                  )
                                );

                                aMsg =
                                  OUStringToOString(
                                    getResString( FILE_PICKER_OVERWRITE_SECONDARY ),
                                    RTL_TEXTENCODING_UTF8
                                  );

                                toReplace = "$dirname$"_ostr;

                                aMsg = aMsg.replaceAt(
                                  aMsg.indexOf( toReplace ),
                                  toReplace.getLength(),
                                  dirName
                                );

                                gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), "%s", aMsg.getStr() );
                            }

                            gtk_window_set_title( GTK_WINDOW( dlg ),
                                OUStringToOString(getResString(FILE_PICKER_TITLE_SAVE ),
                                RTL_TEXTENCODING_UTF8 ).getStr() );
                            gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(m_pDialog));
                            rtl::Reference<RunDialog> pAnotherDialog = new RunDialog(dlg, xToolkit, xDesktop);
                            btn = pAnotherDialog->run();

#if !GTK_CHECK_VERSION(4, 0, 0)
                            gtk_widget_destroy(dlg);
#else
                            gtk_window_destroy(GTK_WINDOW(dlg));
#endif
                        }

                        if( btn == GTK_RESPONSE_YES )
                            retVal = ExecutableDialogResults::OK;
                    }
                }
                else
                    retVal = ExecutableDialogResults::OK;
                break;

            case GTK_RESPONSE_CANCEL:
                retVal = ExecutableDialogResults::CANCEL;
                break;

            case 1: //PLAY
                {
                    FilePickerEvent evt;
                    evt.ElementId = PUSHBUTTON_PLAY;
                    impl_controlStateChanged( evt );
                    btn = GTK_RESPONSE_NO;
                }
                break;

            default:
                retVal = 0;
                break;
        }
    }
    gtk_widget_set_visible(m_pDialog, false);

    if (mnHID_FolderChange)
        g_signal_handler_disconnect(GTK_FILE_CHOOSER( m_pDialog ), mnHID_FolderChange);
    if (mnHID_SelectionChange)
        g_signal_handler_disconnect(GTK_FILE_CHOOSER( m_pDialog ), mnHID_SelectionChange);

    return retVal;
}

// cf. offapi/com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.idl
GtkWidget *SalGtkFilePicker::getWidget( sal_Int16 nControlId, GType *pType )
{
    GType      tType = GTK_TYPE_CHECK_BUTTON; //prevent warning by initializing
    GtkWidget *pWidget = nullptr;

#define MAP_TOGGLE( elem ) \
        case ExtendedFilePickerElementIds::CHECKBOX_##elem: \
            pWidget = m_pToggles[elem]; tType = GTK_TYPE_CHECK_BUTTON; \
            break
#define MAP_BUTTON( elem ) \
        case CommonFilePickerElementIds::PUSHBUTTON_##elem: \
            pWidget = m_pButtons[elem]; tType = GTK_TYPE_BUTTON; \
            break
#define MAP_EXT_BUTTON( elem ) \
        case ExtendedFilePickerElementIds::PUSHBUTTON_##elem: \
            pWidget = m_pButtons[elem]; tType = GTK_TYPE_BUTTON; \
            break
#define MAP_LIST( elem ) \
        case ExtendedFilePickerElementIds::LISTBOX_##elem: \
            pWidget = m_pLists[elem]; tType = GTK_TYPE_COMBO_BOX; \
            break
#define MAP_LIST_LABEL( elem ) \
        case ExtendedFilePickerElementIds::LISTBOX_##elem##_LABEL: \
            pWidget = m_pListLabels[elem]; tType = GTK_TYPE_LABEL; \
            break

    switch( nControlId )
    {
        MAP_TOGGLE( AUTOEXTENSION );
        MAP_TOGGLE( PASSWORD );
        MAP_TOGGLE( GPGENCRYPTION );
        MAP_TOGGLE( GPGSIGN );
        MAP_TOGGLE( FILTEROPTIONS );
        MAP_TOGGLE( READONLY );
        MAP_TOGGLE( LINK );
        MAP_TOGGLE( PREVIEW );
        MAP_TOGGLE( SELECTION );
        MAP_BUTTON( OK );
        MAP_BUTTON( CANCEL );
        MAP_EXT_BUTTON( PLAY );
        MAP_LIST( VERSION );
        MAP_LIST( TEMPLATE );
        MAP_LIST( IMAGE_TEMPLATE );
        MAP_LIST( IMAGE_ANCHOR );
        MAP_LIST_LABEL( VERSION );
        MAP_LIST_LABEL( TEMPLATE );
        MAP_LIST_LABEL( IMAGE_TEMPLATE );
        MAP_LIST_LABEL( IMAGE_ANCHOR );
    case CommonFilePickerElementIds::LISTBOX_FILTER_LABEL:
        // the filter list in gtk typically is not labeled, but has a built-in
        // tooltip to indicate what it does
        break;
    default:
        SAL_WARN( "vcl.gtk", "Handle unknown control " << nControlId);
        break;
    }
#undef MAP

    if( pType )
        *pType = tType;
    return pWidget;
}

// XFilePickerControlAccess functions

static void HackWidthToFirst(GtkComboBox *pWidget)
{
    GtkRequisition requisition;
#if !GTK_CHECK_VERSION(4, 0, 0)
    gtk_widget_size_request(GTK_WIDGET(pWidget), &requisition);
#else
    gtk_widget_get_preferred_size(GTK_WIDGET(pWidget), &requisition, nullptr);
#endif
    gtk_widget_set_size_request(GTK_WIDGET(pWidget), requisition.width, -1);
}

static void ComboBoxAppendText(GtkComboBox *pCombo, std::u16string_view rStr)
{
  GtkTreeIter aIter;
  GtkListStore *pStore = GTK_LIST_STORE(gtk_combo_box_get_model(pCombo));
  OString aStr = OUStringToOString(rStr, RTL_TEXTENCODING_UTF8);
  gtk_list_store_append(pStore, &aIter);
  gtk_list_store_set(pStore, &aIter, 0, aStr.getStr(), -1);
}

void SalGtkFilePicker::HandleSetListValue(GtkComboBox *pWidget, sal_Int16 nControlAction, const uno::Any& rValue)
{
    switch (nControlAction)
    {
        case ControlActions::ADD_ITEM:
            {
                OUString sItem;
                rValue >>= sItem;
                ComboBoxAppendText(pWidget, sItem);
                if (!bVersionWidthUnset)
                {
                    HackWidthToFirst(pWidget);
                    bVersionWidthUnset = true;
                }
            }
            break;
        case ControlActions::ADD_ITEMS:
            {
                Sequence< OUString > aStringList;
                rValue >>= aStringList;
                for (const auto& rString : aStringList)
                {
                    ComboBoxAppendText(pWidget, rString);
                    if (!bVersionWidthUnset)
                    {
                        HackWidthToFirst(pWidget);
                        bVersionWidthUnset = true;
                    }
                }
            }
            break;
        case ControlActions::DELETE_ITEM:
            {
                sal_Int32 nPos=0;
                rValue >>= nPos;

                GtkTreeIter aIter;
                GtkListStore *pStore = GTK_LIST_STORE(
                        gtk_combo_box_get_model(GTK_COMBO_BOX(pWidget)));
                if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pStore), &aIter, nullptr, nPos))
                    gtk_list_store_remove(pStore, &aIter);
            }
            break;
        case ControlActions::DELETE_ITEMS:
            {
                gtk_combo_box_set_active(pWidget, -1);
                GtkListStore *pStore = GTK_LIST_STORE(
                        gtk_combo_box_get_model(GTK_COMBO_BOX(pWidget)));
                gtk_list_store_clear(pStore);
            }
            break;
        case ControlActions::SET_SELECT_ITEM:
            {
                sal_Int32 nPos=0;
                rValue >>= nPos;
                gtk_combo_box_set_active(pWidget, nPos);
            }
            break;
        default:
            SAL_WARN( "vcl.gtk", "undocumented/unimplemented ControlAction for a list " << nControlAction);
            break;
    }

    //I think its best to make it insensitive unless there is the chance to
    //actually select something from the list.
    gint nItems = gtk_tree_model_iter_n_children(
                    gtk_combo_box_get_model(pWidget), nullptr);
    gtk_widget_set_sensitive(GTK_WIDGET(pWidget), nItems > 1);
}

uno::Any SalGtkFilePicker::HandleGetListValue(GtkComboBox *pWidget, sal_Int16 nControlAction)
{
    uno::Any aAny;
    switch (nControlAction)
    {
        case ControlActions::GET_ITEMS:
            {
                Sequence< OUString > aItemList;

                GtkTreeModel *pTree = gtk_combo_box_get_model(pWidget);
                GtkTreeIter iter;
                if (gtk_tree_model_get_iter_first(pTree, &iter))
                {
                    sal_Int32 nSize = gtk_tree_model_iter_n_children(
                        pTree, nullptr);

                    aItemList.realloc(nSize);
                    auto pItemList = aItemList.getArray();
                    for (sal_Int32 i=0; i < nSize; ++i)
                    {
                        gchar *item;
                        gtk_tree_model_get(gtk_combo_box_get_model(pWidget),
                            &iter, 0, &item, -1);
                        pItemList[i] = OUString(item, strlen(item), RTL_TEXTENCODING_UTF8);
                        g_free(item);
                        (void)gtk_tree_model_iter_next(pTree, &iter);
                    }
                }
                aAny <<= aItemList;
            }
            break;
        case ControlActions::GET_SELECTED_ITEM:
            {
                GtkTreeIter iter;
                if (gtk_combo_box_get_active_iter(pWidget, &iter))
                {
                        gchar *item;
                        gtk_tree_model_get(gtk_combo_box_get_model(pWidget),
                            &iter, 0, &item, -1);
                        OUString sItem(item, strlen(item), RTL_TEXTENCODING_UTF8);
                        aAny <<= sItem;
                        g_free(item);
                }
            }
            break;
        case ControlActions::GET_SELECTED_ITEM_INDEX:
            {
                gint nActive = gtk_combo_box_get_active(pWidget);
                aAny <<= static_cast< sal_Int32 >(nActive);
            }
            break;
        default:
            SAL_WARN( "vcl.gtk", "undocumented/unimplemented ControlAction for a list " << nControlAction);
            break;
    }
    return aAny;
}

void SAL_CALL SalGtkFilePicker::setValue( sal_Int16 nControlId, sal_Int16 nControlAction, const uno::Any& rValue )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    GType tType;
    GtkWidget *pWidget;

    if( !( pWidget = getWidget( nControlId, &tType ) ) )
        SAL_WARN( "vcl.gtk", "enable unknown control " << nControlId);
    else if( tType == GTK_TYPE_CHECK_BUTTON)
    {
        bool bChecked = false;
        rValue >>= bChecked;
#if GTK_CHECK_VERSION(4, 0, 0)
        gtk_check_button_set_active(GTK_CHECK_BUTTON(pWidget), bChecked);
#else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pWidget), bChecked);
#endif
    }
    else if( tType == GTK_TYPE_COMBO_BOX )
        HandleSetListValue(GTK_COMBO_BOX(pWidget), nControlAction, rValue);
    else
    {
        SAL_WARN( "vcl.gtk", "Can't set value on button / list " << nControlId << " " << nControlAction );
    }
}

uno::Any SAL_CALL SalGtkFilePicker::getValue( sal_Int16 nControlId, sal_Int16 nControlAction )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    uno::Any aRetval;

    GType tType;
    GtkWidget *pWidget;

    if( !( pWidget = getWidget( nControlId, &tType ) ) )
        SAL_WARN( "vcl.gtk", "enable unknown control " << nControlId);
    else if( tType == GTK_TYPE_CHECK_BUTTON)
    {
#if GTK_CHECK_VERSION(4, 0, 0)
        aRetval <<= bool(gtk_check_button_get_active(GTK_CHECK_BUTTON(pWidget)));
#else
        aRetval <<= bool(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pWidget)));
#endif
    }
    else if( tType == GTK_TYPE_COMBO_BOX )
        aRetval = HandleGetListValue(GTK_COMBO_BOX(pWidget), nControlAction);
    else
        SAL_WARN( "vcl.gtk", "Can't get value on button / list " << nControlId << " " << nControlAction );

    return aRetval;
}

void SAL_CALL SalGtkFilePicker::enableControl( sal_Int16 nControlId, sal_Bool bEnable )
{
    // skip this built-in one which is Enabled by default
    if (nControlId == ExtendedFilePickerElementIds::LISTBOX_FILTER_SELECTOR && bEnable)
        return;

    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    GtkWidget *pWidget;

    if ( ( pWidget = getWidget( nControlId ) ) )
    {
        if( bEnable )
        {
            gtk_widget_set_sensitive( pWidget, true );
        }
        else
        {
            gtk_widget_set_sensitive( pWidget, false );
        }
    }
    else
        SAL_WARN( "vcl.gtk", "enable unknown control " << nControlId );
}

void SAL_CALL SalGtkFilePicker::setLabel( sal_Int16 nControlId, const OUString& rLabel )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    GType tType;
    GtkWidget *pWidget;

    if( !( pWidget = getWidget( nControlId, &tType ) ) )
    {
        SAL_WARN_IF(nControlId != CommonFilePickerElementIds::LISTBOX_FILTER_LABEL,
                    "vcl.gtk", "Set label '" << rLabel << "' on unknown control " << nControlId);
        return;
    }

    OString aTxt = OUStringToOString( rLabel.replace('~', '_'), RTL_TEXTENCODING_UTF8 );
    if( tType == GTK_TYPE_CHECK_BUTTON || tType == GTK_TYPE_BUTTON || tType == GTK_TYPE_LABEL )
        g_object_set( pWidget, "label", aTxt.getStr(),
                      "use_underline", true, nullptr );
    else
        SAL_WARN( "vcl.gtk", "Can't set label on list");
}

OUString SAL_CALL SalGtkFilePicker::getLabel( sal_Int16 nControlId )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    GType tType;
    OString aTxt;
    GtkWidget *pWidget;

    if( !( pWidget = getWidget( nControlId, &tType ) ) )
        SAL_WARN( "vcl.gtk", "Get label on unknown control " << nControlId);
    else if( tType == GTK_TYPE_CHECK_BUTTON || tType == GTK_TYPE_BUTTON || tType == GTK_TYPE_LABEL )
        aTxt = gtk_button_get_label( GTK_BUTTON( pWidget ) );
    else
        SAL_WARN( "vcl.gtk", "Can't get label on list");

    return OStringToOUString( aTxt, RTL_TEXTENCODING_UTF8 );
}

// XFilePreview functions

uno::Sequence<sal_Int16> SAL_CALL SalGtkFilePicker::getSupportedImageFormats()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    // TODO return m_pImpl->getSupportedImageFormats();
    return uno::Sequence<sal_Int16>();
}

sal_Int32 SAL_CALL SalGtkFilePicker::getTargetColorDepth()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    // TODO return m_pImpl->getTargetColorDepth();
    return 0;
}

sal_Int32 SAL_CALL SalGtkFilePicker::getAvailableWidth()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    return g_PreviewImageWidth;
}

sal_Int32 SAL_CALL SalGtkFilePicker::getAvailableHeight()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    return g_PreviewImageHeight;
}

void SAL_CALL SalGtkFilePicker::setImage( sal_Int16 /*aImageFormat*/, const uno::Any& /*aImage*/ )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    // TODO m_pImpl->setImage( aImageFormat, aImage );
}

void SalGtkFilePicker::implChangeType( GtkTreeSelection *selection )
{
    OUString aLabel = getResString( FILE_PICKER_FILE_TYPE );

    GtkTreeIter iter;
    GtkTreeModel *model;
    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gchar *title;
        gtk_tree_model_get (model, &iter, 2, &title, -1);
        aLabel += ": " +
            OUString( title, strlen(title), RTL_TEXTENCODING_UTF8 );
        g_free (title);
    }
    gtk_expander_set_label (GTK_EXPANDER (m_pFilterExpander),
        OUStringToOString( aLabel, RTL_TEXTENCODING_UTF8 ).getStr());
    FilePickerEvent evt;
    evt.ElementId = LISTBOX_FILTER;
    impl_controlStateChanged( evt );
}

void SalGtkFilePicker::type_changed_cb( GtkTreeSelection *selection, SalGtkFilePicker *pobjFP )
{
    pobjFP->implChangeType(selection);
}

void SalGtkFilePicker::unselect_type()
{
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(m_pFilterView)));
}

void SalGtkFilePicker::expander_changed_cb( GtkExpander *expander, SalGtkFilePicker *pobjFP )
{
    if (gtk_expander_get_expanded(expander))
        pobjFP->unselect_type();
}

void SalGtkFilePicker::filter_changed_cb( GtkFileChooser *, GParamSpec *,
    SalGtkFilePicker *pobjFP )
{
    FilePickerEvent evt;
    evt.ElementId = LISTBOX_FILTER;
    SAL_INFO( "vcl.gtk", "filter_changed, isn't it great " << pobjFP );
    pobjFP->impl_controlStateChanged( evt );
}

void SalGtkFilePicker::folder_changed_cb( GtkFileChooser *, SalGtkFilePicker *pobjFP )
{
    FilePickerEvent evt;
    SAL_INFO( "vcl.gtk", "folder_changed, isn't it great " << pobjFP );
    pobjFP->impl_directoryChanged( evt );
}

#if !GTK_CHECK_VERSION(4, 0, 0)
void SalGtkFilePicker::selection_changed_cb( GtkFileChooser *, SalGtkFilePicker *pobjFP )
#else
void SalGtkFilePicker::selection_changed_cb(GtkSelectionModel*, guint, guint,
                                            SalGtkFilePicker* pobjFP)
#endif
{
    FilePickerEvent evt;
    SAL_INFO( "vcl.gtk", "selection_changed, isn't it great " << pobjFP );
    pobjFP->impl_fileSelectionChanged( evt );
}

void SalGtkFilePicker::update_preview_cb( GtkFileChooser *file_chooser, SalGtkFilePicker* pobjFP )
{
#if !GTK_CHECK_VERSION(4, 0, 0)
    bool have_preview = false;

    GtkWidget* preview = pobjFP->m_pPreview;
    char* filename = gtk_file_chooser_get_preview_filename( file_chooser );

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pobjFP->m_pToggles[PREVIEW])) && filename && g_file_test(filename, G_FILE_TEST_IS_REGULAR))
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(
                filename,
                g_PreviewImageWidth,
                g_PreviewImageHeight, nullptr );

        have_preview = ( pixbuf != nullptr );

        gtk_image_set_from_pixbuf( GTK_IMAGE( preview ), pixbuf );
        if( pixbuf )
            g_object_unref( G_OBJECT( pixbuf ) );

    }

    gtk_file_chooser_set_preview_widget_active( file_chooser, have_preview );

    if( filename )
        g_free( filename );
#else
    (void)file_chooser;
    (void)pobjFP;
#endif
}

sal_Bool SAL_CALL SalGtkFilePicker::setShowState( sal_Bool bShowState )
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    // TODO return m_pImpl->setShowState( bShowState );
    if( bool(bShowState) != mbPreviewState )
    {
        if( bShowState )
        {
            // Show
            if( !mHID_Preview )
            {
                mHID_Preview = g_signal_connect(
                    GTK_FILE_CHOOSER( m_pDialog ), "update-preview",
                    G_CALLBACK( update_preview_cb ), static_cast<gpointer>(this) );
            }
            gtk_widget_set_visible(m_pPreview, true);
        }
        else
        {
            // Hide
            gtk_widget_set_visible(m_pPreview, false);
        }

        // also emit the signal
        g_signal_emit_by_name( G_OBJECT( m_pDialog ), "update-preview" );

        mbPreviewState = bShowState;
    }
    return true;
}

sal_Bool SAL_CALL SalGtkFilePicker::getShowState()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    return mbPreviewState;
}

GtkWidget* SalGtkPicker::GetParentWidget(const uno::Sequence<uno::Any>& rArguments)
{
    GtkWidget* pParentWidget = nullptr;

    css::uno::Reference<css::awt::XWindow> xParentWindow;
    if (rArguments.getLength() > 1)
    {
        rArguments[1] >>= xParentWindow;
    }

    if (xParentWindow.is())
    {
        if (SalGtkXWindow* pGtkXWindow = dynamic_cast<SalGtkXWindow*>(xParentWindow.get()))
            pParentWidget = pGtkXWindow->getGtkWidget();
        else
        {
            css::uno::Reference<css::awt::XSystemDependentWindowPeer> xSysDepWin(xParentWindow, css::uno::UNO_QUERY);
            if (xSysDepWin.is())
            {
                css::uno::Sequence<sal_Int8> aProcessIdent(16);
                rtl_getGlobalProcessId(reinterpret_cast<sal_uInt8*>(aProcessIdent.getArray()));
                uno::Any aAny = xSysDepWin->getWindowHandle(aProcessIdent, css::lang::SystemDependent::SYSTEM_XWINDOW);
                css::awt::SystemDependentXWindow tmp;
                aAny >>= tmp;
                pParentWidget = GetGtkSalData()->GetGtkDisplay()->findGtkWidgetForNativeHandle(tmp.WindowHandle);
            }
        }
    }

    return pParentWidget;
}

// XInitialization

void SAL_CALL SalGtkFilePicker::initialize( const uno::Sequence<uno::Any>& aArguments )
{
    // parameter checking
    uno::Any aAny;
    if( !aArguments.hasElements() )
        throw lang::IllegalArgumentException(
            u"no arguments"_ustr,
            static_cast<XFilePicker2*>( this ), 1 );

    aAny = aArguments[0];

    if( ( aAny.getValueType() != cppu::UnoType<sal_Int16>::get()) &&
         (aAny.getValueType() != cppu::UnoType<sal_Int8>::get()) )
         throw lang::IllegalArgumentException(
            u"invalid argument type"_ustr,
            static_cast<XFilePicker2*>( this ), 1 );

    sal_Int16 templateId = -1;
    aAny >>= templateId;

    impl_initialize(GetParentWidget(aArguments), templateId);
}

void SalGtkFilePicker::impl_initialize(GtkWidget* pParentWidget, sal_Int16 templateId)
{
    m_pParentWidget = pParentWidget;

    GtkFileChooserAction eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
    OString sOpen = getOpenText();
    OString sSave = getSaveText();
    const gchar *first_button_text;

    SolarMutexGuard g;

    //   TODO: extract full semantic from
    //   svtools/source/filepicker/filepicker.cxx (getWinBits)
    switch( templateId )
    {
        case FILEOPEN_SIMPLE:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            break;
        case FILESAVE_SIMPLE:
            eAction = GTK_FILE_CHOOSER_ACTION_SAVE;
            first_button_text = sSave.getStr();
                break;
        case FILESAVE_AUTOEXTENSION_PASSWORD:
            eAction = GTK_FILE_CHOOSER_ACTION_SAVE;
            first_button_text = sSave.getStr();
            mbToggleVisibility[PASSWORD] = true;
            mbToggleVisibility[GPGENCRYPTION] = true;
#if HAVE_FEATURE_GPGME
            mbToggleVisibility[GPGSIGN] = true;
#endif
            // TODO
            break;
        case FILESAVE_AUTOEXTENSION_PASSWORD_FILTEROPTIONS:
            eAction = GTK_FILE_CHOOSER_ACTION_SAVE;
            first_button_text = sSave.getStr();
            mbToggleVisibility[PASSWORD] = true;
            mbToggleVisibility[GPGENCRYPTION] = true;
#if HAVE_FEATURE_GPGME
            mbToggleVisibility[GPGSIGN] = true;
#endif
            mbToggleVisibility[FILTEROPTIONS] = true;
            // TODO
                break;
        case FILESAVE_AUTOEXTENSION_SELECTION:
            eAction = GTK_FILE_CHOOSER_ACTION_SAVE; // SELECT_FOLDER ?
            first_button_text = sSave.getStr();
            mbToggleVisibility[SELECTION] = true;
            // TODO
                break;
        case FILESAVE_AUTOEXTENSION_TEMPLATE:
            eAction = GTK_FILE_CHOOSER_ACTION_SAVE;
            first_button_text = sSave.getStr();
            mbListVisibility[TEMPLATE] = true;
            // TODO
                break;
        case FILEOPEN_LINK_PREVIEW_IMAGE_TEMPLATE:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[LINK] = true;
            mbToggleVisibility[PREVIEW] = true;
            mbListVisibility[IMAGE_TEMPLATE] = true;
            // TODO
                break;
        case FILEOPEN_LINK_PREVIEW_IMAGE_ANCHOR:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[LINK] = true;
            mbToggleVisibility[PREVIEW] = true;
            mbListVisibility[IMAGE_ANCHOR] = true;
            // TODO
                break;
        case FILEOPEN_PLAY:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbButtonVisibility[PLAY] = true;
            // TODO
                break;
        case FILEOPEN_LINK_PLAY:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[LINK] = true;
            mbButtonVisibility[PLAY] = true;
            // TODO
                break;
        case FILEOPEN_READONLY_VERSION:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[READONLY] = true;
            mbListVisibility[VERSION] = true;
            break;
        case FILEOPEN_READONLY_VERSION_FILTEROPTIONS:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[FILTEROPTIONS] = true;
            mbToggleVisibility[READONLY] = true;
            mbListVisibility[VERSION] = true;
            break;
        case FILEOPEN_LINK_PREVIEW:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[LINK] = true;
            mbToggleVisibility[PREVIEW] = true;
            // TODO
                break;
        case FILESAVE_AUTOEXTENSION:
            eAction = GTK_FILE_CHOOSER_ACTION_SAVE;
            first_button_text = sSave.getStr();
            // TODO
                break;
        case FILEOPEN_PREVIEW:
            eAction = GTK_FILE_CHOOSER_ACTION_OPEN;
            first_button_text = sOpen.getStr();
            mbToggleVisibility[PREVIEW] = true;
            // TODO
                break;
        default:
                throw lang::IllegalArgumentException(
                u"Unknown template"_ustr,
                static_cast< XFilePicker2* >( this ),
                1 );
    }

    if( GTK_FILE_CHOOSER_ACTION_SAVE == eAction )
    {
        OUString aFilePickerTitle(getResString( FILE_PICKER_TITLE_SAVE ));
        gtk_window_set_title ( GTK_WINDOW( m_pDialog ),
            OUStringToOString( aFilePickerTitle, RTL_TEXTENCODING_UTF8 ).getStr() );
    }

    gtk_file_chooser_set_action( GTK_FILE_CHOOSER( m_pDialog ), eAction);
    m_pButtons[CANCEL] = gtk_dialog_add_button(GTK_DIALOG(m_pDialog), getCancelText().getStr(), GTK_RESPONSE_CANCEL);
    mbButtonVisibility[CANCEL] = true;

    if (mbButtonVisibility[PLAY])
    {
        OString aPlay = OUStringToOString(getResString(PUSHBUTTON_PLAY), RTL_TEXTENCODING_UTF8);
        m_pButtons[PLAY] = gtk_dialog_add_button(GTK_DIALOG(m_pDialog), aPlay.getStr(), 1);
    }

    m_pButtons[OK] = gtk_dialog_add_button(GTK_DIALOG(m_pDialog), first_button_text, GTK_RESPONSE_ACCEPT);
    mbButtonVisibility[OK] = true;

    gtk_dialog_set_default_response( GTK_DIALOG (m_pDialog), GTK_RESPONSE_ACCEPT );

    // Setup special flags
    for( int nTVIndex = 0; nTVIndex < TOGGLE_LAST; nTVIndex++ )
    {
        if( mbToggleVisibility[nTVIndex] )
            gtk_widget_set_visible(m_pToggles[nTVIndex], true);
    }

    for( int nTVIndex = 0; nTVIndex < LIST_LAST; nTVIndex++ )
    {
        if( mbListVisibility[nTVIndex] )
        {
            gtk_widget_set_sensitive( m_pLists[ nTVIndex ], false );
            gtk_widget_set_visible(m_pLists[nTVIndex], true);
            gtk_widget_set_visible(m_pListLabels[nTVIndex], true);
            gtk_widget_set_visible(m_pHBoxs[nTVIndex], true);
        }
    }

    mbInitialized = true;
}

void SalGtkFilePicker::preview_toggled_cb( GObject *cb, SalGtkFilePicker* pobjFP )
{
    if( pobjFP->mbToggleVisibility[PREVIEW] )
        pobjFP->setShowState( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( cb ) ) );
}

// XCancellable

void SAL_CALL SalGtkFilePicker::cancel()
{
    SolarMutexGuard g;

    OSL_ASSERT( m_pDialog != nullptr );

    // TODO m_pImpl->cancel();
}

// Misc

void SalGtkFilePicker::SetCurFilter( const OUString& rFilter )
{
    // Get all the filters already added
#if GTK_CHECK_VERSION(4, 0, 0)
    GListModel *filters = gtk_file_chooser_get_filters(GTK_FILE_CHOOSER(m_pDialog));
#else
    GSList *filters = gtk_file_chooser_list_filters(GTK_FILE_CHOOSER(m_pDialog));
#endif

#if GTK_CHECK_VERSION(4, 0, 0)
    int nIndex = 0;
    while (gpointer pElem = g_list_model_get_item(filters, nIndex))
    {
        GtkFileFilter* pFilter = static_cast<GtkFileFilter*>(pElem);
        ++nIndex;
#else
    for( GSList *iter = filters; iter; iter = iter->next )
    {
        GtkFileFilter* pFilter = static_cast<GtkFileFilter*>( iter->data );
#endif
        const gchar * filtername = gtk_file_filter_get_name( pFilter );
        OUString sFilterName( filtername, strlen( filtername ), RTL_TEXTENCODING_UTF8 );

        OUString aShrunkName = shrinkFilterName( rFilter );
        if( aShrunkName == sFilterName )
        {
            SAL_INFO( "vcl.gtk", "actually setting " << filtername );
            gtk_file_chooser_set_filter( GTK_FILE_CHOOSER( m_pDialog ), pFilter );
            break;
        }
    }

#if !GTK_CHECK_VERSION(4, 0, 0)
    g_slist_free( filters );
#else
    g_object_unref (filters);
#endif
}

#if !GTK_CHECK_VERSION(4, 0, 0)
extern "C"
{
static gboolean
case_insensitive_filter (const GtkFileFilterInfo *filter_info, gpointer data)
{
    bool bRetval = false;
    const char *pFilter = static_cast<const char *>(data);

    g_return_val_if_fail( data != nullptr, false );
    g_return_val_if_fail( filter_info != nullptr, false );

    if( !filter_info->uri )
        return false;

    const char *pExtn = strrchr( filter_info->uri, '.' );
    if( !pExtn )
        return false;
    pExtn++;

    if( !g_ascii_strcasecmp( pFilter, pExtn ) )
        bRetval = true;

    SAL_INFO( "vcl.gtk", "'" << filter_info->uri << "' match extn '" << pExtn << "' vs '" << pFilter << "' yields " << bRetval );

    return bRetval;
}
}
#endif

GtkFileFilter* SalGtkFilePicker::implAddFilter( const OUString& rFilter, const OUString& rType )
{
    GtkFileFilter *filter = gtk_file_filter_new();

    OUString aShrunkName = shrinkFilterName( rFilter );
    OString aFilterName = OUStringToOString( aShrunkName, RTL_TEXTENCODING_UTF8 );
    gtk_file_filter_set_name( filter, aFilterName.getStr() );

    OUStringBuffer aTokens;

    bool bAllGlob = rType == "*.*" || rType == "*";
    if (bAllGlob)
        gtk_file_filter_add_pattern( filter, "*" );
    else
    {
        sal_Int32 nIndex = 0;
        do
        {
            OUString aToken = rType.getToken( 0, ';', nIndex );
            // Assume all have the "*.<extn>" syntax
            sal_Int32 nStarDot = aToken.lastIndexOf( "*." );
            if (nStarDot >= 0)
                aToken = aToken.copy( nStarDot + 2 );
            if (!aToken.isEmpty())
            {
                if (!aTokens.isEmpty())
                    aTokens.append(",");
                aTokens.append(aToken);
#if GTK_CHECK_VERSION(4, 0, 0)
                gtk_file_filter_add_suffix(filter, aToken.toUtf8().getStr());
#else
                gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_URI,
                    case_insensitive_filter,
                    g_strdup( OUStringToOString(aToken, RTL_TEXTENCODING_UTF8).getStr() ),
                    g_free );
#endif

                SAL_INFO( "vcl.gtk", "fustering with " << aToken );
            }
#if OSL_DEBUG_LEVEL > 0
            else
            {
                g_warning( "Duff filter token '%s'\n",
                    OUStringToOString(
                        o3tl::getToken(rType, 0, ';', nIndex ), RTL_TEXTENCODING_UTF8 ).getStr() );
            }
#endif
        }
        while( nIndex >= 0 );
    }

    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( m_pDialog ), filter );

    if (!bAllGlob)
    {
        GtkTreeIter iter;
        gtk_list_store_append (m_pFilterStore, &iter);
        gtk_list_store_set (m_pFilterStore, &iter,
            0, OUStringToOString(shrinkFilterName( rFilter, true ), RTL_TEXTENCODING_UTF8).getStr(),
            1, OUStringToOString(aTokens, RTL_TEXTENCODING_UTF8).getStr(),
            2, aFilterName.getStr(),
            3, OUStringToOString(rType, RTL_TEXTENCODING_UTF8).getStr(),
            -1);
    }
    return filter;
}

void SalGtkFilePicker::implAddFilterGroup( const Sequence< StringPair >& _rFilters )
{
    // Gtk+ has no filter group concept I think so ...
    // implAddFilter( _rFilter, String() );
    for( const auto& rSubFilter : _rFilters )
        implAddFilter( rSubFilter.First, rSubFilter.Second );
}

void SalGtkFilePicker::SetFilters()
{
    if (m_aInitialFilter.isEmpty())
        m_aInitialFilter = m_aCurrentFilter;

    OUString sPseudoFilter;
    if( GTK_FILE_CHOOSER_ACTION_SAVE == gtk_file_chooser_get_action( GTK_FILE_CHOOSER( m_pDialog ) ) )
    {
        std::set<OUString> aAllFormats;
        if( m_pFilterVector )
        {
            for (auto & filter : *m_pFilterVector)
            {
                if( filter.hasSubFilters() )
                {   // it's a filter group
                    css::uno::Sequence< css::beans::StringPair > aSubFilters;
                    filter.getSubFilters( aSubFilters );
                    for (const auto& rSubFilter : aSubFilters)
                        aAllFormats.insert(rSubFilter.Second);
                }
                else
                    aAllFormats.insert(filter.getFilter());
            }
        }
        if (aAllFormats.size() > 1)
        {
            OUStringBuffer sAllFilter;
            for (auto const& format : aAllFormats)
            {
                if (!sAllFilter.isEmpty())
                    sAllFilter.append(";");
                sAllFilter.append(format);
            }
            sPseudoFilter = getResString(FILE_PICKER_ALLFORMATS);
            m_pPseudoFilter = implAddFilter( sPseudoFilter, sAllFilter.makeStringAndClear() );
        }
    }

    if( m_pFilterVector )
    {
        for (auto & filter : *m_pFilterVector)
        {
            if( filter.hasSubFilters() )
            {   // it's a filter group

                css::uno::Sequence< css::beans::StringPair > aSubFilters;
                filter.getSubFilters( aSubFilters );

                implAddFilterGroup( aSubFilters );
            }
            else
            {
                // it's a single filter

                implAddFilter( filter.getTitle(), filter.getFilter() );
            }
        }
    }

    // We always hide the expander now and depend on the user using the glob
    // list, or type a filename suffix, to select a filter by inference.
    gtk_widget_set_visible(m_pFilterExpander, false);

    // set the default filter
    if (!sPseudoFilter.isEmpty())
        SetCurFilter( sPseudoFilter );
    else if(!m_aCurrentFilter.isEmpty())
        SetCurFilter( m_aCurrentFilter );

    SAL_INFO( "vcl.gtk", "end setting filters");
}

SalGtkFilePicker::~SalGtkFilePicker()
{
#if !GTK_CHECK_VERSION(4, 0, 0)
    SolarMutexGuard g;

    int i;

    for( i = 0; i < TOGGLE_LAST; i++ )
        gtk_widget_destroy( m_pToggles[i] );

    for( i = 0; i < LIST_LAST; i++ )
    {
        gtk_widget_destroy( m_pListLabels[i] );
        gtk_widget_destroy( m_pLists[i] );
        gtk_widget_destroy( m_pHBoxs[i] );
    }

    m_pFilterVector.reset();

    gtk_widget_destroy( m_pVBox );
#endif
}

uno::Reference< ui::dialogs::XFilePicker2 >
GtkInstance::createFilePicker( const css::uno::Reference< css::uno::XComponentContext > &xMSF )
{
    return uno::Reference< ui::dialogs::XFilePicker2 >(
                new SalGtkFilePicker( xMSF ) );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
