import math
import os
import sys
import tkinter as tk
from tkinter import filedialog, messagebox

AXES = ("Min X", "Min Y", "Min Z", "Max X", "Max Y", "Max Z")

class MapEditor:
    def __init__(self, root, map_path):
        self.root = root
        self.map_path = os.path.abspath(map_path)
        self.root.title(f"Vcust Map Editor — {os.path.basename(self.map_path)}")
        self.root.geometry("1280x820")
        self.root.minsize(850, 560)

        self.view_mode = "top"
        self.scale = 28.0
        self.offset_x = 0.0
        self.offset_y = 0.0
        self.snap_enabled = tk.BooleanVar(value=True)
        self.grid_enabled = tk.BooleanVar(value=True)
        self.mode_var = tk.StringVar(value="select")

        self.walls = []
        self.floors = [] # NOWE: wiele podłóg
        self.bounds = [-10.0, -1.0, -10.0, 10.0, 10.0, 10.0]
        self.player_start = [0.0, 0.0, 0.0]
        self.other_lines = []
        self.selected_idx = -1
        self.drag_mode = None
        self.drag_start = (0.0, 0.0)
        self.drag_orig_wall = None
        self.drawing = False
        self.start_pos = (0.0, 0.0)
        self.temp_box = None
        self.pan_start = None
        self.pan_origin = None
        self.dirty = False
        self._loading_properties = False
        self._status_text = tk.StringVar(value="Ready")
        self._zoom_label = tk.StringVar(value="28 px / unit")
        self.space_down = False

        self._build_ui()
        self._bind_events()
        self.load_map(silent=True)
        self.root.after(80, self.fit_view)

    def _build_ui(self):
        toolbar = tk.Frame(self.root, bg="#25282d", padx=7, pady=6)
        toolbar.pack(side=tk.TOP, fill=tk.X)

        tk.Label(toolbar, text="VCUST MAP", bg="#25282d", fg="#e7eaf0",
                 font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT, padx=(0, 12))

        for label, mode, shortcut in (
            ("Select", "select", "1"),
            ("Draw wall", "draw", "2"),
            ("Set bounds", "floor", "3"),
            ("Place player", "player", "4"),
            ("Draw floor", "floor_box", "5"), # NOWE
        ):
            tk.Radiobutton(
                toolbar, text=f"{label} [{shortcut}]", variable=self.mode_var,
                value=mode, indicatoron=False, padx=8, pady=4,
                bg="#373b42", fg="#eeeeee", selectcolor="#176b58",
                activebackground="#454b54", activeforeground="white",
                command=self.update_mode
            ).pack(side=tk.LEFT, padx=2)

        tk.Frame(toolbar, width=1, bg="#555b64").pack(side=tk.LEFT, fill=tk.Y, padx=8)
        tk.Button(toolbar, text="Top view [T]", command=lambda: self.set_view("top")).pack(side=tk.LEFT, padx=2)
        tk.Button(toolbar, text="Side view [S]", command=lambda: self.set_view("side")).pack(side=tk.LEFT, padx=2)
        tk.Button(toolbar, text="Fit [F]", command=self.fit_view).pack(side=tk.LEFT, padx=2)
        tk.Button(toolbar, text="Undo [Ctrl+Z]", command=self.undo).pack(side=tk.LEFT, padx=(8, 2))
        tk.Button(toolbar, text="Redo [Ctrl+Y]", command=self.redo).pack(side=tk.LEFT, padx=2)
        tk.Button(toolbar, text="Open", command=self.open_map).pack(side=tk.RIGHT, padx=2)
        tk.Button(toolbar, text="Save [Ctrl+S]", command=self.save_map).pack(side=tk.RIGHT, padx=2)

        body = tk.Frame(self.root, bg="#202226")
        body.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(body, bg="#1b1d21", highlightthickness=0, cursor="arrow", takefocus=True)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        panel = tk.Frame(body, bg="#282b31", width=245, padx=12, pady=12)
        panel.pack(side=tk.RIGHT, fill=tk.Y)
        panel.pack_propagate(False)

        tk.Label(panel, text="PROPERTIES", bg="#282b31", fg="#dfe5ec",
                 font=("Segoe UI", 10, "bold")).pack(anchor="w")
        self.prop_label = tk.Label(panel, text="Nothing selected", bg="#282b31",
                                   fg="#9da6b2", font=("Segoe UI", 9))
        self.prop_label.pack(anchor="w", pady=(4, 12))

        self.entries = {}
        for axis in AXES:
            line = tk.Frame(panel, bg="#282b31")
            line.pack(fill=tk.X, pady=3)
            tk.Label(line, text=axis, width=8, anchor="w", bg="#282b31", fg="#d8dce3").pack(side=tk.LEFT)
            entry = tk.Entry(line, width=13, bg="#17191d", fg="#f2f4f7", insertbackground="white", relief=tk.FLAT)
            entry.pack(side=tk.RIGHT, ipady=4)
            self.entries[axis] = entry
            entry.bind("<Return>", lambda _e: self.apply_properties())

        tk.Button(panel, text="Apply properties", command=self.apply_properties).pack(fill=tk.X, pady=(12, 5))
        tk.Button(panel, text="Delete selected [Del]", command=self.delete_selected,
                  bg="#743b40", fg="white", activebackground="#8b464c").pack(fill=tk.X, pady=3)

        tk.Frame(panel, height=1, bg="#4a4e56").pack(fill=tk.X, pady=12)
        tk.Checkbutton(panel, text="Snap to grid", variable=self.snap_enabled,
                       bg="#282b31", fg="#e0e3e8", selectcolor="#17191d", activebackground="#282b31").pack(anchor="w")
        tk.Checkbutton(panel, text="Show grid", variable=self.grid_enabled, command=self.redraw,
                       bg="#282b31", fg="#e0e3e8", selectcolor="#17191d", activebackground="#282b31").pack(anchor="w")

        tk.Label(panel, text="CONTROLS", bg="#282b31", fg="#dfe5ec",
                 font=("Segoe UI", 9, "bold")).pack(anchor="w", pady=(18, 5))
        help_text = (
            "Left drag: move / draw\n"
            "Corner handles: resize wall\n"
            "Middle mouse or Space+drag: pan\n"
            "Mouse wheel: zoom at cursor\n"
            "1-5: tools   T/S: views\n"
            "F: fit map   Del: delete\n"
            "Ctrl+Z/Y: undo / redo"
        )
        tk.Label(panel, text=help_text, justify=tk.LEFT, anchor="w",
                 bg="#282b31", fg="#aeb6c2", font=("Consolas", 9)).pack(anchor="w")

        status = tk.Frame(self.root, bg="#111317", padx=8, pady=5)
        status.pack(side=tk.BOTTOM, fill=tk.X)
        tk.Label(status, textvariable=self._status_text, bg="#111317", fg="#c9d0da", anchor="w").pack(side=tk.LEFT, fill=tk.X, expand=True)
        tk.Label(status, textvariable=self._zoom_label, bg="#111317", fg="#8f99a7", anchor="e").pack(side=tk.RIGHT)

        self._update_title()

    def _bind_events(self):
        self.canvas.bind("<Button-1>", self.on_mouse_down)
        self.canvas.bind("<B1-Motion>", self.on_mouse_move)
        self.canvas.bind("<ButtonRelease-1>", self.on_mouse_up)
        self.canvas.bind("<MouseWheel>", self.on_zoom)
        self.canvas.bind("<Button-4>", lambda e: self.zoom_at(e.x, e.y, 1.15))
        self.canvas.bind("<Button-5>", lambda e: self.zoom_at(e.x, e.y, 1 / 1.15))
        self.canvas.bind("<ButtonPress-2>", self.start_pan)
        self.canvas.bind("<B2-Motion>", self.do_pan)
        self.canvas.bind("<ButtonRelease-2>", self.end_pan)
        self.canvas.bind("<Configure>", lambda _e: self.redraw())

        self.root.bind("<KeyPress-1>", lambda _e: self.set_mode("select"))
        self.root.bind("<KeyPress-2>", lambda _e: self.set_mode("draw"))
        self.root.bind("<KeyPress-3>", lambda _e: self.set_mode("floor"))
        self.root.bind("<KeyPress-4>", lambda _e: self.set_mode("player"))
        self.root.bind("<KeyPress-5>", lambda _e: self.set_mode("floor_box")) # NOWE
        self.root.bind("<KeyPress-t>", lambda _e: self.set_view("top"))
        self.root.bind("<KeyPress-s>", lambda _e: self.set_view("side"))
        self.root.bind("<KeyPress-f>", lambda _e: self.fit_view())
        self.root.bind("<Delete>", lambda _e: self.delete_selected())
        self.root.bind("<Control-s>", lambda _e: self.save_map())
        self.root.bind("<Control-z>", lambda _e: self.undo())
        self.root.bind("<Control-y>", lambda _e: self.redo())
        self.root.bind("<space>", self._space_down)
        self.root.bind("<KeyRelease-space>", self._space_up)
        self.root.bind("<Escape>", self.cancel_action)

    def _space_down(self, _event): self.space_down = True
    def _space_up(self, _event): self.space_down = False

    def set_mode(self, mode):
        self.mode_var.set(mode)
        self.update_mode()

    def update_mode(self):
        mode = self.mode_var.get()
        cursors = {"select": "arrow", "draw": "crosshair", "floor": "crosshair", "player": "hand2", "floor_box": "crosshair"}
        self.canvas.config(cursor=cursors.get(mode, "arrow"))
        texts = {
            "select": "Select: click a wall to select it; drag to move; drag corners to resize.",
            "draw": "Draw wall: drag on the canvas to create a wall.",
            "floor": "Set bounds: drag to define the main map boundaries.",
            "player": "Place player: click where the player should start.",
            "floor_box": "Draw floor: drag to create a new floor tile."
        }
        self._status_text.set(texts.get(mode, "Ready"))

    def set_view(self, view):
        if view == self.view_mode: return
        self.view_mode = view
        self.selected_idx = -1
        self.clear_properties()
        self.redraw()
        self._status_text.set("Top view (X/Z)" if view == "top" else "Side view (X/Y)")

    def world_to_screen(self, x, y, z):
        if self.view_mode == "top": return self.offset_x + x * self.scale, self.offset_y + z * self.scale
        return self.offset_x + x * self.scale, self.offset_y - y * self.scale

    def screen_to_world(self, sx, sy):
        if self.view_mode == "top": return (sx - self.offset_x) / self.scale, (sy - self.offset_y) / self.scale
        return (sx - self.offset_x) / self.scale, (self.offset_y - sy) / self.scale

    def _snap(self, value): return round(value) if self.snap_enabled.get() else value

    def _snapshot(self):
        return {"walls": [list(w) for w in self.walls], "floors": [list(f) for f in self.floors],
                "bounds": list(self.bounds), "player_start": list(self.player_start)}

    def _restore(self, state):
        self.walls = [list(w) for w in state["walls"]]
        self.floors = [list(f) for f in state["floors"]]
        self.bounds = list(state["bounds"])
        self.player_start = list(state["player_start"])
        self.selected_idx = -1
        self.clear_properties()
        self._mark_dirty()
        self.redraw()

    def _record_undo(self):
        if not hasattr(self, "_undo_stack"): self._undo_stack, self._redo_stack = [], []
        self._undo_stack.append(self._snapshot())
        if len(self._undo_stack) > 100: self._undo_stack.pop(0)
        self._redo_stack.clear()

    def undo(self):
        if not getattr(self, "_undo_stack", []): return
        self._redo_stack.append(self._snapshot())
        self._restore(self._undo_stack.pop())

    def redo(self):
        if not getattr(self, "_redo_stack", []): return
        self._undo_stack.append(self._snapshot())
        self._restore(self._redo_stack.pop())

    def _mark_dirty(self):
        self.dirty = True
        self._update_title()

    def _update_title(self):
        marker = " *" if self.dirty else ""
        self.root.title(f"Vcust Map Editor — {os.path.basename(self.map_path)}{marker}")

    def update_properties(self):
        if 0 <= self.selected_idx < len(self.walls):
            wall = self.walls[self.selected_idx]
            self._loading_properties = True
            for axis, value in zip(AXES, wall):
                entry = self.entries[axis]
                entry.delete(0, tk.END)
                entry.insert(0, f"{value:g}")
            self._loading_properties = False
            self.prop_label.config(text=f"Wall #{self.selected_idx + 1}", fg="#75e0ad")
        else:
            self.clear_properties()

    def clear_properties(self):
        self._loading_properties = True
        for entry in self.entries.values(): entry.delete(0, tk.END)
        self._loading_properties = False
        self.prop_label.config(text="Nothing selected", fg="#9da6b2")

    def apply_properties(self):
        if not (0 <= self.selected_idx < len(self.walls)): return
        try:
            values = [float(self.entries[a].get()) for a in AXES]
            if not all(math.isfinite(v) for v in values): raise ValueError
            wall = [min(values[0], values[3]), min(values[1], values[4]), min(values[2], values[5]),
                    max(values[0], values[3]), max(values[1], values[4]), max(values[2], values[5])]
            if wall[0] == wall[3] or wall[1] == wall[4] or wall[2] == wall[5]:
                if not messagebox.askyesno("Zero-size wall", "One dimension is zero. Apply anyway?"): return
            self._record_undo()
            self.walls[self.selected_idx] = wall
            self._mark_dirty()
            self.update_properties()
            self.redraw()
        except ValueError:
            messagebox.showerror("Invalid values", "Enter six finite numbers.")

    def delete_selected(self):
        if not (0 <= self.selected_idx < len(self.walls)): return
        self._record_undo()
        del self.walls[self.selected_idx]
        self.selected_idx = -1
        self.clear_properties()
        self._mark_dirty()
        self.redraw()

    def load_map(self, silent=False):
        walls, floors, other_lines = [], [], []
        bounds, player = list(self.bounds), list(self.player_start)
        try:
            with open(self.map_path, "r", encoding="utf-8") as file:
                for line_no, raw in enumerate(file, 1):
                    stripped = raw.strip()
                    if not stripped or stripped.startswith("#"):
                        if stripped: other_lines.append(stripped)
                        continue
                    content = stripped.split("#", 1)[0].strip()
                    if not content:
                        other_lines.append(stripped)
                        continue
                    parts = content.split()
                    key = parts[0]
                    try:
                        if key == "bounds":
                            bounds = [float(v) for v in parts[1:7]]
                        elif key == "wall":
                            values = [float(v) for v in parts[1:7]]
                            walls.append([min(values[0], values[3]), min(values[1], values[4]), min(values[2], values[5]),
                                          max(values[0], values[3]), max(values[1], values[4]), max(values[2], values[5])])
                        elif key == "floor":
                            values = [float(v) for v in parts[1:7]]
                            floors.append([min(values[0], values[3]), min(values[1], values[4]), min(values[2], values[5]),
                                           max(values[0], values[3]), max(values[1], values[4]), max(values[2], values[5])])
                        elif key == "player_start":
                            player = [float(v) for v in parts[1:4]]
                        else:
                            other_lines.append(stripped)
                    except ValueError as exc:
                        raise ValueError(f"Line {line_no}: {exc}") from exc
            self.walls, self.floors, self.bounds, self.player_start, self.other_lines = walls, floors, bounds, player, other_lines
            self.selected_idx = -1
            self.clear_properties()
            self.dirty = False
            self._update_title()
            self.redraw()
            return True
        except (OSError, ValueError) as exc:
            if not silent: messagebox.showerror("Could not load map", str(exc))
            return False

    def save_map(self):
        try:
            folder = os.path.dirname(self.map_path)
            if folder: os.makedirs(folder, exist_ok=True)
            with open(self.map_path, "w", encoding="utf-8", newline="\n") as file:
                for line in self.other_lines: file.write(line + "\n")
                file.write(f"player_start {self.player_start[0]:g} {self.player_start[1]:g} {self.player_start[2]:g}\n")
                file.write("bounds " + " ".join(f"{v:g}" for v in self.bounds) + "\n")
                for floor in self.floors:
                    file.write("floor " + " ".join(f"{v:g}" for v in floor) + "\n")
                for wall in self.walls:
                    file.write("wall " + " ".join(f"{v:g}" for v in wall) + "\n")
            self.dirty = False
            self._update_title()
            return True
        except OSError as exc:
            messagebox.showerror("Save failed", str(exc))
            return False

    def open_map(self):
        path = filedialog.askopenfilename(title="Open map", initialdir=os.path.dirname(self.map_path),
            filetypes=[("Map files", "*.txt *.map"), ("Text files", "*.txt"), ("All files", "*.*")])
        if path and (not self.dirty or messagebox.askyesno("Unsaved changes", "Discard unsaved changes?")):
            self.map_path = os.path.abspath(path)
            if self.load_map(): self.fit_view()

    def fit_view(self):
        width, height = max(self.canvas.winfo_width(), 400), max(self.canvas.winfo_height(), 300)
        xs = [v for w in self.walls for v in (w[0], w[3])] + [v for f in self.floors for v in (f[0], f[3])] + [self.bounds[0], self.bounds[3], self.player_start[0]]
        if self.view_mode == "top":
            ys = [v for w in self.walls for v in (w[2], w[5])] + [v for f in self.floors for v in (f[2], f[5])] + [self.bounds[2], self.bounds[5], self.player_start[2]]
        else:
            ys = [v for w in self.walls for v in (w[1], w[4])] + [v for f in self.floors for v in (f[1], f[4])] + [self.bounds[1], self.bounds[4], self.player_start[1]]
        if not xs: xs, ys = [-10, 10], [-10, 10]
        min_x, max_x, min_y, max_y = min(xs), max(xs), min(ys), max(ys)
        span_x, span_y = max(max_x - min_x, 4.0), max(max_y - min_y, 4.0)
        self.scale = max(3.0, min(70.0, min((width - 90) / span_x, (height - 90) / span_y)))
        self.offset_x = width / 2 - ((min_x + max_x) / 2) * self.scale
        self.offset_y = height / 2 - ((min_y + max_y) / 2) * self.scale if self.view_mode == "top" else height / 2 + ((min_y + max_y) / 2) * self.scale
        self._zoom_label.set(f"{self.scale:.1f} px / unit")
        self.redraw()

    def redraw(self):
        if not hasattr(self, "canvas"): return
        self.canvas.delete("all")
        width, height = max(self.canvas.winfo_width(), 1), max(self.canvas.winfo_height(), 1)
        if self.grid_enabled.get(): self._draw_grid(width, height)

        # Bounds
        if self.view_mode == "top":
            x1, y1 = self.world_to_screen(self.bounds[0], 0, self.bounds[2])
            x2, y2 = self.world_to_screen(self.bounds[3], 0, self.bounds[5])
        else:
            x1, y1 = self.world_to_screen(self.bounds[0], self.bounds[4], 0)
            x2, y2 = self.world_to_screen(self.bounds[3], self.bounds[1], 0)
        self.canvas.create_rectangle(x1, y1, x2, y2, outline="#647180", fill="#242a32", stipple="gray12", width=2)

        # Floors
        for f in self.floors:
            if self.view_mode == "top":
                fx1, fy1 = self.world_to_screen(f[0], 0, f[2])
                fx2, fy2 = self.world_to_screen(f[3], 0, f[5])
            else:
                fx1, fy1 = self.world_to_screen(f[0], f[4], 0)
                fx2, fy2 = self.world_to_screen(f[3], f[1], 0)
            self.canvas.create_rectangle(fx1, fy1, fx2, fy2, fill="#2b4c3b", outline="#3d6b52", dash=(2,2))

        # Walls
        for i, wall in enumerate(self.walls):
            selected = i == self.selected_idx
            fill = "#269a71" if selected else "#a54b42"
            if self.view_mode == "top":
                x1, y1 = self.world_to_screen(wall[0], 0, wall[2])
                x2, y2 = self.world_to_screen(wall[3], 0, wall[5])
            else:
                x1, y1 = self.world_to_screen(wall[0], wall[4], 0)
                x2, y2 = self.world_to_screen(wall[3], wall[1], 0)
            self.canvas.create_rectangle(x1, y1, x2, y2, fill=fill, outline="#f4f7fb" if selected else "#d27a6f",
                                         width=2 if selected else 1)
            if selected: self._draw_handles(wall)

        # Player
        px, py = (self.world_to_screen(self.player_start[0], 0, self.player_start[2])
                  if self.view_mode == "top" else self.world_to_screen(self.player_start[0], self.player_start[1], 0))
        self.canvas.create_oval(px-7, py-7, px+7, py+7, fill="#ffd34e", outline="#fff0ad", width=2)

        # Preview
        if self.drawing and self.temp_box:
            b = self.temp_box
            if self.view_mode == "top":
                x1, y1 = self.world_to_screen(b[0], 0, b[2])
                x2, y2 = self.world_to_screen(b[3], 0, b[5])
            else:
                x1, y1 = self.world_to_screen(b[0], b[4], 0)
                x2, y2 = self.world_to_screen(b[3], b[1], 0)
            self.canvas.create_rectangle(x1, y1, x2, y2, outline="#52d9ef", dash=(5, 3), width=2)
        self._zoom_label.set(f"{self.scale:.1f} px / unit")

    def _draw_grid(self, width, height):
        step = 1
        while self.scale * step < 14: step *= 2
        left, right = (0 - self.offset_x) / self.scale, (width - self.offset_x) / self.scale
        top, bottom = ((0 - self.offset_y) / self.scale, (height - self.offset_y) / self.scale) if self.view_mode == "top" else ((self.offset_y - height) / self.scale, self.offset_y / self.scale)
        for x in range(int(math.floor(left / step) * step), int(math.ceil(right / step) * step) + step, step):
            sx, _ = self.world_to_screen(x, 0, 0)
            self.canvas.create_line(sx, 0, sx, height, fill="#41464f" if x else "#718094")
        for y in range(int(math.floor(min(top, bottom) / step) * step), int(math.ceil(max(top, bottom) / step) * step) + step, step):
            _, sy = self.world_to_screen(0, 0, y) if self.view_mode == "top" else self.world_to_screen(0, y, 0)
            self.canvas.create_line(0, sy, width, sy, fill="#41464f" if y else "#718094")

    def _draw_handles(self, wall):
        if self.view_mode == "top":
            pts = [(wall[0], wall[2]), (wall[3], wall[2]), (wall[3], wall[5]), (wall[0], wall[5])]
            screen = [self.world_to_screen(x, 0, z) for x, z in pts]
        else:
            pts = [(wall[0], wall[4]), (wall[3], wall[4]), (wall[3], wall[1]), (wall[0], wall[1])]
            screen = [self.world_to_screen(x, y, 0) for x, y in pts]
        for x, y in screen:
            self.canvas.create_rectangle(x-5, y-5, x+5, y+5, fill="#f3f5f8", outline="#1c2026")

    def _handle_at(self, sx, sy):
        if not (0 <= self.selected_idx < len(self.walls)): return None
        wall = self.walls[self.selected_idx]
        pts = ([(wall[0], wall[2]), (wall[3], wall[2]), (wall[3], wall[5]), (wall[0], wall[5])]
               if self.view_mode == "top" else
               [(wall[0], wall[4]), (wall[3], wall[4]), (wall[3], wall[1]), (wall[0], wall[1])])
        for i, (x, y) in enumerate(pts):
            px, py = self.world_to_screen(x, 0, y) if self.view_mode == "top" else self.world_to_screen(x, y, 0)
            if abs(sx - px) <= 9 and abs(sy - py) <= 9: return i
        return None

    def _wall_at(self, wx, wy):
        for i in range(len(self.walls) - 1, -1, -1):
            w = self.walls[i]
            inside = (w[0] <= wx <= w[3] and w[2] <= wy <= w[5]) if self.view_mode == "top" else (w[0] <= wx <= w[3] and w[1] <= wy <= w[4])
            if inside: return i
        return -1

    def on_mouse_down(self, event):
        self.canvas.focus_set()
        if self.space_down: self.start_pan(event); return
        wx, wy = self.screen_to_world(event.x, event.y)
        mode = self.mode_var.get()

        if mode == "select":
            handle = self._handle_at(event.x, event.y)
            if handle is not None:
                self._record_undo()
                self.drag_mode, self.drag_start, self.drag_orig_wall = f"handle_{handle}", (wx, wy), list(self.walls[self.selected_idx])
                return
            idx = self._wall_at(wx, wy)
            self.selected_idx = idx
            if idx >= 0:
                self._record_undo()
                self.drag_mode, self.drag_start, self.drag_orig_wall = "move", (wx, wy), list(self.walls[idx])
                self.update_properties()
            else: self.drag_mode = None; self.clear_properties()
            self.redraw()

        elif mode in ("draw", "floor", "floor_box"):
            self.drawing = True
            sx, sy = self._snap(wx), self._snap(wy)
            self.start_pos = (sx, sy)
            self._record_undo()
            if mode == "draw": self.temp_box = self._make_box(sx, sy, sx, sy)
            self.redraw()

        elif mode == "player":
            self._record_undo()
            self.player_start[0] = self._snap(wx)
            if self.view_mode == "side": self.player_start[1] = self._snap(wy)
            else: self.player_start[2] = self._snap(wy)
            self._mark_dirty()
            self.redraw()

    def _make_box(self, x1, y1, x2, y2):
        lo_x, hi_x = sorted((x1, x2))
        lo_y, hi_y = sorted((y1, y2))
        if self.view_mode == "top":
            return [lo_x, 0.0, lo_y, hi_x, 3.0, hi_y]
        return [lo_x, lo_y, 0.0, hi_x, hi_y, 1.0]

    def _make_floor_box(self, x1, y1, x2, y2):
        lo_x, hi_x = sorted((x1, x2))
        lo_y, hi_y = sorted((y1, y2))
        return [lo_x, self.bounds[1], lo_y, hi_x, self.bounds[1] + 0.1, hi_y]
        
    def on_mouse_move(self, event):
        if self.pan_start is not None: self.do_pan(event); return
        if not self.drawing and self.drag_mode is None: return
        wx, wy = self._snap(self.screen_to_world(event.x, event.y)[0]), self._snap(self.screen_to_world(event.x, event.y)[1])

        if self.drag_mode == "move" and self.selected_idx >= 0:
            dx, dy = wx - self.drag_start[0], wy - self.drag_start[1]
            w = list(self.drag_orig_wall); w[0] += dx; w[3] += dx
            if self.view_mode == "top": w[2] += dy; w[5] += dy
            else: w[1] += dy; w[4] += dy
            self.walls[self.selected_idx] = w; self.update_properties(); self._mark_dirty(); self.redraw()

        elif self.drag_mode and self.drag_mode.startswith("handle_") and self.selected_idx >= 0:
            idx = int(self.drag_mode.split("_")[1])
            w = list(self.drag_orig_wall)
            if self.view_mode == "top":
                if idx in (0, 3): w[0] = wx
                if idx in (1, 2): w[3] = wx
                if idx in (0, 1): w[2] = wy
                if idx in (2, 3): w[5] = wy
            else:
                if idx in (0, 3): w[0] = wx
                if idx in (1, 2): w[3] = wx
                if idx in (0, 1): w[4] = wy
                if idx in (2, 3): w[1] = wy
            self.walls[self.selected_idx] = [min(w[0], w[3]), min(w[1], w[4]), min(w[2], w[5]), max(w[0], w[3]), max(w[1], w[4]), max(w[2], w[5])]
            self.update_properties(); self._mark_dirty(); self.redraw()

        elif self.drawing:
            if self.mode_var.get() == "draw": self.temp_box = self._make_box(self.start_pos[0], self.start_pos[1], wx, wy)
            elif self.mode_var.get() == "floor":
                x1, x2, y1, y2 = *sorted((self.start_pos[0], wx)), *sorted((self.start_pos[1], wy))
                if self.view_mode == "top": self.bounds = [x1, self.bounds[1], y1, x2, self.bounds[4], y2]
                else: self.bounds = [x1, y1, self.bounds[2], x2, y2, self.bounds[5]]
                self._mark_dirty()
            elif self.mode_var.get() == "floor_box":
                self.temp_box = self._make_floor_box(self.start_pos[0], self.start_pos[1], wx, wy)
            self.redraw()

    def on_mouse_up(self, _event):
        if self.pan_start is not None: self.end_pan(_event); return
        if self.drawing:
            if self.mode_var.get() == "draw" and self.temp_box:
                b = self.temp_box
                if b[0] != b[3] and b[2] != b[5] and (self.view_mode == "top" or b[1] != b[4]):
                    self.walls.append(b); self.selected_idx = len(self.walls) - 1; self.update_properties(); self._mark_dirty()
                else: self._undo_stack.pop()
            elif self.mode_var.get() == "floor_box" and self.temp_box:
                b = self.temp_box
                if b[0] != b[3] and b[2] != b[5]:
                    self.floors.append(b); self._mark_dirty()
                else: self._undo_stack.pop()
            self.temp_box, self.drawing = None, False
            self.redraw()
        self.drag_mode = None

    def start_pan(self, event):
        self.pan_start, self.pan_origin = (event.x, event.y), (self.offset_x, self.offset_y)
        self.canvas.config(cursor="fleur")

    def do_pan(self, event):
        if self.pan_start is None: return
        self.offset_x = self.pan_origin[0] + event.x - self.pan_start[0]
        self.offset_y = self.pan_origin[1] + event.y - self.pan_start[1]
        self.redraw()

    def end_pan(self, _event):
        self.pan_start, self.pan_origin = None, None
        self.update_mode()

    def on_zoom(self, event):
        self.zoom_at(event.x, event.y, 1.15 if event.delta > 0 else 1 / 1.15)

    def zoom_at(self, sx, sy, factor):
        before_x, before_y = self.screen_to_world(sx, sy)
        self.scale = max(2.0, min(100.0, self.scale * factor))
        if self.view_mode == "top": self.offset_x, self.offset_y = sx - before_x * self.scale, sy - before_y * self.scale
        else: self.offset_x, self.offset_y = sx - before_x * self.scale, sy + before_y * self.scale
        self.redraw()

    def cancel_action(self, _event=None):
        self.drawing, self.temp_box, self.drag_mode, self.pan_start = False, None, None, None
        self.redraw()

    def on_close(self):
        if self.dirty:
            answer = messagebox.askyesnocancel("Unsaved changes", "Save changes before closing?")
            if answer is None: return
            if answer and not self.save_map(): return
        self.root.destroy()

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_map = os.path.join(script_dir, "content", "maps", "room01", "map.txt")
    path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else default_map
    root = tk.Tk()
    app = MapEditor(root, path)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    if not os.path.exists(path):
        messagebox.showwarning("Map not found", f"Map file does not exist yet:\n{path}\n\nUse Open to select an existing map, or Save to create it.")
    root.mainloop()