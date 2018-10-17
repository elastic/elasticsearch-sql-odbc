#I "../../packages/build/FAKE.x64/tools"

#r "FakeLib.dll"

open System.Globalization
open System.IO
open Fake

module Paths =
    let BuildDir = "./build/"
    let ToolsDir = BuildDir @@ "tools/"
    let InDir = BuildDir @@ "in/"
    let OutDir = BuildDir @@ "out/"
    let ResultsDir = BuildDir @@ "results/"

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

    let All = [Odbc]

    type Source =
        | Compile
        | Released
        | BuildCandidate of hash:string

        member this.Description =
            match this with
            | Compile -> "compiled from source"
            | Released -> "official release"
            | BuildCandidate hash -> sprintf "build candidate %s" hash

    type Version = {
        Product : string;
        FullVersion : string;
        Major : int;
        Minor : int;
        Patch : int;
        Prerelease : string;
        Source : Source;
        RawValue: string;
    }

    type ProductVersions (product:Product, versions:Version list) =
        member this.Product = product
        member this.Versions = versions
        member this.Name = product.Name
        member this.Title = product.Title

        member private this.ZipFile (version:Version) =
            let fullPathInDir = InDir |> Path.GetFullPath
            Path.Combine(fullPathInDir, sprintf "%s-%s.zip" this.Name version.FullVersion)

        member private this.ExtractedDirectory (version:Version) =
            let fullPathInDir = InDir |> Path.GetFullPath            
            Path.Combine(fullPathInDir, sprintf "%s-%s" this.Name version.FullVersion)

        member this.BinDirs = 
            this.Versions
            |> List.filter (fun v -> v.Source = Compile)
            |> List.map(fun v -> InDir @@ sprintf "%s-%s/bin/" this.Name v.FullVersion)

        static member CreateFromProduct (productToVersion:Product -> Version list) (product: Product)  =
            ProductVersions(product, productToVersion product)