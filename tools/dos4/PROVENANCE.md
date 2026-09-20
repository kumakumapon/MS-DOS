# Source restoration

Historical base: `2d04cacc5322951f187bb17e017c12920ac8ebe2` in this repository.
The historical `v4.0` files are never edited by these scripts.

`restoration.patch` is the byte-preserving `git diff --binary` of `v4.0/src`
from that base to [Ray Chason's restoration commit
f4a7323cca0097dea9cb731380ddddeb33888cd8](https://github.com/chasonr/MS-DOS/commit/f4a7323cca0097dea9cb731380ddddeb33888cd8).
Both repositories publish the source under the root MIT license.

The relevant upstream changes are:

- `1cb88afbbcba10f8868c4d94dbb0134c5be9007e`: CRLF attributes for DOS inputs.
- `0b809432e52c8a74e073df7f3792e3ddcebf79e4`: remove DOS EOF bytes from message skeletons.
- `4819ed889bbc27e4a584b71d150fa238637f1461`,
  `801aa951f5be75846bff3b55f58133f9e0cfa349`,
  `17af44c28b43c751dd50ce0da67aa69e4fd812a7`: enforce required text line endings.
- `0ecc065f329f35fc0997448156de9f78372719d1`: restore OEM bytes in MAPPER and SELECT,
  including screen strings in USA.INF, and correct SETENV.BAT toolchain paths.
- `f4a7323cca0097dea9cb731380ddddeb33888cd8`: restore non-ASCII comments.

The combined patch changes 74 files. No binary tool or library is replaced.
SHA256: `42e6d2315c5284f680a44f3b220ab9c1e36ed2c04604d68818af994f652b2b94`.
It is stored with Git text conversion disabled; do not edit it with a Unicode
text editor. Its raw OEM bytes are intentional. `git apply --check` validates
the expected base before applying it to the isolated build tree.

Our preparation then normalizes CRLF only for an explicit allowlist of source
extensions and known text inputs. It does not transcode character sets or touch
EXE, COM, LIB, OBJ, SYS, or other binary inputs. In particular, converting the
original replacement characters to CP437 would not restore the missing bytes.

The image writer is a small Python implementation for the single required
1.44 MB format. The [chasonr disk script](https://github.com/chasonr/MS-DOS/blob/80efd287942994def1ef00f75a57fdfe2365db57/v4.0/make-disks.rb)
was inspected for BPB and distribution layout. It was not vendored: its multiple
installation-disk formats and Ruby dependency are unnecessary here. The DOS
boot code comes from this build's `BOOT/MSBOOT.BIN`, not another DOS distribution.
