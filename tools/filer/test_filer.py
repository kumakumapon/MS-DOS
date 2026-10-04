"""Exercise real file operations in the native engine, including failed copies."""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

HOST = Path(__file__).resolve().parent / 'build/filer-host'


class FilerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)
        for drive in ('A', 'B'):
            (self.root / drive).mkdir()

    def run_filer(self, *args, error=0, env=None):
        result = subprocess.run([str(HOST), *args], cwd=self.root,
                                env={**os.environ, **(env or {})}, text=True,
                                capture_output=True, timeout=10)
        self.assertEqual(result.stderr, '')
        self.assertEqual(result.returncode, 1 if error else 0, result.stdout)
        if error:
            self.assertIn(f'ERROR {error}', result.stdout)
        return result.stdout

    def test_paths_case_and_parent(self):
        for value, expected in [
            ('dir/../readme.txt', 'A:\\README.TXT'),
            ('B:/sub/./file.bin', 'B:\\SUB\\FILE.BIN'),
            ('../../FILE', 'A:\\FILE'),
            ('\\SUB\\..', 'A:\\'),
        ]:
            self.assertEqual(self.run_filer('PATH', value).strip(), expected)

    def test_reject_invalid_and_device_names(self):
        for path in ('', 'LONGNAME9.TXT', 'A:REL.TXT', 'A:\\A*.TXT', 'NUL.TXT',
                     'CON', 'PRN.DAT', 'CLOCK$', 'COM1.TXT', 'COM9', 'LPT9', 'A:B', 'A B.TXT',
                     'A:\\' + '\\'.join(['ABCDEFGH'] * 16)):
            with self.subTest(path=path):
                self.run_filer('PATH', path, error=123)

    def test_binary_and_empty_copy_and_metadata(self):
        for content in (b'', bytes(range(256)) * 30):
            source = self.root / 'A/SRC.BIN'
            destination = self.root / 'B/DST.BIN'
            source.write_bytes(content)
            os.utime(source, (1780000000, 1780000000))
            self.run_filer('COPY', 'SRC.BIN', 'B:\\DST.BIN')
            self.assertEqual(destination.read_bytes(), content)
            self.assertEqual(source.read_bytes(), content)
            self.assertEqual(int(destination.stat().st_mtime), 1780000000)
            destination.unlink()

    def test_no_overwrite_same_file_or_missing_source(self):
        source = self.root / 'A/SRC.TXT'
        target = self.root / 'A/DST.TXT'
        source.write_bytes(b'SOURCE'); target.write_bytes(b'KEEP')
        for operation in ('COPY', 'MOVE', 'RENAME'):
            self.run_filer(operation, 'SRC.TXT', 'DST.TXT', error=80)
            self.run_filer(operation, 'SRC.TXT', 'src.txt', error=80)
        self.assertEqual(source.read_bytes(), b'SOURCE')
        self.assertEqual(target.read_bytes(), b'KEEP')
        self.run_filer('COPY', 'MISSING.TXT', 'NEW.TXT', error=2)
        self.assertFalse((self.root / 'A/NEW.TXT').exists())

    def test_failed_read_short_write_close_and_cancel_remove_partial(self):
        source = self.root / 'A/SRC.BIN'
        source.write_bytes(b'RETAIN' * 1000)
        for env, error in [
            ({'FD_FAIL_READ': '1'}, 5), ({'FD_WRITE_LIMIT': '0'}, 112),
            ({'FD_WRITE_LIMIT': '2100'}, 112), ({'FD_FAIL_CLOSE': '1'}, 5),
            ({'FD_ABORT_COPY': '1'}, 995),
        ]:
            with self.subTest(env=env):
                self.run_filer('COPY', 'SRC.BIN', 'NEW.BIN', error=error, env=env)
                self.assertFalse((self.root / 'A/NEW.BIN').exists())
                self.assertEqual(source.read_bytes(), b'RETAIN' * 1000)

    def test_move_same_drive_and_cross_drive(self):
        source = self.root / 'A/SRC.TXT'
        source.write_bytes(b'MOVE')
        self.run_filer('MOVE', 'SRC.TXT', 'NEW.TXT')
        self.assertFalse(source.exists())
        self.run_filer('MOVE', 'NEW.TXT', 'B:\\FINAL.TXT')
        self.assertFalse((self.root / 'A/NEW.TXT').exists())
        self.assertEqual((self.root / 'B/FINAL.TXT').read_bytes(), b'MOVE')

    def test_failed_cross_drive_delete_rolls_back_and_reports_cleanup_failure(self):
        source = self.root / 'A/SRC.TXT'
        source.write_bytes(b'RETAIN')
        self.run_filer('MOVE', 'SRC.TXT', 'B:\\DST.TXT', error=5,
                       env={'FD_FAIL_UNLINK': 'SRC.TXT'})
        self.assertFalse((self.root / 'B/DST.TXT').exists())
        self.run_filer('MOVE', 'SRC.TXT', 'B:\\DST.TXT', error=996,
                       env={'FD_FAIL_UNLINK': 'ALL'})
        self.assertEqual(source.read_bytes(), b'RETAIN')
        self.assertEqual((self.root / 'B/DST.TXT').read_bytes(), b'RETAIN')

    def test_read_only_copy_and_delete(self):
        source = self.root / 'A/SRC.TXT'
        source.write_bytes(b'READ ONLY'); source.chmod(0o400)
        self.run_filer('COPY', 'SRC.TXT', 'NEW.TXT')
        self.assertFalse((self.root / 'A/NEW.TXT').stat().st_mode & 0o200)
        self.run_filer('DELETE', 'SRC.TXT', error=5)
        self.assertEqual(source.read_bytes(), b'READ ONLY')

    def test_directories_and_sorted_listing(self):
        self.run_filer('MKDIR', 'SUB')
        (self.root / 'A/Z.TXT').write_bytes(b'Z')
        (self.root / 'A/A.TXT').write_bytes(b'A')
        lines = self.run_filer('LIST', '\\').splitlines()
        self.assertEqual([line.split()[0] for line in lines[:-1]], ['SUB', 'A.TXT', 'Z.TXT'])
        self.run_filer('COPY', 'SUB', 'OTHER', error=5)
        self.run_filer('RENAME', 'SUB', 'OTHER')
        self.assertIn('.. D', self.run_filer('LIST', 'OTHER'))
        (self.root / 'A/OTHER/KEEP.TXT').write_bytes(b'KEEP')
        self.run_filer('DELETE', 'OTHER', error=5)
        self.run_filer('DELETE', 'OTHER/KEEP.TXT')
        self.run_filer('DELETE', 'OTHER')
        self.assertFalse((self.root / 'A/OTHER').exists())

    def test_directory_limit_is_explicit(self):
        for i in range(260):
            (self.root / f'A/F{i:03}.TXT').write_bytes(b'X')
        lines = self.run_filer('LIST', '\\').splitlines()
        self.assertEqual(len(lines), 257)
        self.assertEqual(lines[-1], 'TRUNCATED 1')


if __name__ == '__main__':
    unittest.main()
