#I "../../packages/build/FAKE.x64/tools"
#I "../../packages/build/System.Management.Automation/lib/net45"

#r "FakeLib.dll"
#r "System.Management.Automation.dll"
#load "Products.fsx"
#load "Build.fsx"
#load "Commandline.fsx"

open System
open System.Management.Automation
open Fake
open Products
open Products.Products
open Products.Paths
open Build.Builder
open Commandline
open Fake.Runtime.Trace

let productsToBuild = Commandline.parse()

let productDescriptions = productsToBuild
                          |> List.map(fun p ->
                                 p.Versions 
                                 |> List.map(fun v -> sprintf "%s %s (%s)" p.Title v.FullVersion v.Source.Description)
                             )
                          |> List.concat
                          |> String.concat Environment.NewLine

if (getBuildParam "target" |> toLower <> "help") then 
    traceHeader (sprintf "Products:%s%s%s" Environment.NewLine Environment.NewLine productDescriptions)

Target "Clean" (fun _ ->
    CleanDirs [MsiBuildDir; OutDir; ResultsDir]
    productsToBuild
    |> List.iter(fun p -> CleanDirs [OutDir @@ p.Name;])
)

Target "BuildInstaller" (fun () ->
    productsToBuild |> List.iter (fun p -> BuildMsi p)
)

Target "Release" (fun () ->
    trace "Build in Release mode. Services and MSIs will be signed."
)

Target "Help" (fun () -> trace Commandline.usage)

"Clean"
  ==> "BuildInstaller"
  ==> "Release"

RunTargetOrDefault "BuildInstaller"