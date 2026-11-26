import json
import os
import subprocess
import sys
import tkinter as tk
from tkinter import ttk, messagebox

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATOS_TXT = os.path.join(BASE_DIR, "datos.txt")
SALIDA_JSON = os.path.join(BASE_DIR, "horario_resultado.json")
EXE_NAME = "motor_horarios.exe" if os.name == "nt" else "motor_horarios"
EXE_PATH = os.path.join(BASE_DIR, EXE_NAME)

PALETA_GRUPOS = [
    "#dbeafe", "#dcfce7", "#fef3c7", "#f3e8ff", "#ffe4e6",
    "#cffafe", "#ffedd5", "#e0e7ff", "#fae8ff", "#ecfccb"
]


def compilar_si_es_necesario():
    if not os.path.exists(EXE_PATH):
        cmd = ["gcc", "-O2", "-o", EXE_PATH,
               os.path.join(BASE_DIR, "datos.c"),
               os.path.join(BASE_DIR, "horarios.c"),
               os.path.join(BASE_DIR, "api_bridge.c")]
        try:
            subprocess.check_call(cmd)
        except Exception as e:
            messagebox.showerror(
                "Error de compilacion",
                f"No se encontro {EXE_NAME} y fallo la compilacion automatica con gcc.\n"
                f"Compila manualmente en consola:\n{' '.join(cmd)}\n\nDetalle: {e}"
            )
            sys.exit(1)


class OptimizadorHorariosApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Optimizador de Horarios Academicos")
        self.root.geometry("1280x800")
        self.root.configure(bg="#f4f6f9")

        compilar_si_es_necesario()
        self.data = {}
        self._configurar_estilos()
        self._construir_ui()
        self.recalcular_y_cargar()

    def _configurar_estilos(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TNotebook", background="#f4f6f9", borderwidth=0)
        style.configure("TNotebook.Tab", font=("Segoe UI", 10, "bold"), padding=[14, 8])
        style.configure("Treeview", font=("Segoe UI", 10), rowheight=28)
        style.configure("Treeview.Heading", font=("Segoe UI", 10, "bold"), background="#1e293b", foreground="white")
        style.map("Treeview.Heading", background=[("active", "#334155")])

    def _construir_ui(self):
        header = tk.Frame(self.root, bg="#0f172a", height=64)
        header.pack(fill="x")
        header.pack_propagate(False)

        tk.Label(
            header, text="OPTIMIZADOR DE HORARIOS ACADEMICOS",
            font=("Segoe UI", 15, "bold"), bg="#0f172a", fg="white"
        ).pack(side="left", padx=20)

        

        btn_recalc = tk.Button(
            header, text="Recalcular Horario (Ejecutar C)",
            font=("Segoe UI", 10, "bold"), bg="#2563eb", fg="white",
            activebackground="#1d4ed8", activeforeground="white",
            relief="flat", padx=14, pady=6, cursor="hand2",
            command=self.recalcular_y_cargar
        )
        btn_recalc.pack(side="right", padx=20)

        # KPIs
        self.kpi_frame = tk.Frame(self.root, bg="#f4f6f9")
        self.kpi_frame.pack(fill="x", padx=20, pady=(14, 6))
        self.kpis = {}
        definiciones = [
            ("total_clases", "Clases Agendadas", "#2563eb"),
            ("bloques_usados", "Bloques Horarios Usados", "#059669"),
            ("ocupacion_promedio", "Ocupacion Promedio Aulas", "#7c3aed"),
            ("sin_aula", "Clases Sin Aula", "#dc2626"),
        ]
        for key, titulo, color in definiciones:
            card = tk.Frame(self.kpi_frame, bg="white", bd=1, relief="solid", highlightbackground="#e2e8f0", highlightthickness=1)
            card.pack(side="left", expand=True, fill="x", padx=6)
            tk.Frame(card, bg=color, height=4).pack(fill="x")
            tk.Label(card, text=titulo, font=("Segoe UI", 9), bg="white", fg="#64748b").pack(anchor="w", padx=12, pady=(8, 0))
            lbl = tk.Label(card, text="--", font=("Segoe UI", 16, "bold"), bg="white", fg="#0f172a")
            lbl.pack(anchor="w", padx=12, pady=(2, 10))
            self.kpis[key] = lbl

        # Pestanas
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(expand=True, fill="both", padx=20, pady=(6, 18))

        self.tab_horario = tk.Frame(self.notebook, bg="white")
        self.tab_lista = tk.Frame(self.notebook, bg="white")
        self.tab_datos = tk.Frame(self.notebook, bg="white")
        self.tab_editar = tk.Frame(self.notebook, bg="white")

        self.notebook.add(self.tab_horario, text="  1. Cuadricula Semanal (Horario)  ")
        self.notebook.add(self.tab_lista, text="  2. Detalle por Clase y Aula  ")
        self.notebook.add(self.tab_datos, text="  3. Materias, Profesores y Aulas  ")
        self.notebook.add(self.tab_editar, text="  4. Editar Disponibilidad / Alumnos  ")

        self._construir_tab_horario()
        self._construir_tab_lista()
        self._construir_tab_datos()
        self._construir_tab_editar()

    # CUADRICULA SEMANAL
    def _construir_tab_horario(self):
        barra = tk.Frame(self.tab_horario, bg="white")
        barra.pack(fill="x", padx=16, pady=10)

        tk.Label(barra, text="Filtrar vista por:", font=("Segoe UI", 10, "bold"), bg="white").pack(side="left")
        self.filtro_tipo = ttk.Combobox(barra, values=["Todos", "Grupo", "Profesor", "Aula"], state="readonly", width=12)
        self.filtro_tipo.set("Todos")
        self.filtro_tipo.pack(side="left", padx=8)
        self.filtro_tipo.bind("<<ComboboxSelected>>", self._actualizar_opciones_filtro)

        self.filtro_valor = ttk.Combobox(barra, state="disabled", width=18)
        self.filtro_valor.pack(side="left", padx=4)
        self.filtro_valor.bind("<<ComboboxSelected>>", lambda e: self._renderizar_cuadricula())

        tk.Label(
            barra,
            text="Guia: Usa la rueda del raton sobre una celda con varias clases para desplazarte  |  [Grupo] Materia | Prof. | Aula",
            font=("Segoe UI", 9, "italic"), bg="white", fg="#475569"
        ).pack(side="right")

        self.grid_container = tk.Frame(self.tab_horario, bg="#cbd5e1")
        self.grid_container.pack(expand=True, fill="both", padx=16, pady=(0, 14))

    def _actualizar_opciones_filtro(self, event=None):
        tipo = self.filtro_tipo.get()
        if tipo == "Todos":
            self.filtro_valor.set("")
            self.filtro_valor.configure(state="disabled", values=[])
        elif tipo == "Grupo":
            vals = [g["id"] for g in self.data.get("grupos", [])]
            self.filtro_valor.configure(state="readonly", values=vals)
            if vals: self.filtro_valor.set(vals[0])
        elif tipo == "Profesor":
            vals = [p["nombre"] for p in self.data.get("profesores", [])]
            self.filtro_valor.configure(state="readonly", values=vals)
            if vals: self.filtro_valor.set(vals[0])
        elif tipo == "Aula":
            vals = [a["nombre"] for a in self.data.get("aulas", [])]
            self.filtro_valor.configure(state="readonly", values=vals)
            if vals: self.filtro_valor.set(vals[0])
        self._renderizar_cuadricula()

    def _crear_celda_scrollable(self, parent, row, col):
        contenedor = tk.Frame(parent, bg="#f8fafc")
        contenedor.grid(row=row, column=col, sticky="nsew", padx=1, pady=1)

        canvas = tk.Canvas(contenedor, bg="#f8fafc", highlightthickness=0, bd=0)
        scrollbar = ttk.Scrollbar(contenedor, orient="vertical", command=canvas.yview)
        inner = tk.Frame(canvas, bg="#f8fafc")

        win_id = canvas.create_window((0, 0), window=inner, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        def _on_inner_configure(event):
            canvas.configure(scrollregion=canvas.bbox("all"))
            # Mostrar scrollbar solo si el contenido supera la altura visible de la celda
            if inner.winfo_reqheight() > canvas.winfo_height() and canvas.winfo_height() > 1:
                scrollbar.pack(side="right", fill="y")
            else:
                scrollbar.pack_forget()

        def _on_canvas_configure(event):
            canvas.itemconfig(win_id, width=event.width)
            if inner.winfo_reqheight() > event.height and event.height > 1:
                scrollbar.pack(side="right", fill="y")
            else:
                scrollbar.pack_forget()

        def _on_mousewheel(event):
            if inner.winfo_reqheight() > canvas.winfo_height():
                canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        inner.bind("<Configure>", _on_inner_configure)
        canvas.bind("<Configure>", _on_canvas_configure)

        canvas.pack(side="left", expand=True, fill="both")

        # Vincular rueda del raton al entrar/salir de la celda
        def _bind_wheel(e):
            canvas.bind_all("<MouseWheel>", _on_mousewheel)

        def _unbind_wheel(e):
            canvas.unbind_all("<MouseWheel>")

        contenedor.bind("<Enter>", _bind_wheel)
        contenedor.bind("<Leave>", _unbind_wheel)

        return inner

    def _renderizar_cuadricula(self):
        for w in self.grid_container.winfo_children():
            w.destroy()

        jornada = self.data.get("jornada", {})
        dias = jornada.get("dias", ["Lun", "Mar", "Mie", "Jue", "Vie"])
        horas = jornada.get("horas", ["7-9", "9-11"])
        num_horas = len(horas)

        grupos_lista = [g["id"] for g in self.data.get("grupos", [])]
        colores_grupo = {gid: PALETA_GRUPOS[i % len(PALETA_GRUPOS)] for i, gid in enumerate(grupos_lista)}

        tk.Label(self.grid_container, text="HORARIO", font=("Segoe UI", 10, "bold"),
                 bg="#1e293b", fg="white", pady=7).grid(row=0, column=0, sticky="nsew", padx=1, pady=1)
        for d_idx, dia in enumerate(dias):
            tk.Label(self.grid_container, text=dia.upper(), font=("Segoe UI", 10, "bold"),
                     bg="#1e293b", fg="white", pady=7).grid(row=0, column=d_idx + 1, sticky="nsew", padx=1, pady=1)

        tipo_filtro = self.filtro_tipo.get()
        val_filtro = self.filtro_valor.get()

        for h_idx, hora in enumerate(horas):
            tk.Label(self.grid_container, text=hora, font=("Segoe UI", 10, "bold"),
                     bg="#334155", fg="white", width=11).grid(row=h_idx + 1, column=0, sticky="nsew", padx=1, pady=1)

            for d_idx in range(len(dias)):
                bloque = d_idx * num_horas + h_idx
                celda = self._crear_celda_scrollable(self.grid_container, h_idx + 1, d_idx + 1)

                clases_celda = [c for c in self.data.get("clases", []) if c["bloque"] == bloque]
                if tipo_filtro == "Grupo":
                    clases_celda = [c for c in clases_celda if c["grupo"] == val_filtro]
                elif tipo_filtro == "Profesor":
                    clases_celda = [c for c in clases_celda if c["profesor"] == val_filtro]
                elif tipo_filtro == "Aula":
                    clases_celda = [c for c in clases_celda if c["aula"] == val_filtro]

                if not clases_celda:
                    tk.Label(celda, text="Libre", font=("Segoe UI", 9), bg="#f8fafc", fg="#cbd5e1").pack(pady=18)
                else:
                    # Encabezado discreto si hay multiples clases simultaneas
                    if len(clases_celda) > 1:
                        tk.Label(
                            celda, text=f"{len(clases_celda)} clases simultaneas:",
                            font=("Segoe UI", 7, "bold"), bg="#e2e8f0", fg="#475569", anchor="w", padx=4
                        ).pack(fill="x", padx=4, pady=(3, 1))

                    for c in clases_celda:
                        bg_card = "#fecaca" if c["aula"] == "SIN AULA" else colores_grupo.get(c["grupo"], "#e0f2fe")
                        card = tk.Frame(celda, bg=bg_card, bd=1, relief="solid", highlightbackground="#94a3b8")
                        card.pack(fill="x", padx=4, pady=2)

                        t1 = f"[{c['grupo']}] {c['materia_id']} - {c['materia_nombre']}"
                        t2 = f"{c['profesor']} | {c['aula']} ({c['alumnos']}/{c['capacidad']})"
                        tk.Label(card, text=t1, font=("Segoe UI", 8, "bold"), bg=bg_card, fg="#0f172a", anchor="w").pack(fill="x", padx=4, pady=(1, 0))
                        tk.Label(card, text=t2, font=("Segoe UI", 8), bg=bg_card, fg="#334155", anchor="w").pack(fill="x", padx=4, pady=(0, 1))

        for col in range(len(dias) + 1):
            self.grid_container.grid_columnconfigure(col, weight=1 if col > 0 else 0, uniform="cols" if col > 0 else "")
        for row in range(1, num_horas + 1):
            self.grid_container.grid_rowconfigure(row, weight=1, uniform="rows")

    # ---------------- TAB 2: LISTA DETALLADA ----------------
    def _construir_tab_lista(self):
        cols = ("dia", "hora", "grupo", "materia", "prioridad", "profesor", "aula", "ocupacion")
        self.tree_clases = ttk.Treeview(self.tab_lista, columns=cols, show="headings")
        encabezados = {
            "dia": "Dia", "hora": "Bloque", "grupo": "Grupo", "materia": "Materia",
            "prioridad": "Prioridad", "profesor": "Profesor", "aula": "Aula", "ocupacion": "Alumnos / Cap."
        }
        anchos = {"dia": 90, "hora": 110, "grupo": 70, "materia": 210, "prioridad": 80, "profesor": 130, "aula": 100, "ocupacion": 120}
        for c in cols:
            self.tree_clases.heading(c, text=encabezados[c])
            self.tree_clases.column(c, width=anchos[c], anchor="center" if c != "materia" else "w")
        self.tree_clases.pack(expand=True, fill="both", padx=16, pady=16)

    # ---------------- TAB 3: DATOS ENTRADA Y PASO 1 ----------------
    def _construir_tab_datos(self):
        paned = ttk.PanedWindow(self.tab_datos, orient="horizontal")
        paned.pack(expand=True, fill="both", padx=16, pady=16)

        izq = tk.Frame(paned, bg="white")
        der = tk.Frame(paned, bg="white")
        paned.add(izq, weight=1)
        paned.add(der, weight=1)

        tk.Label(izq, text="Materias y Profesor Asignado (Paso 1)", font=("Segoe UI", 11, "bold"), bg="white").pack(anchor="w", pady=(0, 6))
        self.tree_materias = ttk.Treeview(izq, columns=("id", "nombre", "prio", "prof"), show="headings", height=11)
        for col, txt, w in [("id", "ID", 60), ("nombre", "Materia", 170), ("prio", "Prioridad", 80), ("prof", "Profesor Asignado", 140)]:
            self.tree_materias.heading(col, text=txt)
            self.tree_materias.column(col, width=w, anchor="center" if col != "nombre" else "w")
        self.tree_materias.pack(fill="x", pady=(0, 14))

        tk.Label(izq, text="Aulas Disponibles y Reglas de Incompatibilidad", font=("Segoe UI", 11, "bold"), bg="white").pack(anchor="w", pady=(0, 6))
        self.lbl_aulas_incomp = tk.Label(izq, text="", font=("Segoe UI", 10), bg="#f8fafc", fg="#1e293b", justify="left", anchor="nw", padx=10, pady=10, relief="solid", bd=1)
        self.lbl_aulas_incomp.pack(expand=True, fill="both")

        self.lbl_titulo_profes = tk.Label(der, text="Profesores y Disponibilidad", font=("Segoe UI", 11, "bold"), bg="white")
        self.lbl_titulo_profes.pack(anchor="w", pady=(0, 6))
        self.tree_profes = ttk.Treeview(der, columns=("nombre", "disp", "mats"), show="headings", height=9)
        for col, txt, w in [("nombre", "Profesor", 110), ("disp", "Disponibilidad por Bloque", 190), ("mats", "Materias que imparte", 170)]:
            self.tree_profes.heading(col, text=txt)
            self.tree_profes.column(col, width=w, anchor="center" if col == "disp" else "w")
        self.tree_profes.pack(fill="x", pady=(0, 14))

        tk.Label(der, text="Grupos Academicos", font=("Segoe UI", 11, "bold"), bg="white").pack(anchor="w", pady=(0, 6))
        self.tree_grupos = ttk.Treeview(der, columns=("id", "alumnos", "mats"), show="headings", height=6)
        for col, txt, w in [("id", "Grupo", 70), ("alumnos", "Alumnos", 80), ("mats", "Materias Inscritas", 300)]:
            self.tree_grupos.heading(col, text=txt)
            self.tree_grupos.column(col, width=w, anchor="center" if col != "mats" else "w")
        self.tree_grupos.pack(expand=True, fill="both")

    # ---------------- TAB 4: EDITOR DINAMICO ----------------
    def _construir_tab_editar(self):
        cont = tk.Frame(self.tab_editar, bg="white")
        cont.pack(expand=True, fill="both", padx=24, pady=20)

        f_prof = tk.LabelFrame(cont, text="  Editar Disponibilidad de Profesor  ", font=("Segoe UI", 11, "bold"), bg="white", padx=16, pady=16)
        f_prof.pack(side="left", expand=True, fill="both", padx=(0, 12))

        tk.Label(f_prof, text="Selecciona Profesor:", font=("Segoe UI", 10), bg="white").pack(anchor="w")
        self.cb_edit_prof = ttk.Combobox(f_prof, state="readonly", width=25)
        self.cb_edit_prof.pack(anchor="w", pady=(4, 14))
        self.cb_edit_prof.bind("<<ComboboxSelected>>", self._cargar_checks_profesor)

        self.vars_bloques = []
        self.grid_chk = tk.Frame(f_prof, bg="white")
        self.grid_chk.pack(anchor="w", pady=6)

        tk.Button(
            f_prof, text="Guardar Disponibilidad y Recalcular",
            font=("Segoe UI", 10, "bold"), bg="#059669", fg="white", relief="flat", padx=12, pady=6,
            command=self._guardar_disponibilidad_ui
        ).pack(anchor="w", pady=(16, 0))

        f_grp = tk.LabelFrame(cont, text="  Editar Cantidad de Alumnos por Grupo  ", font=("Segoe UI", 11, "bold"), bg="white", padx=16, pady=16)
        f_grp.pack(side="left", expand=True, fill="both", padx=(12, 0))

        tk.Label(f_grp, text="Selecciona Grupo:", font=("Segoe UI", 10), bg="white").pack(anchor="w")
        self.cb_edit_grupo = ttk.Combobox(f_grp, state="readonly", width=25)
        self.cb_edit_grupo.pack(anchor="w", pady=(4, 14))
        self.cb_edit_grupo.bind("<<ComboboxSelected>>", self._cargar_alumnos_grupo)

        tk.Label(f_grp, text="Nueva Cantidad de Alumnos:", font=("Segoe UI", 10), bg="white").pack(anchor="w")
        self.entry_alumnos = tk.Entry(f_grp, font=("Segoe UI", 11), width=15)
        self.entry_alumnos.pack(anchor="w", pady=(4, 16))

        tk.Button(
            f_grp, text="Actualizar Grupo y Recalcular",
            font=("Segoe UI", 10, "bold"), bg="#2563eb", fg="white", relief="flat", padx=12, pady=6,
            command=self._guardar_alumnos_ui
        ).pack(anchor="w")

    def _reconstruir_checkboxes_jornada(self):
        for w in self.grid_chk.winfo_children():
            w.destroy()
        self.vars_bloques = []

        jornada = self.data.get("jornada", {})
        dias = jornada.get("dias", [])
        horas = jornada.get("horas", [])

        for d_idx, dia in enumerate(dias):
            for h_idx, hora in enumerate(horas):
                var = tk.IntVar(value=1)
                self.vars_bloques.append(var)
                tk.Checkbutton(
                    self.grid_chk, text=f"{dia} {hora}", variable=var,
                    font=("Segoe UI", 9), bg="white"
                ).grid(row=d_idx, column=h_idx, sticky="w", padx=8, pady=3)

    def _cargar_checks_profesor(self, event=None):
        idx = self.cb_edit_prof.current()
        if idx < 0: return
        disp = self.data["profesores"][idx]["disponibilidad"]
        for b in range(min(len(disp), len(self.vars_bloques))):
            self.vars_bloques[b].set(1 if disp[b] == "1" else 0)

    def _guardar_disponibilidad_ui(self):
        idx = self.cb_edit_prof.current()
        if idx < 0: return
        bits = "".join(str(v.get()) for v in self.vars_bloques)
        subprocess.run([EXE_PATH, "disp", DATOS_TXT, str(idx), bits], check=True)
        self.recalcular_y_cargar()
        messagebox.showinfo("Actualizado", "Disponibilidad guardada en datos.txt y horario recalculado.")

    def _cargar_alumnos_grupo(self, event=None):
        idx = self.cb_edit_grupo.current()
        if idx < 0: return
        self.entry_alumnos.delete(0, tk.END)
        self.entry_alumnos.insert(0, str(self.data["grupos"][idx]["alumnos"]))

    def _guardar_alumnos_ui(self):
        idx = self.cb_edit_grupo.current()
        if idx < 0: return
        try:
            cant = int(self.entry_alumnos.get())
            if cant <= 0: raise ValueError
        except ValueError:
            messagebox.showerror("Error", "Ingresa un numero entero positivo de alumnos.")
            return
        subprocess.run([EXE_PATH, "alum", DATOS_TXT, str(idx), str(cant)], check=True)
        self.recalcular_y_cargar()
        messagebox.showinfo("Actualizado", "Cantidad de alumnos actualizada y aulas reasignadas.")

    # ---------------- EJECUTAR C Y ACTUALIZAR UI ----------------
    def recalcular_y_cargar(self):
        proc = subprocess.run([EXE_PATH, "run", DATOS_TXT, SALIDA_JSON])
        res = proc.returncode
        if res == 1:
            messagebox.showerror("Error", "No se pudo leer datos.txt (revisa que la longitud de disponibilidad coincida con la jornada).")
            return
        if not os.path.exists(SALIDA_JSON):
            return

        with open(SALIDA_JSON, "r", encoding="utf-8") as f:
            self.data = json.load(f)

        if res == 2:
            messagebox.showwarning("Advertencia", "Faltan profesores disponibles para cubrir todas las materias en el Paso 1.")

        r = self.data["resumen"]
        self.kpis["total_clases"].config(text=str(r["total_clases"]))
        self.kpis["bloques_usados"].config(text=f"{r['bloques_usados']} / {r['total_bloques']}")
        self.kpis["ocupacion_promedio"].config(text=f"{r['ocupacion_promedio']:.1f}%")
        self.kpis["sin_aula"].config(text=str(r["sin_aula"]))

        self._renderizar_cuadricula()

        for item in self.tree_clases.get_children():
            self.tree_clases.delete(item)
        clases_ord = sorted(self.data["clases"], key=lambda x: (x["bloque"], x["grupo"]))
        for c in clases_ord:
            ocup = f"{c['alumnos']} / {c['capacidad']}" if c["aula"] != "SIN AULA" else f"{c['alumnos']} / -"
            self.tree_clases.insert("", "end", values=(
                c["dia"], c["hora"], c["grupo"], f"{c['materia_nombre']} ({c['materia_id']})",
                c["prioridad"], c["profesor"], c["aula"], ocup
            ))

        for item in self.tree_materias.get_children(): self.tree_materias.delete(item)
        for m in self.data["materias"]:
            self.tree_materias.insert("", "end", values=(m["id"], m["nombre"], m["prioridad"], m["profesor_asignado"]))

        j = self.data["jornada"]
        self.lbl_titulo_profes.config(text=f"Profesores y Disponibilidad ({j['total_bloques']} Bloques: {j['num_dias']} dias x {j['num_horas']} horas)")

        for item in self.tree_profes.get_children(): self.tree_profes.delete(item)
        for p in self.data["profesores"]:
            self.tree_profes.insert("", "end", values=(p["nombre"], p["disponibilidad"], ", ".join(p["materias"])))

        for item in self.tree_grupos.get_children(): self.tree_grupos.delete(item)
        for g in self.data["grupos"]:
            self.tree_grupos.insert("", "end", values=(g["id"], g["alumnos"], ", ".join(g["materias"])))

        aulas_txt = "AULAS REGISTRADAS:\n" + "   |   ".join(f"{a['nombre']} (Cap: {a['capacidad']})" for a in self.data["aulas"])
        incomp_txt = "\n\nMATERIAS INCOMPATIBLES (No pueden coincidir en hora):\n" + ",   ".join(
            f"{x['a']} <-> {x['b']}" for x in self.data["incompatibles"]
        )
        self.lbl_aulas_incomp.config(text=aulas_txt + incomp_txt)

        self._reconstruir_checkboxes_jornada()

        profes_nombres = [p["nombre"] for p in self.data["profesores"]]
        self.cb_edit_prof["values"] = profes_nombres
        if profes_nombres:
            if self.cb_edit_prof.current() < 0:
                self.cb_edit_prof.current(0)
            self._cargar_checks_profesor()

        grupos_ids = [g["id"] for g in self.data["grupos"]]
        self.cb_edit_grupo["values"] = grupos_ids
        if grupos_ids:
            if self.cb_edit_grupo.current() < 0:
                self.cb_edit_grupo.current(0)
            self._cargar_alumnos_grupo()


if __name__ == "__main__":
    root = tk.Tk()
    app = OptimizadorHorariosApp(root)
    root.mainloop()