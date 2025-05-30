'
' This file is part of the LibreOffice project.
'
' This Source Code Form is subject to the terms of the Mozilla Public
' License, v. 2.0. If a copy of the MPL was not distributed with this
' file, You can obtain one at http://mozilla.org/MPL/2.0/.
'

Option Explicit

Function doUnitTest as String
    dim aDate as Date
    aDate = Time$() ' Use string variant of the function, to limit precision to seconds
    ' CDateToUnoTime CDateFromUnoTime
    If ( CDateFromUnoTime( CDateToUnoTime( aDate ) ) <> aDate ) Then
        doUnitTest = "FAIL"
    Else
        doUnitTest = "OK"
    End If
End Function
