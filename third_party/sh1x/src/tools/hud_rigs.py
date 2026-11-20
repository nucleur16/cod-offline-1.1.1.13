"""Periodic sprite rigs: one piece of source artwork per moving part.

No pose morphing, optical flow, frame blending, or accumulated frame buffers.
Complete drawn cycles replace the rejected segmented dog and satellite sway.
"""
import math

import numpy as np
from PIL import Image, ImageDraw

SIZE = 256
TAU = math.tau


def clean(image):
    """Remove broad translucent bloom; retain a narrow antialiased edge."""
    a = np.array(image.convert('RGBA'))
    a[:, :, 3] = np.clip((a[:, :, 3].astype(float)-100)*2.55, 0, 255).astype('uint8')
    a[a[:, :, 3] == 0, :3] = 0
    return Image.fromarray(a)


def blank():
    return Image.new('RGBA', (SIZE, SIZE))


def layer(image, source, target, angle=0, scale=1):
    """Single spatial resample around a subpixel pivot; angle is clockwise."""
    c, s = math.cos(angle)/scale, math.sin(angle)/scale
    x, y = target
    return image.transform((SIZE, SIZE), Image.Transform.AFFINE,
        (c, s, source[0]-c*x-s*y, -s, c, source[1]+s*x-c*y),
        resample=Image.Resampling.BICUBIC)


def polygon_parts(image, points):
    mask = Image.new('L', image.size)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    a = np.array(image)
    upper, lower = a.copy(), a.copy()
    upper[:, :, 3] = np.where(np.asarray(mask), a[:, :, 3], 0)
    lower[:, :, 3] = np.where(np.asarray(mask), 0, a[:, :, 3])
    return Image.fromarray(upper), Image.fromarray(lower)


class MachineRig:
    def __init__(self, key, frames):
        self.key = key
        self.image = clean(frames[8 if key == 'spy_sat' else 0].resize((SIZE, SIZE), Image.Resampling.LANCZOS))
        if key == 'sam':
            self.top, self.base = polygon_parts(self.image,
                [(35, 67), (184, 61), (216, 95), (218, 151), (169, 166), (107, 135), (69, 117), (37, 116)])
        if key == 'mortar':
            # Retain the whole shell but remove detached generated wind flecks.
            self.image = self.image.rotate(-16, Image.Resampling.BICUBIC, center=(128,128))
        if key == 'rcxd':
            # Two visible sidewalls. Rotate their interior in elliptical space,
            # while the silhouette of each tire retains its camera perspective.
            self.hubs = [(51,155,10,17,-.25), (107,190,11,18,-.25)]

    def render(self, phase):
        p = phase % 1
        t = TAU*p
        if self.key == 'rcxd':
            car = self.image.copy()
            yy, xx = np.mgrid[:SIZE, :SIZE]
            import cv2
            original = np.asarray(self.image)
            for cx,cy,rx,ry,tilt in self.hubs:
                # Elliptical turn covers the original hub completely; there is
                # never a stationary copy underneath a transparent moving copy.
                x = (xx-cx-tilt*(yy-cy))/rx
                y = (yy-cy)/ry
                angle = t*4
                u = x*math.cos(angle)+y*math.sin(angle)
                v = -x*math.sin(angle)+y*math.cos(angle)
                sx = (cx+u*rx+tilt*v*ry).astype('float32')
                sy = (cy+v*ry).astype('float32')
                warped = cv2.remap(original, sx, sy, cv2.INTER_CUBIC,
                    borderMode=cv2.BORDER_CONSTANT)
                mask = np.uint8(np.clip((1-x*x-y*y)*12,0,1)*255)
                car.paste(Image.fromarray(warped), (0,0), Image.fromarray(mask))
            return layer(car, (128,160), (128,158+6*math.sin(t*2)+2*math.sin(t*4)), .022*math.sin(t*2+.5))
        if self.key == 'sam':
            out = self.base.copy()
            out.alpha_composite(layer(self.top,(121,127),(121,127),.075*math.sin(t)))
            return out
        if self.key == 'mortar':
            # Wind curls originate near the downward-pointing nose and travel
            # upward. Draw one distinct moving stroke per lane, never trails.
            out = blank()
            d = ImageDraw.Draw(out)
            for side in (-1,1):
                for k in range(3):
                    q = (p+k/3+(.12 if side == 1 else 0)) % 1
                    opacity = int(210*math.sin(math.pi*q)**2)
                    y = 209-148*q
                    x = 128+side*(24+17*math.sin(math.pi*q))
                    d.line([(x+side*1,y),(x+side*3,y-6),(x+side*4,y-13)],
                           fill=(12,208,255,opacity),width=2)
            shell = layer(self.image,(128,128),(128,128+2*math.sin(t)),.025*math.sin(t))
            out.alpha_composite(shell)
            return out
        raise ValueError(self.key)


class DrawnCycle:
    """Play complete drawn poses. No limb assembly or synthesized in-betweens."""
    def __init__(self, key, frames):
        order = ([0,1,2,4,6,5,9,8,7,10,3,12,11,13,14,15] if key == 'dogs'
                 else [0,1,31,2,3,4,5,6,7,8,9,10,11,12,13,14,
                       15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30])
        self.frames = []
        for i,index in enumerate(order):
            image = clean(frames[index].resize((SIZE,SIZE),Image.Resampling.LANCZOS))
            if key == 'dogs':
                # Register the face rather than centering the changing reach
                # of the paws; keep only a small intentional gallop head bob.
                a = np.asarray(image)[:,:,3] > 180
                ys,xs = np.where(a & (np.indices(a.shape)[1] > 150))
                top = int(ys.min())
                head_y,head_x = np.where(a & (np.indices(a.shape)[0] < top+48))
                nose = float(np.percentile(head_x,99.5))
                # Common scale leaves room for extended forepaws; moving the
                # head alone can otherwise crop a long airborne pose.
                image = layer(image,(nose,top),
                              (218,76+2*math.sin(TAU*i/len(order))),scale=.9)
                self.frames.append(image)
                continue
            else:
                box = image.getbbox()
                offset = (128-(box[0]+box[2])/2,128-(box[1]+box[3])/2)
            self.frames.append(layer(image,(0,0),offset))

    def render(self, phase):
        index = min(len(self.frames)-1,int((phase%1)*len(self.frames)+1e-8))
        return self.frames[index].copy()


def build_rig(key,source_frames,root):
    if key in ('dogs','spy_sat'):
        return DrawnCycle(key,source_frames)
    return MachineRig(key,source_frames)
