#I "../../packages/build/FAKE.x64/tools"

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

let productsToBuild = Commandline.parse()

let productDescriptions = productsToBuild.Versions
                          |> List.map(fun v -> sprintf "%s %s (%s)" productsToBuild.Title v.FullVersion "Compile")
                          |> String.concat Environment.NewLine

if (getBuildParam "target" |> toLower <> "help") then 
    traceHeader (sprintf "Products:%s%s%s" Environment.NewLine Environment.NewLine productDescriptions)

Target "Clean" (fun _ ->
    CleanDirs [MsiBuildDir; OutDir;]
)

Target "BuildInstaller" (fun () ->
    BuildMsi productsToBuild
)

Target "Release" (fun () ->
    trace "Build in Release mode. Services and MSIs will be signed."
)

Target "Help" (fun () -> trace Commandline.usage)

"Clean"
  ==> "BuildInstaller"
  ==> "Release"

RunTargetOrDefault "BuildInstaller"