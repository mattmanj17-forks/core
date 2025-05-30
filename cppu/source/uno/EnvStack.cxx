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

#include <uno/environment.hxx>
#include <uno/lbnames.h>

#include <cppu/EnvDcp.hxx>
#include <cppu/Enterable.hxx>

#include <osl/thread.h>
#include <osl/thread.hxx>
#include <o3tl/string_view.hxx>

#include <mutex>
#include <unordered_map>

using namespace com::sun::star;

namespace {

struct oslThreadIdentifier_equal
{
    bool operator()(oslThreadIdentifier s1, oslThreadIdentifier s2) const;
};

}

bool oslThreadIdentifier_equal::operator()(oslThreadIdentifier s1, oslThreadIdentifier s2) const
{
    bool result = s1 == s2;

    return result;
}

namespace {

struct oslThreadIdentifier_hash
{
    size_t operator()(oslThreadIdentifier s1) const;
};

}

size_t oslThreadIdentifier_hash::operator()(oslThreadIdentifier s1) const
{
    return s1;
}

typedef std::unordered_map<oslThreadIdentifier,
                        uno_Environment *,
                        oslThreadIdentifier_hash,
                        oslThreadIdentifier_equal>  ThreadMap;

namespace
{
    std::mutex s_threadMap_mutex;
    ThreadMap s_threadMap;
}

static void s_setCurrent(uno_Environment * pEnv)
{
    oslThreadIdentifier threadId = osl::Thread::getCurrentIdentifier();

    std::scoped_lock guard(s_threadMap_mutex);
    if (pEnv)
    {
        s_threadMap[threadId] = pEnv;
    }
    else
    {
        ThreadMap::iterator iEnv = s_threadMap.find(threadId);
        if( iEnv != s_threadMap.end())
            s_threadMap.erase(iEnv);
    }
}

static uno_Environment * s_getCurrent()
{
    uno_Environment * pEnv = nullptr;

    oslThreadIdentifier threadId = osl::Thread::getCurrentIdentifier();

    std::scoped_lock guard(s_threadMap_mutex);
    ThreadMap::iterator iEnv = s_threadMap.find(threadId);
    if(iEnv != s_threadMap.end())
        pEnv = iEnv->second;

    return pEnv;
}


extern "C" void SAL_CALL uno_getCurrentEnvironment(uno_Environment ** ppEnv, rtl_uString * pTypeName) noexcept
{
    if (*ppEnv)
    {
        (*ppEnv)->release(*ppEnv);
        *ppEnv = nullptr;
    }

    OUString currPurpose;

    uno_Environment * pCurrEnv = s_getCurrent();
    if (pCurrEnv) // no environment means no purpose
        currPurpose = cppu::EnvDcp::getPurpose(pCurrEnv->pTypeName);

    if (pTypeName && rtl_uString_getLength(pTypeName))
    {
        OUString envDcp = OUString::unacquired(&pTypeName) + currPurpose;

        uno_getEnvironment(ppEnv, envDcp.pData, nullptr);
    }
    else
    {
        if (pCurrEnv)
        {
            *ppEnv = pCurrEnv;
            (*ppEnv)->acquire(*ppEnv);
        }
        else
        {
            OUString uno_envDcp(u"" UNO_LB_UNO ""_ustr);
            uno_getEnvironment(ppEnv, uno_envDcp.pData, nullptr);
        }
    }
}

static OUString s_getPrefix(std::u16string_view str1, std::u16string_view str2)
{
    sal_Int32 nIndex1 = 0;
    sal_Int32 nIndex2 = 0;
    sal_Int32 sim = 0;

    std::u16string_view token1;
    std::u16string_view token2;

    do
    {
        token1 = o3tl::getToken(str1, 0, ':', nIndex1);
        token2 = o3tl::getToken(str2, 0, ':', nIndex2);

        if (token1 == token2)
            sim += token1.size() + 1;
    }
    while(nIndex1 == nIndex2 && nIndex1 >= 0 && token1 == token2);

    OUString result;

    if (sim)
        result = str1.substr(0, sim - 1);

    return result;
}

static int s_getNextEnv(uno_Environment ** ppEnv, uno_Environment * pCurrEnv, uno_Environment * pTargetEnv)
{
    int res = 0;

    std::u16string_view nextPurpose;

    OUString currPurpose;
    if (pCurrEnv)
        currPurpose = cppu::EnvDcp::getPurpose(pCurrEnv->pTypeName);

    OUString targetPurpose;
    if (pTargetEnv)
        targetPurpose = cppu::EnvDcp::getPurpose(pTargetEnv->pTypeName);

    OUString intermPurpose(s_getPrefix(currPurpose, targetPurpose));
    if (currPurpose.getLength() > intermPurpose.getLength())
    {
        sal_Int32 idx = currPurpose.lastIndexOf(':');
        nextPurpose = currPurpose.subView(0, idx);

        res = -1;
    }
    else if (intermPurpose.getLength() < targetPurpose.getLength())
    {
        sal_Int32 idx = targetPurpose.indexOf(':', intermPurpose.getLength() + 1);
        if (idx == -1)
            nextPurpose = targetPurpose;

        else
            nextPurpose = targetPurpose.subView(0, idx);

        res = 1;
    }

    if (!nextPurpose.empty())
    {
        OUString next_envDcp = OUString::Concat(UNO_LB_UNO) + nextPurpose;
        uno_getEnvironment(ppEnv, next_envDcp.pData, nullptr);
    }
    else
    {
        if (*ppEnv)
            (*ppEnv)->release(*ppEnv);

        *ppEnv = nullptr;
    }

    return res;
}

extern "C" { static void s_pull(va_list * pParam)
{
    uno_EnvCallee * pCallee = va_arg(*pParam, uno_EnvCallee *);
    va_list       * pXparam = va_arg(*pParam, va_list *);

    pCallee(pXparam);
}}

static void s_callInto_v(uno_Environment * pEnv, uno_EnvCallee * pCallee, va_list * pParam)
{
    cppu::Enterable * pEnterable = static_cast<cppu::Enterable *>(pEnv->pReserved);
    if (pEnterable)
        pEnterable->callInto(s_pull, pCallee, pParam);

    else
        pCallee(pParam);
}

static void s_callInto(uno_Environment * pEnv, uno_EnvCallee * pCallee, ...)
{
    va_list param;

    va_start(param, pCallee);
    s_callInto_v(pEnv, pCallee, &param);
    va_end(param);
}

static void s_callOut_v(uno_Environment * pEnv, uno_EnvCallee * pCallee, va_list * pParam)
{
    cppu::Enterable * pEnterable = static_cast<cppu::Enterable *>(pEnv->pReserved);
    if (pEnterable)
        pEnterable->callOut_v(pCallee, pParam);

    else
        pCallee(pParam);
}

static void s_callOut(uno_Environment * pEnv, uno_EnvCallee * pCallee, ...)
{
    va_list param;

    va_start(param, pCallee);
    s_callOut_v(pEnv, pCallee, &param);
    va_end(param);
}

static void s_environment_invoke_v(uno_Environment *, uno_Environment *, uno_EnvCallee *, va_list *);

extern "C" { static void s_environment_invoke_vv(va_list * pParam)
{
    uno_Environment * pCurrEnv    = va_arg(*pParam, uno_Environment *);
    uno_Environment * pTargetEnv  = va_arg(*pParam, uno_Environment *);
    uno_EnvCallee   * pCallee     = va_arg(*pParam, uno_EnvCallee *);
    va_list         * pXparam     = va_arg(*pParam, va_list *);

    s_environment_invoke_v(pCurrEnv, pTargetEnv, pCallee, pXparam);
}}

static void s_environment_invoke_v(uno_Environment * pCurrEnv, uno_Environment * pTargetEnv, uno_EnvCallee * pCallee, va_list * pParam)
{
    uno_Environment * pNextEnv = nullptr;
    switch(s_getNextEnv(&pNextEnv, pCurrEnv, pTargetEnv))
    {
    case -1:
        s_setCurrent(pNextEnv);
        s_callOut(pCurrEnv, s_environment_invoke_vv, pNextEnv, pTargetEnv, pCallee, pParam);
        s_setCurrent(pCurrEnv);
        break;

    case 0: {
        uno_Environment * hld = s_getCurrent();
        s_setCurrent(pCurrEnv);
        pCallee(pParam);
        s_setCurrent(hld);
    }
        break;

    case 1:
        s_setCurrent(pNextEnv);
        s_callInto(pNextEnv, s_environment_invoke_vv, pNextEnv, pTargetEnv, pCallee, pParam);
        s_setCurrent(pCurrEnv);
        break;
    }

    if (pNextEnv)
        pNextEnv->release(pNextEnv);
}

extern "C" void SAL_CALL uno_Environment_invoke_v(uno_Environment * pTargetEnv, uno_EnvCallee * pCallee, va_list * pParam) noexcept
{
    s_environment_invoke_v(s_getCurrent(), pTargetEnv, pCallee, pParam);
}

extern "C" void SAL_CALL uno_Environment_invoke(uno_Environment * pEnv, uno_EnvCallee * pCallee, ...) noexcept
{
    va_list param;

    va_start(param, pCallee);
    uno_Environment_invoke_v(pEnv, pCallee, &param);
    va_end(param);
}

extern "C" void SAL_CALL uno_Environment_enter(uno_Environment * pTargetEnv) noexcept
{
    uno_Environment * pNextEnv = nullptr;
    uno_Environment * pCurrEnv = s_getCurrent();

    int res;
    while ( (res = s_getNextEnv(&pNextEnv, pCurrEnv, pTargetEnv)) != 0)
    {
        cppu::Enterable * pEnterable;

        switch(res)
        {
        case -1:
            pEnterable = static_cast<cppu::Enterable *>(pCurrEnv->pReserved);
            if (pEnterable)
                pEnterable->leave();
            pCurrEnv->release(pCurrEnv);
            break;

        case 1:
            pNextEnv->acquire(pNextEnv);
            pEnterable = static_cast<cppu::Enterable *>(pNextEnv->pReserved);
            if (pEnterable)
                pEnterable->enter();
            break;
        }

        s_setCurrent(pNextEnv);
        pCurrEnv = pNextEnv;
    }
}

int SAL_CALL uno_Environment_isValid(uno_Environment * pEnv, rtl_uString ** pReason) noexcept
{
    int result = 1;

    OUString typeName(cppu::EnvDcp::getTypeName(pEnv->pTypeName));
    if (typeName == UNO_LB_UNO)
    {
        cppu::Enterable * pEnterable = static_cast<cppu::Enterable *>(pEnv->pReserved);
        if (pEnterable)
            result = pEnterable->isValid(reinterpret_cast<OUString *>(pReason));
    }
    else
    {
        OUString envDcp = UNO_LB_UNO + cppu::EnvDcp::getPurpose(pEnv->pTypeName);

        uno::Environment env(envDcp);

        result = env.isValid(reinterpret_cast<OUString *>(pReason));
    }

    return result;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
