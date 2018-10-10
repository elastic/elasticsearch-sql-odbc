// uncomment to have the assembly loading to ask for (various) resources; various solutions: 
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor
{
    public static class ExtensionMethods
    {
        public static string StripBraces(this string input)
        {
            if (input.StartsWith("{") && input.EndsWith("}"))
            {
                return input.Substring(1, input.Length - 2);
            }

            return input;
        }
    }
}