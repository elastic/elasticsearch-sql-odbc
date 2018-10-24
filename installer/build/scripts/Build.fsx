#I "../../packages/build/FAKE.x64/tools"

#r "FakeLib.dll"
#load "Products.fsx"

open System
open System.Diagnostics
open System.IO
open Fake
open Products.Products
open Products.Paths
open Products
open System.Text
open Fake.FileHelper

module Builder =

    let Sign file (product : ProductVersions) =
        tracefn "Signing MSI"
        let release = getBuildParam "release" = "1"
        if release then
            let certificate = getBuildParam "certificate"
            let password = getBuildParam "password"
            let timestampServer = "http://timestamp.comodoca.com"
            let timeout = TimeSpan.FromMinutes 1.

            let sign () =
                let signToolExe = ToolsDir @@ "signtool/signtool.exe"
                let args = ["sign"; "/f"; certificate; "/p"; password; "/t"; timestampServer; "/d"; product.Title; "/v"; file] |> String.concat " "
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
            if exitCode <> 0 then failwithf "Signing %s returned error exit code: %i" product.Title exitCode

    let BuildMsi (product : ProductVersions) =

        !! (MsiDir @@ "*.csproj")
        |> MSBuildRelease MsiBuildDir "Build"
        |> ignore

        product.Versions
        |> List.iter(fun version -> 

            let exitCode = ExecProcess (fun info ->
                             info.FileName <- sprintf "%sOdbcInstaller" MsiBuildDir
                             info.WorkingDirectory <- MsiDir
                             info.Arguments <- [version.FullVersion; System.IO.Path.GetFullPath(DriverFilesDir)] |> String.concat " "
                            ) <| TimeSpan.FromMinutes 20.
    
            if exitCode <> 0 then failwithf "Error building MSI"

            let MsiFile = MsiDir
                           |> directoryInfo
                           |> filesInDirMatching ("*.msi")
                           |> Seq.map (fun f -> f.FullName)
                           |> Seq.head
            
            Sign MsiFile product
            CopyFile OutDir MsiFile
            DeleteFile MsiFile
        )