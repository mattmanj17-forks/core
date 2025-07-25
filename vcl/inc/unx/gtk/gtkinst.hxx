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

#include <sal/config.h>

#include <stack>

#include <DropTarget.hxx>
#include <unx/salinst.h>
#include <unx/gensys.h>
#include <headless/svpinst.hxx>
#include <com/sun/star/datatransfer/DataFlavor.hpp>
#include <com/sun/star/datatransfer/dnd/XDragSource.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <cppuhelper/compbase.hxx>
#include <vcl/weld.hxx>
#include <vcl/weldutils.hxx>
#include <gtk/gtk.h>

vcl::Font pango_to_vcl(const PangoFontDescription* font, const css::lang::Locale& rLocale);

class GenPspGraphics;
class GtkYieldMutex final : public SalYieldMutex
{
    thread_local static std::stack<sal_uInt32> yieldCounts;

public:
         GtkYieldMutex() {}
    void ThreadsEnter();
    void ThreadsLeave();
};

class GtkSalFrame;

#if GTK_CHECK_VERSION(4, 0, 0)
gint gtk_dialog_run(GtkDialog *dialog);

struct read_transfer_result
{
    enum { BlockSize = 8192 };
    size_t nRead = 0;
    bool bDone = false;

    std::vector<sal_Int8> aVector;

    static void read_block_async_completed(GObject* source, GAsyncResult* res, gpointer user_data);

    OUString get_as_string() const;
    css::uno::Sequence<sal_Int8> get_as_sequence() const;
};

#endif

struct VclToGtkHelper
{
    std::vector<css::datatransfer::DataFlavor> aInfoToFlavor;
#if GTK_CHECK_VERSION(4, 0, 0)
    std::vector<OString> FormatsToGtk(const css::uno::Sequence<css::datatransfer::DataFlavor> &rFormats);
#else
    std::vector<GtkTargetEntry> FormatsToGtk(const css::uno::Sequence<css::datatransfer::DataFlavor> &rFormats);
#endif
#if GTK_CHECK_VERSION(4, 0, 0)
    void setSelectionData(const css::uno::Reference<css::datatransfer::XTransferable> &rTrans,
                          GdkContentProvider* provider,
                          const char* mime_type,
                          GOutputStream* stream,
                          int io_priority,
                          GCancellable* cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data);
#else
    void setSelectionData(const css::uno::Reference<css::datatransfer::XTransferable> &rTrans,
                          GtkSelectionData *selection_data, guint info);
#endif
private:
#if GTK_CHECK_VERSION(4, 0, 0)
    OString makeGtkTargetEntry(const css::datatransfer::DataFlavor& rFlavor);
#else
    GtkTargetEntry makeGtkTargetEntry(const css::datatransfer::DataFlavor& rFlavor);
#endif
};

class GtkTransferable : public cppu::WeakImplHelper<css::datatransfer::XTransferable>
{
protected:
#if GTK_CHECK_VERSION(4, 0, 0)
    std::map<OUString, OString> m_aMimeTypeToGtkType;
#else
    std::map<OUString, GdkAtom> m_aMimeTypeToGtkType;
#endif

#if GTK_CHECK_VERSION(4, 0, 0)
    std::vector<css::datatransfer::DataFlavor> getTransferDataFlavorsAsVector(const char * const *targets, gint n_targets);
#else
    std::vector<css::datatransfer::DataFlavor> getTransferDataFlavorsAsVector(GdkAtom *targets, gint n_targets);
#endif

public:
    virtual css::uno::Any SAL_CALL getTransferData(const css::datatransfer::DataFlavor& rFlavor) override = 0;
    virtual std::vector<css::datatransfer::DataFlavor> getTransferDataFlavorsAsVector() = 0;
    virtual css::uno::Sequence<css::datatransfer::DataFlavor> SAL_CALL getTransferDataFlavors() override;
    virtual sal_Bool SAL_CALL isDataFlavorSupported(const css::datatransfer::DataFlavor& rFlavor) override;
};

class GtkDnDTransferable;

class GtkInstDropTarget final
    : public cppu::ImplInheritanceHelper<DropTarget, css::lang::XServiceInfo>
{
    GtkSalFrame* m_pFrame;
    GtkDnDTransferable* m_pFormatConversionRequest;
    bool m_bInDrag;

public:
    GtkInstDropTarget();
    GtkInstDropTarget(GtkSalFrame* pFrame);
    virtual ~GtkInstDropTarget() override;

    void deinitialize();

    OUString SAL_CALL getImplementationName() override;

    sal_Bool SAL_CALL supportsService(OUString const & ServiceName) override;

    css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;

    void SetFormatConversionRequest(GtkDnDTransferable *pRequest)
    {
        m_pFormatConversionRequest = pRequest;
    }

#if !GTK_CHECK_VERSION(4, 0, 0)
    gboolean signalDragMotion(GtkWidget* pWidget, GdkDragContext* context, gint x, gint y, guint time);
    gboolean signalDragDrop(GtkWidget* pWidget, GdkDragContext* context, gint x, gint y, guint time);
#else
    GdkDragAction signalDragMotion(GtkDropTargetAsync *context, GdkDrop *drop, double x, double y);
    gboolean signalDragDrop(GtkDropTargetAsync *context, GdkDrop *drop, double x, double y);
#endif

    void signalDragLeave(GtkWidget* pWidget);

#if !GTK_CHECK_VERSION(4, 0, 0)
    void signalDragDropReceived(GtkWidget* pWidget, GdkDragContext* context, gint x, gint y, GtkSelectionData* data, guint ttype, guint time);
#endif
};

class GtkInstDragSource final
    : public cppu::WeakComponentImplHelper<css::datatransfer::dnd::XDragSource,
                                           css::lang::XServiceInfo>
{
    osl::Mutex m_aMutex;
    GtkSalFrame* m_pFrame;
    css::uno::Reference<css::datatransfer::dnd::XDragSourceListener> m_xListener;
    css::uno::Reference<css::datatransfer::XTransferable> m_xTrans;
    VclToGtkHelper m_aConversionHelper;
public:
    GtkInstDragSource()
        : WeakComponentImplHelper(m_aMutex)
        , m_pFrame(nullptr)
    {
    }
    GtkInstDragSource(GtkSalFrame* pFrame);

    void set_datatransfer(const css::uno::Reference<css::datatransfer::XTransferable>& rTrans,
                          const css::uno::Reference<css::datatransfer::dnd::XDragSourceListener>& rListener);

#if !GTK_CHECK_VERSION(4, 0, 0)
    std::vector<GtkTargetEntry> FormatsToGtk(const css::uno::Sequence<css::datatransfer::DataFlavor> &rFormats);
#endif

    void setActiveDragSource();

    virtual ~GtkInstDragSource() override;

    // XDragSource
    virtual sal_Bool    SAL_CALL isDragImageSupported() override;
    virtual sal_Int32   SAL_CALL getDefaultCursor(sal_Int8 dragAction) override;
    virtual void        SAL_CALL startDrag(
        const css::datatransfer::dnd::DragGestureEvent& trigger, sal_Int8 sourceActions, sal_Int32 cursor, sal_Int32 image,
        const css::uno::Reference< css::datatransfer::XTransferable >& transferable,
        const css::uno::Reference< css::datatransfer::dnd::XDragSourceListener >& listener) override;

    void deinitialize();

    OUString SAL_CALL getImplementationName() override;

    sal_Bool SAL_CALL supportsService(OUString const & ServiceName) override;

    css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;

    void dragFailed();
    void dragDelete();
#if GTK_CHECK_VERSION(4, 0, 0)
    void dragEnd(GdkDrag* drag);
#else
    void dragEnd(GdkDragContext* context);
    void dragDataGet(GtkSelectionData *data, guint info);
#endif

    // For LibreOffice internal D&D we provide the Transferable without Gtk
    // intermediaries as a shortcut, see tdf#100097 for how dbaccess depends on this
    static GtkInstDragSource* g_ActiveDragSource;
    css::uno::Reference<css::datatransfer::XTransferable> const & GetTransferable() const { return m_xTrans; }
};

enum SelectionType { SELECTION_CLIPBOARD = 0, SELECTION_PRIMARY = 1 };

class GtkSalTimer;
class GtkInstance final : public SvpSalInstance
{
public:
            GtkInstance( std::unique_ptr<SalYieldMutex> pMutex );
    virtual ~GtkInstance() override;
    void    EnsureInit();
    virtual void AfterAppInit() override;

    virtual SalFrame*           CreateFrame( SalFrame* pParent, SalFrameStyleFlags nStyle ) override;
    virtual SalFrame*           CreateChildFrame( SystemParentData* pParent, SalFrameStyleFlags nStyle ) override;
    virtual SalObject*          CreateObject( SalFrame* pParent, SystemWindowData* pWindowData, bool bShow ) override;
    virtual SalSystem*          CreateSalSystem() override;
    virtual SalInfoPrinter*     CreateInfoPrinter(SalPrinterQueueInfo* pPrinterQueueInfo, ImplJobSetup* pJobSetup) override;
    virtual std::unique_ptr<SalPrinter> CreatePrinter( SalInfoPrinter* pInfoPrinter ) override;
    virtual std::unique_ptr<SalMenu>     CreateMenu( bool, Menu* ) override;
    virtual std::unique_ptr<SalMenuItem> CreateMenuItem( const SalItemParams& ) override;
    virtual SalTimer*           CreateSalTimer() override;
    virtual void                AddToRecentDocumentList(const OUString& rFileUrl, const OUString& rMimeType, const OUString& rDocumentService) override;
    virtual std::unique_ptr<SalVirtualDevice>
                                CreateVirtualDevice( SalGraphics&,
                                                     tools::Long nDX, tools::Long nDY,
                                                     DeviceFormat eFormat,
                                                     bool bAlphaMaskTransparent = false ) override;
    virtual std::unique_ptr<SalVirtualDevice>
                                CreateVirtualDevice( SalGraphics&,
                                                     tools::Long &nDX, tools::Long &nDY,
                                                     DeviceFormat eFormat,
                                                     const SystemGraphicsData& ) override;
    virtual std::shared_ptr<SalBitmap> CreateSalBitmap() override;

    virtual bool                DoYield(bool bWait, bool bHandleAllCurrentEvents) override;
    virtual bool                AnyInput( VclInputFlags nType ) override;
    // impossible to handle correctly, as "main thread" depends on the dispatch mutex
    virtual bool                IsMainThread() const override { return false; }

    virtual std::unique_ptr<GenPspGraphics> CreatePrintGraphics() override;

    virtual bool hasNativeFileSelection() const override { return true; }

    virtual css::uno::Reference< css::ui::dialogs::XFilePicker2 >
        createFilePicker( const css::uno::Reference< css::uno::XComponentContext >& ) override;
    virtual css::uno::Reference< css::ui::dialogs::XFolderPicker2 >
        createFolderPicker( const css::uno::Reference< css::uno::XComponentContext >& ) override;

    virtual css::uno::Reference<css::datatransfer::clipboard::XClipboard>
    CreateClipboard(const css::uno::Sequence<css::uno::Any>& i_rArguments) override;
    virtual css::uno::Reference<css::datatransfer::dnd::XDragSource>
    ImplCreateDragSource(const SystemEnvData& rSysEnv) override;
    virtual css::uno::Reference<css::datatransfer::dnd::XDropTarget>
    ImplCreateDropTarget(const SystemEnvData& rSysEnv) override;
    virtual OpenGLContext* CreateOpenGLContext() override;
    virtual std::unique_ptr<weld::Builder> CreateBuilder(weld::Widget* pParent, const OUString& rUIRoot, const OUString& rUIFile) override;
    virtual std::unique_ptr<weld::Builder> CreateInterimBuilder(vcl::Window* pParent, const OUString& rUIRoot, const OUString& rUIFile,
                                                bool bAllowCycleFocusOut, sal_uInt64 nLOKWindowId = 0) override;
    virtual weld::MessageDialog* CreateMessageDialog(weld::Widget* pParent, VclMessageType eMessageType, VclButtonsType eButtonType, const OUString &rPrimaryMessage) override;
    std::unique_ptr<weld::ColorChooserDialog>
    CreateColorChooserDialog(weld::Window* pParent, vcl::ColorPickerMode eMode) override;
    virtual weld::Window* GetFrameWeld(const css::uno::Reference<css::awt::XWindow>& rWindow) override;

    virtual const cairo_font_options_t* GetCairoFontOptions() override;
            const cairo_font_options_t* GetLastSeenCairoFontOptions() const;
                                   void ResetLastSeenCairoFontOptions(const cairo_font_options_t* pOptions);

    void                        RemoveTimer ();

    void* CreateGStreamerSink(const SystemChildWindow*) override;

private:
    GtkSalTimer *m_pTimer;
    css::uno::Reference<css::datatransfer::clipboard::XClipboard> m_aClipboards[2];
    bool                        IsTimerExpired();
    bool                        bNeedsInit;
    cairo_font_options_t*       m_pLastCairoFontOptions;
};

inline GtkInstance* GetGtkInstance() { return static_cast<GtkInstance*>(GetSalInstance()); }

class SalGtkXWindow final : public weld::TransportAsXWindow
{
private:
    weld::Window* m_pWeldWidget;
    GtkWidget* m_pWidget;
public:

    SalGtkXWindow(weld::Window* pWeldWidget, GtkWidget* pWidget)
        : TransportAsXWindow(pWeldWidget)
        , m_pWeldWidget(pWeldWidget)
        , m_pWidget(pWidget)
    {
    }

    virtual void clear() override
    {
        m_pWeldWidget = nullptr;
        m_pWidget = nullptr;
        TransportAsXWindow::clear();
    }

    GtkWidget* getGtkWidget() const
    {
        return m_pWidget;
    }

    weld::Window* getFrameWeld() const
    {
        return m_pWeldWidget;
    }
};

GdkPixbuf* load_icon_by_name(const OUString& rIconName);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
