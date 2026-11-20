"""Make HUD-scale GIF previews (Pillow/numpy/scipy/OpenCV).

Generated atlases are not necessarily on integer cell boundaries. Extract their
dominant silhouettes to select consistent source art for periodic part rigs.
The UAV retains its original atlas frames and timing. Sources are never changed.
These are illustrative HUD previews, not game captures or engine integration.
"""
from pathlib import Path
import argparse
import json
import math

import numpy as np
from hud_rigs import build_rig
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt, find_objects, label

ROOT = Path(r'C:\Users\Michael\Desktop\codol_imaegs')
BACKGROUND = (12, 19, 27)
TICK_MS = 20
# Exact divisors of the comparison period: no icon is cut off at GIF wrap.
CYCLE_MS = dict(uav=1920, rcxd=1920, sam=3840, mortar=3840,
                dogs=960, spy_sat=1920)
ASSETS = [
    ('uav', 'UAV reference', ROOT / 'dpad_ks_uav.png', 60),
    ('rcxd', 'RC-XD', ROOT / 'animated/v2/dpad_rcbomb_animated_v2.png', 40),
    ('sam', 'SAM turret', ROOT / 'animated/v2/dpad_sam_turret_animated_v2.png', 70),
    ('mortar', 'Mortar', ROOT / 'animated/v2/dpad_mortar_shell_animated_v2.png', 40),
    ('dogs', 'Running dog', ROOT / 'animated/v5/dogs_gallop_drawn.png', 30),
    ('spy_sat', 'Spy satellite', ROOT / 'animated/v5/spy_sat_turn.png', 60),
]


def extract(path, native=False, cols=8, rows=4):
    image = Image.open(path).convert('RGBA')
    w, h = image.size
    if native:
        return [image.crop((col*w//cols, row*h//rows, (col+1)*w//cols, (row+1)*h//rows))
                for row in range(rows) for col in range(cols)]
    alpha = np.asarray(image)[:, :, 3]
    labels, _ = label(alpha > 160)
    counts = np.bincount(labels.ravel())
    objects = find_objects(labels)
    components = []
    selected_ids = np.argsort(counts[1:])[-cols*rows:] + 1
    for idx in selected_ids:
        ys, xs = objects[idx-1]
        components.append((xs.start, ys.start, xs.stop, ys.stop, int(idx)))
    # Assign feathered alpha to its nearest silhouette, so a nearby pose's
    # paws/glow cannot leak into this frame when the AI omitted cell padding.
    seeds = np.where(np.isin(labels, selected_ids), labels, 0)
    nearest = distance_transform_edt(seeds == 0, return_distances=False, return_indices=True)
    owners = seeds[nearest[0], nearest[1]]
    # First group rows by vertical center, then order each row horizontally.
    components.sort(key=lambda box: (box[1] + box[3]) / 2)
    ordered = []
    for row in range(rows):
        ordered += sorted(components[row*cols:(row+1)*cols], key=lambda box: box[0])
    if len(ordered) != cols*rows:
        raise ValueError(f'{path}: expected {cols*rows} silhouette components')
    # Same scale for every frame. Center horizontally; align each animation's
    # mean vertical offset while retaining its drawn up/down motion.
    cell = max(w/cols, h/rows)
    frame_size = math.ceil(cell + 40)
    y_offsets = [(box[1]+box[3])/2 - (i//cols + .5)*h/rows for i, box in enumerate(ordered)]
    mean_y = float(np.median(y_offsets))
    frames = []
    for i, box in enumerate(ordered):
        x0, y0, x1, y1, component_id = box
        pad = 10
        crop = image.crop((x0-pad, y0-pad, x1+pad, y1+pad))
        owner_mask = Image.fromarray(np.uint8(owners == component_id)*255).crop((x0-pad, y0-pad, x1+pad, y1+pad))
        pixels = np.array(crop)
        pixels[:, :, 3] = np.where(np.asarray(owner_mask) > 0, pixels[:, :, 3], 0)
        crop = Image.fromarray(pixels)
        frame = Image.new('RGBA', (frame_size, frame_size))
        dx = round((frame_size - crop.width)/2)
        dy = round((frame_size - crop.height)/2 + y_offsets[i] - mean_y)
        frame.alpha_composite(crop, (dx, dy))
        frames.append(frame)
    return frames


def rendered(frame, icon_size, canvas_size):
    canvas = Image.new('RGB', (canvas_size, canvas_size), BACKGROUND)
    sprite = frame.resize((icon_size, icon_size), Image.Resampling.LANCZOS)
    offset = (canvas_size-icon_size)//2
    canvas.paste(sprite, (offset, offset), sprite)
    return canvas


def closed_cycle(rig, period_ms):
    """Sample a deterministic periodic rig; never duplicate the end frame."""
    assert np.array_equal(np.asarray(rig.render(0)), np.asarray(rig.render(1)))
    count = period_ms // TICK_MS
    return [rig.render(n/count) for n in range(count)]


def seam_report(frames):
    arrays = [np.asarray(rendered(frame, 208, 256), dtype=np.float32) for frame in frames]
    diffs = [float(np.abs(a-arrays[(i+1) % len(arrays)]).mean()) for i,a in enumerate(arrays)]
    return dict(last_to_first_mae=round(diffs[-1], 4),
                median_adjacent_mae=round(float(np.median(diffs)), 4),
                largest_adjacent_mae=round(max(diffs), 4),
                seam_to_median=round(diffs[-1]/max(float(np.median(diffs)), 1e-6), 3))


def save_gif(frames, path, duration):
    # A shared palette prevents per-frame color flicker.
    sample = Image.new('RGB', (frames[0].width*8, frames[0].height*4))
    for i, index in enumerate(np.linspace(0, len(frames)-1, min(32, len(frames)), dtype=int)):
        frame = frames[index]
        sample.paste(frame, ((i % 8)*frame.width, (i//8)*frame.height))
    palette = sample.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    quantized = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    quantized[0].save(path, save_all=True, append_images=quantized[1:],
                      duration=duration, loop=0, disposal=2, optimize=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'animated/gifs_redo')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype(r'C:\Windows\Fonts\segoeui.ttf', 18)
    small = ImageFont.truetype(r'C:\Windows\Fonts\segoeui.ttf', 12)
    animations = []
    manifest = []
    for key, title, path, duration in ASSETS:
        source_frames = extract(path, native=key == 'uav', cols=4 if key == 'dogs' else 8)
        rig = None if key == 'uav' else build_rig(key, source_frames, ROOT)
        frames = (source_frames if key == 'uav' else rig.frames
                  if key in ('dogs','spy_sat') else closed_cycle(rig, CYCLE_MS[key]))
        step_ms = 60 if key in ('uav','dogs','spy_sat') else TICK_MS
        small_frames = [rendered(frame, 64, 96) for frame in frames]
        large_frames = [rendered(frame, 208, 256) for frame in frames]
        save_gif(small_frames, args.output / f'{key}_hud.gif', step_ms)
        save_gif(large_frames, args.output / f'{key}_large.gif', step_ms)
        if rig is not None:
            # Exact uniform phases, not rounded indices into a 50 fps preview.
            atlas_frames = [rig.render(i/32) for i in range(32)]
            for cell in (64,128):
                atlas = Image.new('RGBA', (8*cell,4*cell))
                for i, frame in enumerate(atlas_frames):
                    atlas.alpha_composite(frame.resize((cell,cell),Image.Resampling.LANCZOS),
                                          ((i%8)*cell,(i//8)*cell))
                atlas.save(args.output / f'{key}_sheet_{cell}px.png')
        animations.append((title, frames, step_ms))
        manifest.append(dict(name=key, source=str(path), frames=len(frames),
                             milliseconds_per_frame=step_ms, loop_seconds=CYCLE_MS[key]/1000,
                             atlas_frames=32, atlas_milliseconds_per_frame=CYCLE_MS[key]/32,
                             method=('original atlas' if key == 'uav' else 'complete drawn pose cycle' if key in ('dogs','spy_sat') else 'periodic part rig; no pose blending'), corrected_seam=seam_report(frames)))
        print(f'{key}: closed {CYCLE_MS[key]/1000:.2f}s cycle; {manifest[-1]["corrected_seam"]}', flush=True)
    comparison_ms = math.lcm(*CYCLE_MS.values())
    assert all(comparison_ms % (len(fs)*step) == 0 for _, fs, step in animations)
    montage = []
    for tick in range(comparison_ms // TICK_MS):
        canvas = Image.new('RGB', (768, 550), BACKGROUND)
        draw = ImageDraw.Draw(canvas)
        draw.text((18, 10), 'CoDOL / HUD animation preview', font=font, fill='#b3e7fa')
        draw.text((18, 36), 'Large preview + 64 px HUD scale  |  illustrative mockup, not gameplay', font=small, fill='#7896a5')
        for idx, (title, frames, duration) in enumerate(animations):
            x, y = (idx % 3)*256, 65 + (idx//3)*240
            draw.rounded_rectangle((x+8, y, x+248, y+232), radius=8, outline='#253947', fill='#101b25')
            draw.text((x+20, y+10), title, font=font, fill='#d4eaf4')
            frame = frames[(tick*TICK_MS//duration) % len(frames)]
            large = frame.resize((168, 168), Image.Resampling.LANCZOS)
            canvas.paste(large, (x+12, y+52), large)
            native = frame.resize((64, 64), Image.Resampling.LANCZOS)
            canvas.paste(native, (x+177, y+95), native)
            draw.text((x+188, y+170), '64 px', font=small, fill='#7896a5')
        montage.append(canvas)
    save_gif(montage, args.output / 'all_icons_hud_preview.gif', TICK_MS)
    montage[10].save(args.output / 'preview_still.png')
    (args.output / 'playback.json').write_text(json.dumps({
        'note': 'UAV original 32 frames at 60ms preserved. Dog uses 16 complete drawn gallop poses. Satellite uses 32 full-turn views. Other icons use fixed artwork with periodic part transforms. No optical flow or temporal blending. Montage contains whole cycles only. Preview timing is not a measured game playback rate.',
        'comparison_seconds': comparison_ms/1000,
        'assets': manifest}, indent=2), encoding='utf-8')
    for path in sorted(args.output.glob('*.gif')):
        with Image.open(path) as gif:
            assert gif.n_frames > 1
            assert gif.info.get('loop') == 0
            elapsed = 0
            for frame_index in range(gif.n_frames):
                gif.seek(frame_index)
                elapsed += gif.info['duration']
            key = path.stem.removesuffix('_hud').removesuffix('_large')
            assert elapsed == (comparison_ms if key == 'all_icons_hud_preview' else CYCLE_MS[key]), (path, elapsed)
            print(f'{path.name}: {gif.size}, {gif.n_frames} frames, {path.stat().st_size:,} bytes')


if __name__ == '__main__':
    main()
