using System.Linq;
using WixSharp;

namespace ODBCInstaller
{
    partial class Program
    {
        public class ODBCDriver : GenericNestedEntity, IGenericEntity
        {
            [Xml]
            new public string Id
            {
                get => base.Id;
                set => Id = value;
            }

            [Xml]
            new public string Name;

            public ODBCDriver(string name)
            {
                this.Name = name;
                this.Children = Enumerable.Empty<IGenericEntity>().ToArray();
            }

            public ODBCDriver(string name, IGenericEntity element)
            {
                this.Name = name;
                this.Children = new[] { element };
            }

            public void Process(ProcessingContext context)
            {
                // http://wixtoolset.org/documentation/manual/v3/xsd/wix/odbcdriver.html

                var driver = this.ToXElement("ODBCDriver");

                var newContext = new ProcessingContext
                {
                    Project = context.Project,
                    Parent = this,
                    XParent = driver,
                    FeatureComponents = context.FeatureComponents,
                };

                foreach (var child in this.Children)
                {
                    child.Process(newContext);
                }

                context.XParent.AddElement(driver);
            }
        }
    }
}