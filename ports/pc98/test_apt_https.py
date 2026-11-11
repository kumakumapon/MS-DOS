import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from urllib.error import URLError


spec = importlib.util.spec_from_file_location(
    "apt_https", Path(__file__).resolve().parents[2] / ".github/scripts/apt-https.py"
)
apt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(apt)


class Response(io.BytesIO):
    status = 200


class AptHttpsTests(unittest.TestCase):
    def test_dns_failure_uses_reachable_mirror(self):
        calls = []

        def fetch(url, timeout):
            calls.append((url, timeout))
            if len(calls) == 1:
                raise URLError("Temporary failure resolving archive.ubuntu.com")
            return Response(b"-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA512\n")

        self.assertEqual(apt.available_mirror("resolute", fetch), apt.UBUNTU_MIRRORS[1])
        self.assertEqual(len(calls), 2)
        self.assertTrue(all(url.endswith("dists/resolute/InRelease") and timeout == 5 for url, timeout in calls))

    def test_bad_responses_fail_closed_after_finite_candidates(self):
        calls = []

        def fetch(url, timeout):
            calls.append(url)
            return Response(b"<html>temporary outage</html>")

        with self.assertRaisesRegex(RuntimeError, "refusing stale indexes"):
            apt.available_mirror("resolute", fetch)
        self.assertEqual(len(calls), len(apt.UBUNTU_MIRRORS))
        with self.assertRaises(ValueError):
            apt.available_mirror("../resolute", fetch)

    def test_actual_mirror_file_and_security_source_preserve_archive_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sources.list.d").mkdir()
            source = root / "sources.list.d/ubuntu.sources"
            original = (
                "Types: deb\nURIs: mirror+file:/etc/apt/apt-mirrors.txt\n"
                "Suites: resolute resolute-updates resolute-security\nComponents: main universe\n"
                "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n"
            )
            source.write_text(original)
            mirrors = root / "apt-mirrors.txt"
            mirrors.write_text("http://azure.archive.ubuntu.com/ubuntu/\nhttps://security.ubuntu.com/ubuntu\n")
            other = root / "sources.list.d/vendor.list"
            other.write_text("deb https://packages.microsoft.com/repos/code stable main\n")
            apt.configure(root, apt.UBUNTU_MIRRORS[1])
            self.assertEqual(source.read_text(), original)
            self.assertEqual(mirrors.read_text(), apt.UBUNTU_MIRRORS[1] + "\n" + apt.UBUNTU_MIRRORS[1].rstrip("/") + "\n")
            self.assertEqual(other.read_text(), "deb https://packages.microsoft.com/repos/code stable main\n")
            with self.assertRaises(ValueError):
                apt.configure(root, "http://untrusted.invalid/")


if __name__ == "__main__":
    unittest.main()
