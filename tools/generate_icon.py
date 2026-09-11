import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SIZES = [(16, 16), (20, 20), (24, 24), (32, 32), (40, 40),
         (48, 48), (64, 64), (96, 96), (128, 128), (256, 256)]


def main():
    parser = argparse.ArgumentParser(description="Render the TechDraw SVG into PNG and multi-size ICO files.")
    parser.add_argument("--qt-root", default=r"F:\Qt\5.15.2\mingw81_64")
    parser.add_argument("--compiler-root", default=r"F:\Qt\Tools\mingw810_64")
    args = parser.parse_args()

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
    project = "QT += core gui svg\nCONFIG += console c++17\nTEMPLATE = app\nTARGET = icon-render\nSOURCES += main.cpp\n"

    with tempfile.TemporaryDirectory(prefix="techdraw-icon-") as temporary:
        work = Path(temporary)
        (work / "main.cpp").write_text(cpp, encoding="utf-8")
        (work / "icon-render.pro").write_text(project, encoding="ascii")
        environment = os.environ.copy()
        environment["PATH"] = os.pathsep.join((str(qt_root / "bin"), str(compiler_root / "bin"), environment.get("PATH", "")))
        subprocess.run([str(qt_root / "bin" / "qmake.exe"), "icon-render.pro", "CONFIG+=release"], cwd=work, env=environment, check=True)
        subprocess.run([str(compiler_root / "bin" / "mingw32-make.exe"), "-j4"], cwd=work, env=environment, check=True)
        raster = work / "techdraw-1024.png"
        subprocess.run([str(work / "release" / "icon-render.exe"), str(source), str(raster)], cwd=work, env=environment, check=True)
        image = Image.open(raster).convert("RGBA")
        image.resize((256, 256), Image.Resampling.LANCZOS).save(ROOT / "resources" / "techdraw.png")
        image.save(ROOT / "resources" / "techdraw.ico", format="ICO", sizes=SIZES)


if __name__ == "__main__":
    main()
