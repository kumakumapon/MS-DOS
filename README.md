# MS-DOS PC-98 and 32-bit application environment

[日本語](README.ja.md) · [Documentation index](docs/README.md) · [Tools and dependencies](tools/README.md)

This fork adds a PC-98 OEM port of MS-DOS 2.0/4.0, reproducible boot media,
KDESK and the KGUI32 protected-mode desktop with native 32-bit applications.
The paired [webnp2](https://github.com/kumakumapon/webnp2) repository builds
and tests the browser distribution. See the [CI pairing procedure](docs/ci-pairing.md).

Issues and pull requests for this fork's ports, tools, applications and
integration are welcome. Historical upstream source directories remain preserved;
new work belongs in `ports/`, `tools/` and integration/documentation files.

Start with [DOS 4.0 build](docs/build-msdos4.md), [PC-98 DOS 4.0](docs/pc98-dos4.md),
[PC-98 DOS 2.0](docs/pc98-webnp2.md), [KDESK](docs/window-desktop.md),
[KGUI32](tools/kgui32/README.md) and [32-bit SDK](tools/k38632/README.md).

All repository files use the MIT license; individual tools carry their own
notices. The following upstream README is retained as historical context.
Its pull-request policy refers to the upstream historical sources.

---

## Historical upstream README

<img width="150" height="150" align="left" style="float: left; margin: 0 10px 0 0;" alt="MS-DOS logo" src="https://github.com/Microsoft/MS-DOS/blob/main/.readmes/msdos-logo.png">   

# MS-DOS v1.25, v2.0, v4.0 Source Code

This repo contains the original source-code and compiled binaries for MS-DOS v1.25 and MS-DOS v2.0, plus the source-code for MS-DOS v4.00 jointly developed by IBM and
Microsoft.

The MS-DOS v1.25 and v2.0 files [were originally shared at the Computer History Museum on March 25th, 2014]( http://www.computerhistory.org/atchm/microsoft-ms-dos-early-source-code/) and are being (re)published in this repo to make them easier to find, reference-to in external writing and works, and to allow exploration and experimentation for those interested in early PC Operating Systems.  

# License

All files within this repo are released under the [MIT License]( https://en.wikipedia.org/wiki/MIT_License) as per the [LICENSE file](https://github.com/Microsoft/MS-DOS/blob/main/LICENSE) stored in the root of this repo.

# For historical reference

The source files in this repo are for historical reference and will be kept static, so please **don’t send** Pull Requests suggesting any modifications to the source files, but feel free to fork this repo and experiment 😊.  

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).  For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

# Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
