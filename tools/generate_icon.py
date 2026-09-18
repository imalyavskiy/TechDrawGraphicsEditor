import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SIZES = [(16, 16), (20, 20), (24, 24), (32, 32), (40, 40),
         (48, 48), (64, 64), (96, 96), (128, 128), (256, 256)]


def main():
    parser = argparse.ArgumentParser(description="Render the TechDraw SVG into PNG and multi-size ICO files.")
    parser.add_argument("--qt-root", default=os.environ.get("QT_ROOT"))
    parser.add_argument("--compiler-root", default=os.environ.get("MINGW_ROOT"))
    parser.add_argument("--cmake", default=shutil.which("cmake"))
    args = parser.parse_args()

    if not args.qt_root or not args.compiler_root:
        parser.error("set QT_ROOT and MINGW_ROOT or pass --qt-root and --compiler-root")
    if not args.cmake:
        parser.error("cmake was not found in PATH; pass --cmake")

    qt_root = Path(args.qt_root)
    compiler_root = Path(args.compiler_root)
    source = ROOT / "resources" / "techdraw.svg"

    cpp = r'''#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if (argc!=3) return 2;
    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid()) return 3;
    QImage image(1024,1024,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);renderer.render(&painter,image.rect());painter.end();
    return image.save(QString::fromLocal8Bit(argv[2]),"PNG")?0:4;
}'''
    project = """cmake_minimum_required(VERSION 3.16)
project(TechDrawIconRenderer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Svg)
add_executable(icon-render main.cpp)
target_link_libraries(icon-render PRIVATE Qt5::Core Qt5::Gui Qt5::Svg)
"""

    with tempfile.TemporaryDirectory(prefix="techdraw-icon-") as temporary:
        work = Path(temporary)
        (work / "main.cpp").write_text(cpp, encoding="utf-8")
        (work / "CMakeLists.txt").write_text(project, encoding="ascii")
        environment = os.environ.copy()
        environment["PATH"] = os.pathsep.join((str(qt_root / "bin"), str(compiler_root / "bin"), environment.get("PATH", "")))
        build = work / "build"
        subprocess.run(
            [
                args.cmake,
                "-S",
                str(work),
                "-B",
                str(build),
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_PREFIX_PATH={qt_root}",
                f"-DCMAKE_CXX_COMPILER={compiler_root / 'bin' / 'g++.exe'}",
            ],
            env=environment,
            check=True,
        )
        subprocess.run([args.cmake, "--build", str(build), "--parallel"], env=environment, check=True)
        raster = work / "techdraw-1024.png"
        subprocess.run([str(build / "icon-render.exe"), str(source), str(raster)], cwd=work, env=environment, check=True)
        image = Image.open(raster).convert("RGBA")
        image.resize((256, 256), Image.Resampling.LANCZOS).save(ROOT / "resources" / "techdraw.png")
        image.save(ROOT / "resources" / "techdraw.ico", format="ICO", sizes=SIZES)


if __name__ == "__main__":
    main()
