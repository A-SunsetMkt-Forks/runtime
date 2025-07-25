using Mono.Linker.Tests.Cases.Expectations.Assertions;

namespace Mono.Linker.Tests.Cases.Inheritance.Interfaces.OnValueType
{
    public class UnusedInterfaceTypeIsRemoved
    {
        public static void Main()
        {
            IFoo i = new A();
            i.Foo();
        }

        [Kept]
        interface IFoo
        {
            [Kept]
            void Foo();
        }

        interface IBar
        {
            void Bar();
        }

        [Kept]
        [KeptMember(".ctor()")]
        [KeptInterface(typeof(IFoo))]
        struct A : IBar, IFoo
        {
            [Kept]
            public void Foo()
            {
            }

            public void Bar()
            {
            }
        }
    }
}
