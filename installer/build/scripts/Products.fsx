#I "../../packages/build/FAKE.x64/tools"

#r "FakeLib.dll"

open Fake

module Paths =
    let BuildDir = "./build/"
    let ToolsDir = BuildDir @@ "tools/"
    let OutDir = BuildDir @@ "out/"

    let SrcDir = "./src/"
    let MsiDir = SrcDir @@ "Installer/"
    let MsiBuildDir = MsiDir @@ "bin/Release/"
    let DriverBuildsDir = SrcDir @@ "../../builds/" // Output folder from driver compilation

module Products =
    type Version = {
        FullVersion : string;
        Major : int;
        Minor : int;
        Patch : int;
        Prerelease : string;
        RawValue: string;
    }