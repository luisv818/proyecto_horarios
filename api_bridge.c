#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "horarios.h"

static void escribir_json(const Datos *datos, const Plan *plan, const char *ruta_json) {
    FILE *f = fopen(ruta_json, "w");
    int i, j, aulas_usadas = 0, sin_aula = 0, bloques_usados = 0;
    int bloque_activo[MAX_BLOQUES] = {0};
    double suma_ocupacion = 0.0;

    if (!f) return;

    for (i = 0; i < plan->cantidad_clases; i++) {
        const Clase *c = &plan->clases[i];
        if (c->bloque >= 0 && c->bloque < datos->total_bloques) bloque_activo[c->bloque] = 1;
        if (c->aula >= 0) {
            suma_ocupacion += (double)c->alumnos / datos->aulas[c->aula].capacidad;
            aulas_usadas++;
        } else {
            sin_aula++;
        }
    }
    for (i = 0; i < datos->total_bloques; i++) bloques_usados += bloque_activo[i];

    fprintf(f, "{\n");
    // Jornada dinamica
    fprintf(f, "  \"jornada\": {\n");
    fprintf(f, "    \"num_dias\": %d,\n", datos->num_dias);
    fprintf(f, "    \"num_horas\": %d,\n", datos->num_horas);
    fprintf(f, "    \"total_bloques\": %d,\n", datos->total_bloques);
    fprintf(f, "    \"dias\": [");
    for (i = 0; i < datos->num_dias; i++)
        fprintf(f, "\"%s\"%s", datos->dias[i], (i < datos->num_dias - 1) ? ", " : "");
    fprintf(f, "],\n    \"horas\": [");
    for (i = 0; i < datos->num_horas; i++)
        fprintf(f, "\"%s\"%s", datos->horas[i], (i < datos->num_horas - 1) ? ", " : "");
    fprintf(f, "]\n  },\n");

    // Resumen metricas
    fprintf(f, "  \"resumen\": {\n");
    fprintf(f, "    \"total_clases\": %d,\n", plan->cantidad_clases);
    fprintf(f, "    \"bloques_usados\": %d,\n", bloques_usados);
    fprintf(f, "    \"total_bloques\": %d,\n", datos->total_bloques);
    fprintf(f, "    \"aulas_usadas\": %d,\n", aulas_usadas);
    fprintf(f, "    \"sin_aula\": %d,\n", sin_aula);
    fprintf(f, "    \"ocupacion_promedio\": %.1f\n", aulas_usadas > 0 ? (100.0 * suma_ocupacion / aulas_usadas) : 0.0);
    fprintf(f, "  },\n");

    // Materias + Paso 1
    fprintf(f, "  \"materias\": [\n");
    for (i = 0; i < datos->cantidad_materias; i++) {
        int p_idx = plan->profesor_de_materia[i];
        const char *prof_nombre = (p_idx >= 0) ? datos->profesores[p_idx].nombre : "SIN ASIGNAR";
        fprintf(f, "    {\"id\": \"%s\", \"nombre\": \"%s\", \"prioridad\": %d, \"profesor_asignado\": \"%s\"}%s\n",
                datos->materias[i].id, datos->materias[i].nombre, datos->materias[i].prioridad,
                prof_nombre, (i < datos->cantidad_materias - 1) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Profesores
    fprintf(f, "  \"profesores\": [\n");
    for (i = 0; i < datos->cantidad_profesores; i++) {
        const Profesor *p = &datos->profesores[i];
        char disp[MAX_BLOQUES + 1];
        for (j = 0; j < datos->total_bloques; j++) disp[j] = p->disponible[j] ? '1' : '0';
        disp[datos->total_bloques] = '\0';

        fprintf(f, "    {\"nombre\": \"%s\", \"disponibilidad\": \"%s\", \"materias\": [", p->nombre, disp);
        for (j = 0; j < p->cantidad_materias; j++) {
            fprintf(f, "\"%s\"%s", p->materias[j], (j < p->cantidad_materias - 1) ? ", " : "");
        }
        fprintf(f, "]}%s\n", (i < datos->cantidad_profesores - 1) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Grupos
    fprintf(f, "  \"grupos\": [\n");
    for (i = 0; i < datos->cantidad_grupos; i++) {
        const Grupo *g = &datos->grupos[i];
        fprintf(f, "    {\"id\": \"%s\", \"alumnos\": %d, \"materias\": [", g->id, g->alumnos);
        for (j = 0; j < g->cantidad_materias; j++) {
            fprintf(f, "\"%s\"%s", g->materias[j], (j < g->cantidad_materias - 1) ? ", " : "");
        }
        fprintf(f, "]}%s\n", (i < datos->cantidad_grupos - 1) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Aulas
    fprintf(f, "  \"aulas\": [\n");
    for (i = 0; i < datos->cantidad_aulas; i++) {
        fprintf(f, "    {\"nombre\": \"%s\", \"capacidad\": %d}%s\n",
                datos->aulas[i].nombre, datos->aulas[i].capacidad,
                (i < datos->cantidad_aulas - 1) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Incompatibles
    fprintf(f, "  \"incompatibles\": [\n");
    for (i = 0; i < datos->cantidad_incompatibles; i++) {
        fprintf(f, "    {\"a\": \"%s\", \"b\": \"%s\"}%s\n",
                datos->incompatibles[i].id_a, datos->incompatibles[i].id_b,
                (i < datos->cantidad_incompatibles - 1) ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Clases agendadas
    fprintf(f, "  \"clases\": [\n");
    for (i = 0; i < plan->cantidad_clases; i++) {
        const Clase *c = &plan->clases[i];
        const char *aula_nom = (c->aula >= 0) ? datos->aulas[c->aula].nombre : "SIN AULA";
        int cap = (c->aula >= 0) ? datos->aulas[c->aula].capacidad : 0;
        const char *dia = (c->bloque >= 0) ? datos->dias[c->bloque / datos->num_horas] : "N/A";
        const char *hora = (c->bloque >= 0) ? datos->horas[c->bloque % datos->num_horas] : "N/A";

        fprintf(f, "    {\"id\": %d, \"materia_id\": \"%s\", \"materia_nombre\": \"%s\", "
                   "\"prioridad\": %d, \"profesor\": \"%s\", \"grupo\": \"%s\", "
                   "\"alumnos\": %d, \"bloque\": %d, \"dia\": \"%s\", \"hora\": \"%s\", "
                   "\"aula\": \"%s\", \"capacidad\": %d}%s\n",
                i, datos->materias[c->materia].id, datos->materias[c->materia].nombre,
                datos->materias[c->materia].prioridad, datos->profesores[c->profesor].nombre,
                datos->grupos[c->grupo].id, c->alumnos, c->bloque, dia, hora,
                aula_nom, cap, (i < plan->cantidad_clases - 1) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

API int ejecutar_todo_y_exportar_json(const char *archivo_datos, const char *archivo_json) {
    Datos datos;
    Plan plan = {0};
    if (!cargar_datos(&datos, archivo_datos)) return 0;
    asignar_profesores(&datos, &plan);
    if (!plan.paso1_hecho) {
        escribir_json(&datos, &plan, archivo_json);
        return -1;
    }
    asignar_grupos(&datos, &plan);
    generar_horario(&datos, &plan);
    escribir_json(&datos, &plan, archivo_json);
    return 1;
}

API void actualizar_disponibilidad_api(const char *archivo_datos, int idx_prof, const char *bits) {
    Datos datos;
    int b;
    if (!cargar_datos(&datos, archivo_datos)) return;
    if (idx_prof < 0 || idx_prof >= datos.cantidad_profesores) return;
    if ((int)strlen(bits) != datos.total_bloques) return;
    for (b = 0; b < datos.total_bloques; b++) {
        datos.profesores[idx_prof].disponible[b] = (bits[b] == '1');
    }
    guardar_datos(&datos, archivo_datos);
}

API void actualizar_alumnos_api(const char *archivo_datos, int idx_grupo, int alumnos) {
    Datos datos;
    if (!cargar_datos(&datos, archivo_datos)) return;
    if (idx_grupo < 0 || idx_grupo >= datos.cantidad_grupos) return;
    if (alumnos > 0) {
        datos.grupos[idx_grupo].alumnos = alumnos;
        guardar_datos(&datos, archivo_datos);
    }
}

int main(int argc, char *argv[]) {
    if (argc >= 4 && strcmp(argv[1], "run") == 0) {
        int r = ejecutar_todo_y_exportar_json(argv[2], argv[3]);
        return (r == -1) ? 2 : (r == 1 ? 0 : 1);
    } else if (argc >= 5 && strcmp(argv[1], "disp") == 0) {
        actualizar_disponibilidad_api(argv[2], atoi(argv[3]), argv[4]);
        return 0;
    } else if (argc >= 5 && strcmp(argv[1], "alum") == 0) {
        actualizar_alumnos_api(argv[2], atoi(argv[3]), atoi(argv[4]));
        return 0;
    }
    return 1;
}