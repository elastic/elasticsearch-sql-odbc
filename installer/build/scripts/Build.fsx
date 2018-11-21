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
open Fake.FileHelper
open Fake.AssemblyInfoFile

module Builder =

    let patchAssemblyInformation (version:Version) = 
        let version = version.FullVersion
        let commitHash = Information.getCurrentHash()
        CreateCSharpAssemblyInfo (MsiDir @@ "Properties/AssemblyInfo.cs")
            [Attribute.Title "Installer"
             Attribute.Description "Elasticsearch ODBC Installer."
             Attribute.Guid "44555887-c439-470c-944d-8866ec3d7067"
             Attribute.Product "Elasticsearch ODBC Installer"
             Attribute.Metadata("GitBuildHash", commitHash)
             Attribute.Company  "Elasticsearch B.V."
             Attribute.Copyright "Elastic License. Copyright Elasticsearch."
             Attribute.Trademark "Elasticsearch is a trademark of Elasticsearch B.V."
             Attribute.Version  version
             Attribute.FileVersion version
             Attribute.InformationalVersion version // Attribute.Version and Attribute.FileVersion normalize the version number, so retain the prelease suffix
            ]

    let Sign file (version : Version) =
        tracefn "Signing MSI"
        let release = getBuildParam "release" = "1"
        if release then
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

    let BuildMsi (version : Version) =

        !! (MsiDir @@ "*.csproj")
        |> MSBuildRelease MsiBuildDir "Build"
        |> ignore

        patchAssemblyInformation (version)

        let zipfile = DriverBuildsDir
                      |> directoryInfo
                      |> filesInDirMatching ("*.zip")
                      |> Seq.head
                          
        unzipFile (zipfile.FullName, DriverBuildsDir)
        tracefn "Unzipped zip file in %s" zipfile.FullName

        let exitCode = ExecProcess (fun info ->
                         info.FileName <- sprintf "%sOdbcInstaller" MsiBuildDir
                         info.WorkingDirectory <- MsiDir
                         info.Arguments <- [version.FullVersion; System.IO.Path.GetFullPath(DriverBuildsDir); zipfile.FullName] |> String.concat " "
                        ) <| TimeSpan.FromMinutes 20.
    
        if exitCode <> 0 then failwithf "Error building MSI"

        let MsiFile = MsiDir
                       |> directoryInfo
                       |> filesInDirMatching ("*.msi")
                       |> Seq.map (fun f -> f.FullName)
                       |> Seq.head
        
        Sign MsiFile version
        CopyFile OutDir MsiFile
        DeleteFile MsiFile
