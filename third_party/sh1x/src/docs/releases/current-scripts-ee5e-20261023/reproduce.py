#!/usr/bin/env python3
"""Reproduce the exact reviewed ee5e script IWD from verified source bytes.

Uses the unchanged authoritative codol_package.build_pack with the pinned ordered
inventory. Writes only to a fresh external output directory. No clients, services,
Git mutations, game assets or credentials are involved.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def digest(body):
    return hashlib.sha256(body).hexdigest()


def git(root, *args, optional=False):
    result = subprocess.run(
        ["git", "--no-optional-locks", "-C", str(root), *args],
        capture_output=True,
        env=dict(os.environ, GIT_OPTIONAL_LOCKS="0"),
    )
    if result.returncode and not optional:
        raise ValueError("Git source inspection failed: " + result.stderr.decode("utf-8", "replace").strip())
    return result.stdout.decode("utf-8", "strict")


def source_path(root, relative):
    rel = PurePosixPath(relative)
    if rel.is_absolute() or not rel.parts or ".." in rel.parts or "\\" in relative or ":" in relative:
        raise ValueError("Unsafe manifest source path: " + relative)
    path = root.joinpath(*rel.parts)
    if not path.resolve().is_relative_to(root) or path.is_symlink() or not path.is_file():
        raise ValueError("Missing, linked or out-of-root source: " + relative)
    return path


def committed_raw_sources(root, commit):
    listing = git(root, "ls-tree", "-r", "-z", commit)
    entries = {}
    for record in filter(None, listing.split("\0")):
        header, name = record.split("\t", 1)
        mode, kind, oid = header.split()
        if kind != "blob" or mode not in ("100644", "100755"):
            raise ValueError("Unsupported committed raw entry: " + name)
        entries[name] = oid
    oids = list(dict.fromkeys(entries.values()))
    process = subprocess.run(
        ["git", "--no-optional-locks", "-C", str(root), "cat-file", "--batch"],
        input=("\n".join(oids) + "\n").encode("ascii"), capture_output=True,
        env=dict(os.environ, GIT_OPTIONAL_LOCKS="0"), check=True,
    )
    blobs = {}
    cursor = 0
    for expected in oids:
        stop = process.stdout.index(b"\n", cursor)
        oid, kind, size = process.stdout[cursor:stop].decode("ascii").split()
        if oid != expected or kind != "blob":
            raise ValueError("Unexpected committed raw blob response")
        cursor = stop + 1
        size = int(size)
        blobs[oid] = process.stdout[cursor:cursor + size]
        cursor += size
        if process.stdout[cursor:cursor + 1] != b"\n":
            raise ValueError("Truncated committed raw blob response")
        cursor += 1
    if cursor != len(process.stdout):
        raise ValueError("Unexpected committed raw blob trailer")
    return {name: (oid, blobs[oid]) for name, oid in entries.items()}


def archive_bytes_from_canonical(body, policy):
    if policy == {"kind": "identity"}:
        return body
    if set(policy) != {"kind", "canonical_lf_count", "archive_crlf_count"} or policy["kind"] != "reviewed-lf-to-crlf":
        raise ValueError("Unsupported line-ending policy")
    count = body.count(b"\n")
    if b"\r" in body or count <= 0 or policy["canonical_lf_count"] != count or policy["archive_crlf_count"] != count:
        raise ValueError("Mismatched line-ending policy")
    return body.replace(b"\n", b"\r\n")


def reproduce(native_root, raw_root, output, allow_reviewed_working_tree=False):
    manifest_path = HERE / "member-order.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest["schema"] != "codol-exact-ee5e-source-recipe-v2":
        raise ValueError("Unsupported release manifest")
    if digest(Path(__file__).read_bytes()) != manifest["reproducer_sha256"]:
        raise ValueError("Release reproducer drift")
    native_root = Path(native_root).resolve()
    raw_root = Path(raw_root).resolve()
    supplied_output = Path(output)
    if not supplied_output.is_absolute():
        raise ValueError("Output must be an explicit absolute new directory")
    output = supplied_output.resolve()
    if output.exists() or output.is_relative_to(native_root) or output.is_relative_to(raw_root):
        raise ValueError("Output must be fresh and outside both source roots")
    if Path(git(raw_root, "rev-parse", "--show-toplevel").strip()).resolve() != raw_root:
        raise ValueError("Raw root is not an initialized repository root")
    raw_status = git(raw_root, "status", "--porcelain=v1", "--untracked-files=all")
    raw_merge = git(raw_root, "rev-parse", "-q", "--verify", "MERGE_HEAD", optional=True).strip()
    if (raw_status or raw_merge) and not allow_reviewed_working_tree:
        raise ValueError("Raw source is uncommitted; use the explicit reviewed-working-tree flag only for pre-closure QA")
    raw_head = git(raw_root, "rev-parse", "HEAD").strip()
    if raw_head != manifest["canonical_raw_commit"]:
        raise ValueError("Raw HEAD differs from the published canonical release commit")
    canonical = committed_raw_sources(raw_root, raw_head)

    for name, expected in manifest["required_recipe_files"].items():
        if digest(source_path(native_root, name).read_bytes()) != expected:
            raise ValueError("Recipe/native profile input drift: " + name)

    trees = tuple(tuple(row) for row in manifest["pack_trees"])
    ordered = {tree.rstrip("/"): [] for tree, _prefix in trees}
    source_bytes = {}
    member_names = []
    expected_raw = set()
    expected_support = {tree.rstrip("/"): set() for tree, _prefix in trees[1:]}
    representations = {"canonical": 0, "reviewed_archive": 0, "identity": 0}
    restored_crlf = 0
    for item in manifest["ordered_members"]:
        name, relative = item["member"], item["source"]
        if name in member_names or relative in source_bytes:
            raise ValueError("Duplicate ordered member/source")
        for tree, prefix in trees:
            if relative.startswith(tree) and name == prefix + relative[len(tree):]:
                break
        else:
            raise ValueError("Member/source mapping is outside the exact pack trees")
        if tree == trees[0][0]:
            raw_relative = relative[len(tree):]
            path = source_path(raw_root, raw_relative)
            expected_raw.add(raw_relative)
            oid, committed = canonical[raw_relative]
            pin = item["canonical_source"]
            if oid != pin["git_blob_oid"] or len(committed) != pin["bytes"] or digest(committed) != pin["sha256"]:
                raise ValueError("Canonical Git blob drift: " + raw_relative)
            body = archive_bytes_from_canonical(committed, item["line_ending_policy"])
            working = path.read_bytes()
            if working == committed:
                representations["identity" if body == committed else "canonical"] += 1
            elif len(working) == item["bytes"] and digest(working) == item["sha256"]:
                representations["reviewed_archive"] += 1
            else:
                raise ValueError("Source drift: " + relative)
            if item["line_ending_policy"]["kind"] == "reviewed-lf-to-crlf":
                restored_crlf += 1
        else:
            path = source_path(native_root, relative)
            expected_support[tree.rstrip("/")].add(relative)
            if item["line_ending_policy"] != {"kind": "identity"}:
                raise ValueError("Support member line-ending policy must be identity")
            body = path.read_bytes()
        if len(body) != item["bytes"] or digest(body) != item["sha256"]:
            raise ValueError("Archive member/policy mismatch: " + relative)
        source_bytes[relative] = body
        ordered[tree.rstrip("/")].append(relative)
        member_names.append(name)

    if len(member_names) != manifest["member_count"]:
        raise ValueError("Manifest member count changed")
    raw_inventory = set(filter(None, git(raw_root, "ls-files", "-c", "-o", "--exclude-standard", "-z").split("\0")))
    raw_inventory = {name for name in raw_inventory if PurePosixPath(name).name not in manifest["pack_skip"]}
    canonical_inventory = {name for name in canonical if PurePosixPath(name).name not in manifest["pack_skip"]}
    if raw_inventory != expected_raw or canonical_inventory != expected_raw:
        raise ValueError("Raw inventory differs from the complete reviewed tree")
    for tree, expected in expected_support.items():
        actual = set(filter(None, git(native_root, "ls-files", "-z", "--", tree).split("\0")))
        if actual != expected:
            raise ValueError("Tracked support inventory drift: " + tree)

    # Everything is verified before the first output write. Retain the exact
    # copied inputs, without hardlinks, for review of the produced artifact.
    output.mkdir(parents=True, exist_ok=False)
    frozen = output / "verified-source"
    for relative, body in source_bytes.items():
        dest = frozen.joinpath(*PurePosixPath(relative).parts)
        dest.parent.mkdir(parents=True, exist_ok=True)
        with dest.open("xb") as handle:
            handle.write(body)

    tool = native_root / "tools/codol_package.py"
    sys.path.insert(0, str(tool.parent))
    spec = importlib.util.spec_from_file_location("ee5e_authoritative_packager", tool)
    pack = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(pack)
    if tuple(pack.PACK_TREES) != trees or set(pack.PACK_SKIP) != set(manifest["pack_skip"]):
        raise ValueError("Authoritative pack tree policy changed")
    pack.REPO = str(frozen)

    def verified_order(root, subpath=""):
        if Path(root).resolve() != frozen.resolve() or subpath not in ordered:
            raise ValueError("Unexpected authoritative packager source request")
        return list(ordered[subpath])

    pack.git_files = verified_order
    archive = output / "sh1x_svr_scripts.iwd"
    count = pack.build_pack(str(archive))
    if count != manifest["member_count"] or digest(archive.read_bytes()) != manifest["iwd_sha256"]:
        raise ValueError("Produced archive does not equal exact ee5e bytes; output retained for inspection")
    with zipfile.ZipFile(archive) as contents:
        if contents.namelist() != member_names:
            raise ValueError("Produced archive order changed")
        for item in manifest["ordered_members"]:
            if digest(contents.read(item["member"])) != item["sha256"]:
                raise ValueError("Produced member drift: " + item["member"])
    receipt = {
        "schema": "codol-exact-ee5e-reproduction-v2",
        "manifest_sha256": digest(manifest_path.read_bytes()),
        "reproducer_sha256": digest(Path(__file__).read_bytes()),
        "packager_sha256": digest(tool.read_bytes()),
        "raw_head": raw_head,
        "canonical_git_blob_pins_verified": len(expected_raw),
        "raw_input_representations": representations,
        "explicit_crlf_archive_policies": restored_crlf,
        "raw_merge_head": raw_merge or None,
        "raw_clean": not bool(raw_status or raw_merge),
        "reviewed_working_tree_explicitly_allowed": bool(allow_reviewed_working_tree),
        "native_head": git(native_root, "rev-parse", "HEAD").strip(),
        "native_profile_manifest_sha256_at_reproduction": digest((native_root / "native_profiles/manifest.json").read_bytes()),
        "members": count,
        "raw_members": len(expected_raw),
        "support_members": sum(len(values) for values in expected_support.values()),
        "iwd_bytes": archive.stat().st_size,
        "iwd_sha256": digest(archive.read_bytes()),
        "all_member_source_hashes_verified": True,
        "native_build_or_C1_test_performed_by_this_recipe": False,
        "publication_or_runtime_changed": False,
    }
    with (output / "reproduction.json").open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(receipt, handle, indent=2)
        handle.write("\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", type=Path, default=HERE.parents[2])
    parser.add_argument("--raw-root", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--allow-reviewed-working-tree", action="store_true")
    args = parser.parse_args()
    raw = args.raw_root or args.native_root / "bin/home/codol_rawfiles"
    try:
        receipt = reproduce(args.native_root, raw, args.output, args.allow_reviewed_working_tree)
    except (AssertionError, KeyError, ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(2, "REJECT: " + str(error) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
