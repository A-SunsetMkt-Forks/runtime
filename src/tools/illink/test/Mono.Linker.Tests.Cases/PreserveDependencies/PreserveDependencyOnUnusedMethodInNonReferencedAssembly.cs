﻿using System.Runtime.CompilerServices;
using Mono.Linker.Tests.Cases.Expectations.Assertions;
using Mono.Linker.Tests.Cases.Expectations.Metadata;
using Mono.Linker.Tests.Cases.PreserveDependencies.Dependencies;

namespace Mono.Linker.Tests.Cases.PreserveDependencies
{
    [SetupCompileBefore("FakeSystemAssembly.dll", new[] { "Dependencies/PreserveDependencyAttribute.cs" })]
    [SetupCompileBefore("base.dll", new[] { "Dependencies/PreserveDependencyMethodInNonReferencedAssemblyBase.cs" })]
    [SetupCompileBefore("library.dll", new[] { "Dependencies/PreserveDependencyMethodInNonReferencedAssemblyLibrary.cs" }, references: new[] { "base.dll" }, addAsReference: false)]
    [KeptAssembly("base.dll")]
    [RemovedAssembly("library.dll")]
    [KeptMemberInAssembly("base.dll", typeof(PreserveDependencyMethodInNonReferencedAssemblyBase), "Method()")]
    public class PreserveDependencyOnUnusedMethodInNonReferencedAssembly
    {
        public static void Main()
        {
            var obj = new Foo();
            var val = obj.Method();
        }

        [PreserveDependency(".ctor()", "Mono.Linker.Tests.Cases.PreserveDependencies.Dependencies.PreserveDependencyMethodInNonReferencedAssemblyLibrary", "library")]
        static void Dependency()
        {
        }

        [Kept]
        [KeptMember(".ctor()")]
        [KeptBaseType(typeof(PreserveDependencyMethodInNonReferencedAssemblyBase))]
        class Foo : PreserveDependencyMethodInNonReferencedAssemblyBase
        {
            [Kept]
            public override string Method()
            {
                return "Foo";
            }
        }
    }
}
