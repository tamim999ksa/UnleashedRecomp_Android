## Memory

The following memories are available to use:
* The `release.yml` workflow requires manual setup of Java 17, Android SDK/NDK 29.0.14206865, and vcpkg bootstrapping for Android builds. These steps are not automatically provided by the default runner environment.
* Case-insensitive regex `[0-9a-fA-F]\{8,42\}` should be used for matching STFS container files to ensure robust extraction of original game assets.
* GitHub Actions workflow syntax can be validated using `ruby -e "require 'yaml'; YAML.load_file('.github/workflows/release.yml')"`.
* To prevent compiler Out-Of-Memory (OOM) failures when building large embedded assets (like the generated `shader_cache.cpp`), host tools like `XenosRecomp` and `file_to_c` output binary data as chunked hexadecimal string literals (e.g., "\xHH" in 4096-byte chunks) rather than large comma-separated integer arrays. The generated arrays use `const char[]` for compatibility with string literal initialization.
