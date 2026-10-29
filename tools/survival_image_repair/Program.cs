// Read-only asset porting helper: exact installed Survival references, real donor IFS data, encrypted/decoded verification.
using System.Reflection;
using Codol.FastFiles.Ifs;
using Codol.FastFiles.Iwd;
using System.Text.Json;
using Codol.FastFiles;
using Codol.FastFiles.Porting;
using Codol.Assets.Game;
using Codol.Assets.Porting;
var zone=args[0]; var targetRoot=args[1]; var sourceRoot=args[2]; var outFile=args[3];
var assembly=Assembly.Load("CodolSoundDumper");
var type=assembly.GetTypes().Single(t=>t.Name=="PortCommands");
var capture=type.GetMethod("CaptureFile",BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,new object[]{zone,Path.Combine(targetRoot,"C1.exe")});
var image=(ZoneImage)capture.GetType().GetProperty("Image").GetValue(capture);
using var source=GameInstallation.Open(sourceRoot);
using var target=GameInstallation.Open(targetRoot);
var wanted=MapImagePorter.CollectReferences(GameBuild.ForFastFile(zone),new[]{image}).Select(x=>"main/images/"+x.Name.TrimStart(',')+".iwi").ToHashSet(StringComparer.OrdinalIgnoreCase);
var packageFolder=GameFiles.FindPackageFolder(source.RootDirectory);
var mount=GameFiles.GetMount(packageFolder);
var files=mount.Files.Where(f=>wanted.Contains(f.Path)).ToDictionary(f=>f.Path,StringComparer.OrdinalIgnoreCase);
Console.WriteLine($"Reading only {files.Count} matching donor image entries");
var filtered=(IfsMount)typeof(IfsMount).GetConstructors(BindingFlags.NonPublic|BindingFlags.Instance).Single().Invoke(new object[]{mount.Folder,mount.Packages,files});
var loose=LooseFileArchive.Open(filtered,"main/images","images/",source.IwdKey);
var set=IwdArchiveSet.FromArchives(new[]{loose},null);
typeof(GameInstallation).GetField("_imageArchives",BindingFlags.NonPublic|BindingFlags.Instance).SetValue(source,set);
typeof(GameInstallation).GetField("_strippedImageArchives",BindingFlags.NonPublic|BindingFlags.Instance).SetValue(source,IwdArchiveSet.FromArchives(Array.Empty<IGameFileArchive>(),null));
Console.WriteLine("Captured installed Survival zone; collecting exact streamed image dependencies from original donor");
var result=MapImagePorter.Port(GameBuild.ForFastFile(zone),new[]{image},source,target,outFile,false);
File.WriteAllText(outFile+".json",JsonSerializer.Serialize(result,new JsonSerializerOptions{WriteIndented=true}));
foreach(var group in result.Images.GroupBy(x=>x.Status))Console.WriteLine($"{group.Key}: {group.Count()}");
Console.WriteLine($"Wrote {result.EntryCount} exact dependencies to {result.OutputPath}");

if(result.OutputPath is null) {
    var absent=result.Images.Count(i=>i.Status==MapImageStatus.Missing || i.Status==MapImageStatus.Failed);
    Console.WriteLine($"No repair archive needed/written; unavailable dependencies {absent}");
    Environment.ExitCode=absent==0 ? 0 : 1;
    return;
}
var bytes=MapImagePorter.ReadBack(outFile,target);
Console.WriteLine($"Read back {bytes.Count} encrypted names/data; all CRCs match");
var checks=MapImagePorter.Verify(outFile,result.Images,source,target);
File.WriteAllText(outFile+".verify.json",JsonSerializer.Serialize(checks,new JsonSerializerOptions{WriteIndented=true}));
var bad=checks.Where(c=>c.Problem is not null || c.DifferentPixels!=0).ToList();
Console.WriteLine($"Verified {checks.Count} images, {checks.Sum(c=>c.Surfaces)} decoded surfaces; failures {bad.Count}");
foreach(var failure in bad)Console.WriteLine(failure);
if(bad.Count>0 || result.Images.Any(i=>i.Status==MapImageStatus.Missing || i.Status==MapImageStatus.Failed))Environment.ExitCode=1;
