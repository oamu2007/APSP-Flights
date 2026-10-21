"""La ventana muestra resultados; el grafo y Floyd-Warshall viven en C++."""
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def binary_path():
    for folder in (ROOT / "build/bin", ROOT / "build/bin/Release", ROOT / "build/bin/Debug"):
        for name in ("apsp_demo.exe", "apsp_demo"):
            if (folder / name).is_file():
                return folder / name
    raise RuntimeError("Compila primero con CMake o usa iniciar_demo.cmd")


class Backend:
    """Un proceso C++: carga y calcula una vez, luego responde consultas."""
    def __init__(self, limit=150):
        self.process = subprocess.Popen(
            [str(binary_path()), "--serve", "--limit", str(limit)], cwd=ROOT,
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
    def __init__(self, window, backend, source, target, output):
        import tkinter as tk
        from tkinter import ttk
        self.backend, self.output = backend, output
        self.result = None
        window.title("Demo estudiantil de rutas aereas - Floyd-Warshall")
        window.geometry("1100x760")
        window.minsize(850, 620)
        frame = ttk.Frame(window, padding=16)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Rutas aereas con Floyd-Warshall", font=("Segoe UI", 20)).pack(anchor="w")
        count = len(backend.graph["nodes"])
        ttk.Label(frame, text=f"OpenFlights: {count} aeropuertos fijos, {len(backend.graph['edges'])} conexiones dirigidas. "
                  "Floyd-Warshall calculado una vez al iniciar.", wraplength=1000).pack(anchor="w", pady=8)
        controls = ttk.Frame(frame)
        controls.pack(fill="x", pady=8)
        self.source = tk.StringVar(value=source)
        self.target = tk.StringVar(value=target)
        for column, (label, variable) in enumerate((("Origen", self.source), ("Destino", self.target))):
            ttk.Label(controls, text=label).grid(row=0, column=column, sticky="w")
            ttk.Combobox(controls, textvariable=variable, values=sorted(backend.codes),
                         state="readonly", width=12).grid(row=1, column=column, padx=(0, 12))
        ttk.Button(controls, text="Consultar ruta", command=self.calculate).grid(row=1, column=2)
        self.show_edges = tk.BooleanVar(value=False)
        ttk.Checkbutton(controls, text="Mostrar todas las conexiones", variable=self.show_edges,
                        command=self.render).grid(row=1, column=3, padx=16)

        tabs = ttk.Notebook(frame)
        tabs.pack(fill="both", expand=True, pady=8)
        self.canvas = tk.Canvas(tabs, background="#f5f8fb", highlightthickness=0)
        tabs.add(self.canvas, text="Esquema de la ruta")
        airport_tab = ttk.Frame(tabs)
        tabs.add(airport_tab, text=f"Aeropuertos incluidos ({count})")
        table = ttk.Treeview(airport_tab, columns=("index", "code", "city", "country", "name"), show="headings")
        for key, title, size in (("index", "#", 40), ("code", "IATA", 60), ("city", "Ciudad", 160),
                                 ("country", "Pais", 160), ("name", "Aeropuerto", 370)):
            table.heading(key, text=title)
            table.column(key, width=size, minwidth=35)
        scroll = ttk.Scrollbar(airport_tab, orient="vertical", command=table.yview)
        table.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        table.pack(fill="both", expand=True)
        for index, node in enumerate(backend.graph["nodes"], 1):
            table.insert("", "end", values=(index, node["code"], node["city"], node["country"], node["name"]))
        self.status = tk.StringVar()
        self.details = tk.StringVar()
        ttk.Label(frame, textvariable=self.status, font=("Segoe UI", 12), wraplength=1000).pack(anchor="w", pady=6)
        ttk.Label(frame, textvariable=self.details, wraplength=1000).pack(anchor="w")
        ttk.Label(frame, text="Lista editable: dataset/aeropuertos_demo.txt. Se usan sus primeros N codigos.\n"
                  "Puntos grises: aeropuertos incluidos. Verde: ruta consultada. Los segmentos son un esquema geografico.",
                  wraplength=1000).pack(anchor="w", pady=(10, 0))
        self.canvas.bind("<Configure>", self.render)
        window.bind("<Return>", lambda event: self.calculate())
        self.calculate()

    def calculate(self):
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
        path = self.result["path"] if self.result else []
        draw_graph(self.canvas, self.backend.graph, path, self.show_edges.get())


def main():
    parser = argparse.ArgumentParser(description="Demo estudiantil con lista fija de aeropuertos")
    parser.add_argument("--limit", type=int, default=150, help="Usar los primeros N codigos de aeropuertos_demo.txt")
    parser.add_argument("--from", dest="source", default="MEX")
    parser.add_argument("--to", dest="target", default="NRT")
    parser.add_argument("--headless", action="store_true", help="Consultar sin abrir la ventana")
    parser.add_argument("--list", action="store_true", help="Mostrar los aeropuertos incluidos sin abrir la ventana")
    parser.add_argument("--output", type=Path, default=ROOT / "results/ultima_ruta.json")
    parser.add_argument("--real", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--gui-smoke", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    backend = Backend(args.limit)
    try:
        if args.list:
            for index, node in enumerate(backend.graph["nodes"], 1):
                print(f"{index:3}  {node['code']}  {node['city']}, {node['country']}  |  {node['name']}")
        elif args.headless:
            result = backend.query(args.source, args.target)
            save_result(result, args.output)
            print(describe(result))
            print(f"Grafo fijo: {len(result['nodes'])} aeropuertos, {len(result['edges'])} conexiones dirigidas")
        else:
            import tkinter as tk
            window = tk.Tk()
            if args.gui_smoke:
                window.withdraw()
            app = DemoWindow(window, backend, args.source.upper(), args.target.upper(), args.output)
            if args.gui_smoke:
                window.update_idletasks()
                app.render()
                valid = app.result is not None and bool(app.canvas.find_all())
                window.destroy()
                if not valid:
                    raise RuntimeError("Fallo la comprobacion de ventana y dibujo")
                print("OK: ventana, tabla de aeropuertos, consulta y dibujo")
            else:
                window.mainloop()
    finally:
        backend.close()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
