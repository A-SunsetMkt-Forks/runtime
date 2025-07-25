// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using ILCompiler.Dataflow;
using ILLink.Shared.DataFlow;
using ILLink.Shared.TypeSystemProxy;
using Internal.TypeSystem;

#nullable enable

namespace ILLink.Shared.TrimAnalysis
{
    /// <summary>
    /// Return value from a method
    /// </summary>
    internal partial record MethodReturnValue
    {
        public MethodReturnValue(MethodDesc method, bool isNewObj, DynamicallyAccessedMemberTypes dynamicallyAccessedMemberTypes)
        {
            Debug.Assert(!isNewObj || method.IsConstructor, "isNewObj can only be true for constructors");
            StaticType = isNewObj ? method.OwningType : method.Signature.ReturnType;
            MethodDesc = method;
            Method = new MethodProxy(method);
            DynamicallyAccessedMemberTypes = dynamicallyAccessedMemberTypes;
        }

        public readonly MethodDesc MethodDesc;

        public override DynamicallyAccessedMemberTypes DynamicallyAccessedMemberTypes { get; }

        public override IEnumerable<string> GetDiagnosticArgumentsForAnnotationMismatch()
            => new string[] { DiagnosticUtilities.GetMethodSignatureDisplayName(MethodDesc) };

        public override SingleValue DeepCopy() => this; // This value is immutable

        public override string ToString() => this.ValueToString(MethodDesc, DynamicallyAccessedMemberTypes);
    }
}
