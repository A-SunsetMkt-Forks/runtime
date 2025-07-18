// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;
using ComTypes = System.Runtime.InteropServices.ComTypes;

namespace Microsoft.CSharp.RuntimeBinder.ComInterop
{
    /// <summary>
    /// This is similar to ComTypes.EXCEPINFO, but lets us do our own custom marshaling
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct ExcepInfo
    {
        private short wCode;
        private short wReserved;
        private IntPtr bstrSource;
        private IntPtr bstrDescription;
        private IntPtr bstrHelpFile;
        private int dwHelpContext;
        private IntPtr pvReserved;
        private IntPtr pfnDeferredFillIn;
        private int scode;

#if DEBUG
        static ExcepInfo()
        {
            Debug.Assert(Marshal.SizeOf<ExcepInfo>() == Marshal.SizeOf<ComTypes.EXCEPINFO>());
        }
#endif

        private static string? ConvertAndFreeBstr(ref IntPtr bstr)
        {
            if (bstr == IntPtr.Zero)
            {
                return null;
            }

            string result = Marshal.PtrToStringBSTR(bstr);
            Marshal.FreeBSTR(bstr);
            bstr = IntPtr.Zero;
            return result;
        }

        [RequiresUnreferencedCode(Binder.TrimmerWarning)]
        internal Exception GetException()
        {
            Debug.Assert(pfnDeferredFillIn == IntPtr.Zero);
#if DEBUG
            System.Diagnostics.Debug.Assert(wReserved != -1);
            wReserved = -1; // to ensure that the method gets called only once
#endif

            // If the scode is zero, we use the wCode. The wCode is a legacy of
            // 16-bit Windows error codes. This means it will never be an error
            // scode (HRESULT, < 0) and we will get a null exception.
            int errorCode = (scode != 0) ? scode : wCode;

            // If the error code doesn't resolve to an exception, we create a
            // generic COMException with the error code and no message.
            Exception exception = Marshal.GetExceptionForHR(errorCode) ?? new COMException(null, errorCode);

            string? message = ConvertAndFreeBstr(ref bstrDescription);
            if (message is not null)
            {
                // If we have a custom message, create a new Exception object with the message set correctly.
                // We need to create a new object because "exception.Message" is a read-only property.
                if (exception is COMException)
                {
                    exception = new COMException(message, errorCode);
                }
                else
                {
                    Type exceptionType = exception.GetType();
                    ConstructorInfo ctor = exceptionType.GetConstructor(new Type[] { typeof(string) });
                    if (ctor is not null)
                    {
                        exception = (Exception)ctor.Invoke(new object[] { message });
                    }
                }
            }

            exception.Source = ConvertAndFreeBstr(ref bstrSource);

            string? helpLink = ConvertAndFreeBstr(ref bstrHelpFile);
            if (helpLink is not null && dwHelpContext != 0)
            {
                helpLink += "#" + dwHelpContext;
            }
            exception.HelpLink = helpLink;

            return exception;
        }
    }
}
