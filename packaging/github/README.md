# Valkyria Revolution: current Linux quick start

**This is a legacy GitHub packaging reference.** Please use the current
[repository README](../../README.md), [Running on Linux](../../docs/RUNNING_LINUX.md)
and [Linux distribution guide](../../docs/DISTRIBUTING_LINUX.md).

**Players:** Download **`valkyria-revolution-linux-x86_64-35f35f5.tar.gz`** from the
[latest release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/latest), not the developer source archive.

```bash
tar -xzf valkyria-revolution-linux-x86_64-35f35f5.tar.gz
cd valkyria-revolution-linux-x86_64-35f35f5
bash Play-Valkyria.sh --game "/path/to/CUSA06810"
```

Supply your own extracted, plaintext `CUSA06810` folder. The v1.1 runtime
includes the twelve required application libraries, including FFmpeg 8 and SDL3.
No compiler, Podman or `LD_LIBRARY_PATH` setup is needed. glibc 2.35+, Python 3,
native x86-64 Linux and a compatible host Vulkan stack are required.
ARM64 is unsupported; other Linux distributions remain unverified.

**Developers:** See [portable builder](../linux/README.md) for build instructions.
