"""Check the real editor engine under ASan/UBSan and failure-injected file IO."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

HOST = Path(__file__).resolve().parent / 'build/editor-host'

class EditorTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / 'A').mkdir()
        (self.root / 'B').mkdir()

    def run_editor(self, *args, error=0, env=None):
        p = subprocess.run([str(HOST), *args], cwd=self.root, capture_output=True,
                           text=True, timeout=10, env={**os.environ, **(env or {})})
        self.assertEqual(p.stderr, '')
        self.assertEqual(p.returncode, 1 if error else 0, p.stdout)
        if args != ('CORE',): self.assertIn(f'ERROR {error} ', p.stdout)
        return p.stdout

    def test_character_motion_deletion_undo_capacity(self):
        self.run_editor('CORE')

    def test_japanese_crlf_round_trip_and_empty(self):
        path = self.root / 'A/JP.TXT'
        for original in ('日本語\r\nｶﾅ\n漢字\r最後'.encode('shift_jis'), b'', b'A'*16384):
            path.write_bytes(original)
            self.run_editor('ROUND', 'A:\\JP.TXT')
            normalized = original.replace(b'\r\n', b'\n').replace(b'\r', b'\n').replace(b'\n', b'\r\n')
            self.assertEqual(path.read_bytes(), normalized)
            self.assertEqual([p.name for p in path.parent.iterdir()], ['JP.TXT'])

    def test_new_file_and_save_as(self):
        self.run_editor('SAVE', 'A:\\NEW.TXT')
        self.assertEqual((self.root / 'A/NEW.TXT').read_bytes(), b'!')
        self.run_editor('SAVE', 'A:\\NEW.TXT', 'B:\\COPY.TXT')
        self.assertEqual((self.root / 'B/COPY.TXT').read_bytes(), b'!!')
        self.assertEqual((self.root / 'A/NEW.TXT').read_bytes(), b'!')

    def test_rejected_binary_invalid_pair_and_large_preserve_buffer(self):
        path = self.root / 'A/BAD.TXT'
        for content, error in ((b'A\0B',1000),(b'\x93',1000),(b'\x93\x7f',1000),(b'A'*16385,8)):
            path.write_bytes(content)
            self.run_editor('LOAD', 'A:\\BAD.TXT', error=error)
            self.assertEqual(path.read_bytes(), content)

    def test_read_failure_preserves_buffer(self):
        (self.root / 'A/OLD.TXT').write_bytes(b'ORIGINAL')
        self.run_editor('LOAD', 'A:\\OLD.TXT', error=5, env={'FD_FAIL_READ':'1'})

    def test_failed_write_short_write_close_preserve_original(self):
        path = self.root / 'A/OLD.TXT'
        for env, error in (({'FD_WRITE_LIMIT':'0'},112),({'FD_WRITE_LIMIT':'1'},112),({'FD_FAIL_CLOSE':'1'},5)):
            path.write_bytes(b'ORIGINAL')
            self.run_editor('SAVE', 'A:\\OLD.TXT', error=error, env=env)
            self.assertEqual(path.read_bytes(), b'ORIGINAL')
            self.assertEqual([p.name for p in path.parent.iterdir()], ['OLD.TXT'])

    def test_failed_install_rolls_back(self):
        path = self.root / 'A/OLD.TXT'
        path.write_bytes(b'ORIGINAL')
        self.run_editor('SAVE', 'A:\\OLD.TXT', error=5, env={'FD_FAIL_RENAME':'.TMP'})
        self.assertEqual(path.read_bytes(), b'ORIGINAL')
        self.assertEqual([p.name for p in path.parent.iterdir()], ['OLD.TXT'])

    def test_failed_cleanup_names_recovery(self):
        path = self.root / 'A/OLD.TXT'
        path.write_bytes(b'ORIGINAL')
        output = self.run_editor('SAVE', 'A:\\OLD.TXT', error=1001,
                                env={'FD_WRITE_LIMIT':'1', 'FD_FAIL_UNLINK':'.TMP'})
        self.assertEqual(path.read_bytes(), b'ORIGINAL')
        self.assertIn('RECOVERY A:\\ED000000.TMP', output)

    def test_failed_restore_preserves_original_backup(self):
        (self.root / 'A/OLD.TXT').write_bytes(b'ORIGINAL')
        output = self.run_editor('SAVE', 'A:\\OLD.TXT', error=1001,
                                env={'FD_FAIL_RENAME_AT':'23'})
        self.assertIn('RECOVERY A:\\ED000000.BAK', output)
        self.assertEqual((self.root / 'A/ED000000.BAK').read_bytes(), b'ORIGINAL')

    def test_retained_backup_is_reported_after_successful_save(self):
        path = self.root / 'A/OLD.TXT'; path.write_bytes(b'ORIGINAL')
        output = self.run_editor('SAVE', 'A:\\OLD.TXT', error=1002,
                                env={'FD_FAIL_UNLINK_AT':'2'})
        self.assertIn('DIRTY 0 RECOVERY A:\\ED000000.BAK', output)
        self.assertEqual(path.read_bytes(), b'ORIGINAL!')
        self.assertEqual((self.root / 'A/ED000000.BAK').read_bytes(), b'ORIGINAL')

    def test_readonly_and_existing_temporary_file(self):
        path = self.root / 'A/OLD.TXT'
        path.write_bytes(b'ORIGINAL'); path.chmod(0o400)
        self.run_editor('LOAD', 'A:\\OLD.TXT')
        self.run_editor('SAVE', 'A:\\OLD.TXT', error=5)
        path.chmod(0o600)
        other = path.parent / 'ED000000.TMP'; other.write_bytes(b'KEEP')
        self.run_editor('SAVE', 'A:\\OLD.TXT')
        self.assertEqual(path.read_bytes(), b'ORIGINAL!')
        self.assertEqual(other.read_bytes(), b'KEEP')

if __name__ == '__main__': unittest.main()
