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
            | Odbc -> "ODBC Driver"
            
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
        member this.Title = product.Title

        static member CreateFromProduct (productToVersion:Product -> Version list) (product: Product)  =
            ProductVersions(product, productToVersion product)