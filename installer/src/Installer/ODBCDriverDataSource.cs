using WixSharp;

namespace ODBCInstaller
{
    partial class Program
    {
        public class ODBCDriverDataSource : GenericNestedEntity, IGenericEntity
        {
            [Xml]
            new public string Id
            {
                get => base.Id;
                set => Id = value;
            }

            [Xml]
            new public string Name;

            [Xml]
            public string Registration;

            public ODBCDriverDataSource(string name, string registration)
            {
                this.Name = name;
                this.Registration = registration;
            }

            public void Process(ProcessingContext context)
            {
                // http://wixtoolset.org/documentation/manual/v3/xsd/wix/odbcdatasource.html
                context.XParent.AddElement(this.ToXElement("ODBCDataSource"));
            }
        }
    }
}