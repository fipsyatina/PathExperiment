"""Convert --record-demo PNG captures to the README GIF. Requires Pillow.

Usage: python tools/make_demo.py artifacts/demo docs/media/search.gif
Original native SFML captures are retained. Existing output is refused.
"""

import argparse
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit(f"Output already exists: {args.output}")
    paths = sorted(args.captures.glob("frame-*.png"))
    if not paths:
        raise SystemExit("No frame-*.png captures found")
    # Use one shared palette and retain unchanged pixels between frames. This
    # keeps UI text steady and avoids storing the entire window for every step.
    palette_samples = Image.new("RGB", (960, 615 * 5))
    for sample in range(5):
        with Image.open(paths[sample * (len(paths) - 1) // 4]) as source:
            source.thumbnail((960, 615), Image.Resampling.LANCZOS)
            palette_samples.paste(source.convert("RGB"), (0, sample * 615))
    palette = palette_samples.quantize(colors=128)
    frames = []
    for path in paths:
        with Image.open(path) as source:
            source.thumbnail((960, 615), Image.Resampling.LANCZOS)
            frames.append(source.convert("RGB").quantize(palette=palette, dither=Image.Dither.NONE))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        args.output,
        save_all=True,
        append_images=frames[1:],
        duration=83,
        loop=0,
        optimize=False,
        disposal=1,
    )
    print(f"Saved {len(frames)} native-rendered frames to {args.output}")


if __name__ == "__main__":
    main()
