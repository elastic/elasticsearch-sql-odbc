#I "../../packages/build/FAKE.x64/tools"

#r "FakeLib.dll"
#load "Products.fsx"
#load "Build.fsx"
#load "Commandline.fsx"

open System
open Fake
open Products
open Products.Paths
open Build.Builder
open Commandline
open Fake.Runtime.Trace

let versionToBuild = Commandline.parse()

Target "Clean" (fun _ ->
    CleanDirs [MsiBuildDir; OutDir;]
)

Target "BuildInstaller" (fun () ->
    traceHeader (sprintf "Products:%s%s%s" Environment.NewLine Environment.NewLine versionToBuild.FullVersion)
    BuildMsi versionToBuild
)

Target "Release" (fun () ->
    trace "Build in Release mode. MSI will be signed."
)

Target "PatchVersions" (fun () ->
    trace "Patching versions."
    PatchAssemblyInfos versionToBuild
)

Target "Help" (fun () -> trace Commandline.usage)

"Clean"
  ==> "PatchVersions"
  ==> "BuildInstaller"
  ==> "Release"

RunTargetOrDefault "BuildInstaller"