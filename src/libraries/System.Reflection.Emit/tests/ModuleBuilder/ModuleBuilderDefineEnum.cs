// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.IO;
using Xunit;

namespace System.Reflection.Emit.Tests
{
    public class ModuleBuilderDefineEnum
    {
        public static IEnumerable<object[]> DefineEnum_TestData()
        {
            yield return new object[] { "TestEnum", TypeAttributes.Public, typeof(byte) };
            yield return new object[] { "testenum", TypeAttributes.NotPublic, typeof(sbyte) };
            yield return new object[] { "enum", TypeAttributes.Public, typeof(short) };
            yield return new object[] { "\uD800\uDC00", TypeAttributes.Public, typeof(ushort) };
            yield return new object[] { "a\0b\0c", TypeAttributes.Public, typeof(int) };
            yield return new object[] { "Name", TypeAttributes.Public, typeof(uint) };
            yield return new object[] { "Name", TypeAttributes.Public, typeof(long) };
            yield return new object[] { "N%ame", TypeAttributes.Public, typeof(char) };
            yield return new object[] { "N`ame", TypeAttributes.Public, typeof(bool) };
            yield return new object[] { "N'ame", TypeAttributes.Public, typeof(ulong) };
            yield return new object[] { "Name", TypeAttributes.Public, typeof(float) };
            yield return new object[] { "Nam<e>", TypeAttributes.Public, typeof(double) };
            yield return new object[] { "Nam~e", TypeAttributes.Public, typeof(IntPtr) };
            yield return new object[] { "\rName", TypeAttributes.Public, typeof(UIntPtr) };
            yield return new object[] { "Name", TypeAttributes.Public, typeof(Int32Enum) };
        }

        [Theory]
        [ActiveIssue("https://github.com/dotnet/runtime/issues/2389", TestRuntimes.Mono)]
        [MemberData(nameof(DefineEnum_TestData))]
        public void DefineEnum(string name, TypeAttributes visibility, Type underlyingType)
        {
            ModuleBuilder module = Helpers.DynamicModule();
            EnumBuilder enumBuilder = module.DefineEnum(name, visibility, underlyingType);
            Assert.True(enumBuilder.IsEnum);

            Assert.Equal(module.Assembly, enumBuilder.Assembly);
            Assert.Equal(module, enumBuilder.Module);

            Assert.Equal(name, enumBuilder.Name);
            Assert.Equal(Helpers.GetFullName(name), enumBuilder.FullName);
            Assert.Equal(enumBuilder.FullName + ", " + module.Assembly.FullName, enumBuilder.AssemblyQualifiedName);

            Assert.Equal(typeof(Enum), enumBuilder.BaseType);
            Assert.Null(enumBuilder.DeclaringType);

            Assert.Equal(visibility | TypeAttributes.Sealed, enumBuilder.Attributes);

            Assert.Equal("value__", enumBuilder.UnderlyingField.Name);
            Assert.Equal(underlyingType, enumBuilder.UnderlyingField.FieldType);
            Assert.Equal(FieldAttributes.Public | FieldAttributes.SpecialName, enumBuilder.UnderlyingField.Attributes);

            // Verify we can create the Enum properly
            TypeInfo createdEnum = enumBuilder.CreateTypeInfo();
            Assert.True(createdEnum.IsEnum);

            Assert.Equal(module.Assembly.ToString(), createdEnum.Assembly.ToString());
            Assert.Equal(module.ToString(), createdEnum.Module.ToString());

            Assert.Equal(Helpers.GetFullName(name), createdEnum.Name);
            Assert.Equal(Helpers.GetFullName(name), enumBuilder.FullName);
            Assert.Equal(enumBuilder.FullName + ", " + module.Assembly.FullName, enumBuilder.AssemblyQualifiedName);

            Assert.Equal(typeof(Enum), createdEnum.BaseType);
            Assert.Null(createdEnum.DeclaringType);

            Assert.Equal(visibility | TypeAttributes.Sealed, createdEnum.Attributes);
            Type expectedUnderlyingType = underlyingType.GetTypeInfo().IsEnum ? Enum.GetUnderlyingType(underlyingType) : underlyingType;
            Assert.Equal(expectedUnderlyingType, Enum.GetUnderlyingType(createdEnum.AsType()));

            // There should be a field called "value__" created
            FieldInfo createdUnderlyingField = createdEnum.AsType().GetField("value__", Helpers.AllFlags);
            Assert.Equal(FieldAttributes.Public | FieldAttributes.SpecialName | FieldAttributes.RTSpecialName, createdUnderlyingField.Attributes);
        }

        [ConditionalTheory(typeof(PlatformDetection), nameof(PlatformDetection.HasAssemblyFiles))]
        [MemberData(nameof(DefineEnum_TestData))]
        public void DefineEnumPersistedAssembly(string name, TypeAttributes visibility, Type underlyingType)
        {
            PersistedAssemblyBuilder ab = AssemblySaveTools.PopulateAssemblyBuilder(new AssemblyName("MyAssembly"));
            ModuleBuilder module = ab.DefineDynamicModule("MyModule");
            EnumBuilder enumBuilder = module.DefineEnum(name, visibility, underlyingType);
            enumBuilder.CreateType();

            Assert.True(enumBuilder.IsEnum);
            Assert.Equal(module.Assembly, enumBuilder.Assembly);
            Assert.Equal(module, enumBuilder.Module);
            Assert.Equal(name, enumBuilder.Name);
            Assert.Equal(Helpers.GetFullName(name), enumBuilder.FullName);
            Assert.Equal(typeof(Enum), enumBuilder.BaseType);
            Assert.Null(enumBuilder.DeclaringType);
            Assert.Equal(visibility | TypeAttributes.Sealed, enumBuilder.Attributes);
            Assert.Equal("value__", enumBuilder.UnderlyingField.Name);
            Assert.Equal(underlyingType, enumBuilder.UnderlyingField.FieldType);
            Assert.Equal(FieldAttributes.Public | FieldAttributes.SpecialName, enumBuilder.UnderlyingField.Attributes);

            using (var stream = new MemoryStream())
            using (MetadataLoadContext mlc = new MetadataLoadContext(new CoreMetadataAssemblyResolver()))
            {
                ab.Save(stream);
                Assembly assemblyFromStream = mlc.LoadFromStream(stream);
                Type createdEnum = assemblyFromStream.GetType(name);
                if (createdEnum != null) // null when name = "a\0b\0c"
                {
                    Assert.True(createdEnum.IsEnum);
                    Assert.Equal(Helpers.GetFullName(name), createdEnum.Name);
                    Assert.Equal(Helpers.GetFullName(name), enumBuilder.FullName);
                    Assert.Equal(typeof(Enum).FullName, createdEnum.BaseType.FullName);
                    Assert.Null(createdEnum.DeclaringType);
                    Assert.Equal(visibility | TypeAttributes.Sealed, createdEnum.Attributes);
                }
            }
        }

        [Fact]
        [ActiveIssue("https://github.com/dotnet/runtime/issues/2389", TestRuntimes.Mono)]
        public void DefineEnum_DynamicUnderlyingType_Works()
        {
            ModuleBuilder module = Helpers.DynamicModule();
            EnumBuilder underlyingEnumTypeBuilder = module.DefineEnum("Enum1", TypeAttributes.Public, typeof(int));
            Type underlyingEnumType = underlyingEnumTypeBuilder.CreateType();

            EnumBuilder enumBuilder = module.DefineEnum("Enum2", TypeAttributes.Public, underlyingEnumType);
            Type enumType = enumBuilder.CreateType();

            Assert.Equal(typeof(int), Enum.GetUnderlyingType(enumType));
        }

        [Fact]
        public void DefineEnum_EnumWithSameNameExists_ThrowsArgumentException()
        {
            ModuleBuilder module = Helpers.DynamicModule();
            module.DefineEnum("Name", TypeAttributes.Public, typeof(int));
            AssertExtensions.Throws<ArgumentException>(null, () => module.DefineEnum("Name", TypeAttributes.Public, typeof(int)));
        }

        [Fact]
        public void DefineEnum_NullName_ThrowsArgumentNullException()
        {
            ModuleBuilder module = Helpers.DynamicModule();
            AssertExtensions.Throws<ArgumentNullException>("name", () => module.DefineEnum(null, TypeAttributes.Public, typeof(object)));
        }

        [Theory]
        [InlineData("")]
        [InlineData("\0")]
        [InlineData("\0abc")]
        public void DefineEnum_EmptyName_ThrowsArgumentNullException(string name)
        {
            ModuleBuilder module = Helpers.DynamicModule();
            AssertExtensions.Throws<ArgumentException>("name", () => module.DefineEnum(name, TypeAttributes.Public, typeof(object)));
        }

        [Theory]
        [InlineData((TypeAttributes)(-1), "name")]
        [InlineData(TypeAttributes.Abstract, "name")]
        [InlineData(TypeAttributes.AutoClass, "name")]
        [InlineData(TypeAttributes.BeforeFieldInit, "name")]
        [InlineData(TypeAttributes.ClassSemanticsMask, "name")]
        [InlineData(TypeAttributes.CustomFormatClass, "name")]
        [InlineData(TypeAttributes.CustomFormatMask, "name")]
        [InlineData(TypeAttributes.ExplicitLayout, "name")]
        [InlineData(TypeAttributes.HasSecurity, "name")]
        [InlineData(TypeAttributes.Import, "name")]
        [InlineData(TypeAttributes.LayoutMask, "name")]
        [InlineData(TypeAttributes.RTSpecialName, "name")]
        [InlineData(TypeAttributes.Sealed, "name")]
        [InlineData(TypeAttributes.SequentialLayout, "name")]
        [InlineData(TypeAttributes.Serializable, "name")]
        [InlineData(TypeAttributes.SpecialName, "name")]
        [InlineData(TypeAttributes.UnicodeClass, "name")]
        [InlineData(TypeAttributes.NestedAssembly, null)]
        [InlineData(TypeAttributes.NestedFamANDAssem, null)]
        [InlineData(TypeAttributes.NestedFamily, null)]
        [InlineData(TypeAttributes.NestedFamORAssem, null)]
        [InlineData(TypeAttributes.NestedPrivate, null)]
        [InlineData(TypeAttributes.NestedPublic, null)]
        public void DefineEnum_IncorrectVisibilityAttributes_ThrowsArgumentException(TypeAttributes visibility, string paramName)
        {
            ModuleBuilder module = Helpers.DynamicModule();
            AssertExtensions.Throws<ArgumentException>(paramName, () => module.DefineEnum("Enum", visibility, typeof(int)));
        }

        [Fact]
        public void DefineEnum_NullUnderlyingType_ThrowsArgumentNullException()
        {
            ModuleBuilder module = Helpers.DynamicModule();
            AssertExtensions.Throws<ArgumentNullException>("type", () => module.DefineEnum("Name", TypeAttributes.Public, null));
        }

        [Fact]
        public void DefineEnum_VoidUnderlyingType_ThrowsArgumentException()
        {
            ModuleBuilder module = Helpers.DynamicModule();
            AssertExtensions.Throws<ArgumentException>(null, () => module.DefineEnum("Name", TypeAttributes.Public, typeof(void)));
        }

        [Fact]
        public void DefineEnum_ByRefUnderlyingType_ThrowsTypeLoadExceptionOnCreation()
        {
            ModuleBuilder module = Helpers.DynamicModule();
            EnumBuilder enumBuilder = module.DefineEnum("Name", TypeAttributes.Public, typeof(int).MakeByRefType());
            Assert.Throws<TypeLoadException>(() => enumBuilder.CreateTypeInfo());
        }

        [Theory]
        [InlineData(typeof(string))]
        [InlineData(typeof(Guid))]
        [InlineData(typeof(int*))]
        [InlineData(typeof(int[]))]
        [InlineData(typeof(List<>))]
        [InlineData(typeof(List<string>))]
        [InlineData(typeof(object))]
        [InlineData(typeof(ValueType))]
        [InlineData(typeof(Enum))]
        [InlineData(typeof(int?))]
        public void DefineEnum_InvalidUnderlyingType_ThrowsTypeLoadExceptionOnCreation(Type underlyingType)
        {
            ModuleBuilder module = Helpers.DynamicModule();
            EnumBuilder enumBuilder = module.DefineEnum("Name", TypeAttributes.Public, underlyingType);
            Assert.Equal(underlyingType, enumBuilder.UnderlyingField.FieldType);
            Assert.Throws<TypeLoadException>(() => enumBuilder.CreateType());
        }

        protected enum Int32Enum { }
    }
}
