# frozen_string_literal: true

# Homebrew formula for AffrescoUI, built from the release's source tarball. A template: tools/release/packaging.py
# fills in the version and the hash (packaging/README.md). Try it from a local tap: brew install --build-from-source.
class Affrescoui < Formula
  desc "GUI library whose UIs are JSON screen documents, with a host, a Launcher and a CLI"
  homepage "https://github.com/RiccardoSottini/AffrescoUI"
  url "https://github.com/RiccardoSottini/AffrescoUI/releases/download/v@VERSION@/affrescoui-@VERSION@-source.tar.gz"
  sha256 "@SOURCE_SHA256@"
  license "Apache-2.0"

  depends_on "cmake" => :build
  depends_on "ninja" => :build

  on_linux do
    depends_on "pkgconf" => :build
    depends_on "wayland-protocols" => :build
    depends_on "libx11"
    depends_on "libxcursor"
    depends_on "libxi"
    depends_on "libxinerama"
    depends_on "libxkbcommon"
    depends_on "libxrandr"
    depends_on "mesa"
    depends_on "openssl@3"
    depends_on "wayland"
  end

  def install
    # The library, its CMake package and the affrescoui tool, as cmake --install puts them; the pinned dependencies
    # are downloaded by CMake and checked against their SHA-256 (cmake/dependencies.json)
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja", "-DAFFRESCOUI_BUILD_TESTS=OFF", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
    # The host finds its templates (the Launcher too) and fonts in "assets" next to itself, as in a release archive
    libexec.install "build/affrescoui_launcher"
    # (the templates' folders, examples/templates.json, and examples/shared, without the C++ sources)
    folders = JSON.parse(File.read("examples/templates.json"))["templates"].values.uniq
    (libexec/"assets").install "examples/shared", *folders.map { |folder| "examples/#{folder}" }
    rm Dir[libexec/"assets/**/*.{cpp,hpp}"]
    bin.write_exec_script libexec/"affrescoui_launcher"
  end

  test do
    assert_match "affrescoui #{version}", shell_output("#{bin}/affrescoui --version")
    system bin/"affrescoui", "check", libexec/"assets/flow/index.screen.json"
    assert_path_exists lib/"cmake/AffrescoUI/AffrescoUIConfig.cmake"
  end
end
