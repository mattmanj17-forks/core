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

#include <string_view>

#include <curl/curl.h>

#include <systools/curlinit.hxx>

#include <o3tl/string_view.hxx>
#include <osl/diagnose.h>
#include <osl/file.h>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>

#include "download.hxx"

namespace beans = css::beans ;
namespace container = css::container ;
namespace lang = css::lang ;
namespace uno = css::uno ;

namespace {

struct OutData
{
    rtl::Reference< DownloadInteractionHandler >Handler;
    OUString   File;
    OUString   DestinationDir;
    oslFileHandle   FileHandle;
    sal_uInt64      Offset;
    osl::Condition& StopCondition;
    CURL *curl;

    explicit OutData(osl::Condition& rCondition) : FileHandle(nullptr), Offset(0), StopCondition(rCondition), curl(nullptr) {};
};

}

static void openFile( OutData& out )
{
    char * effective_url;
    curl_easy_getinfo(out.curl, CURLINFO_EFFECTIVE_URL, &effective_url);

    curl_off_t nDownloadSize;
    curl_easy_getinfo(out.curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &nDownloadSize);

    OString aURL(effective_url);

    // ensure no trailing '/'
    sal_Int32 nLen = aURL.getLength();
    while( (nLen > 0) && ('/' == aURL[nLen-1]) )
        aURL = aURL.copy(0, --nLen);

    // extract file name last '/'
    sal_Int32 nIndex = aURL.lastIndexOf('/');
    if( nIndex > 0 )
    {
        out.File = out.DestinationDir
            + OStringToOUString(aURL.subView(nIndex), RTL_TEXTENCODING_UTF8);

        oslFileError rc;

        // Give the user an overwrite warning if the target file exists
        const sal_Int32 openFlags = osl_File_OpenFlag_Write | osl_File_OpenFlag_Create;
        do
        {
            rc = osl_openFile(out.File.pData, &out.FileHandle, openFlags);

            if( osl_File_E_EXIST == rc && ! out.Handler->downloadTargetExists(out.File) )
            {
                out.StopCondition.set();
                break;
            }

        } while( osl_File_E_EXIST == rc );

        if( osl_File_E_None == rc )
            out.Handler->downloadStarted(out.File, static_cast<sal_Int64>(nDownloadSize));
    }
}


static OString
getStringValue(const uno::Reference< container::XNameAccess >& xNameAccess, const OUString& aName)
{
    OSL_ASSERT(xNameAccess->hasByName(aName));
    uno::Any aValue = xNameAccess->getByName(aName);

    return OUStringToOString(aValue.get<OUString>(), RTL_TEXTENCODING_UTF8);
}


static sal_Int32
getInt32Value(const uno::Reference< container::XNameAccess >& xNameAccess,
                    const OUString& aName)
{
    OSL_ASSERT(xNameAccess->hasByName(aName));
    uno::Any aValue = xNameAccess->getByName(aName);

    sal_Int32 n = -1;
    aValue >>= n;
    return n;
}


static size_t
write_function( void *ptr, size_t size, size_t nmemb, void *stream )
{
    OutData *out = static_cast < OutData * > (stream);

    if( nullptr == out->FileHandle )
        openFile(*out);

    sal_uInt64 nBytesWritten = 0;

    if( nullptr != out->FileHandle )
        osl_writeFile(out->FileHandle, ptr, size * nmemb, &nBytesWritten);

    return static_cast<size_t>(nBytesWritten);
}


static int
progress_callback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, SAL_UNUSED_PARAMETER curl_off_t, SAL_UNUSED_PARAMETER curl_off_t)
{
    OutData *out = static_cast < OutData * > (clientp);

    assert(out);

    if (out && !out->StopCondition.check())
    {
        float fPercent = 0;
        if ( dltotal + out->Offset )
            fPercent = (dlnow + out->Offset) * 100 / (dltotal + out->Offset);
        if( fPercent < 0 )
            fPercent = 0;

        // Do not report progress for redirection replies
        long nCode;
        curl_easy_getinfo(out->curl, CURLINFO_RESPONSE_CODE, &nCode);
        if( (nCode != 302) && (nCode != 303) && (dltotal > 0) )
            out->Handler->downloadProgressAt(static_cast<sal_Int8>(fPercent));

        return 0;
    }

    // If stop condition is set, return non 0 value to abort
    return -1;
}


void
Download::getProxyForURL(std::u16string_view rURL, OString& rHost, sal_Int32& rPort) const
{
    uno::Reference< lang::XMultiServiceFactory > xConfigProvider(
        css::configuration::theDefaultProvider::get( m_xContext ) );

    beans::PropertyValue aProperty;
    aProperty.Name  = "nodepath";
    aProperty.Value <<= u"org.openoffice.Inet/Settings"_ustr;

    uno::Sequence< uno::Any > aArgumentList{ uno::Any(aProperty) };

    uno::Reference< container::XNameAccess > xNameAccess(
        xConfigProvider->createInstanceWithArguments(
            u"com.sun.star.configuration.ConfigurationAccess"_ustr, aArgumentList ),
        uno::UNO_QUERY_THROW );

    OSL_ASSERT(xNameAccess->hasByName(u"ooInetProxyType"_ustr));
    uno::Any aValue = xNameAccess->getByName(u"ooInetProxyType"_ustr);

    sal_Int32 nProxyType = aValue.get< sal_Int32 >();
    if( 0 != nProxyType ) // type 0 means "direct connection to the internet
    {
        if( o3tl::starts_with(rURL, u"http:") )
        {
            rHost = getStringValue(xNameAccess, u"ooInetHTTPProxyName"_ustr);
            rPort = getInt32Value(xNameAccess, u"ooInetHTTPProxyPort"_ustr);
        }
        else if( o3tl::starts_with(rURL, u"https:") )
        {
            rHost = getStringValue(xNameAccess, u"ooInetHTTPSProxyName"_ustr);
            rPort = getInt32Value(xNameAccess, u"ooInetHTTPSProxyPort"_ustr);
        }
    }
}


static bool curl_run(std::u16string_view rURL, OutData& out, const OString& aProxyHost, sal_Int32 nProxyPort)
{
    /* Need to investigate further whether it is necessary to call
     * curl_global_init or not - leave it for now (as the ftp UCB content
     * provider does as well).
     */

    CURL * pCURL = curl_easy_init();
    bool ret = false;

    if( nullptr != pCURL )
    {
        ::InitCurl_easy(pCURL);

        out.curl = pCURL;

        OString aURL(OUStringToOString(rURL, RTL_TEXTENCODING_UTF8));
        (void)curl_easy_setopt(pCURL, CURLOPT_URL, aURL.getStr());

        // abort on http errors
        (void)curl_easy_setopt(pCURL, CURLOPT_FAILONERROR, 1);

        // enable redirection
        (void)curl_easy_setopt(pCURL, CURLOPT_FOLLOWLOCATION, 1);
        // only allow redirect to https://
#if (LIBCURL_VERSION_MAJOR > 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 85)
        (void)curl_easy_setopt(pCURL, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
        (void)curl_easy_setopt(pCURL, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif

        // write function
        (void)curl_easy_setopt(pCURL, CURLOPT_WRITEDATA, &out);
        (void)curl_easy_setopt(pCURL, CURLOPT_WRITEFUNCTION, &write_function);

        // progress handler - Condition::check unfortunately is not defined const
        (void)curl_easy_setopt(pCURL, CURLOPT_NOPROGRESS, 0);
        (void)curl_easy_setopt(pCURL, CURLOPT_XFERINFOFUNCTION, &progress_callback);
        (void)curl_easy_setopt(pCURL, CURLOPT_PROGRESSDATA, &out);

        // proxy
        (void)curl_easy_setopt(pCURL, CURLOPT_PROXY, aProxyHost.getStr());
        (void)curl_easy_setopt(pCURL, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
        if( -1 != nProxyPort )
            (void)curl_easy_setopt(pCURL, CURLOPT_PROXYPORT, nProxyPort);

        if( out.Offset > 0 )
        {
            // curl_off_t offset = nOffset; libcurl seems to be compiled with large
            // file support (and we not) ..
            sal_Int64 offset = static_cast<sal_Int64>(out.Offset);
            (void)curl_easy_setopt(pCURL, CURLOPT_RESUME_FROM_LARGE, offset);
        }

        CURLcode cc = curl_easy_perform(pCURL);

        // treat zero byte downloads as errors
        if( nullptr == out.FileHandle )
            openFile(out);

        if( CURLE_OK == cc )
        {
            out.Handler->downloadFinished(out.File);
            ret = true;
        }

        if ( CURLE_PARTIAL_FILE  == cc )
        {
            // this sometimes happens, when a user throws away his user data, but has already
            // completed the download of an update.
            curl_off_t nDownloadSize;
            curl_easy_getinfo( pCURL, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &nDownloadSize );
            if ( -1 == nDownloadSize )
            {
                out.Handler->downloadFinished(out.File);
                ret = true;
            }
        }

        // Avoid target file being removed
        else if( (CURLE_ABORTED_BY_CALLBACK == cc) || out.StopCondition.check() )
            ret = true;

        // Only report errors when not stopped
        else
        {
            OString aMessage("Unknown error"_ostr);

            const char * error_message = curl_easy_strerror(cc);
            if( nullptr != error_message )
                aMessage = error_message;

            if ( CURLE_HTTP_RETURNED_ERROR == cc )
            {
                long nError;
                curl_easy_getinfo( pCURL, CURLINFO_RESPONSE_CODE, &nError );

                if ( 403 == nError )
                    aMessage += " 403: Access denied!";
                else if ( 404 == nError )
                    aMessage += " 404: File not found!";
                else if ( 416 == nError )
                {
                    // we got this error probably, because we already downloaded the file
                    out.Handler->downloadFinished(out.File);
                    ret = true;
                }
                else
                {
                    aMessage += ":error code = " + OString::number( nError ) + " !";
                }
            }
            if ( !ret )
                out.Handler->downloadStalled( OStringToOUString(aMessage, RTL_TEXTENCODING_UTF8) );
        }

        curl_easy_cleanup(pCURL);
    }

    return ret;
}


bool
Download::start(const OUString& rURL, const OUString& rFile, const OUString& rDestinationDir)
{
    OSL_ASSERT( m_aHandler.is() );

    OutData out(m_aCondition);
    OUString aFile( rFile );

    // when rFile is empty, there is no remembered file name. If there is already a file with the
    // same name ask the user if she wants to resume a download or restart the download
    if ( aFile.isEmpty() )
    {
        // GetFileName()
        OUString aURL( rURL );
        // ensure no trailing '/'
        sal_Int32 nLen = aURL.getLength();
        while( (nLen > 0) && ('/' == aURL[ nLen-1 ]) )
            aURL = aURL.copy( 0, --nLen );

        // extract file name last '/'
        sal_Int32 nIndex = aURL.lastIndexOf('/');
        aFile = rDestinationDir + aURL.subView( nIndex );

        // check for existing file
        oslFileError rc = osl_openFile( aFile.pData, &out.FileHandle, osl_File_OpenFlag_Write | osl_File_OpenFlag_Create );
        osl_closeFile(out.FileHandle);
        out.FileHandle = nullptr;

        if( osl_File_E_EXIST == rc )
        {
            if ( m_aHandler->checkDownloadDestination( aURL.copy( nIndex+1 ) ) )
            {
                osl_removeFile( aFile.pData );
                aFile.clear();
            }
            else
                m_aHandler->downloadStarted( aFile, 0 );
        }
        else
        {
            osl_removeFile( aFile.pData );
            aFile.clear();
        }
    }

    out.File = aFile;
    out.DestinationDir = rDestinationDir;
    out.Handler = m_aHandler;

    if( !aFile.isEmpty() )
    {
        oslFileError rc = osl_openFile(aFile.pData, &out.FileHandle, osl_File_OpenFlag_Write);

        if( osl_File_E_None == rc )
        {
            // Set file pointer to the end of the file on resume
            if( osl_File_E_None == osl_setFilePos(out.FileHandle, osl_Pos_End, 0) )
            {
                osl_getFilePos(out.FileHandle, &out.Offset);
            }
        }
        else if( osl_File_E_NOENT == rc ) // file has been deleted meanwhile ..
            out.File.clear();
    }

    OString aProxyHost;
    sal_Int32    nProxyPort = -1;
    getProxyForURL(rURL, aProxyHost, nProxyPort);

    bool ret = curl_run(rURL, out, aProxyHost, nProxyPort);

    if( nullptr != out.FileHandle )
    {
        osl_syncFile(out.FileHandle);
        osl_closeFile(out.FileHandle);

// #i90930# Don't remove already downloaded bits, when curl_run reports an error
// because later calls might be successful
//        if( ! ret )
//            osl_removeFile(out.File.pData);
    }

    m_aCondition.reset();
    return ret;
}


void
Download::stop()
{
    m_aCondition.set();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
