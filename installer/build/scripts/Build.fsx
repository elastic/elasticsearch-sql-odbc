// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.

#I "../../packages/build/FAKE.x64/tools"
#I "../../packages/build/DotNetZip/lib/net20"

#r "FakeLib.dll"
#r "DotNetZip.dll"

#load "Products.fsx"

open System
open System.Diagnostics
open System.IO
open Fake
open Fake.Git
open Products.Products
open Products.Paths
open Products
open System.Text
open System.Text.RegularExpressions
open Fake.FileHelper
open Fake.AssemblyInfoFile

module Builder =
    open Fake.FileSystemHelper

    type AssemblyInfo = {
        Path : string;
        Title: string;
        Description : string;
        Guid: string;
        Product: string;
        Version: Version;
    }

    let patchAssemblyInformation (assemblyInfo:AssemblyInfo, isClean:bool) = 
        let version = assemblyInfo.Version.FullVersion
        let informationalVersion = if isClean then version else version + "-" + Information.getCurrentHash()
        CreateCSharpAssemblyInfo assemblyInfo.Path
            [Attribute.Title assemblyInfo.Title
             Attribute.Description assemblyInfo.Description
             Attribute.Guid assemblyInfo.Guid
             Attribute.Product assemblyInfo.Product
             Attribute.Company "Elasticsearch B.V."
             Attribute.Copyright "Elastic License. Copyright Elasticsearch."
             Attribute.Trademark "Elasticsearch is a trademark of Elasticsearch B.V."
             Attribute.Version version
             Attribute.FileVersion version
             Attribute.InformationalVersion informationalVersion
            ]

    let Sign file () =
        let release = getBuildParam "release" = "1"
        if release then
            tracefn "Signing MSI"
            let certificate = getBuildParam "certificate"
            let password = getBuildParam "password"
            let timestampServer = "http://timestamp.comodoca.com"
            let timeout = TimeSpan.FromMinutes 1.

            let sign () =
                let signToolExe = ToolsDir @@ "signtool/signtool.exe"
                let args = ["sign"; "/f"; certificate; "/p"; password; "/t"; timestampServer; "/d"; "\"Elasticsearch ODBC Driver\""; "/v"; file] |> String.concat " "
                let redactedArgs = args.Replace(password, "<redacted>")

                use proc = new Process()
                proc.StartInfo.UseShellExecute <- false
                proc.StartInfo.FileName <- signToolExe
                proc.StartInfo.Arguments <- args
                platformInfoAction proc.StartInfo
                proc.StartInfo.RedirectStandardOutput <- true
                proc.StartInfo.RedirectStandardError <- true
                if isMono then
                    proc.StartInfo.StandardOutputEncoding <- Encoding.UTF8
                    proc.StartInfo.StandardErrorEncoding  <- Encoding.UTF8
                proc.ErrorDataReceived.Add(fun d -> if d.Data <> null then traceError d.Data)
                proc.OutputDataReceived.Add(fun d -> if d.Data <> null then trace d.Data)

                try
                    tracefn "%s %s" proc.StartInfo.FileName redactedArgs
                    start proc
                with exn -> failwithf "Start of process %s failed. %s" proc.StartInfo.FileName exn.Message
                proc.BeginErrorReadLine()
                proc.BeginOutputReadLine()
                if not <| proc.WaitForExit(int timeout.TotalMilliseconds) then
                    try
                        proc.Kill()
                    with exn ->
                        traceError
                        <| sprintf "Could not kill process %s  %s after timeout." proc.StartInfo.FileName redactedArgs
                    failwithf "Process %s %s timed out." proc.StartInfo.FileName redactedArgs
                proc.WaitForExit()
                proc.ExitCode

            let exitCode = sign()
            if exitCode <> 0 then failwithf "Signing returned error exit code: %i" exitCode

    // Using DotNetZip due to errors with CMAKE zip files: https://github.com/fsharp/FAKE/issues/775
    let unzipFile(zipFolder: string, unzipFolder: string) =
        use zip = Ionic.Zip.ZipFile.Read(zipFolder)
        for e in zip do
            e.Extract(unzipFolder, Ionic.Zip.ExtractExistingFileAction.OverwriteSilently)

    let PatchAssemblyInfos (version : Version) =
        let isClean = (version.FullVersion = "0.0.0")
        patchAssemblyInformation ({
            Path = MsiDir @@ "Properties/AssemblyInfo.cs";
            Title = "Elasticsearch ODBC Installer";
            Description = "MSI installer for the Elasticsearch ODBC driver.";
            Guid = "44555887-c439-470c-944d-8866ec3d7067";
            Product = "Elasticsearch ODBC Installer";
            Version = version;
        },isClean)
        patchAssemblyInformation ({
            Path = MsiDir @@ "../InstallerCA/Properties/AssemblyInfo.cs";
            Title = "Elasticsearch ODBC Installer Custom Actions";
            Description = "MSI installer custom actions for the Elasticsearch ODBC driver.";
            Guid = "4498d74b-e5c5-48bb-a9d4-8cc55b7b0909";
            Product = "Elasticsearch ODBC Installer Custom Actions";
            Version = version;
        },isClean)
        patchAssemblyInformation ({
            Path = SrcDir @@ "../../dsneditor/EsOdbcDsnEditor/Properties/AssemblyInfo.cs";
            Title = "Elasticsearch DSN Editor";
            Description = "Elasticsearch DSN Editor for managing ODBC connection strings.";
            Guid = "fac0512c-e595-4bf4-acb7-617611df5715";
            Product = "Elasticsearch DSN Editor";
            Version = version;
        },isClean)
        patchAssemblyInformation ({
            Path = SrcDir @@ "../../dsneditor/EsOdbcDsnEditorLauncher/Properties/AssemblyInfo.cs";
            Title = "Elasticsearch DSN Editor Launcher";
            Description = "Elasticsearch DSN Editor Launcher.";
            Guid = "71bebff7-652e-4b26-9ec3-caef947d368c";
            Product = "Elasticsearch DSN Editor Launcher";
            Version = version;
        },isClean)

    let copyFileWithLog outDir file = 
        let targetDirexists = directoryExists outDir
        if (targetDirexists = false) then
            tracefn "%s does not exist" outDir
        else
            tracefn "%s exists" outDir
            tracefn "Copying: %s <- %s" outDir file
            CopyFile outDir file

    let BuildMsi (version : Version) =

        !! (MsiDir @@ "*.csproj")
        |> MSBuildRelease MsiBuildDir "Build"
        |> ignore

        PatchAssemblyInfos version

        let zipFile = getBuildParam "zipFile"
        let buildDir = Regex.Replace(zipFile, "(^.*)\\\[^\\\]*\.zip$", "$1/")

        unzipFile (zipFile, buildDir)
        tracefn "Unzipped zip file in %s" zipFile

        // sign every DLL to be part of the MSI
        let unzippedDir = Regex.Replace(zipFile, "(^.*)\.zip$", "$1/")
        let dllFiles = unzippedDir
                        |> directoryInfo
                        |> filesInDirMatching ("*.dll")
                        |> Seq.map (fun f -> f.FullName)
        for dllFile in dllFiles do
            Sign dllFile

        let exitCode = ExecProcess (fun info ->
                         info.FileName <- sprintf "%sInstaller" MsiBuildDir
                         info.WorkingDirectory <- MsiDir
                         info.Arguments <- [version.FullVersion; System.IO.Path.GetFullPath(buildDir); zipFile] |> String.concat " "
                        ) <| TimeSpan.FromMinutes 20.

        if exitCode <> 0 then failwithf "Error building MSI"

        let MsiFile = MsiDir
                       |> directoryInfo
                       |> filesInDirMatching ("*.msi")
                       |> Seq.map (fun f -> f.FullName)
                       |> Seq.head

        Sign MsiFile
        copyFileWithLog OutDir MsiFile
        DeleteFile MsiFile
