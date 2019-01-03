// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.

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

    let IntegrationTestsDir = FullName (SrcDir @@ "Tests/")

module Products =
    type Version = {
        FullVersion : string;
        Major : int;
        Minor : int;
        Patch : int;
        Prerelease : string;
        RawValue: string;
    }

    let public EmptyVersion = { FullVersion = "0.0.0";
                              Major = 0;
                              Minor = 0;
                              Patch = 0;
                              Prerelease = ""; 
                              RawValue = "0.0.0"; }
