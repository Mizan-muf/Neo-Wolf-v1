"""Unit tests for DevKit release packaging."""

import tempfile
import unittest
from pathlib import Path

from tools.devkit.packager import create_release_package


class TestPackager(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.dir_path = Path(self.temp_dir.name)

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_package_real_project(self) -> None:
        root = Path(__file__).resolve().parent.parent.parent.parent
        out_dir = self.dir_path / "dist"

        res = create_release_package(
            project_root=root,
            output_dir=out_dir,
            version="1.0.0-test",
            build_executable=False,
            create_zip=True,
            create_tar=True,
        )

        self.assertTrue(res.success)
        self.assertTrue(res.package_dir.exists())
        self.assertIsNotNone(res.zip_path)
        assert res.zip_path is not None
        self.assertTrue(res.zip_path.exists())
        self.assertIsNotNone(res.tar_path)
        assert res.tar_path is not None
        self.assertTrue(res.tar_path.exists())

        # Check launch scripts and docs
        self.assertTrue((res.package_dir / "launch.sh").is_file())
        self.assertTrue((res.package_dir / "launch.cmd").is_file())
        self.assertTrue((res.package_dir / "README.txt").is_file())
        self.assertTrue((res.package_dir / "RELEASE_NOTES.md").is_file())
        self.assertTrue((res.package_dir / "SHA256SUMS").is_file())
        self.assertTrue((res.package_dir / "game" / "maps" / "level01.map").is_file())


if __name__ == "__main__":
    unittest.main()
