class Hyprmark < Formula
  desc "Markdown viewer for the Hyprland ecosystem"
  homepage "https://github.com/robinduckett/hyprmark"
  url "https://github.com/robinduckett/hyprmark/archive/refs/tags/v0.1.1.tar.gz"
  license "BSD-3-Clause"
  head "https://github.com/robinduckett/hyprmark.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build
  depends_on "md4c"
  depends_on "pixman"
  depends_on "qt@6"

  # hyprutils main rejects Darwin at configure time; the tagged releases
  # still build there. Pinned by tag and commit.
  resource "hyprutils" do
    url "https://github.com/hyprwm/hyprutils.git",
        tag:      "v0.14.1",
        revision: "2db328fe2b3e8b6a2eee5d17a91ff1ca4177719f"
  end

  resource "hyprlang" do
    url "https://github.com/hyprwm/hyprlang.git",
        tag:      "v0.6.8",
        revision: "3a1c1b25b059dae2c6bbc46991562ba1158d125c"
  end

  def install
    resource("hyprutils").stage do
      system "cmake", "-S", ".", "-B", "build",
             *std_cmake_args(install_prefix: buildpath/"deps")
      system "cmake", "--build", "build"
      system "cmake", "--install", "build"
    end

    resource("hyprlang").stage do
      ENV.prepend_path "PKG_CONFIG_PATH", buildpath/"deps/lib/pkgconfig"
      system "cmake", "-S", ".", "-B", "build",
             *std_cmake_args(install_prefix: buildpath/"deps")
      system "cmake", "--build", "build"
      system "cmake", "--install", "build"
    end

    ENV.prepend_path "PKG_CONFIG_PATH", buildpath/"deps/lib/pkgconfig"
    ENV.prepend_path "CMAKE_PREFIX_PATH", Formula["qt@6"].opt_prefix

    system "cmake", "-S", ".", "-B", "build",
           "-DCMAKE_BUILD_TYPE=Release",
           "-GNinja",
           *std_cmake_args
    system "cmake", "--build", "build"
    # On macOS the install rule places a self-contained hyprmark.app at the
    # prefix root (no bin/ entry).
    system "cmake", "--install", "build"

    # Expose the CLI (`hyprmark file.md`, --dispatch, --version) via a shim.
    # It execs the bundle binary by its real path, so the app still finds its
    # resources in hyprmark.app/Contents/Resources.
    bin.write_exec_script prefix/"hyprmark.app/Contents/MacOS/hyprmark"
  end

  def caveats
    <<~EOS
      To make hyprmark visible to Finder / "Open With", link the bundle into
      /Applications:
        ln -s #{opt_prefix}/hyprmark.app /Applications/hyprmark.app
    EOS
  end

  test do
    assert_match "hyprmark version", shell_output("#{bin}/hyprmark --version")
  end
end
