import struct
import unittest

from image import IMAGE_SIZE, make_image, read_files


class ImageTests(unittest.TestCase):
    def setUp(self):
        self.boot = b"\xeb\x3c\x90" + bytes(507) + b"\x55\xaa"
        self.files = [("IO.SYS", b"I" * 1600), ("MSDOS.SYS", b"M" * 777),
                      ("COMMAND.COM", bytes(range(256)) * 5), ("EMPTY.TXT", b"")]

    def test_layout_and_roundtrip(self):
        image = make_image(self.boot, self.files)
        self.assertEqual(len(image), IMAGE_SIZE)
        self.assertEqual(struct.unpack_from("<HBHBHHBHHHII", image, 11),
                         (512, 1, 1, 2, 224, 2880, 240, 9, 18, 2, 0, 0))
        self.assertEqual(image[19 * 512:19 * 512 + 11], b"IO      SYS")
        self.assertEqual(image[19 * 512 + 32:19 * 512 + 43], b"MSDOS   SYS")
        self.assertEqual(image[512:5120], image[5120:9728])
        self.assertEqual(read_files(image), dict(self.files))
        self.assertEqual(image, make_image(self.boot, self.files))

    def test_reject_invalid_input(self):
        for boot, files in [(bytes(512), self.files), (self.boot, self.files[::-1]),
                            (self.boot, self.files + [("TOOLONGNAME.EXE", b"x")]),
                            (self.boot, self.files + [("IO.SYS", b"x")]),
                            (self.boot, self.files + [("BIG.BIN", bytes(IMAGE_SIZE))])]:
            with self.subTest(files=[name for name, _ in files]):
                with self.assertRaises(ValueError):
                    make_image(boot, files)

    def test_reject_corrupt_fat(self):
        image = bytearray(make_image(self.boot, self.files))
        image[515] ^= 1
        with self.assertRaises(ValueError):
            read_files(image)


if __name__ == "__main__":
    unittest.main()
