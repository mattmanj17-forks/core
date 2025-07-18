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

#ifndef INCLUDED_SD_SOURCE_UI_INC_ACCESSIBLEDOCUMENTVIEWBASE_HXX
#define INCLUDED_SD_SOURCE_UI_INC_ACCESSIBLEDOCUMENTVIEWBASE_HXX

#include <editeng/AccessibleContextBase.hxx>
#include <editeng/AccessibleSelectionBase.hxx>
#include "AccessibleViewForwarder.hxx"
#include <svx/AccessibleShapeTreeInfo.hxx>
#include <svx/IAccessibleViewForwarderListener.hxx>

#include <com/sun/star/awt/XWindowListener.hpp>
#include <com/sun/star/awt/XFocusListener.hpp>
#include <com/sun/star/beans/XPropertyChangeListener.hpp>
#include <tools/link.hxx>

#include <com/sun/star/accessibility/XAccessibleExtendedAttributes.hpp>

#include "Window.hxx"

namespace com::sun::star::accessibility { class XAccessible; }
namespace com::sun::star::frame { class XModel; }
namespace com::sun::star::awt { class XWindow; }

class VclWindowEvent;

namespace sd {
class ViewShell;
}

namespace accessibility {

/** Base class for the various document views of the Draw and
    Impress applications.

    <p>The different view modes of the Draw and Impress applications
    are made accessible by derived classes.  When the view mode is
    changed then the object representing the document view is
    disposed and replaced by a new instance of the then appropriate
    derived class.</p>

    <p>This base class also manages an optionally active accessible OLE
    object.  If you overwrite the <member>getAccessibleChildCount</member>
    and <member>getAccessibleChild</member> methods then make sure to first
    call the corresponding method of this class and adapt your child count
    and indices accordingly.  Only one active OLE object is allowed at a
    time.  This class does not listen for disposing calls at the moment
    because it does not use the accessible OLE object directly and trusts on
    getting informed through VCL window events.</p>

    <p>This class implements three kinds of listeners:
    <ol><li>The property change listener is not used directly but exists as
    convenience for derived classes.  May be moved to those classes
    instead.</li>
    <li>As window listener it waits for changes of the window geometry and
    forwards those as view forwarder changes.</li>
    <li>As focus listener it keeps track of the focus to give this class and
    derived classes the opportunity to set and remove the focus to/from
    shapes.</li>
    </ol>
    </p>
*/
class AccessibleDocumentViewBase
    :   public AccessibleContextBase,
        public AccessibleSelectionBase,
        public IAccessibleViewForwarderListener,
        public css::beans::XPropertyChangeListener,
        public css::awt::XWindowListener,
        public css::awt::XFocusListener,
        public css::accessibility::XAccessibleExtendedAttributes
{
public:
    /** Create a new object.  Note that the caller has to call the
        Init method directly after this constructor has finished.
    @param pSdWindow
        The window whose content is to be made accessible.
    @param pViewShell
        The view shell associated with the given window.
    @param rxController
        The controller from which to get the model.
    @param rxParent
        The accessible parent of the new object.  Note that this parent does
        not necessarily correspond with the parent of the given window.
     */
    AccessibleDocumentViewBase (
        ::sd::Window* pSdWindow,
        ::sd::ViewShell* pViewShell,
        css::uno::Reference<css::frame::XController> xController,
        const css::uno::Reference<css::accessibility::XAccessible>& rxParent);

    virtual ~AccessibleDocumentViewBase() override;

    /** Initialize a new object.  Call this method directly after creating a
        new object.  It finished the initialization begun in the constructor
        but which needs a fully created object.
     */
    virtual void Init();

    /** Define callback for listening to window child events of VCL.
        Listen for creation or destruction of OLE objects.
    */
    DECL_LINK( WindowChildEventListener, VclWindowEvent&, void );

    //=====  IAccessibleViewForwarderListener  ================================

    /** A view forwarder change is signalled for instance when any of the
        window events is received.  Thus, instead of overriding the four
        windowResized... methods it will be sufficient in most cases just to
        override this method.
     */
    virtual void ViewForwarderChanged() override;

    //=====  XAccessibleContext  ==============================================

    virtual css::uno::Reference<css::accessibility::XAccessible> SAL_CALL
        getAccessibleParent() override;

    /** This implementation returns either 1 or 0 depending on whether there
        is an active accessible OLE object or not.
    */
    virtual sal_Int64 SAL_CALL
        getAccessibleChildCount() override;

    /** This implementation either returns the active accessible OLE object
        if it exists and the given index is 0 or throws an exception.
    */
    virtual css::uno::Reference<css::accessibility::XAccessible> SAL_CALL
        getAccessibleChild (sal_Int64 nIndex) override;

    // OAccessible
    virtual css::awt::Rectangle implGetBounds() override;

    //=====  XAccessibleComponent  ============================================

    virtual css::uno::Reference<css::accessibility::XAccessible > SAL_CALL
        getAccessibleAtPoint (const css::awt::Point& aPoint) override;

    //=====  XInterface  ======================================================

    virtual css::uno::Any SAL_CALL
        queryInterface (const css::uno::Type & rType) override;

    virtual void SAL_CALL
        acquire()
        noexcept override;

    virtual void SAL_CALL
        release()
        noexcept override;

    //=====  XServiceInfo  ====================================================

    /** Returns an identifier for the implementation of this object.
    */
    virtual OUString SAL_CALL
        getImplementationName() override;

    virtual css::uno::Sequence< OUString> SAL_CALL
        getSupportedServiceNames() override;

    //=====  lang::XEventListener  ============================================

    virtual void SAL_CALL
        disposing (const css::lang::EventObject& rEventObject) override;

    //=====  XPropertyChangeListener  =========================================

    virtual void SAL_CALL
        propertyChange (const css::beans::PropertyChangeEvent& rEventObject) override;

    //=====  XWindowListener  =================================================

    virtual void SAL_CALL
        windowResized (const css::awt::WindowEvent& e) override;

    virtual void SAL_CALL
        windowMoved (const css::awt::WindowEvent& e) override;

    virtual void SAL_CALL
        windowShown (const css::lang::EventObject& e) override;

    virtual void SAL_CALL
        windowHidden (const css::lang::EventObject& e) override;

    //=====  XFocusListener  =================================================

    virtual void SAL_CALL focusGained (const css::awt::FocusEvent& e) override;
    virtual void SAL_CALL focusLost (const css::awt::FocusEvent& e) override;
    //----------------------------xAttribute----------------------------
    virtual OUString SAL_CALL getExtendedAttributes() override;
     ::sd::ViewShell* mpViewShell;
private:

    // return the member maMutex;
    virtual ::osl::Mutex&
        implGetMutex() override;

    // return ourself as context in default case
    virtual css::uno::Reference< css::accessibility::XAccessibleContext >
        implGetAccessibleContext() override;

    // return sal_False in default case
    virtual bool
        implIsSelected( sal_Int64 nAccessibleChildIndex ) override;

    // return nothing in default case
    virtual void
        implSelect( sal_Int64 nAccessibleChildIndex, bool bSelect ) override;

protected:
    /// The API window that is made accessible.
    css::uno::Reference< css::awt::XWindow>
         mxWindow;

    /// The controller of the window in which this view is displayed.
    css::uno::Reference< css::frame::XController>
         mxController;

    /// Model of the document.
    css::uno::Reference < css::frame::XModel>
        mxModel;

    // Bundle of information that is passed down the shape tree.
    AccessibleShapeTreeInfo maShapeTreeInfo;

    /// The view forwarder passed to the children manager.
    AccessibleViewForwarder maViewForwarder;

    /** Accessible OLE object.  Set or removed by the
        <member>SetAccessibleOLEObject</member> method.
    */
    css::uno::Reference< css::accessibility::XAccessible>
        mxAccessibleOLEObject;

    Link<VclWindowEvent&,void> maWindowLink;

    // This method is called from the component helper base class while
    // disposing.
    virtual void SAL_CALL disposing() override;

    /** Create a name string.  The current name is not modified and,
        therefore, no events are sent.  This method is usually called once
        by the <member>getAccessibleName</member> method of the base class.
        @return
           A name string.
    */
    virtual OUString
        CreateAccessibleName () override;

    /** This method is called when (after) the frame containing this
        document has been activated.  Can be used to send FOCUSED state
        changes for the currently selected element.

        Note: Currently used as a substitute for FocusGained.  Should be
        renamed in the future.
    */
    virtual void Activated();

    /** This method is called when (before or after?) the frame containing
        this document has been deactivated.  Can be used to send FOCUSED
        state changes for the currently selected element.

        Note: Currently used as a substitute for FocusLost.  Should be
        renamed in the future.
    */
    virtual void Deactivated();

    /** Set or remove the currently active accessible OLE object.
        @param xOLEObject
            If this is a valid reference then a child event is send that
            informs the listeners of a new child.  If there has already been
            an active accessible OLE object then this is removed first and
            appropriate events are sent.

            If this is an empty reference then the currently active
            accessible OLE object (if there is one) is removed.
    */
    void SetAccessibleOLEObject (
        const css::uno::Reference<css::accessibility::XAccessible>& xOLEObject);

public:
    void SwitchViewActivated() { Activated(); }
     virtual sal_Int32 SAL_CALL getForeground(  ) override;

    virtual sal_Int32 SAL_CALL getBackground(  ) override;
    virtual void impl_dispose();
};

} // end of namespace accessibility

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
