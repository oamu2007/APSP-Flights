"""La ventana muestra resultados; el grafo y Floyd-Warshall viven en C++."""
import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from time import monotonic, sleep

ROOT = Path(__file__).resolve().parents[1]
PRESETS = {"Historical (150)": "historical", "Expanded (500)": "expanded"}


def binary_path():
    for folder in (ROOT / "build/bin", ROOT / "build/bin/Release", ROOT / "build/bin/Debug"):
        for name in ("apsp_demo.exe", "apsp_demo"):
            if (folder / name).is_file():
                return folder / name
    raise RuntimeError("Compila primero con CMake o usa iniciar_demo.cmd")


class Backend:
    """Un proceso C++: carga y calcula una vez, luego responde consultas."""
    def __init__(self, limit=None, preset="historical"):
        command = [str(binary_path()), "--serve", "--preset", preset]
        if limit is not None:
            command.extend(("--limit", str(limit)))
        self.process = subprocess.Popen(
            command, cwd=ROOT,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8", creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            self.graph = self.read_json()
        except Exception:
            self.close()
            raise
        self.codes = {node["code"] for node in self.graph["nodes"]}

    def read_json(self):
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(self.process.stderr.read().strip() or "El programa C++ se cerro")
        result = json.loads(line)
        if "error" in result:
            raise ValueError(result["error"])
        return result

    def query(self, source, target):
        source, target = source.strip().upper(), target.strip().upper()
        for code in (source, target):
            if code not in self.codes:
                raise ValueError(f"Aeropuerto fuera de la lista seleccionada: {code}")
        self.process.stdin.write(f"{source} {target}\n")
        self.process.stdin.flush()
        return {**self.graph, **self.read_json()}

    def close(self):
        if self.process.stdin:
            self.process.stdin.close()  # EOF: el bucle de consultas C++ termina.
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdout.close()
        self.process.stderr.close()


def describe(result):
    path = result["path"]
    if not path:
        return "No existe ruta entre esos aeropuertos en este grafo."
    flights = len(path) - 1
    return (f"{' -> '.join(path)} | {result['distance_km']:,.2f} km | "
            f"{flights} {'vuelo' if flights == 1 else 'vuelos'}")


def save_result(result, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")


def draw_graph(canvas, graph, path, show_edges):
    """Esquema longitud/latitud. Las lineas no son trayectorias geodesicas."""
    canvas.delete("all")
    width, height = max(canvas.winfo_width(), 600), max(canvas.winfo_height(), 300)
    margin = 30
    nodes = {node["code"]: node for node in graph["nodes"]}

    def xy(lon, lat):
        return margin+(lon+180)/360*(width-2*margin), margin+(90-lat)/180*(height-2*margin)

    for lon in range(-180, 181, 60):
        x, _ = xy(lon, 0)
        canvas.create_line(x, margin, x, height-margin, fill="#dbe3ec", dash=(2, 4))
        canvas.create_text(x, height-12, text=f"{lon}°", fill="#607080")
    for lat in range(-60, 61, 30):
        _, y = xy(0, lat)
        canvas.create_line(margin, y, width-margin, y, fill="#dbe3ec", dash=(2, 4))

    def connection(a, b, color, thickness, arrow):
        start, end = nodes[a], nodes[b]
        x1, y1 = xy(start["lon"], start["lat"])
        x2, y2 = xy(end["lon"], end["lat"])
        delta = end["lon"]-start["lon"]
        if abs(delta) <= 180:
            canvas.create_line(x1, y1, x2, y2, fill=color, width=thickness, arrow=arrow)
        else:
            # Dividir en el meridiano 180 para no dibujar un desvio falso.
            unwrapped = end["lon"] + (-360 if delta > 0 else 360)
            border = -180 if unwrapped < -180 else 180
            ratio = (border-start["lon"])/(unwrapped-start["lon"])
            latitude = start["lat"]+ratio*(end["lat"]-start["lat"])
            xb, yb = xy(border, latitude)
            xo, yo = xy(-border, latitude)
            canvas.create_line(x1, y1, xb, yb, fill=color, width=thickness)
            canvas.create_line(xo, yo, x2, y2, fill=color, width=thickness, arrow=arrow)

    if show_edges:
        for a, b, _ in graph["edges"]:
            connection(a, b, "#d2dce5", 1, "none")
    for a, b in zip(path, path[1:]):
        connection(a, b, "#087e70", 3, "last")
    for code, node in nodes.items():
        x, y = xy(node["lon"], node["lat"])
        active = code in path
        radius = 5 if active else 2
        canvas.create_oval(x-radius, y-radius, x+radius, y+radius,
                           fill="#087e70" if active else "#869aae", outline="")
        if active or len(nodes) <= 10:
            canvas.create_text(x+8, y-12, text=code, anchor="w", fill="#17354d")


class DemoWindow:
    def __init__(self, window, preset, limit, source, target, output):
        import tkinter as tk
        from tkinter import ttk
        self.window, self.output = window, output
        self.backend = None
        self.result = None
        self.worker = ThreadPoolExecutor(max_workers=1)
        self.pending = None
        self.closing = False
        self.active_preset = preset
        self.limits = {preset: limit}
        window.title("Demo estudiantil de rutas aereas - Floyd-Warshall")
        window.geometry("1100x760")
        window.minsize(850, 620)
        frame = ttk.Frame(window, padding=16)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Rutas aereas con Floyd-Warshall", font=("Segoe UI", 20)).pack(anchor="w")
        self.graph_status = tk.StringVar()
        ttk.Label(frame, textvariable=self.graph_status).pack(anchor="w", pady=8)
        controls = ttk.Frame(frame)
        controls.pack(fill="x", pady=8)
        self.source = tk.StringVar(value=source)
        self.target = tk.StringVar(value=target)
        self.selectors = []
        for column, (label, variable) in enumerate((("Source", self.source), ("Destination", self.target))):
            ttk.Label(controls, text=label).grid(row=0, column=column, sticky="w")
            selector = ttk.Combobox(controls, textvariable=variable, width=12)
            selector.configure(postcommand=lambda box=selector, value=variable: self.filter_codes(box, value))
            selector.grid(row=1, column=column, padx=(0, 12))
            self.selectors.append(selector)
        self.query_button = ttk.Button(controls, text="Find route", command=self.calculate)
        self.query_button.grid(row=1, column=2)
        ttk.Label(controls, text="Airport selection").grid(row=0, column=3, padx=16, sticky="w")
        self.preset = tk.StringVar()
        self.select_preset(preset)
        self.preset_box = ttk.Combobox(controls, textvariable=self.preset, values=list(PRESETS),
                                      state="readonly", width=18)
        self.preset_box.grid(row=1, column=3, padx=16)
        self.preset_box.bind("<<ComboboxSelected>>", lambda event: self.load(PRESETS[self.preset.get()]))
        self.show_edges = tk.BooleanVar(value=False)
        ttk.Checkbutton(controls, text="Show all connections", variable=self.show_edges,
                        command=self.render).grid(row=1, column=4)

        self.tabs = ttk.Notebook(frame)
        self.tabs.pack(fill="both", expand=True, pady=8)
        self.canvas = tk.Canvas(self.tabs, background="#f5f8fb", highlightthickness=0)
        self.tabs.add(self.canvas, text="Route map")
        self.airport_tab = ttk.Frame(self.tabs)
        self.tabs.add(self.airport_tab, text="Airports")
        search_bar = ttk.Frame(self.airport_tab)
        search_bar.pack(fill="x", padx=8, pady=8)
        ttk.Label(search_bar, text="Search airports by IATA, city, country or name").pack(side="left")
        self.search = tk.StringVar()
        ttk.Entry(search_bar, textvariable=self.search, width=28).pack(side="left", padx=8)
        self.search.trace_add("write", lambda *_: self.fill_airports())
        airport_tab = self.airport_tab
        table = ttk.Treeview(airport_tab, columns=("index", "code", "city", "country", "name"), show="headings")
        self.table = table
        for key, title, size in (("index", "#", 40), ("code", "IATA", 60), ("city", "City", 160),
                                 ("country", "Country", 160), ("name", "Airport", 370)):
            table.heading(key, text=title)
            table.column(key, width=size, minwidth=35)
        scroll = ttk.Scrollbar(airport_tab, orient="vertical", command=table.yview)
        table.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        table.pack(fill="both", expand=True)
        self.status = tk.StringVar()
        self.details = tk.StringVar()
        ttk.Label(frame, textvariable=self.status, font=("Segoe UI", 12), wraplength=1000).pack(anchor="w", pady=6)
        ttk.Label(frame, textvariable=self.details, wraplength=1000).pack(anchor="w")
        ttk.Label(frame, text="Gray dots mark airports in the selection. Green lines show the selected route.",
                  wraplength=1000).pack(anchor="w", pady=(10, 0))
        self.canvas.bind("<Configure>", self.render)
        window.bind("<Return>", lambda event: self.calculate())
        window.protocol("WM_DELETE_WINDOW", self.close)
        self.load(preset)

    def select_preset(self, preset):
        self.preset.set(next(label for label, key in PRESETS.items() if key == preset))

    def filter_codes(self, selector, variable):
        codes = sorted(self.backend.codes) if self.backend else []
        query = variable.get().strip().upper()
        selector.configure(values=[code for code in codes if query in code] if query not in codes else codes)

    def fill_airports(self):
        self.table.delete(*self.table.get_children())
        if not self.backend:
            return
        query = self.search.get().strip().casefold()
        for index, node in enumerate(self.backend.graph["nodes"], 1):
            fields = (node["code"], node["city"], node["country"], node["name"])
            if any(query in field.casefold() for field in fields):
                self.table.insert("", "end", values=(index, *fields))

    def refresh_graph(self):
        graph = self.backend.graph
        count = len(graph["nodes"])
        self.graph_status.set(f"OpenFlights · {count:,} airports · {len(graph['edges']):,} directed connections")
        self.tabs.tab(self.airport_tab, text=f"Airports ({count})")
        self.fill_airports()
        for selector in self.selectors:
            selector.configure(values=sorted(self.backend.codes))

    def set_controls(self, busy):
        self.preset_box.configure(state="disabled" if busy else "readonly")
        state = "disabled" if busy or not self.backend else "normal"
        self.query_button.configure(state=state)
        for selector in self.selectors:
            selector.configure(state=state)

    def submit(self, work, install, message):
        self.set_controls(True)
        self.status.set(message)
        self.details.set("")
        self.pending = self.worker.submit(work)
        self.window.after(20, self.poll, install)

    def poll(self, install):
        if self.closing:
            return
        if not self.pending.done():
            self.window.after(20, self.poll, install)
            return
        completed, self.pending = self.pending, None
        self.set_controls(False)
        try:
            install(completed.result())
        except (RuntimeError, ValueError, OSError) as error:
            self.select_preset(self.active_preset)
            self.status.set(str(error))

    def load(self, preset):
        if self.pending or (self.backend and self.active_preset == preset):
            return

        def install(backend):
            if self.backend:
                self.worker.submit(self.backend.close)
            self.backend, self.active_preset = backend, preset
            self.set_controls(False)
            self.refresh_graph()
            self.calculate()

        self.submit(lambda: Backend(self.limits.get(preset), preset), install, "Loading airport selection…")

    def close(self):
        if self.closing:
            return
        self.closing = True
        pending, backend = self.pending, self.backend

        def close_backends():
            try:
                if pending and isinstance(result := pending.result(), Backend):
                    result.close()
            finally:
                if backend:
                    backend.close()

        self.worker.submit(close_backends)
        self.worker.shutdown(wait=False)
        self.window.destroy()

    def calculate(self):
        if self.pending or not self.backend:
            return
        try:
            self.result = self.backend.query(self.source.get(), self.target.get())
            self.status.set(describe(self.result))
            weights = {(a, b): km for a, b, km in self.backend.graph["edges"]}
            path = self.result["path"]
            self.details.set(" | ".join(f"{a} -> {b}: {weights[a,b]:,.2f} km" for a, b in zip(path, path[1:])))
            save_result(self.result, self.output)
        except (ValueError, RuntimeError, OSError) as error:
            self.result = None
            self.status.set(str(error))
            self.details.set("")
        self.render()

    def render(self, event=None):
        if not self.backend:
            return
        path = self.result["path"] if self.result else []
        draw_graph(self.canvas, self.backend.graph, path, self.show_edges.get())


def wait_for_gui(window, app):
    deadline = monotonic() + 15
    while app.pending and monotonic() < deadline:
        window.update()
        sleep(0.01)
    if app.pending or app.result is None or not app.canvas.find_all():
        raise RuntimeError(f"Window check failed: {app.status.get()}")


def main():
    parser = argparse.ArgumentParser(description="Demo estudiantil con lista fija de aeropuertos")
    parser.add_argument("--preset", choices=tuple(PRESETS.values()), default="historical",
                        help="Choose the fixed 150 or the expanded 500 airports")
    parser.add_argument("--limit", type=int, help="Use the first N airports in the chosen selection")
    parser.add_argument("--from", dest="source", default="MEX")
    parser.add_argument("--to", dest="target", default="NRT")
    parser.add_argument("--headless", action="store_true", help="Consultar sin abrir la ventana")
    parser.add_argument("--list", action="store_true", help="Mostrar los aeropuertos incluidos sin abrir la ventana")
    parser.add_argument("--output", type=Path, default=ROOT / "results/ultima_ruta.json")
    parser.add_argument("--real", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--gui-smoke", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not (args.list or args.headless):
        import tkinter as tk
        window = tk.Tk()
        if args.gui_smoke:
            window.withdraw()
        app = DemoWindow(window, args.preset, args.limit, args.source.upper(), args.target.upper(), args.output)
        try:
            if args.gui_smoke:
                wait_for_gui(window, app)
                app.search.set(args.source)
                if len(app.table.get_children()) == 0:
                    raise RuntimeError("Airport search did not find the source")
                if args.preset == "historical" and args.limit is None:
                    app.select_preset("expanded")
                    app.preset_box.event_generate("<<ComboboxSelected>>")
                    window.update()
                    wait_for_gui(window, app)
                    if len(app.backend.codes) != 500:
                        raise RuntimeError("The expanded graph did not load")
                print("OK: window, airport search, route, map and selection")
            else:
                window.mainloop()
        finally:
            app.close()
        return

    backend = Backend(args.limit, args.preset)
    try:
        if args.list:
            for index, node in enumerate(backend.graph["nodes"], 1):
                print(f"{index:3}  {node['code']}  {node['city']}, {node['country']}  |  {node['name']}")
        else:
            result = backend.query(args.source, args.target)
            save_result(result, args.output)
            print(describe(result))
            print(f"OpenFlights: {len(result['nodes'])} airports, {len(result['edges'])} directed connections")
    finally:
        backend.close()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
