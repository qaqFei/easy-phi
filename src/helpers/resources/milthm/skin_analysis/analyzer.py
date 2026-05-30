import json
import math
import typing
import os

from PIL import Image

meta = json.load(open("./origin/meta.json", "r", encoding="utf-8"))
with open("./drag_shape.js", "r", encoding="utf-8") as f:
    drag_shape = f.read()

def rotate_point(x: float, y: float, r: float, deg: float):
    rad = deg * math.pi / 180
    return (
        x + r * math.cos(rad),
        y + r * math.sin(rad)
    )

class Color:
    def __init__(self, r: int, g: int, b: int, a: int = 255):
        self.r = r
        self.g = g
        self.b = b
        self.a = a
    
    def hex(self):
        return f"#{self.r:02x}{self.g:02x}{self.b:02x}{self.a:02x}"
    
    def lst(self):
        return [self.r, self.g, self.b, self.a]

def _show_chart(*data: list[float], ext: typing.Optional[typing.Callable[[float], float]] = None, extcast: type = float):
    import matplotlib.pyplot as plt
    for i in data:
        plt.plot(i, color="blue")
        if ext is not None:
            plt.plot(list(map(extcast, [ext(j/len(i)) * (max(i)-min(i)) + min(i) for j in range(len(i))])), color="red")
    plt.show()

def _float_rmse(data1: list[float], data2: list[float]) -> float:
    return (sum((i-j)**2 for i, j in zip(data1, data2)) / min(len(data1), len(data2))) ** 0.5

def _fold_repeated[T](data: list[T], *, float_index: bool = False) -> list[tuple[int|float, T]]:
    result = []
    last = None
    
    for i, this in enumerate(data):
        if last is None or this != last:
            result.append((i if not float_index else i/(len(data)-1), this))
            last = this
    
    return result

class EnumColorInterplateType:
    rgb = "rgb"
    oklch = "oklch"

class Analyzer:
    def __init__(self, folder: str, skin_name: str):
        self.tap = Image.open(f"{folder}/tap.png")
        self.drag = Image.open(f"{folder}/drag.png")
        self.hold = Image.open(f"{folder}/hold.png")
        self.tap_double = Image.open(f"{folder}/tap_double.png")
        self.hold_double = Image.open(f"{folder}/hold_double.png")
        self.extap = Image.open(f"{folder}/extap.png")
        self.extap_double = Image.open(f"{folder}/extap_double.png")
        self.exhold = Image.open(f"{folder}/exhold.png")
        self.exhold_double = Image.open(f"{folder}/exhold_double.png")
        self.line_head = Image.open(f"{folder}/line_head.png")
        self.line_pixel = Image.open(f"{folder}/line_pixel.png")
        self.skin_name = skin_name
    
    def analyze(self):
        return {
            "skin_name": self.skin_name,
            "notes": {
                "tap": self.analyze_tap(),
                "drag": self.analyze_drag(),
                "hold": self.analyze_hold(),
                "tap_double": self.analyze_tap_double(),
                "hold_double": self.analyze_hold_double(),
                "extap": self.analyze_extap(),
                "extap_double": self.analyze_extap_double(),
                "exhold": self.analyze_exhold(),
                "exhold_double": self.analyze_exhold_double()
            },
            "line_head": self.analyze_line_head(),
            "line_pixel": self.analyze_line_pixel()
        }
    
    def _analyze_path_circle_note(self, note: Image.Image):
        result = {
            "size": note.size,
        }
        
        top_point = -1
        shadow_start = -1
        shadow_size = -1
        shadow_steps = []
        stroke_color = None
        stroke_end = -1
        for y in range(note.height):
            *rgb, a = note.getpixel((note.width//2, y))
            if not shadow_steps and shadow_start == -1 and a > 0:
                shadow_start = y
            
            if shadow_start != -1 and a < 200:
                shadow_steps.append((y, *rgb, a))
                
            if top_point == -1 and a > 240:
                top_point = y
                shadow_size = y - shadow_start
                shadow_start = -1
            
            if stroke_color is None and top_point != -1 and a == 255:
                stroke_color = rgb
            
            if stroke_end == -1 and stroke_color is not None and _float_rmse(stroke_color, rgb) > 8:
                stroke_end = y
        
        if top_point == -1: raise ValueError("No top point found")
        if stroke_end == -1: raise ValueError("No stroke end found")

        circ_size = note.height/2 - top_point
        stroke_center_r = note.height/2 - top_point - (stroke_end-top_point)/2
        
        if not shadow_steps:
            shadow_steps.append((-1, 0, 0, 0, 0))
            
        result["outter_stroke_circle_radius"] = circ_size
        result["inner_fill_circle_radius"] = note.height/2 - stroke_end
        result["shadow_extended_radius"] = shadow_size
        result["outter_stroke_left_point_color"] = Color(*note.getpixel((int(note.width/2-stroke_center_r), note.height/2))).lst()[:-1]
        result["outter_stroke_right_point_color"] = Color(*note.getpixel((int(note.width/2+stroke_center_r), note.height/2))).lst()[:-1]
        result["outter_stroke_color_interplate_type"] = EnumColorInterplateType.oklch
        result["inner_fill_left_point_color"] = Color(*note.getpixel((int(note.width/2-result["inner_fill_circle_radius"]+1), note.height/2))).lst()[:-1]
        result["inner_fill_right_point_color"] = Color(*note.getpixel((int(note.width/2+result["inner_fill_circle_radius"]-1), note.height/2))).lst()[:-1]
        result["inner_fill_color_interplate_type"] = EnumColorInterplateType.oklch
        result["shadow_color"] = _fold_repeated([Color(*step[1:]).lst() for step in shadow_steps], float_index=True)
        result["shadow_color_interplate_type"] = EnumColorInterplateType.rgb
        
        return result
    
    def analyze_tap(self):
        return self._analyze_path_circle_note(self.tap)
        
    def analyze_drag(self):
        note = self.drag
        result = {
            "size": note.size,
        }
        
        top_point = -1
        top_point_x = -1
        
        for y in range(note.height):
            top_break = False
            for x in range(note.width):
                *_, a = note.getpixel((x, y))
                if a >= 200:
                    top_point_x, top_point = x, y
                    top_break = True
                    break
            
            if top_break:
                break
        else:
            raise ValueError("No top point found")
        
        stroke_end = -1
        stroke_color = None
        for y in range(top_point, note.height):
            *rgb, a = note.getpixel((top_point_x, y))
            
            if stroke_color is None and a == 255:
                stroke_color = rgb
                
            if stroke_end == -1 and stroke_color is not None and _float_rmse(stroke_color, rgb) > 8:
                stroke_end = y
        
        if stroke_end == -1:
            raise ValueError("No stroke end found")
        
        fill_top_points = []
        for x in range(note.width):
            stroke_started = False
            tmp_stroke_color = None
            
            for y in range(top_point, note.height):
                *rgb, a = note.getpixel((x, y))
                
                if not stroke_started and a == 255:
                    stroke_started = True
                    tmp_stroke_color = rgb
                    
                if stroke_started and _float_rmse(tmp_stroke_color, rgb) > 8 and a == 255:
                    fill_top_points.append((x, y))
                    break
                
                if stroke_started:
                    tmp_stroke_color = rgb
        
        if len(fill_top_points) < 2:
            raise ValueError("No fill top points found")
        
        eps = 2
        stroke_width = stroke_end - top_point
        fill_end_k = (fill_top_points[-2][0] - fill_top_points[-1][0]) / (fill_top_points[-1][1] - fill_top_points[-2][1])
        fill_top_points.append((fill_top_points[-1][0] + (fill_top_points[-1][1] - note.height/2) / fill_end_k, note.height/2))
        fill_start_x = min(fill_top_points, key=lambda x: x[0])[0]
        fill_end_x = max(fill_top_points, key=lambda x: x[0])[0]
        fill_start_y = min(fill_top_points, key=lambda x: x[1])[1]
        fill_end_y = max(fill_top_points, key=lambda x: x[1])[1]
        fill_leftbottom_pos = min(fill_top_points[:-1], key=lambda p: (p[0]-fill_start_x)**2 + (p[1]-fill_start_y)**2)
        fill_leftbottom_pos = tuple(map(lambda x: x+eps, fill_leftbottom_pos))
        fill_leftbottom_pos = (fill_leftbottom_pos[0], note.height/2 - fill_leftbottom_pos[1] + note.height/2)
        fill_lefttop_pos = min(fill_top_points[:-1], key=lambda p: (p[0]-fill_start_x)**2 + (p[1]-fill_start_y)**2)
        fill_rightbottom_pos = min(fill_top_points[:-1], key=lambda p: (p[0]-fill_end_x)**2 + (p[1]-fill_end_y)**2)
        stroke_lefttop_pos = tuple(map(lambda x: x-stroke_width/2, fill_lefttop_pos))
        stroke_rightbottom_pos = tuple(map(lambda x: x+stroke_width/2, fill_rightbottom_pos))
        
        result["fill_start_x"] = fill_start_x
        result["fill_end_x"] = fill_end_x
        result["fill_max_height"] = max(map(lambda p: note.height/2 - p[1], fill_top_points))
        result["fill_shape"] = drag_shape
        result["fill_right_color"] = Color(*note.getpixel((fill_top_points[-2][0]-eps, note.height/2))).lst()[:-1]
        result["fill_left_color"] = Color(*note.getpixel(tuple(map(int, fill_leftbottom_pos)))).lst()[:-1]
        result["fill_color_interplate_type"] = EnumColorInterplateType.oklch
        result["stroke_left_color"] = Color(*note.getpixel(tuple(map(int, stroke_lefttop_pos)))).lst()[:-1]
        result["stroke_right_color"] = Color(*note.getpixel(tuple(map(int, stroke_rightbottom_pos)))).lst()[:-1]
        result["stroke_color_interplate_type"] = EnumColorInterplateType.oklch
        result["stroke_width"] = stroke_width
        
        return result

    def _analyze_path_hold_note(self, note: Image.Image, is_double: bool):
        result = {
            "size": note.size,
        }
        
        top_point = -1
        shadow_start = -1
        shadow_size = -1
        shadow_steps = []
        stroke_color = None
        stroke_end = -1
        for y in range(note.height):
            *rgb, a = note.getpixel((note.width//2, y))
            
            if not shadow_steps and shadow_start == -1 and a > 0:
                shadow_start = y
            
            if shadow_start != -1 and a < 200:
                shadow_steps.append((y, *rgb, a))
            
            if top_point == -1 and a > 240:
                top_point = y
                shadow_size = y - shadow_start
                shadow_start = -1
            
            if stroke_color is None and top_point != -1 and a == 255:
                stroke_color = rgb
            
            if stroke_end == -1 and stroke_color is not None and _float_rmse(stroke_color, rgb) > 8:
                stroke_end = y
            
        if top_point == -1: raise ValueError("No top point found")
        if stroke_end == -1: raise ValueError("No stroke end found")
        
        inner_height = (note.height/2 - stroke_end) * 2
        stroke_width = stroke_end - top_point
        result["fill_height"] = inner_height
        result["stroke_width"] = stroke_width
        
        left_point = -1
        for x in range(note.width):
            *_, a = note.getpixel((x, note.height//2))

            if left_point == -1 and a == 255:
                left_point = x
                break

        if left_point == -1: raise ValueError("No left point found")

        right_point = -1
        for x in range(note.width-1, -1, -1):
            *_, a = note.getpixel((x, note.height//2))

            if right_point == -1 and a == 255:
                right_point = x
                break
        
        if right_point == -1: raise ValueError("No right point found")
        
        if not shadow_steps:
            shadow_steps = [(-1, 0, 0, 0, 0)]
        
        eps = 2
        result["stroke_left_color"] = Color(*note.getpixel((left_point+stroke_width//2, note.height//2))).lst()[:-1]
        result["stroke_right_color"] = Color(*note.getpixel((right_point-stroke_width//2, note.height//2))).lst()[:-1]
        result["stroke_color_interplate_type"] = EnumColorInterplateType.oklch
        result["fill_width"] = right_point - left_point - stroke_width * 2 - inner_height
        result["fill_left_color"] = Color(*note.getpixel((left_point+stroke_width+eps, note.height//2))).lst()[:-1]
        result["fill_right_color"] = Color(*note.getpixel((right_point-stroke_width-eps, note.height//2))).lst()[:-1]
        result["fill_color_interplate_type"] = EnumColorInterplateType.oklch
        result["shadow_alpha"] = _fold_repeated([step[-1] for step in shadow_steps], float_index=True)
        result["shadow_size"] = shadow_size
        result["shadow_color_type"] = "stroke" if is_double else "designate"
        if not is_double:
            result["shadow_color"] = Color(*shadow_steps[0][1:]).lst()[:-1]
        
        return result
    
    def analyze_hold(self):
        return self._analyze_path_hold_note(self.hold, False)

    def analyze_tap_double(self):
        return self._analyze_path_circle_note(self.tap_double)
    
    def analyze_hold_double(self):
        return self._analyze_path_hold_note(self.hold_double, True)
    
    def analyze_extap(self):
        return self._analyze_path_circle_note(self.extap)
    
    def analyze_extap_double(self):
        return self._analyze_path_circle_note(self.extap_double)

    def analyze_exhold(self):
        return self._analyze_path_hold_note(self.exhold, False)
    
    def analyze_exhold_double(self):
        return self._analyze_path_hold_note(self.exhold_double, True)
    
    def analyze_line_head(self):
        line_head = self.line_head
        result = {
            "size": line_head.size,
        }
        
        start = -1
        end = -1
        
        for y in range(line_head.height):
            *_, a = line_head.getpixel((line_head.width//2, y))

            if start == -1 and a == 255:
                start = y
            
            if start != -1 and a == 0:
                end = y
                break
        
        if end == -1: raise ValueError("No end found")
        
        result["radius"] = line_head.height/2 - end
        result["stroke_width"] = end - start
        result["stroke_left_color"] = Color(*line_head.getpixel((start+(end-start)//2, line_head.height//2))).lst()[:-1]
        result["stroke_right_color"] = Color(*line_head.getpixel((line_head.width-(start+(end-start)//2), line_head.height//2))).lst()[:-1]
        result["stroke_color_interplate_type"] = EnumColorInterplateType.oklch
        
        return result
    
    def analyze_line_pixel(self):
        line_pixel = self.line_pixel
        result = {
            "size": line_pixel.size,
        }
        
        result["color"] = Color(*line_pixel.getpixel((line_pixel.width//2, line_pixel.height//2))).lst()
        
        return result
    
with open("./drawer_template.html", "r", encoding="utf-8") as f:
    template = f.read()

with open("./ref/canvas2d_ext.js", "r", encoding="utf-8") as f:
    template = template.replace("/* link canvas2d_ext.js */", f.read())

with open("./ref/jszip.min.js", "r", encoding="utf-8") as f:
    template = template.replace("/* link jszip/jszip.min.js */", f.read())
    
for skin in meta["skins"]:
    analyzer = Analyzer(f"./origin/{skin}", f"Milthm Builtin {skin}")
    data = analyzer.analyze()
    
    os.makedirs(f"./analyzed", exist_ok=True)
    with open(f"./analyzed/{skin}.json", "w") as f:
        json.dump(data, f, indent=4)
    
    with open(f"./analyzed/{skin}.html", "w") as f:
        replaced = template.replace("{/* analyzed data */}", json.dumps(data))
        f.write(replaced)
