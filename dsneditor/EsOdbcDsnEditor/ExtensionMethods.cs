/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

// uncomment to have the assembly loading to ask for (various) resources; various solutions:
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor
{
	public static class ExtensionMethods
	{
		public static string StripBraces(this string input)
		{
			if (input.StartsWith("{") && input.EndsWith("}")) {
				return input.Substring(1, input.Length - 2);
			}

			return input;
		}
	}
}
