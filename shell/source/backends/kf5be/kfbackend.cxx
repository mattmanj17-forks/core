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

#include <sal/config.h>

#include <memory>

#include <QtWidgets/QApplication>

#include <com/sun/star/beans/Optional.hpp>
#include <com/sun/star/beans/UnknownPropertyException.hpp>
#include <com/sun/star/beans/XPropertyChangeListener.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/beans/XVetoableChangeListener.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/uno/XCurrentContext.hpp>
#include <cppuhelper/implbase.hxx>
#include <cppuhelper/weak.hxx>
#include <rtl/ustring.hxx>
#include <sal/types.h>
#include <uno/current_context.hxx>
#include <vcl/svapp.hxx>

#include <osl/process.h>
#include <osl/thread.h>

#include "kfaccess.hxx"

namespace
{
class Service : public cppu::WeakImplHelper<css::lang::XServiceInfo, css::beans::XPropertySet>
{
public:
    Service();

private:
    // noncopyable until we have good reasons...
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;

    virtual ~Service() override {}

    virtual OUString SAL_CALL getImplementationName() override
    {
        return u"com.sun.star.comp.configuration.backend.KF5Backend"_ustr;
    }

    virtual sal_Bool SAL_CALL supportsService(OUString const& ServiceName) override
    {
        return ServiceName == getSupportedServiceNames()[0];
    }

    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override
    {
        return { u"com.sun.star.configuration.backend.KF5Backend"_ustr };
    }

    virtual css::uno::Reference<css::beans::XPropertySetInfo> SAL_CALL getPropertySetInfo() override
    {
        return css::uno::Reference<css::beans::XPropertySetInfo>();
    }

    virtual void SAL_CALL setPropertyValue(OUString const&, css::uno::Any const&) override;

    virtual css::uno::Any SAL_CALL getPropertyValue(OUString const& PropertyName) override;

    virtual void SAL_CALL addPropertyChangeListener(
        OUString const&, css::uno::Reference<css::beans::XPropertyChangeListener> const&) override
    {
    }

    virtual void SAL_CALL removePropertyChangeListener(
        OUString const&, css::uno::Reference<css::beans::XPropertyChangeListener> const&) override
    {
    }

    virtual void SAL_CALL addVetoableChangeListener(
        OUString const&, css::uno::Reference<css::beans::XVetoableChangeListener> const&) override
    {
    }

    virtual void SAL_CALL removeVetoableChangeListener(
        OUString const&, css::uno::Reference<css::beans::XVetoableChangeListener> const&) override
    {
    }

    std::map<OUString, css::beans::Optional<css::uno::Any>> m_KDESettings;
};

OString getDisplayArg()
{
    OUString aParam;
    const sal_uInt32 nParams = osl_getCommandArgCount();
    for (sal_uInt32 nIdx = 0; nIdx < nParams; ++nIdx)
    {
        osl_getCommandArg(nIdx, &aParam.pData);
        if (aParam != "-display")
            continue;

        ++nIdx;
        osl_getCommandArg(nIdx, &aParam.pData);
        return OUStringToOString(aParam, osl_getThreadTextEncoding());
    }
    return {};
}

OString getExecutable()
{
    OUString aParam, aBin;
    osl_getExecutableFile(&aParam.pData);
    osl_getSystemPathFromFileURL(aParam.pData, &aBin.pData);
    return OUStringToOString(aBin, osl_getThreadTextEncoding());
}

void readKDESettings(std::map<OUString, css::beans::Optional<css::uno::Any>>& rSettings)
{
    const std::vector<OUString> aKeys
        = { u"ExternalMailer"_ustr,       u"SourceViewFontHeight"_ustr, u"SourceViewFontName"_ustr,
            u"WorkPathVariable"_ustr,     u"ooInetHTTPProxyName"_ustr,  u"ooInetHTTPProxyPort"_ustr,
            u"ooInetHTTPSProxyName"_ustr, u"ooInetHTTPSProxyPort"_ustr, u"ooInetNoProxy"_ustr,
            u"ooInetProxyType"_ustr };

    for (const OUString& aKey : aKeys)
    {
        css::beans::Optional<css::uno::Any> aValue = kfaccess::getValue(aKey);
        std::pair<OUString, css::beans::Optional<css::uno::Any>> elem
            = std::make_pair(aKey, aValue);
        rSettings.insert(elem);
    }
}

// init the QApplication when we load the kfbackend into a non-Qt vclplug (e.g. gtk3_kde5)
// TODO: use a helper process to read these values without linking to Qt directly?
// TODO: share this code somehow with Qt5Instance.cxx?
void initQApp(std::map<OUString, css::beans::Optional<css::uno::Any>>& rSettings)
{
    const auto aDisplay = getDisplayArg();
    int nFakeArgc = aDisplay.isEmpty() ? 2 : 3;
    char** pFakeArgv = new char*[nFakeArgc];

    pFakeArgv[0] = strdup(getExecutable().getStr());
    pFakeArgv[1] = strdup("--nocrashhandler");
    if (!aDisplay.isEmpty())
        pFakeArgv[2] = strdup(aDisplay.getStr());

    char* session_manager = nullptr;
    if (auto* session_manager_env = getenv("SESSION_MANAGER"))
    {
        session_manager = strdup(session_manager_env);
        unsetenv("SESSION_MANAGER");
    }

    {
        // rhbz#2047319 drop the SolarMutex during the execution of QApplication::init()
        // https://invent.kde.org/qt/qt/qtwayland/-/merge_requests/24#note_383915
        SolarMutexReleaser aReleaser; // rhbz#2047319 drop the SolarMutex during the execution

        std::unique_ptr<QApplication> app(new QApplication(nFakeArgc, pFakeArgv));
        QObject::connect(app.get(), &QObject::destroyed, app.get(), [nFakeArgc, pFakeArgv]() {
            for (int i = 0; i < nFakeArgc; ++i)
                free(pFakeArgv[i]);
            delete[] pFakeArgv;
        });

        readKDESettings(rSettings);
    }

    if (session_manager != nullptr)
    {
        // coverity[tainted_string] - trusted source for setenv
        setenv("SESSION_MANAGER", session_manager, 1);
        free(session_manager);
    }
}

Service::Service()
{
    if (Application::GetDesktopEnvironment() == u"PLASMA5")
    {
        if (!qApp) // no qt event loop yet
        {
            // so we start one and read KDE settings
            initQApp(m_KDESettings);
        }
        else // someone else (most likely kde/qt vclplug) has started qt event loop
            // all that is left to do is to read KDE settings
            readKDESettings(m_KDESettings);
    }
}

void Service::setPropertyValue(OUString const&, css::uno::Any const&)
{
    throw css::lang::IllegalArgumentException(u"setPropertyValue not supported"_ustr, getXWeak(),
                                              -1);
}

css::uno::Any Service::getPropertyValue(OUString const& PropertyName)
{
    if (PropertyName == "ExternalMailer" || PropertyName == "SourceViewFontHeight"
        || PropertyName == "SourceViewFontName" || PropertyName == "WorkPathVariable"
        || PropertyName == "ooInetHTTPProxyName" || PropertyName == "ooInetHTTPProxyPort"
        || PropertyName == "ooInetHTTPSProxyName" || PropertyName == "ooInetHTTPSProxyPort"
        || PropertyName == "ooInetNoProxy" || PropertyName == "ooInetProxyType")
    {
        std::map<OUString, css::beans::Optional<css::uno::Any>>::iterator it
            = m_KDESettings.find(PropertyName);
        if (it != m_KDESettings.end())
            return css::uno::Any(it->second);
        else
            return css::uno::Any(css::beans::Optional<css::uno::Any>());
    }
    else if (PropertyName == "givenname" || PropertyName == "sn"
             || PropertyName == "TemplatePathVariable")
    {
        return css::uno::Any(css::beans::Optional<css::uno::Any>());
        //TODO: obtain values from KDE?
    }
    throw css::beans::UnknownPropertyException(PropertyName, getXWeak());
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
shell_kf5desktop_get_implementation(css::uno::XComponentContext*,
                                    css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Service());
}
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
