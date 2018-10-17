#I "../../packages/build/FAKE.x64/tools"
#I @"../../packages/build/Fsharp.Data/lib/net45"
#I @"../../packages/build/FSharp.Text.RegexProvider/lib/net40"

#r @"FakeLib.dll"
#r "Fsharp.Data.dll"
#r "Fsharp.Text.RegexProvider.dll"
#r "System.Xml.Linq.dll"

#load "Products.fsx"

open System
open System.IO
open System.Text.RegularExpressions
open System.Net
open Fake
open FSharp.Text.RegexProvider
open Products.Products
open Products.Paths

ServicePointManager.SecurityProtocol <- SecurityProtocolType.Ssl3 ||| SecurityProtocolType.Tls ||| SecurityProtocolType.Tls11 ||| SecurityProtocolType.Tls12;
ServicePointManager.ServerCertificateValidationCallback <- (fun _ _ _ _ -> true)

module Commandline =

    let usage = """
USAGE:

build.bat [Target] [Target specific params] [skiptests]

Target:
-------

* buildinstaller
  - default target if none provided. Builds ODBC installer

* clean
  - cleans build output folders

* release [CertFile] [PasswordFile]
  - create a release version of the MSI by building and then signing the installer.
  - when CertFile and PasswordFile are specified, these will be used for signing otherwise the values in ELASTIC_CERT_FILE
    and ELASTIC_CERT_PASSWORD environment variables will be used

  Example: build.bat release C:/path_to_cert_file C:/path_to_password_file

* help or ?
  - show this usage summary

"""
    type VersionRegex = Regex< @"^(?:\s*(?<Product>.*?)\s*)?((?<Source>\w*)\:)?(?<Version>(?<Major>\d+)\.(?<Minor>\d+)\.(?<Patch>\d+)(?:\-(?<Prerelease>[\w\-]+))?)", noMethodPrefix=true >

    let private parseSource = function
        | "r" -> Released
        | hash when isNotNullOrEmpty hash -> BuildCandidate hash
        | _ -> Compile

    let parseVersion version =
        let m = VersionRegex().Match version
        if m.Success |> not then failwithf "Could not parse version from %s" version
        let source = parseSource m.Source.Value

        let rawValue =
            match source with
            | Compile -> m.Version.Value
            | _ -> sprintf "%s:%s" m.Source.Value m.Version.Value

        { Product = m.Product.Value;
          FullVersion = m.Version.Value;
          Major = m.Major.Value |> int;
          Minor = m.Minor.Value |> int;
          Patch = m.Patch.Value |> int;
          Prerelease = m.Prerelease.Value; 
          Source = source;
          RawValue = rawValue; }

    let private args = getBuildParamOrDefault "cmdline" "buildinstaller" |> split ' '

    let private versionFromInDir (product : Product) =
        let extractVersion (fileInfo:FileInfo) =
            Regex.Replace(fileInfo.Name, "^" + product.Name + "\-(.*)\.zip$", "$1")

        let zips = InDir
                   |> directoryInfo
                   |> filesInDirMatching ("es*" + product.Name + "*.zip")

        match zips.Length with
        | 0 -> failwithf "No %s zip file found in %s" product.Name InDir
        | 1 ->
            let version = zips.[0] |> extractVersion |> parseVersion
            tracefn "Extracted %s from %s" version.FullVersion zips.[0].FullName
            [version]
        | _ -> failwithf "Expecting one %s zip file in %s but found %i" product.Name InDir zips.Length

    let private (|IsTarget|_|) (candidate: string) =
        match candidate.ToLowerInvariant() with
        | "buildinstaller"
        | "clean"
        | "release" -> Some candidate
        | _ -> None

    let target =
        match (args |> List.tryHead) with
        | Some t -> 
            match (t.ToLowerInvariant()) with
            | IsTarget t -> t
            | "help" 
            | "?" -> "help"
            | _ -> "buildinstaller"
        | _ -> "buildinstaller"

    let arguments =
        match args with
        | IsTarget head :: tail -> head :: tail
        | [] -> [target]
        | _ -> target :: args

    let private certAndPasswordFromEnvVariables () =
        trace "Getting signing cert and password from environment variables"
        //[("ELASTIC_CERT_FILE", "certificate");("ELASTIC_CERT_PASSWORD", "password")]
        //|> List.iter(fun (v, b) ->
        //        let ev = Environment.GetEnvironmentVariable(v, EnvironmentVariableTarget.Machine)
        //        if isNullOrWhiteSpace ev then failwithf "Expecting non-null value for %s environment variable" v
        //        setBuildParam b ev
        //   )

    let private certAndPasswordFromFile certFile passwordFile =
        trace "Getting signing cert and password from file arguments"
        //match (fileExists certFile, fileExists passwordFile) with
        //| (true, true) ->
        //    setBuildParam "certificate" certFile
        //    passwordFile |> File.ReadAllText |> setBuildParam "password"
        //| (false, _) -> failwithf "certificate file does not exist at %s" certFile
        //| (_, false) -> failwithf "password file does not exist at %s" passwordFile

    let parse () =
        setEnvironVar "FAKEBUILD" "1"
        let products = match arguments with
                       | ["release"] ->
                           setBuildParam "release" "1"
                           certAndPasswordFromEnvVariables ()
                           All |> List.map (ProductVersions.CreateFromProduct versionFromInDir)
                       | ["release"; certFile; passwordFile ] ->
                           setBuildParam "release" "1"
                           certAndPasswordFromFile certFile passwordFile
                           All |> List.map (ProductVersions.CreateFromProduct versionFromInDir)
                       | _ ->
                           traceError usage
                           exit 2

        setBuildParam "target" target
        products
