#!/usr/bin/env python3
"""Tableau de bord local CESI. Python 3 + Tkinter + pyserial.

Usage : python station_dashboard.py [--demo | --self-test]
Les données restent locales. Aucun accès réseau.
"""
import csv
import math
import queue
import random
import re
import sys
import threading
import time
from collections import deque
from datetime import datetime
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

METRICS = [("Temperature", "Température", "°C", "#fb923c"),
           ("Humidite", "Humidité", "%", "#38bdf8"),
           ("Pression", "Pression", "hPa", "#a78bfa"),
           ("Luminosite", "Luminosité", "/ 1023", "#facc15")]
KEYS = ["Date / heure", "Mode", "Temperature", "Humidite", "Pression",
        "Luminosite", "Latitude", "Longitude"]
BG, PANEL, FG, MUTED = "#0b1220", "#162238", "#edf4ff", "#9baec7"


def number(value):
    try:
        x = float(str(value).split()[0])
        return x if math.isfinite(x) else None
    except (ValueError, IndexError, TypeError):
        return None


class Parser:
    """Accepte le format historique CSV et les blocs lisibles du firmware."""
    def __init__(self):
        self.pending = None

    def feed(self, line):
        line = line.strip()
        parts = line.split(";")
        if len(parts) == 8 and parts[1] in ("STANDARD", "ECONOMIQUE", "MAINTENANCE"):
            self.pending = None
            return dict(zip(KEYS, parts))
        if ":" not in line:
            return None
        key, value = (x.strip() for x in line.split(":", 1))
        if key == "Date / heure":
            self.pending = {}
        if self.pending is not None and key in KEYS:
            self.pending[key] = value
            if key == "Longitude":
                result = self.pending
                self.pending = None
                if all(k in result for k in KEYS):
                    return result
        return None


def serial_worker(port, incoming, commands, stop):
    """Tout accès au port est effectué par ce thread, jamais par Tkinter."""
    try:
        import serial
        with serial.Serial(port, 115200, timeout=0.2, write_timeout=0.5,
                           exclusive=True) as connection:
            connection.dtr = True
            connection.rts = True
            incoming.put(("connected", port))
            buffer = b""
            while not stop.is_set():
                try:
                    command = commands.get_nowait()
                except queue.Empty:
                    command = None
                if command:
                    connection.write((command + "\r\n").encode("ascii"))
                buffer += connection.read(min(connection.in_waiting or 1, 4096))
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    incoming.put(("line", line.decode("utf-8", errors="replace").rstrip("\r")))
                if len(buffer) > 8192:
                    buffer = b""
                    incoming.put(("line", "[PC] Ligne trop longue ignorée."))
    except Exception as error:
        incoming.put(("error", str(error)))
    finally:
        incoming.put(("closed", ""))


class Dashboard:
    def __init__(self, root, demo=False):
        self.root, self.demo = root, demo
        self.parser = Parser()
        self.incoming, self.commands = queue.Queue(), queue.Queue()
        self.stop = threading.Event()
        self.worker = None
        self.connected = False
        self.last_sample = None
        self.next_status = 0
        self.history = deque(maxlen=600)
        self.values, self.plots = {}, {}
        root.title("CESI • Station météo Groupe 1")
        root.geometry("1100x850")
        root.minsize(880, 720)
        root.configure(bg=BG)
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background=BG)
        style.configure("TLabel", background=BG, foreground=FG)
        style.configure("TButton", padding=7)
        style.configure("TNotebook", background=BG)
        style.configure("TNotebook.Tab", padding=(18, 8))
        top = ttk.Frame(root, padding=18)
        top.pack(fill="x")
        ttk.Label(top, text="STATION MÉTÉO", font=("Sans", 23, "bold")).pack(side="left")
        self.connection_text = tk.StringVar(value="Déconnecté")
        ttk.Label(top, textvariable=self.connection_text).pack(side="right")
        toolbar = ttk.Frame(root, padding=(18, 0, 18, 12))
        toolbar.pack(fill="x")
        ttk.Label(toolbar, text="Port USB :").pack(side="left")
        self.port = ttk.Combobox(toolbar, width=22)
        self.port.pack(side="left", padx=8)
        ttk.Button(toolbar, text="Actualiser", command=self.refresh_ports).pack(side="left")
        self.connect_button = ttk.Button(toolbar, text="Connecter", command=self.toggle)
        self.connect_button.pack(side="left", padx=8)
        ttk.Button(toolbar, text="Lire le statut", command=lambda: self.send("STATUS")).pack(side="left")
        ttk.Button(toolbar, text="Exporter CSV", command=self.export).pack(side="right")
        self.banner = tk.StringVar(value="Ferme miniterm, connecte la station, puis maintiens le bouton rouge 5 s pour la maintenance.")
        tk.Label(root, textvariable=self.banner, bg="#20354d", fg=FG, anchor="w",
                 padx=18, pady=12, wraplength=1000).pack(fill="x", padx=18)
        cards = ttk.Frame(root, padding=12)
        cards.pack(fill="x")
        for column, (key, title, unit, color) in enumerate(METRICS):
            cards.columnconfigure(column, weight=1)
            card = tk.Frame(cards, bg=PANEL, padx=14, pady=14)
            card.grid(row=0, column=column, sticky="ew", padx=6)
            tk.Label(card, text=title, bg=PANEL, fg=MUTED, font=("Sans", 11)).pack(anchor="w")
            value = tk.StringVar(value="—")
            self.values[key] = value
            tk.Label(card, textvariable=value, bg=PANEL, fg=color,
                     font=("Sans", 27, "bold")).pack(anchor="w")
            tk.Label(card, text=unit, bg=PANEL, fg=MUTED).pack(anchor="w")
        self.state = tk.StringVar(value="Mode : inconnu   |   RTC : inconnu   |   SD : inconnu   |   Erreurs : inconnu")
        ttk.Label(root, textvariable=self.state, padding=(18, 4)).pack(anchor="w")
        self.gps = tk.StringVar(value="GPS : aucune mesure reçue")
        ttk.Label(root, textvariable=self.gps, padding=(18, 4)).pack(anchor="w")
        self.freshness = tk.StringVar(value="Aucune acquisition reçue")
        ttk.Label(root, textvariable=self.freshness, padding=(18, 4)).pack(anchor="w")
        self.mode, self.rtc, self.sd, self.errors = "inconnu", "inconnu", "inconnu", "inconnu"
        notebook = ttk.Notebook(root)
        notebook.pack(fill="both", expand=True, padx=18, pady=12)
        charts = ttk.Frame(notebook)
        notebook.add(charts, text="Évolution des mesures")
        for i, (key, title, unit, color) in enumerate(METRICS):
            charts.rowconfigure(i // 2, weight=1)
            charts.columnconfigure(i % 2, weight=1)
            canvas = tk.Canvas(charts, bg=PANEL, highlightthickness=0, height=170)
            canvas.grid(row=i // 2, column=i % 2, sticky="nsew", padx=4, pady=4)
            canvas.bind("<Configure>", lambda event: self.draw())
            self.plots[key] = (canvas, title, unit, color)
        logs = ttk.Frame(notebook)
        notebook.add(logs, text="Console technique")
        self.log = tk.Text(logs, bg=PANEL, fg=FG, font=("Monospace", 10),
                           wrap="none", state="disabled")
        scrollbar = ttk.Scrollbar(logs, command=self.log.yview)
        scrollbar.pack(side="right", fill="y")
        self.log.configure(yscrollcommand=scrollbar.set)
        self.log.pack(fill="both", expand=True)
        ttk.Label(root, text="Historique : 600 acquisitions maximum • Courbes : temps de réception PC • NA : valeur indisponible",
                  padding=(18, 0, 18, 12)).pack(anchor="w")
        self.refresh_ports()
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(100, self.poll)
        if demo:
            self.connect_button.configure(state="disabled")
            self.connection_text.set("DÉMONSTRATION — données fictives")
            self.banner.set("DÉMONSTRATION : ces valeurs sont simulées, aucune station n’est connectée.")
            root.after(300, self.demo_sample)

    def refresh_ports(self):
        try:
            from serial.tools import list_ports
            ports = [p.device for p in list_ports.comports()]
        except ImportError:
            ports = []
        self.port["values"] = ports
        if not self.port.get():
            self.port.set(ports[0] if ports else "/dev/ttyACM0")

    def toggle(self):
        if self.worker:
            self.stop.set()
            self.connect_button.configure(text="Fermeture…", state="disabled")
            return
        self.parser = Parser()
        self.history.clear()
        self.last_sample = None
        for value in self.values.values():
            value.set("—")
        self.mode = self.rtc = self.sd = self.errors = "inconnu"
        self.gps.set("GPS : aucune mesure reçue")
        self.update_state()
        self.draw()
        self.commands = queue.Queue()
        self.stop = threading.Event()
        self.connect_button.configure(text="Connexion…", state="disabled")
        self.worker = threading.Thread(target=serial_worker,
            args=(self.port.get().strip(), self.incoming, self.commands, self.stop), daemon=True)
        self.worker.start()

    def send(self, command):
        if self.connected:
            self.commands.put(command)

    def update_state(self):
        self.state.set(f"Mode : {self.mode}   |   RTC : {self.rtc}   |   SD : {self.sd}   |   Erreurs : {self.errors}")

    def receive(self, line):
        self.log.configure(state="normal")
        self.log.insert("end", line + "\n")
        if int(self.log.index("end-1c").split(".")[0]) > 2200:
            self.log.delete("1.0", "201.0")
        self.log.see("end")
        self.log.configure(state="disabled")
        match = re.search(r"MODE=(\w+) ERR=(0x[0-9A-Fa-f]+).*RTC=(\S+)", line)
        if match:
            self.mode, self.errors, self.rtc = match.groups()
        elif line.startswith("MODE "):
            self.mode = line[5:].split(" ;")[0]
        if "CYCLE " in line and "SD=" in line:
            self.sd = "Dernière écriture OK" if "SD=OK" in line else "Erreur — voir console"
        if line.startswith("SD liberee"):
            self.sd = "Libérée (maintenance)"
        elif line == "SD remontee":
            self.sd = "Montée"
        elif "ERREUR remontage SD" in line:
            self.sd = "Erreur de montage"
        sample = self.parser.feed(line)
        if sample:
            self.accept(sample)
        self.update_state()

    def accept(self, sample):
        self.last_sample = time.monotonic()
        self.history.append((time.time(), dict(sample)))
        self.mode = sample["Mode"]
        for key, _, _, _ in METRICS:
            value = number(sample[key])
            self.values[key].set("—" if value is None else
                                 (str(round(value)) if key == "Luminosite" else f"{value:.2f}"))
        lat, lon = number(sample["Latitude"]), number(sample["Longitude"])
        if lat is None or lon is None:
            self.gps.set("GPS : position indisponible pour cette acquisition")
        else:
            self.gps.set(f"GPS : latitude {lat:.6f}° • longitude {lon:.6f}°")
        self.draw()

    def draw(self):
        for key, (canvas, title, unit, color) in self.plots.items():
            canvas.delete("all")
            w, h = canvas.winfo_width(), canvas.winfo_height()
            if w < 120 or h < 90:
                continue
            canvas.create_text(14, 15, text=f"{title} ({unit})", fill=FG, anchor="w", font=("Sans", 11, "bold"))
            valid = [number(s[key]) for _, s in self.history if number(s[key]) is not None]
            if not valid:
                canvas.create_text(w/2, h/2, text="En attente de mesures", fill=MUTED)
                continue
            low, high = min(valid), max(valid)
            margin = max((high-low)*0.12, 0.1 if key != "Luminosite" else 1)
            low, high = low-margin, high+margin
            left, right, top, bottom = 65, w-18, 38, h-30
            for i in range(3):
                y = top + (bottom-top)*i/2
                canvas.create_line(left, y, right, y, fill="#2b3b52")
                canvas.create_text(left-7, y, text=f"{high-(high-low)*i/2:.1f}", fill=MUTED, anchor="e")
            start, end = self.history[0][0], self.history[-1][0]
            previous = None
            for timestamp, sample in self.history:
                value = number(sample[key])
                if value is None:
                    previous = None
                    continue
                x = left + (right-left)*(timestamp-start)/max(end-start, 1)
                y = bottom - (bottom-top)*(value-low)/(high-low)
                if previous:
                    canvas.create_line(*previous, x, y, fill=color, width=2)
                canvas.create_oval(x-2, y-2, x+2, y+2, fill=color, outline="")
                previous = (x, y)
            for x, timestamp, anchor in [(left, start, "w"), (right, end, "e")]:
                canvas.create_text(x, h-12, text=datetime.fromtimestamp(timestamp).strftime("%H:%M:%S"),
                                   fill=MUTED, anchor=anchor)

    def poll(self):
        for _ in range(300):
            try:
                kind, payload = self.incoming.get_nowait()
            except queue.Empty:
                break
            if kind == "connected":
                self.connected = True
                self.connection_text.set("Connecté • " + payload)
                self.connect_button.configure(text="Déconnecter", state="normal")
                self.next_status = 0
                self.banner.set("Connexion établie. Pour les mesures en direct : bouton rouge 5 s depuis le mode standard.")
            elif kind == "line":
                self.receive(payload)
            elif kind == "error":
                self.banner.set("Connexion impossible / interrompue : " + payload)
            elif kind == "closed":
                self.connected = False
                self.worker = None
                self.connection_text.set("Déconnecté • dernières valeurs conservées à l’écran")
                self.connect_button.configure(text="Connecter", state="normal")
        now = time.monotonic()
        if self.connected and now >= self.next_status:
            self.send("STATUS")
            self.next_status = now + 5
        if self.last_sample is not None:
            age = int(now-self.last_sample)
            stamp = self.history[-1][1]["Date / heure"]
            self.freshness.set(f"Horodatage station : {stamp} • Acquisition reçue il y a {age} s" +
                               (" • Valeurs anciennes" if age > 30 else ""))
        else:
            self.freshness.set("Aucune acquisition reçue • En standard/économique, le firmware affiche surtout les états SD.")
        self.root.after(100, self.poll)

    def export(self):
        if not self.history:
            messagebox.showinfo("Export", "Aucune acquisition à exporter.")
            return
        path = filedialog.asksaveasfilename(defaultextension=".csv",
            initialfile="mesures_demo.csv" if self.demo else "mesures_station.csv",
            filetypes=[("CSV", "*.csv")])
        if path:
            try:
                with open(path, "w", newline="", encoding="utf-8-sig") as file:
                    writer = csv.writer(file, delimiter=";")
                    writer.writerow(["Source", "Reception_PC"] + KEYS)
                    for timestamp, sample in self.history:
                        writer.writerow(["SIMULATION" if self.demo else "SERIE",
                            datetime.fromtimestamp(timestamp).isoformat()] + [sample[k] for k in KEYS])
            except OSError as error:
                messagebox.showerror("Export impossible", str(error))

    def demo_sample(self):
        sample = dict(zip(KEYS, [datetime.now().isoformat(timespec="seconds"), "MAINTENANCE",
            f"{23+random.uniform(-0.3,0.3):.2f}", f"{45+random.uniform(-2,2):.2f}",
            f"{968+random.uniform(-0.2,0.2):.2f}", str(random.randint(150,450)), "NA", "NA"]))
        self.accept(sample)
        self.rtc, self.sd, self.errors = "Simulée", "Simulée", "Simulation"
        self.update_state()
        self.root.after(2000, self.demo_sample)

    def close(self):
        self.stop.set()
        self.root.destroy()


def self_test():
    parser = Parser()
    assert number("NA") is None and number("nan") is None
    assert number("25.28 deg C") == 25.28
    row = "2026-10-20T16:16:18;MAINTENANCE;25.28;42.60;968.40;483;NA;NA"
    expected = parser.feed(row)
    assert expected["Temperature"] == "25.28"
    result = None
    for key, value in expected.items():
        result = parser.feed(f"{key:13} : {value}")
        if key == "Temperature":
            assert parser.feed("[GPS diag] octets=123 phrases=4") is None
    assert result == expected
    parser.feed("Date / heure : 2026-10-20T16:16:18")
    parser.feed("Temperature : 12.0")
    assert parser.feed("Longitude : NA") is None
    assert parser.feed(row) == expected
    print("OK : formats CSV et lisible, unités, NA, blocs incomplets et messages intercalés.")


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        self_test()
    else:
        Dashboard(tk.Tk(), demo="--demo" in sys.argv)
        tk.mainloop()
