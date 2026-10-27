"""Linux-only offline runner. All generated artifacts go to --out or a temp dir."""
from pathlib import Path
import argparse, hashlib, json, subprocess, sys, tempfile
from common import image_argument, load_image, guard_regions, fnv

HERE=Path(__file__).resolve().parent

def run(args,out):
    source=args.source_root.resolve()
    for name in ("news_refresh.c","news_refresh_policy.h","news_refresh_guards.h"):
        if not (source/name).is_file():
            raise SystemExit("Source root must contain the three reviewed news-refresh files.")
    image=image_argument(args.image)
    data,pe,mapped=load_image(image)
    image_sha=hashlib.sha256(data).hexdigest()
    guards=guard_regions(source)
    mutations=0
    for row in guards:
        start=row["va"]-pe.OPTIONAL_HEADER.ImageBase
        raw=mapped[start:start+row["length"]]
        assert fnv(raw)==row["hash"],"Exact native guard differs: "+row["name"]
        for i in range(len(raw)):
            changed=bytearray(raw);changed[i]^=1
            assert fnv(changed)!=row["hash"],"Guard mutation was not rejected"
            mutations+=1
    controls=[]
    builds=[("candidate",None,None),("baseline","NR_DISABLE_REFRESH","repeated"),
            ("leak-control","NR_SKIP_FREE","repeated"),
            ("zero-body-control","NR_NO_PENDING_PROMOTION","zero-body")]
    positive=None
    for name,flag,selection in builds:
        library=out/(name+".so")
        command=[args.cc,"-std=c11","-O2","-Wall","-Wextra","-Werror","-fPIC","-shared",
                 "-I",str(source),str(HERE/"policy_bridge.c"),"-o",str(library)]
        if flag:command.insert(1,"-D"+flag)
        subprocess.run(command,check=True)
        command=[sys.executable,"-B",str(HERE/"test_native_lifecycle.py"),"--image",str(image),
                 "--library",str(library),"--out",str(out)]
        if selection:command.extend(["--select",selection])
        result=subprocess.run(command,capture_output=True,text=True)
        (out/(name+".log")).write_text(result.stdout+result.stderr,encoding="utf-8")
        if not flag:
            assert result.returncode==0,result.stdout+result.stderr
            positive=json.loads((out/"candidate-result.json").read_text())
        else:
            assert result.returncode!=0 and "AssertionError" in result.stderr and "UcError" not in result.stderr,(name,result.stdout,result.stderr)
            controls.append(dict(control=name,flag=flag,contour=selection,expected_failure=True))
    win32="not_requested"
    if args.win32_cc:
        subprocess.run([args.win32_cc,"-std=gnu11","-O2","-Wall","-Wextra","-Werror",
                        "-I",str(source),"-include",str(HERE/"compile_shim.h"),"-c",str(HERE/"compile_only.c"),
                        "-o",str(out/"news_refresh_compile.o")],check=True)
        win32="pass"
    result=dict(status="offline_pass",image_sha256=image_sha,scenario_groups=positive["passed"],
                negative_controls=controls,exact_guard_regions=len(guards),
                single_byte_guard_mutations_rejected=mutations,win32_compile=win32,
                source_sha256={name:hashlib.sha256((source/name).read_bytes()).hexdigest()
                               for name in ("news_refresh.c","news_refresh_policy.h","news_refresh_guards.h")},
                boundary=positive["boundary"],real_C1_acceptance=False)
    (out/"regression-result.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image",help="Owned private C1 image; otherwise CODOL_NEWS_TEST_IMAGE")
    parser.add_argument("--source-root",type=Path,default=HERE.parents[2],help="Client source root containing the three news files")
    parser.add_argument("--out",type=Path,help="External directory for generated test artifacts; defaults to an ephemeral temp directory")
    parser.add_argument("--cc",default="gcc")
    parser.add_argument("--win32-cc",help="Optional i686-w64-mingw32-gcc executable for compile-only verification")
    args=parser.parse_args()
    if args.out:
        out=args.out.expanduser().resolve()
        if out.is_relative_to(HERE.parents[2]) or out.is_relative_to(args.source_root.resolve()):
            raise SystemExit("Place --out outside the client/source repository; generated private-image test artifacts must not enter Git.")
        out.mkdir(parents=True,exist_ok=True);run(args,out)
    else:
        with tempfile.TemporaryDirectory(prefix="codol-news-refresh-") as temp:run(args,Path(temp))

if __name__=="__main__":main()
