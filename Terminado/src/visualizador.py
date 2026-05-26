import ctypes
import ctypes.wintypes
import os
import sys
import tkinter as tk
from tkinter import font as tkfont
import time
import threading

MAX_CELDAS = 20
MAX_PLACA = 10

class CeldaParqueadero(ctypes.Structure):
    _fields_ = [
        ("placa", ctypes.c_char * MAX_PLACA),
        ("hora_entrada", ctypes.c_char * 20),
        ("celda", ctypes.c_int),
        ("ocupada", ctypes.c_int),
    ]

class EventoPlaca(ctypes.Structure):
    _fields_ = [
        ("placa", ctypes.c_char * MAX_PLACA),
        ("hora", ctypes.c_char * 20),
        ("celda", ctypes.c_int),
        ("tipo", ctypes.c_int),
    ]

class VisualizadorParqueadero:
    def __init__(self, root):
        self.root = root
        self.root.title("Visualizador de Parqueadero")
        self.root.configure(bg="#1a1a2e")
        self.root.geometry("900x700")
        self.root.resizable(True, True)

        # Detectar y cargar SWIG o ctypes
        self.usar_swig = False
        script_dir = os.path.dirname(os.path.abspath(__file__))
        if script_dir not in sys.path:
            sys.path.append(script_dir)

        try:
            import parqueadero_lib_swig as pqlib
            self.pqlib = pqlib
            self.usar_swig = True
            print("[INFO] Libreria de parqueadero cargada mediante SWIG exitosamente.")
        except ImportError as e:
            print(f"[INFO] SWIG no disponible ({e}). Cargando DLL mediante ctypes...")
            self.dll = self.cargar_dll()
            self.configurar_funciones()

        self.celdas_frames = []
        self.celdas_labels = []
        self.log_entries = []
        self.corriendo = False

        self.crear_interfaz()

    def cargar_dll(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        dll_path = os.path.join(script_dir, "parqueadero_lib.dll")
        if not os.path.exists(dll_path):
            dll_path = os.path.join(script_dir, "..", "build", "parqueadero_lib.dll")
        if not os.path.exists(dll_path):
            print(f"Error: No se encontro parqueadero_lib.dll")
            sys.exit(1)
        return ctypes.CDLL(dll_path)

    def configurar_funciones(self):
        self.dll.iniciar_servidor.argtypes = [ctypes.c_int]
        self.dll.iniciar_servidor.restype = ctypes.c_int

        self.dll.detener_servidor.argtypes = []
        self.dll.detener_servidor.restype = ctypes.c_int

        self.dll.obtener_evento.argtypes = [ctypes.POINTER(EventoPlaca)]
        self.dll.obtener_evento.restype = ctypes.c_int

        self.dll.obtener_estado_celdas.argtypes = [ctypes.POINTER(CeldaParqueadero * MAX_CELDAS), ctypes.c_int]
        self.dll.obtener_estado_celdas.restype = ctypes.c_int

        self.dll.obtener_total_celdas.argtypes = []
        self.dll.obtener_total_celdas.restype = ctypes.c_int

        self.dll.obtener_celdas_ocupadas.argtypes = []
        self.dll.obtener_celdas_ocupadas.restype = ctypes.c_int

    def safe_decode(self, val):
        if isinstance(val, bytes):
            return val.decode("utf-8", errors="replace").strip('\x00')
        elif isinstance(val, str):
            return val.strip('\x00')
        return ""

    def api_iniciar_servidor(self, puerto):
        if self.usar_swig:
            return self.pqlib.iniciar_servidor(puerto)
        else:
            return self.dll.iniciar_servidor(puerto)

    def api_detener_servidor(self):
        if self.usar_swig:
            return self.pqlib.detener_servidor()
        else:
            return self.dll.detener_servidor()

    def api_obtener_evento(self):
        if self.usar_swig:
            ev = self.pqlib.EventoPlaca()
            hay_ev = self.pqlib.obtener_evento(ev)
            if hay_ev:
                return True, self.safe_decode(ev.placa), self.safe_decode(ev.hora), ev.celda, ev.tipo
            return False, "", "", 0, 0
        else:
            ev = EventoPlaca()
            hay_ev = self.dll.obtener_evento(ctypes.byref(ev))
            if hay_ev:
                return True, self.safe_decode(ev.placa), self.safe_decode(ev.hora), ev.celda, ev.tipo
            return False, "", "", 0, 0

    def api_obtener_estado_celda(self, idx):
        if self.usar_swig:
            celda = self.pqlib.obtener_celda(idx)
            return celda.ocupada, self.safe_decode(celda.placa), self.safe_decode(celda.hora_entrada)
        else:
            if not hasattr(self, '_ctypes_celdas_buffer'):
                self._ctypes_celdas_buffer = (CeldaParqueadero * MAX_CELDAS)()
            self.dll.obtener_estado_celdas(ctypes.byref(self._ctypes_celdas_buffer), MAX_CELDAS)
            celda = self._ctypes_celdas_buffer[idx]
            return celda.ocupada, self.safe_decode(celda.placa), self.safe_decode(celda.hora_entrada)

    def api_obtener_celdas_ocupadas(self):
        if self.usar_swig:
            return self.pqlib.obtener_celdas_ocupadas()
        else:
            return self.dll.obtener_celdas_ocupadas()

    def crear_interfaz(self):
        title_font = tkfont.Font(family="Segoe UI", size=18, weight="bold")
        header_font = tkfont.Font(family="Segoe UI", size=12, weight="bold")
        cell_font = tkfont.Font(family="Consolas", size=10)
        log_font = tkfont.Font(family="Consolas", size=9)

        header = tk.Frame(self.root, bg="#16213e", pady=10)
        header.pack(fill=tk.X)

        modo_label = " [SWIG]" if self.usar_swig else " [ctypes]"
        tk.Label(header, text="SISTEMA DE PARQUEADERO" + modo_label, font=title_font,
                 fg="#e94560", bg="#16213e").pack()

        self.status_label = tk.Label(header, text="Estado: Detenido", font=header_font,
                                      fg="#aaaaaa", bg="#16213e")
        self.status_label.pack()

        self.counter_label = tk.Label(header, text="Ocupadas: 0 / 20 | Libres: 20",
                                       font=header_font, fg="#0f3460", bg="#16213e")
        self.counter_label.pack()

        btn_frame = tk.Frame(header, bg="#16213e")
        btn_frame.pack(pady=5)

        self.btn_iniciar = tk.Button(btn_frame, text="INICIAR SERVIDOR", command=self.iniciar,
                                      bg="#0f3460", fg="white", font=header_font,
                                      activebackground="#1a1a5e", padx=20, pady=5, relief=tk.FLAT)
        self.btn_iniciar.pack(side=tk.LEFT, padx=5)

        self.btn_detener = tk.Button(btn_frame, text="DETENER SERVIDOR", command=self.detener,
                                      bg="#e94560", fg="white", font=header_font,
                                      activebackground="#c23050", padx=20, pady=5,
                                      state=tk.DISABLED, relief=tk.FLAT)
        self.btn_detener.pack(side=tk.LEFT, padx=5)

        main_frame = tk.Frame(self.root, bg="#1a1a2e")
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        parking_frame = tk.LabelFrame(main_frame, text=" Celdas de Parqueadero ",
                                       font=header_font, fg="#e94560", bg="#1a1a2e",
                                       labelanchor="n")
        parking_frame.pack(fill=tk.BOTH, expand=True, side=tk.LEFT, padx=(0, 5))

        grid_frame = tk.Frame(parking_frame, bg="#1a1a2e")
        grid_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        for i in range(MAX_CELDAS):
            row = i // 5
            col = i % 5

            cell_frame = tk.Frame(grid_frame, bg="#2a2a4a", relief=tk.RAISED,
                                   borderwidth=2, padx=5, pady=5)
            cell_frame.grid(row=row, column=col, padx=4, pady=4, sticky="nsew")

            lbl_num = tk.Label(cell_frame, text=f"C-{i+1:02d}", font=cell_font,
                                fg="#aaaaaa", bg="#2a2a4a")
            lbl_num.pack()

            lbl_placa = tk.Label(cell_frame, text="LIBRE", font=cell_font,
                                  fg="#00cc66", bg="#2a2a4a", width=10)
            lbl_placa.pack()

            lbl_hora = tk.Label(cell_frame, text="", font=tkfont.Font(family="Consolas", size=8),
                                 fg="#888888", bg="#2a2a4a")
            lbl_hora.pack()

            self.celdas_frames.append(cell_frame)
            self.celdas_labels.append((lbl_num, lbl_placa, lbl_hora))

        for c in range(5):
            grid_frame.columnconfigure(c, weight=1)
        for r in range(4):
            grid_frame.rowconfigure(r, weight=1)

        log_frame = tk.LabelFrame(main_frame, text=" Registro de Eventos ",
                                    font=header_font, fg="#e94560", bg="#1a1a2e",
                                    labelanchor="n", width=280)
        log_frame.pack(fill=tk.BOTH, expand=False, side=tk.RIGHT)
        log_frame.pack_propagate(False)

        self.log_text = tk.Text(log_frame, bg="#0a0a1a", fg="#00cc66", font=log_font,
                                 state=tk.DISABLED, wrap=tk.WORD, width=35)
        scrollbar = tk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.log_text.tag_configure("entrada", foreground="#00cc66")
        self.log_text.tag_configure("salida", foreground="#e94560")
        self.log_text.tag_configure("info", foreground="#aaaaaa")
        self.log_text.tag_configure("error", foreground="#ff6666")

    def agregar_log(self, texto, tag="info"):
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, texto + "\n", tag)
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def iniciar(self):
        resultado = self.api_iniciar_servidor(5000)
        if resultado == 0:
            self.corriendo = True
            modo = "SWIG" if self.usar_swig else "ctypes"
            self.status_label.config(text=f"Estado: Servidor activo (puerto 5000) [{modo}]", fg="#00cc66")
            self.btn_iniciar.config(state=tk.DISABLED)
            self.btn_detener.config(state=tk.NORMAL)
            self.agregar_log(f"Servidor iniciado en puerto 5000 ({modo})", "info")
            self.agregar_log("Esperando conexion del generador...", "info")
            self.actualizar()
        else:
            self.agregar_log(f"Error al iniciar servidor: codigo {resultado}", "error")

    def detener(self):
        self.corriendo = False
        self.api_detener_servidor()
        self.status_label.config(text="Estado: Detenido", fg="#aaaaaa")
        self.btn_iniciar.config(state=tk.NORMAL)
        self.btn_detener.config(state=tk.DISABLED)
        self.agregar_log("Servidor detenido", "info")

    def actualizar(self):
        if not self.corriendo:
            return

        while True:
            hay_ev, placa, hora, celda, tipo = self.api_obtener_evento()
            if not hay_ev:
                break

            if tipo == 0:
                if celda >= 0:
                    self.agregar_log(f"[{hora}] ENTRADA: {placa} -> Celda C-{celda+1:02d}", "entrada")
                else:
                    self.agregar_log(f"[{hora}] LLENO: {placa} sin celda disponible", "error")
            else:
                self.agregar_log(f"[{hora}] SALIDA:  {placa} <- Celda C-{celda+1:02d}", "salida")

        ocupadas = self.api_obtener_celdas_ocupadas()
        libres = MAX_CELDAS - ocupadas

        self.counter_label.config(
            text=f"Ocupadas: {ocupadas} / {MAX_CELDAS} | Libres: {libres}",
            fg="#e94560" if ocupadas > 15 else "#00cc66"
        )

        for i in range(MAX_CELDAS):
            lbl_num, lbl_placa, lbl_hora = self.celdas_labels[i]
            frame = self.celdas_frames[i]

            ocupada, placa, hora = self.api_obtener_estado_celda(i)
            if ocupada:
                frame.config(bg="#3d0000")
                lbl_num.config(bg="#3d0000", fg="#ff6666")
                lbl_placa.config(text=placa, bg="#3d0000", fg="#ffffff")
                lbl_hora.config(text=hora, bg="#3d0000", fg="#ff9999")
            else:
                frame.config(bg="#002a00")
                lbl_num.config(bg="#002a00", fg="#66cc66")
                lbl_placa.config(text="LIBRE", bg="#002a00", fg="#00cc66")
                lbl_hora.config(text="", bg="#002a00")

        self.root.after(500, self.actualizar)

    def on_close(self):
        if self.corriendo:
            self.detener()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = VisualizadorParqueadero(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    main()
