// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.

#I "../../packages/build/FAKE.x64/tools"
#I @"../../packages/build/Fsharp.Data/lib/net45"
#I @"../../packages/build/FSharp.Text.RegexProvider/lib/net40"

#r "FakeLib.dll"
#r "Fsharp.Data.dll"
#r "Fsharp.Text.RegexProvider.dll"

#load "Products.fsx"

open System
open System.IO
open System.Text.RegularExpressions
open System.Net
open Fake
open FSharp.Text.RegexProvider
open Products.Products

ServicePointManager.SecurityProtocol <- SecurityProtocolType.Ssl3 ||| SecurityProtocolType.Tls ||| SecurityProtocolType.Tls11 ||| SecurityProtocolType.Tls12;
ServicePointManager.ServerCertificateValidationCallback <- (fun _ _ _ _ -> true)

module Commandline =
    open Fake.Core

    let usage = """
USAGE:

build.bat <Target> [Target specific params] [skiptests]

Target:
-------

* buildinstaller <PathToZipFile>
  - builds the installer for the ODBC driver in the provided ZIP file

* clean
  - cleans build output folders

* patchversions [VersionString]
  - patches version numbers in assemblyinfo files to [VersionString]

* release <PathToZipFile> [CertFile] [PasswordFile]
  - create a release version of the MSI by building and then signing the installer.
  - when CertFile and PasswordFile are specified, these will be used for signing otherwise the values in ELASTIC_CERT_FILE
    and ELASTIC_CERT_PASSWORD environment variables will be used

  Example: build.bat release C:/path_to_zip_file C:/path_to_cert_file C:/path_to_password_file

* help or ?
  - show this usage summary

"""
    type VersionRegex = Regex< @"^(esodbc\-)?(?<Version>(?<Major>\d+)\.(?<Minor>\d+)\.(?<Patch>\d+)(?:\-(?<Prerelease>[\w\-]+))?)", noMethodPrefix=true >

    let parseVersion version =
        let m = VersionRegex().Match version
        if m.Success |> not then failwithf "Could not parse version from %s" version

        { FullVersion = m.Version.Value;
          Major = m.Major.Value |> int;
          Minor = m.Minor.Value |> int;
          Patch = m.Patch.Value |> int;
          Prerelease = m.Prerelease.Value; 
          RawValue = m.Version.Value; }

    let private args = getBuildParamOrDefault "cmdline" "buildinstaller" |> split ' '

    let private (|IsTarget|_|) (candidate: string) =
        match candidate.ToLowerInvariant() with
        | "buildinstaller"
        | "clean"
        | "help"
        | "patchversions"
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
        [("ELASTIC_CERT_FILE", "certificate");("ELASTIC_CERT_PASSWORD", "password")]
        |> List.iter(fun (v, b) ->
                let ev = Environment.GetEnvironmentVariable(v, EnvironmentVariableTarget.Machine)
                if isNullOrWhiteSpace ev then failwithf "Expecting non-null value for %s environment variable" v
                setBuildParam b ev
           )

    let private certAndPasswordFromFile certFile passwordFile =
        trace "Getting signing cert and password from file arguments"
        match (fileExists certFile, fileExists passwordFile) with
        | (true, true) ->
            setBuildParam "certificate" certFile
            passwordFile |> File.ReadAllText |> setBuildParam "password"
        | (false, _) -> failwithf "certificate file does not exist at %s" certFile
        | (_, false) -> failwithf "password file does not exist at %s" passwordFile

    let private versionFromZipFile zipFile =
        trace "Getting zip location from arguments"

        let extractVersion (fileName) =
            Regex.Replace(fileName, "^.*\\\([^\\\]*)\.zip$", "$1")

        match (fileExists zipFile) with
        | (false) -> failwithf "ZIP file does not exist at %s" zipFile
        | (true) ->
            setBuildParam "zipFile" zipFile
            trace "Getting zip location from arguments"
            let version = zipFile |> extractVersion |> parseVersion
            tracefn "Extracted version information %s from %s" version.FullVersion zipFile
            version

    let parse () =
        setEnvironVar "FAKEBUILD" "1"
        let version = match arguments with
                       | ["release"; zipFile ] ->
                           setBuildParam "release" "1"
                           certAndPasswordFromEnvVariables ()
                           versionFromZipFile zipFile
                       | ["release"; zipFile; certFile; passwordFile ] ->
                           setBuildParam "release" "1"
                           certAndPasswordFromFile certFile passwordFile
                           versionFromZipFile zipFile
                       | ["buildinstaller"; zipFile] ->
                           versionFromZipFile zipFile
                       | ["patchversions"; version] ->
                            version |> parseVersion
                       | [IsTarget target;] when target = "clean" || target = "help" ->
                            Products.Products.EmptyVersion
                       | _ ->
                           traceError usage
                           exit 2

        setBuildParam "target" target
        version
