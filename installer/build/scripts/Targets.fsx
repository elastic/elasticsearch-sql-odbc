// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.

#I "../../packages/build/FAKE.x64/tools"
#I "../../packages/build/System.Management.Automation/lib/net45"

#r "System.Management.Automation.dll"
#r "FakeLib.dll"

#load "Products.fsx"
#load "Build.fsx"
#load "Commandline.fsx"

open System
open Fake
open Products
open Products.Products
open Products.Paths
open Build.Builder
open Commandline
open Fake.Runtime.Trace
open System.Management.Automation

let versionToBuild = Commandline.parse()

if (getBuildParam "target" |> toLower <> "help") then 
    traceHeader (sprintf "Products:%s%s%s" Environment.NewLine Environment.NewLine versionToBuild.FullVersion)

Target "Clean" (fun _ ->
    CleanDirs [MsiBuildDir; OutDir;]
)

Target "BuildInstaller" (fun () ->
    BuildMsi versionToBuild
)

Target "Release" (fun () ->
    trace "Build in Release mode. MSI will be signed."
)

Target "Integrate" (fun () ->
    let version = Commandline.parse()
    let integrationTestsTargets = getBuildParamOrDefault "testtargets" "*"
    let vagrantProvider = getBuildParamOrDefault "vagrantprovider" "local"
    let gui = getBuildParamOrDefault "gui" "$false"
    let noDestroy = getBuildParamOrDefault "no-destroy" "$true"

    let script = sprintf @"cd '%s'; .\Bootstrapper.ps1 -Tests '%s' -Version '%s' -VagrantProvider '%s' -Gui:%s -VagrantDestroy:%s" 
                    IntegrationTestsDir
                    integrationTestsTargets
                    version.RawValue
                    vagrantProvider
                    gui
                    noDestroy
        
    trace (sprintf "Running Powershell script: '%s'" script)
    use p = PowerShell.Create()
    use output = new PSDataCollection<PSObject>()
    output.DataAdded.Add(fun data -> trace (sprintf "%O" output.[data.Index]))
    p.Streams.Verbose.DataAdded.Add(fun data -> trace (sprintf "%O" p.Streams.Verbose.[data.Index]))
    p.Streams.Debug.DataAdded.Add(fun data -> trace (sprintf "%O" p.Streams.Debug.[data.Index]))
    p.Streams.Progress.DataAdded.Add(fun data -> trace (sprintf "%O" p.Streams.Progress.[data.Index]))
    p.Streams.Warning.DataAdded.Add(fun data -> traceError (sprintf "%O" p.Streams.Warning.[data.Index]))
    p.Streams.Error.DataAdded.Add(fun data -> traceError (sprintf "%O" p.Streams.Error.[data.Index]))
    let async =
        p.AddScript(script).BeginInvoke(null, output)
              |> Async.AwaitIAsyncResult
              |> Async.Ignore
    Async.RunSynchronously async

    if (p.InvocationStateInfo.State = PSInvocationState.Failed) then
        failwith "PowerShell completed abnormally due to an error"
)

Target "Help" (fun () -> trace Commandline.usage)

"Clean"
  ==> "BuildInstaller"
  ==> "Release"

"Clean"
"BuildInstaller"
  ==> "Integrate"

RunTargetOrDefault "BuildInstaller"