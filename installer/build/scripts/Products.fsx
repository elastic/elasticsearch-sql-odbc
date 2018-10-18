#I "../../packages/build/FAKE.x64/tools"

#r "FakeLib.dll"

open System.Globalization
open Fake

module Paths =
    let BuildDir = "./build/"
    let ToolsDir = BuildDir @@ "tools/"
    let InDir = BuildDir @@ "in/"
    let OutDir = BuildDir @@ "out/"

    let SrcDir = "./src/"
    let MsiDir = SrcDir @@ "Installer/"
    let MsiBuildDir = MsiDir @@ "bin/Release/"

module Products =
    open Paths

    type Product =
        | Odbc

        member this.Name =
            match this with
            | Odbc -> "odbc"
            
        member this.AssemblyTitle =
            match this with
            | Odbc -> "ODBC Driver"
            
        member this.AssemblyDescription =
            match this with
            | Odbc -> "Elasticsearch ODBC Driver."
            
        member this.AssemblyGuid =
            match this with
            | Odbc -> "44555887-c439-470c-944d-8866ec3d7067"

        member this.Title =
            CultureInfo.InvariantCulture.TextInfo.ToTitleCase this.Name

    type Version = {
        Product : string;
        FullVersion : string;
        Major : int;
        Minor : int;
        Patch : int;
        Prerelease : string;
        RawValue: string;
    }

    type ProductVersions (product:Product, versions:Version list) =
        member this.Product = product
        member this.Versions = versions
        member this.Name = product.Name
        member this.Title = product.Title

        static member CreateFromProduct (productToVersion:Product -> Version list) (product: Product)  =
            ProductVersions(product, productToVersion product)