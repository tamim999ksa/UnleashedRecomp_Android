## Memory

The following memories are available to use:
* The `release.yml` workflow requires manual setup of Java 17, Android SDK/NDK 29.0.14206865, and vcpkg bootstrapping for Android builds. These steps are not automatically provided by the default runner environment.
* Case-insensitive regex `[0-9a-fA-F]\{8,42\}` should be used for matching STFS container files to ensure robust extraction of original game assets.
* GitHub Actions workflow syntax can be validated using `ruby -e "require 'yaml'; YAML.load_file('.github/workflows/release.yml')"`.
