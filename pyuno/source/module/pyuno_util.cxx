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

#include "pyuno_impl.hxx"

#include <osl/thread.h>
#include <osl/thread.hxx>

#include <rtl/ustrbuf.hxx>
#include <osl/time.h>

using com::sun::star::uno::Sequence;
using com::sun::star::uno::Any;
using com::sun::star::uno::RuntimeException;

namespace pyuno
{
PyRef ustring2PyUnicode( const OUString & str )
{
    PyRef ret;
#if Py_UNICODE_SIZE == 2
#ifdef MACOSX
    ret = PyRef( PyUnicode_FromWideChar( reinterpret_cast<const unsigned short *>(str.getStr()), str.getLength() ), SAL_NO_ACQUIRE );
#else
    static_assert(sizeof (wchar_t) == Py_UNICODE_SIZE, "bad assumption");
    ret = PyRef( PyUnicode_FromWideChar( reinterpret_cast<wchar_t const *>(str.getStr()), str.getLength() ), SAL_NO_ACQUIRE );
#endif
#else
    OString sUtf8(OUStringToOString(str, RTL_TEXTENCODING_UTF8));
    ret = PyRef( PyUnicode_DecodeUTF8( sUtf8.getStr(), sUtf8.getLength(), nullptr) , SAL_NO_ACQUIRE );
#endif
    return ret;
}

PyRef ustring2PyString( std::u16string_view str )
{
    OString o = OUStringToOString( str, osl_getThreadTextEncoding() );
    return PyRef( PyUnicode_FromString( o.getStr() ), SAL_NO_ACQUIRE );
}

OUString pyString2ustring( PyObject *pystr )
{
    OUString ret;
    if( PyUnicode_Check( pystr ) )
    {
        Py_ssize_t size(0);
#if Py_UNICODE_SIZE == 2
        ret = OUString(
            reinterpret_cast<sal_Unicode const *>(PyUnicode_AsWideCharString( pystr, &size )) );
#else
        char const *pUtf8(PyUnicode_AsUTF8AndSize(pystr, &size));
        ret = OUString(pUtf8, size, RTL_TEXTENCODING_UTF8);
#endif
    }
    else
    {
        char *name = PyBytes_AsString(pystr); // hmmm... is this a good idea?
        ret = OUString( name, strlen(name), osl_getThreadTextEncoding() );
    }
    return ret;
}

PyRef getObjectFromUnoModule( const Runtime &runtime, const char * func )
{
    PyRef object(PyDict_GetItemString( runtime.getImpl()->cargo->getUnoModule().get(), func ) );
    if( !object.is() )
    {
        throw RuntimeException("couldn't find core function " + OUString::createFromAscii(func));
    }
    return object;
}


// Logging


bool isLog( RuntimeCargo const * cargo, sal_Int32 loglevel )
{
    return cargo && cargo->logFile && loglevel <= cargo->logLevel;
}

void log( RuntimeCargo * cargo, sal_Int32 level, std::u16string_view logString )
{
    log( cargo, level, OUStringToOString( logString, osl_getThreadTextEncoding() ).getStr() );
}

void log( RuntimeCargo * cargo, sal_Int32 level, const char *str )
{
    if( !isLog( cargo, level ) )
        return;

    static const char* const strLevel[] = { "NONE", "CALL", "ARGS" };

    TimeValue systemTime;
    TimeValue localTime;
    oslDateTime localDateTime;

    osl_getSystemTime( &systemTime );
    osl_getLocalTimeFromSystemTime( &systemTime, &localTime );
    osl_getDateTimeFromTimeValue( &localTime, &localDateTime );

    fprintf( cargo->logFile,
             "%4i-%02i-%02i %02i:%02i:%02i,%03lu [%s,tid %ld]: %s\n",
             localDateTime.Year,
             localDateTime.Month,
             localDateTime.Day,
             localDateTime.Hours,
             localDateTime.Minutes,
             localDateTime.Seconds,
             sal::static_int_cast< unsigned long >(
                 localDateTime.NanoSeconds/1000000),
             strLevel[level],
             sal::static_int_cast< long >(
                 static_cast<sal_Int32>(osl::Thread::getCurrentIdentifier())),
             str );
}

namespace {

void appendPointer(OUStringBuffer & buffer, void * pointer) {
    buffer.append(
        sal::static_int_cast< sal_Int64 >(
            reinterpret_cast< sal_IntPtr >(pointer)),
        16);
}

}

void logException( RuntimeCargo *cargo, const char *intro,
                   void * ptr, std::u16string_view aFunctionName,
                   const void * data, const css::uno::Type & type )
{
    if( isLog( cargo, LogLevel::CALL ) )
    {
        OUStringBuffer buf( 128 );
        buf.appendAscii( intro );
        appendPointer(buf, ptr);
        buf.append( OUString::Concat("].") + aFunctionName + " = " );
        buf.append(
            val2str( data, type.getTypeLibType(), VAL2STR_MODE_SHALLOW ) );
        log( cargo,LogLevel::CALL, buf );
    }

}

void logReply(
    RuntimeCargo *cargo,
    const char *intro,
    void * ptr,
    std::u16string_view aFunctionName,
    const Any &returnValue,
    const Sequence< Any > & aParams )
{
    OUStringBuffer buf( 128 );
    buf.appendAscii( intro );
    appendPointer(buf, ptr);
    buf.append( OUString::Concat("].") + aFunctionName + "()=" );
    if( isLog( cargo, LogLevel::ARGS ) )
    {
        buf.append(
            val2str( returnValue.getValue(), returnValue.getValueTypeRef(), VAL2STR_MODE_SHALLOW) );
        for( const auto & p : aParams )
        {
            buf.append( ", " + val2str( p.getValue(), p.getValueTypeRef(), VAL2STR_MODE_SHALLOW) );
        }
    }
    log( cargo,LogLevel::CALL, buf );

}

void logCall( RuntimeCargo *cargo, const char *intro,
              void * ptr, std::u16string_view aFunctionName,
              const Sequence< Any > & aParams )
{
    OUStringBuffer buf( 128 );
    buf.appendAscii( intro );
    appendPointer(buf, ptr);
    buf.append( OUString::Concat("].") + aFunctionName + "(" );
    if( isLog( cargo, LogLevel::ARGS ) )
    {
        for( int i = 0; i < aParams.getLength() ; i ++ )
        {
            if( i > 0 )
                buf.append( ", " );
            buf.append(
                val2str( aParams[i].getValue(), aParams[i].getValueTypeRef(), VAL2STR_MODE_SHALLOW) );
        }
    }
    buf.append( ")" );
    log( cargo,LogLevel::CALL, buf );
}


}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
