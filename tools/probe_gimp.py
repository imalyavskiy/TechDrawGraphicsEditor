import json
import os
from pathlib import Path

import gi

gi.require_version("Gimp", "3.0")
from gi.repository import Gimp, Gio, GObject


def main():
    output = Path(os.environ["PERSPECTIVE_PROBE_OUTPUT"]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "version": Gimp.version(),
        "profile": Gimp.directory(),
        "display_methods": sorted(Gimp.Display.__dict__),
        "display_signals": list(GObject.signal_list_names(Gimp.Display)),
        "canvas_types": [name for name in dir(Gimp) if any(
            token in name.lower() for token in ("canvas", "drawtool", "toolwidget")
        )],
        "procedures": sorted(Gimp.get_pdb().query_procedures(
            ".*", ".*", ".*", ".*", ".*", ".*", ".*", ".*"
        )),
    }
    assert Path(Gimp.directory()).resolve() == Path(os.environ["GIMP3_DIRECTORY"]).resolve()
    image = Gimp.Image.new(64, 64, Gimp.ImageBaseType.RGB)
    layer = Gimp.Layer.new(image, "Probe", 64, 64, Gimp.ImageType.RGBA_IMAGE,
                           100.0, Gimp.LayerMode.NORMAL)
    assert image.insert_layer(layer, None, 0)
    layer.fill(Gimp.FillType.TRANSPARENT)
    key = "gimp-perspective-plugin-probe"
    payload = json.dumps({"schema": 1, "point": [32, 32]}, sort_keys=True).encode()
    assert image.attach_parasite(Gimp.Parasite.new(
        key, Gimp.PARASITE_PERSISTENT | Gimp.PARASITE_UNDOABLE, payload
    ))
    xcf = Gio.File.new_for_path(str(output / "parasite-roundtrip.xcf"))
    assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, xcf)
    image.delete()
    restored = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, xcf)
    assert restored is not None
    recovered = bytes(restored.get_parasite(key).get_data())
    report["parasite_roundtrip"] = recovered == payload
    report["restored_layer_count"] = len(restored.get_layers())
    restored.delete()
    (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    assert report["parasite_roundtrip"]
    print("PERSPECTIVE_PROBE_OK " + str(output / "report.json"))


main()
