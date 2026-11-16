"""Unit tests for the DevKit release packager (Roadmap Phase 9, known issue D1).

CMake is simulated by a fake command runner: `cmake --install ... --prefix <dir>` writes a
package tree (with or without the executable), every other command succeeds. The real CMake
path is covered by the opt-in CTest tests package.build / package.smoke and by CI.
"""

import io
import json
import os
import plistlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from contextlib import redirect_stdout
from pathlib import Path
from typing import List, Sequence
from unittest import mock

from tools.devkit import cli, packager
from tools.devkit.packager import (
    CHECKSUM_FILE,
    calculate_sha256,
    check_package_tree,
    create_release_package,
    find_cmake,
    update_checksums,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
SMOKE_SCRIPT = REPO_ROOT / "tools" / "ci" / "package_smoke.py"


def package_info(os_name: str = "linux", version: str = "0.9.0", fmt: str = "tar.gz") -> dict:
    platform = {"linux": "linux-x86_64", "windows": "windows-x64", "macos": "macos-universal"}[os_name]
    executable = {"linux": "neo_wolf", "windows": "neo_wolf.exe",
                  "macos": "NeoWolf.app/Contents/MacOS/NeoWolf"}[os_name]
    content_root = "NeoWolf.app/Contents/Resources" if os_name == "macos" else "."
    return {"schema": 1, "version": version, "os": os_name, "platform": platform,
            "package": f"neo-wolf-{version}-{platform}", "format": fmt, "executable": executable,
            "content_root": content_root, "compiler": "Fake", "multi_config": False}


def write_tree(prefix: Path, info: dict, with_exe: bool = True, extra: Sequence[str] = ()) -> None:
    """What `cmake --install` writes, from the real game/ folder."""
    content = prefix / info["content_root"]
    shutil.copytree(REPO_ROOT / "game", content / "game")
    if with_exe:
        exe = prefix / info["executable"]
        exe.parent.mkdir(parents=True, exist_ok=True)
        exe.write_bytes(b"\x7fELF fake game")
        exe.chmod(0o755)
    if info["os"] == "macos":
        with open(prefix / "NeoWolf.app" / "Contents" / "Info.plist", "wb") as handle:
            plistlib.dump({"CFBundleExecutable": "NeoWolf", "CFBundleShortVersionString": info["version"]}, handle)
    (prefix / "README.txt").write_text(f"Neo-Wolf {info['version']}\nFree for noncommercial use; see LICENSE.md.\n",
                                       encoding="utf-8")
    shutil.copy2(REPO_ROOT / "LICENSE.md", prefix / "LICENSE.md")
    (prefix / "docs").mkdir(parents=True, exist_ok=True)
    for doc in ("CONTROLS.md", "TROUBLESHOOTING.md"):
        shutil.copy2(REPO_ROOT / "docs" / doc, prefix / "docs" / doc)
    (prefix / "LICENSES").mkdir(parents=True, exist_ok=True)
    (prefix / "LICENSES" / "SDL2-LICENSE.txt").write_text("zlib licence\n", encoding="utf-8")
    (prefix / "LICENSES" / "SDL2-hidapi-LICENSE-bsd.txt").write_text("BSD licence\n", encoding="utf-8")
    (prefix / "LICENSES" / "SDL2-yuv2rgb-LICENSE.txt").write_text("BSD licence\n", encoding="utf-8")
    shutil.copy2(REPO_ROOT / "third_party" / "nlohmann" / "LICENSE.MIT",
                 prefix / "LICENSES" / "nlohmann-json-LICENSE.MIT.txt")
    shutil.copy2(REPO_ROOT / "docs" / "THIRD_PARTY_LICENSES.md", prefix / "LICENSES" / "THIRD_PARTY_LICENSES.md")
    for rel in extra:
        path = prefix / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"binary")


class FakeCMake:
    """Records commands; `--install --prefix P` writes a package tree into P."""

    def __init__(self, info: dict, with_exe: bool = True, extra: Sequence[str] = (), fail_on: str = "") -> None:
        self.info = info
        self.with_exe = with_exe
        self.extra = extra
        self.fail_on = fail_on
        self.commands: List[List[str]] = []

    def __call__(self, command: Sequence[str], cwd: Path) -> int:
        command = list(command)
        self.commands.append(command)
        if self.fail_on and self.fail_on in command:
            return 1
        if "--install" in command:
            write_tree(Path(command[command.index("--prefix") + 1]), self.info, self.with_exe, self.extra)
        return 0


class PackagerTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.out = self.base / "release out"

    def build_dir(self, info: dict) -> Path:
        build = self.base / "build tree"
        build.mkdir(parents=True, exist_ok=True)
        (build / "CMakeCache.txt").write_text("# fake\n", encoding="utf-8")
        (build / packager.INFO_FILE).write_text(json.dumps(info), encoding="utf-8")
        return build

    def package(self, fake: FakeCMake, **kwargs):
        kwargs.setdefault("build_dir", self.build_dir(fake.info))
        kwargs.setdefault("validate", False)
        kwargs.setdefault("system", "linux")
        return create_release_package(REPO_ROOT, self.out, cmake="cmake", runner=fake, **kwargs)


class TestPackagerFailures(PackagerTestCase):
    def test_fails_without_an_executable(self) -> None:
        """Known issue D1: a package without the game must fail, write no archive and exit non-zero."""
        fake = FakeCMake(package_info("linux"), with_exe=False)
        res = self.package(fake)
        self.assertFalse(res.success)
        self.assertIsNone(res.archive)
        self.assertTrue(any("no executable in the package" in e for e in res.errors), res.errors)
        self.assertEqual([p.name for p in self.out.glob("neo-wolf-*")], [])
        self.assertFalse((self.out / CHECKSUM_FILE).exists())

        # The same through the command line: exit code 1 and the reason on stdout.
        build = self.build_dir(fake.info)
        real = packager.create_release_package

        def without_exe(**kwargs):
            return real(**dict(kwargs, runner=FakeCMake(fake.info, with_exe=False), validate=False, cmake="cmake"))

        with mock.patch.object(cli, "create_release_package", without_exe):
            out = io.StringIO()
            with redirect_stdout(out):
                code = cli.main(["--no-color", "package", "--build-dir", str(build), "--out-dir", str(self.out)])
        self.assertEqual(code, 1, out.getvalue())
        self.assertIn("Packaging FAILED", out.getvalue())
        self.assertIn("no executable in the package", out.getvalue())

    def test_fails_without_an_executable_for_every_os(self) -> None:
        for os_name, fmt in (("macos", "zip"), ("windows", "zip"), ("linux", "tar.gz")):
            with self.subTest(os=os_name):
                shutil.rmtree(self.out, ignore_errors=True)
                res = self.package(FakeCMake(package_info(os_name, fmt=fmt), with_exe=False))
                self.assertFalse(res.success)
                self.assertTrue(any(package_info(os_name)["executable"] in e for e in res.errors), res.errors)
                self.assertEqual(list(self.out.glob("neo-wolf-*")), [])

    def test_refuses_test_and_autoplay_executables(self) -> None:
        res = self.package(FakeCMake(package_info("windows", fmt="zip"),
                                     extra=("neo_wolf_autoplay.exe", "bin/neo_wolf_tests.exe")))
        self.assertFalse(res.success)
        joined = "\n".join(res.errors)
        self.assertIn("neo_wolf_autoplay.exe must not be shipped", joined)
        self.assertIn("bin/neo_wolf_tests.exe must not be shipped", joined)

    def test_missing_docs_licences_or_content(self) -> None:
        info = package_info("linux")
        tree = self.base / "tree"
        write_tree(tree, info)
        self.assertEqual(check_package_tree(tree, info), [])
        (tree / "LICENSES" / "SDL2-LICENSE.txt").unlink()
        (tree / "LICENSE.md").unlink()
        shutil.rmtree(tree / "game" / "maps")
        (tree / "README.txt").write_text("no version here\n", encoding="utf-8")
        problems = "\n".join(check_package_tree(tree, info))
        self.assertIn("LICENSES/SDL2-LICENSE.txt is missing", problems)
        self.assertIn("LICENSE.md is missing", problems)
        self.assertIn("no game/maps/", problems)
        self.assertIn("README.txt does not mention version 0.9.0", problems)

    def test_macos_info_plist_must_match(self) -> None:
        info = package_info("macos", fmt="zip")
        tree = self.base / "tree"
        write_tree(tree, info)
        self.assertEqual(check_package_tree(tree, info), [])
        info_other = dict(info, version="1.2.3")
        (tree / "README.txt").write_text("Neo-Wolf 1.2.3\n", encoding="utf-8")
        self.assertIn("CFBundleShortVersionString", "\n".join(check_package_tree(tree, info_other)))

    def test_build_and_install_failures_stop_early(self) -> None:
        fake = FakeCMake(package_info("linux"), fail_on="--build")
        res = self.package(fake)
        self.assertFalse(res.success)
        self.assertIn("build failed", res.errors[-1])
        self.assertFalse(any("--install" in c for c in fake.commands))

        res = self.package(FakeCMake(package_info("linux"), fail_on="--install"))
        self.assertFalse(res.success)
        self.assertIn("cmake --install failed", res.errors[-1])

    def test_expect_version(self) -> None:
        res = self.package(FakeCMake(package_info("linux", version="0.9.0")), expect_version="1.0.0")
        self.assertFalse(res.success)
        self.assertIn("version mismatch", res.errors[-1])
        res = self.package(FakeCMake(package_info("linux", version="1.0.0")), expect_version="1.0.0")
        self.assertTrue(res.success, res.errors)

    def test_unconfigured_build_dir_and_missing_info(self) -> None:
        empty = self.base / "empty"
        empty.mkdir()
        res = self.package(FakeCMake(package_info("linux")), build_dir=empty)
        self.assertFalse(res.success)
        self.assertIn("not a configured CMake build tree", res.errors[-1])
        (empty / "CMakeCache.txt").write_text("", encoding="utf-8")
        res = self.package(FakeCMake(package_info("linux")), build_dir=empty)
        self.assertFalse(res.success)
        self.assertIn("neowolf-package.json not found", res.errors[-1])

    def test_content_validation_failure_packages_nothing(self) -> None:
        bad_root = self.base / "bad root"
        (bad_root / "game" / "maps").mkdir(parents=True)
        (bad_root / "game" / "defs").mkdir(parents=True)
        fake = FakeCMake(package_info("linux"))
        res = create_release_package(bad_root, self.out, build_dir=self.build_dir(fake.info), cmake="cmake",
                                     runner=fake, system="linux")
        self.assertFalse(res.success)
        self.assertIn("content validation failed", res.errors[-1])
        self.assertEqual(fake.commands, [])


class TestPackagerSuccess(PackagerTestCase):
    def test_zip_layout_permissions_and_checksums(self) -> None:
        info = package_info("macos", fmt="zip")
        res = self.package(FakeCMake(info))
        self.assertTrue(res.success, res.errors)
        assert res.archive is not None
        self.assertEqual(res.archive.name, "neo-wolf-0.9.0-macos-universal.zip")
        top = "neo-wolf-0.9.0-macos-universal/"
        with zipfile.ZipFile(res.archive) as zf:
            names = zf.namelist()
            self.assertEqual(names[0], top)
            self.assertTrue(all(n.startswith(top) for n in names))
            self.assertEqual(names[1:], sorted(names[1:]))
            exe = zf.getinfo(top + "NeoWolf.app/Contents/MacOS/NeoWolf")
            self.assertEqual((exe.external_attr >> 16) & 0o777, 0o755)
            readme = zf.getinfo(top + "README.txt")
            self.assertEqual((readme.external_attr >> 16) & 0o777, 0o644)
            self.assertIn(top + "NeoWolf.app/Contents/Resources/game/defs/campaign.json", names)
        sums = (self.out / CHECKSUM_FILE).read_text(encoding="utf-8")
        self.assertEqual(sums, f"{calculate_sha256(res.archive)}  {res.archive.name}\n")
        self.assertFalse((self.out / ".staging").exists())

    def test_tar_gz_for_linux(self) -> None:
        res = self.package(FakeCMake(package_info("linux")))
        self.assertTrue(res.success, res.errors)
        assert res.archive is not None
        self.assertTrue(res.archive.name.endswith("-linux-x86_64.tar.gz"))
        with tarfile.open(res.archive) as tf:
            exe = tf.getmember("neo-wolf-0.9.0-linux-x86_64/neo_wolf")
            self.assertEqual(exe.mode & 0o777, 0o755)
            self.assertEqual((exe.uid, exe.gid, exe.uname), (0, 0, ""))

    def test_executable_bit_forced_when_host_has_none(self) -> None:
        """A host without exec bits (Windows) must still produce 0755 executables in the
        archive; regression for CI run 43862713525, where both tests above failed on Windows."""
        tree = self.base / "tree-no-exec"
        exe_rel = "NeoWolf.app/Contents/MacOS/NeoWolf"
        (tree / exe_rel).parent.mkdir(parents=True)
        (tree / exe_rel).write_bytes(b"binary")
        (tree / "README.txt").write_text("readme", encoding="utf-8")
        os.chmod(tree / exe_rel, 0o644)  # what a Windows filesystem reports
        out = self.base / "archives"
        out.mkdir()
        zip_path = packager.write_archive(tree, out, "pkg", "zip", executables=[exe_rel])
        with zipfile.ZipFile(zip_path) as zf:
            self.assertEqual((zf.getinfo("pkg/" + exe_rel).external_attr >> 16) & 0o777, 0o755)
            self.assertEqual((zf.getinfo("pkg/README.txt").external_attr >> 16) & 0o777, 0o644)
        tar_path = packager.write_archive(tree, out, "pkg", "tar.gz", executables=[exe_rel])
        with tarfile.open(tar_path) as tf:
            self.assertEqual(tf.getmember("pkg/" + exe_rel).mode & 0o777, 0o755)
            self.assertEqual(tf.getmember("pkg/README.txt").mode & 0o777, 0o644)

    def test_checksums_keep_other_packages(self) -> None:
        self.out.mkdir(parents=True)
        other = self.out / "neo-wolf-0.9.0-windows-x64.zip"
        other.write_bytes(b"windows package")
        update_checksums(self.out, [other])
        res = self.package(FakeCMake(package_info("linux")))
        self.assertTrue(res.success, res.errors)
        lines = (self.out / CHECKSUM_FILE).read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(lines), 2)
        self.assertIn(f"{calculate_sha256(other)}  {other.name}", lines)
        # --clean removes earlier archives and the checksum file before packaging.
        res = self.package(FakeCMake(package_info("linux")), clean=True)
        self.assertTrue(res.success, res.errors)
        self.assertFalse(other.exists())
        self.assertEqual(len((self.out / CHECKSUM_FILE).read_text(encoding="utf-8").splitlines()), 1)

    def test_preset_mode_runs_configure_build_install_and_codesign(self) -> None:
        info = package_info("macos", fmt="zip")
        preset = "package-macos"
        root = self.base / "project"
        build = (root / "build" / preset)
        build.mkdir(parents=True)
        (build / packager.INFO_FILE).write_text(json.dumps(info), encoding="utf-8")
        fake = FakeCMake(info)
        res = create_release_package(root, self.out, cmake="cmake", runner=fake, system="darwin", validate=False)
        self.assertTrue(res.success, res.errors)
        self.assertEqual(fake.commands[0], ["cmake", "--preset", preset])
        self.assertEqual(fake.commands[1], ["cmake", "--build", "--preset", preset])
        self.assertEqual(fake.commands[2][:5], ["cmake", "--install", str(build.resolve()), "--config", "Release"])
        self.assertEqual(fake.commands[3][:4], ["codesign", "--verify", "--deep", "--strict"])
        self.assertTrue(fake.commands[3][-1].endswith("NeoWolf.app"))
        self.assertEqual(len(fake.commands), 4)

    def test_default_presets_per_os(self) -> None:
        self.assertEqual(packager.PACKAGE_PRESETS,
                         {"darwin": "package-macos", "windows": "package-windows-msvc", "linux": "package-linux"})
        presets = json.loads((REPO_ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
        configure = {p["name"] for p in presets["configurePresets"]}
        build = {p["name"] for p in presets["buildPresets"]}
        for preset in packager.PACKAGE_PRESETS.values():
            self.assertIn(preset, configure)
            self.assertIn(preset, build)

    def test_package_smoke_static_checks_pass_on_the_result(self) -> None:
        """tools/ci/package_smoke.py --no-run accepts what the packager writes, and notices tampering."""
        res = self.package(FakeCMake(package_info("linux")))
        self.assertTrue(res.success, res.errors)
        done = subprocess.run([sys.executable, str(SMOKE_SCRIPT), str(self.out), "--no-run"],
                              capture_output=True, text=True, timeout=120)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("PASS: 1 package(s)", done.stdout)
        assert res.archive is not None
        with open(res.archive, "ab") as handle:
            handle.write(b"tampered")
        done = subprocess.run([sys.executable, str(SMOKE_SCRIPT), str(self.out), "--no-run"],
                              capture_output=True, text=True, timeout=120)
        self.assertEqual(done.returncode, 1, done.stdout + done.stderr)
        self.assertIn("FAIL: SHA256SUMS matches the archive", done.stdout)


class TestFindCMakeAndLaunchers(unittest.TestCase):
    def test_find_cmake(self) -> None:
        self.assertEqual(find_cmake("linux", which=lambda name: "/usr/bin/cmake"), "/usr/bin/cmake")
        self.assertIsNone(find_cmake("darwin", which=lambda name: None))
        with tempfile.TemporaryDirectory() as empty:  # no vswhere there
            self.assertIsNone(find_cmake("windows", which=lambda name: None, environ={"ProgramFiles(x86)": empty}))

    def test_package_cmd(self) -> None:
        data = (REPO_ROOT / "package.cmd").read_bytes()
        self.assertNotIn(b"\n", data.replace(b"\r\n", b""))  # CRLF only (cmd.exe labels/blocks)
        text = data.decode("ascii")
        for part in ("setlocal", 'set "NEO_PY=py -3"', "sys.version_info >= (3, 8)",
                     '%NEO_PY% "%SCRIPT_DIR%tools\\devkit\\devkit.py" package %*',
                     'set "NEO_RC=%ERRORLEVEL%"', "endlocal & exit /b %NEO_RC%"):
            self.assertIn(part, text)

    def test_package_ps1(self) -> None:
        text = (REPO_ROOT / "package.ps1").read_text(encoding="utf-8")
        for part in ('@("py", "-3")', "$devkit package @args", "$code = $LASTEXITCODE", "exit $code"):
            self.assertIn(part, text)

    def test_package_sh(self) -> None:
        script = REPO_ROOT / "package.sh"
        text = script.read_text(encoding="utf-8")
        self.assertIn('exec "$PYTHON" "$SCRIPT_DIR/tools/devkit/devkit.py" package "$@"', text)
        bash = shutil.which("bash")
        if sys.platform == "win32" or bash is None:
            self.skipTest("bash: not on this platform")
        self.assertTrue(os.access(script, os.X_OK))
        old = subprocess.run([bash, str(script), "1.0.0", "release"], capture_output=True, text=True, timeout=60)
        self.assertEqual(old.returncode, 2, old.stdout + old.stderr)  # the old version argument is refused
        bad = subprocess.run([bash, str(script), "--no-such-option"], capture_output=True, text=True, timeout=60)
        self.assertEqual(bad.returncode, 2, bad.stdout + bad.stderr)  # argparse's exit code is passed on


if __name__ == "__main__":
    unittest.main()
