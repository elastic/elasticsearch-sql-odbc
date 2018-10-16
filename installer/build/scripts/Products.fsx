#I "../../packages/build/FAKE.x64/tools"

#r "FakeLib.dll"
#load "Snapshots.fsx"

open System
open System.Globalization
open System.IO
open Fake
open Fake.FileHelper
open Snapshots

module Paths =
    let BuildDir = "./build/"
    let ToolsDir = BuildDir @@ "tools/"
    let InDir = BuildDir @@ "in/"
    let OutDir = BuildDir @@ "out/"
    let ResultsDir = BuildDir @@ "results/"

    let SrcDir = "./src/"
    let MsiDir = SrcDir @@ "Installer/"
    let MsiBuildDir = MsiDir @@ "bin/Release/"

    let IntegrationTestsDir = FullName "./src/Tests/Odbc.Installer.Integration.Tests"
    let UnitTestsDir = "src/Tests/Odbc.Tests"

    let ArtifactDownloadsUrl = "https://artifacts.elastic.co/downloads"

    let StagingDownloadsUrl product hash fullVersion = 
        sprintf "https://staging.elastic.co/%s-%s/downloads/%s/%s-%s.msi" fullVersion hash product product fullVersion

    let SnapshotDownloadsUrl product versionNumber hash fullVersion =
        sprintf "https://snapshots.elastic.co/%s-%s/downloads/%s/%s-%s.msi" versionNumber hash product product fullVersion

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

    let lastSnapshotVersionAsset (product:Product, version:Version) =
        let latestAsset = Snapshots.GetVersionsFiltered version.Major version.Minor version.Patch version.Prerelease
                            |> Seq.map (fun x -> (x, (Snapshots.GetSnapshotBuilds x) |> Seq.head))
                            |> Seq.map (fun xy -> Snapshots.GetSnapshotBuildAssets product.Name (fst xy) (snd xy))
                            |> Seq.head
        latestAsset

    type ProductVersions (product:Product, versions:Version list) =
        member this.Product = product
        member this.Versions = versions
        member this.Name = product.Name
        member this.Title = product.Title

        member private this.DownloadUrl (version:Version) =
            match version.Source with
            | Compile ->
                match product with
                | Odbc ->               
                    sprintf "%s/kibana/kibana-%s-windows-x86.zip" ArtifactDownloadsUrl version.FullVersion 
            | Released ->
                sprintf "%s/%s/%s-%s.msi" ArtifactDownloadsUrl this.Name this.Name version.FullVersion 
            | BuildCandidate hash ->
                if (version.FullVersion.EndsWith("snapshot", StringComparison.OrdinalIgnoreCase)) then
                    SnapshotDownloadsUrl this.Name (sprintf "%i.%i.%i" version.Major version.Minor version.Patch) hash version.FullVersion
                else StagingDownloadsUrl this.Name hash version.FullVersion

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

        member this.DownloadPath (version:Version) =
            let fullPathInDir = InDir |> Path.GetFullPath 
            let releaseFile version dir =
                let downloadUrl = this.DownloadUrl version     
                Path.Combine(fullPathInDir, dir, Path.GetFileName downloadUrl)
            match version.Source with
            | Compile ->  this.ZipFile version
            | Released -> releaseFile version "releases"
            | BuildCandidate hash -> releaseFile version hash

        member this.Download () =
            this.Versions
            |> List.iter (fun version ->
                let useSnapshots = getBuildParamOrDefault "snapshots" "$false"
                let zipFile = this.DownloadPath version
                let extractedDirectory = this.ExtractedDirectory version
                
                if (useSnapshots = "$true") then
                    if (File.Exists(zipFile)) then
                        tracefn "Deleting snapshot zip file: %s" zipFile
                        File.Delete(zipFile)
                    if (Directory.Exists(extractedDirectory)) then
                        tracefn "Deleting snapshot existing directory: %s" extractedDirectory
                        Directory.Delete(extractedDirectory, true)

                match (this.DownloadUrl version, zipFile) with
                | (_, file) when fileExists file ->
                    tracefn "Already downloaded %s to %s" this.Name file
                | (url, file) ->
                    tracefn "Downloading %s from %s" this.Name url 
                    let targetDirectory = file |> Path.GetDirectoryName
                    if (directoryExists targetDirectory |> not) then CreateDir targetDirectory
                    use webClient = new System.Net.WebClient()
                    (url, file) |> webClient.DownloadFile
                    tracefn "Done downloading %s from %s to %s" this.Name url file 

                match version.Source with
                | Compile -> 
                    if directoryExists extractedDirectory |> not
                    then
                        tracefn "Unzipping %s %s" this.Name zipFile
                        Unzip InDir zipFile
                        match this.Product with
                            | Kibana ->
                                let original = sprintf "kibana-%s-windows-x86" version.FullVersion
                                if directoryExists original |> not then
                                    Rename (InDir @@ (sprintf "kibana-%s" version.FullVersion)) (InDir @@ original)
                            | _ -> ()
                        
                        // Snapshots need renaming as folder inside zip is named differently
                        // An example: The hosted zip filename is:
                        // https://snapshots.elastic.co/7.0.0-alpha1-ea57ee52/downloads/elasticsearch/elasticsearch-7.0.0-alpha1-SNAPSHOT.zip
                        // This is downloaded locally into the in dir as: elasticsearch-7.0.0-alpha1-ea57ee52.zip
                        // When extracted it creates a folder called: elasticsearch-7.0.0-alpha1-snapshot
                        // This is then renamed to elasticsearch-7.0.0-alpha1-ea57ee52
                        if (useSnapshots = "$true") then
                            let existing = InDir @@ (sprintf "%s-%s" product.Name (Snapshots.GetVersionsFiltered version.Major version.Minor version.Patch version.Prerelease |> Seq.head))
                            let target = InDir @@ (sprintf "%s-%s" product.Name version.FullVersion)
                            Rename target existing

                    else tracefn "Extracted directory %s already exists" extractedDirectory   
                | _ -> ()
            )

        static member CreateFromProduct (productToVersion:Product -> Version list) (product: Product)  =
            ProductVersions(product, productToVersion product)


