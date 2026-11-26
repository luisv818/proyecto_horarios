#include <stdio.h>
#include <string.h>
#include "horarios.h"

#define LIMITE_NODOS 2000000L

// ---------- U2: MergeSort (ordena de mayor a menor segun 'clave') ----------
static void merge_sort(int vector[], int temporal[], int inicio, int fin, const int clave[]) {
    int medio, i, j, k;
    if (fin - inicio <= 1) return;
    medio = (inicio + fin) / 2;
    merge_sort(vector, temporal, inicio, medio, clave);
    merge_sort(vector, temporal, medio, fin, clave);
    i = inicio; j = medio; k = inicio;
    while (i < medio && j < fin)
        temporal[k++] = clave[vector[i]] >= clave[vector[j]] ? vector[i++] : vector[j++];
    while (i < medio) temporal[k++] = vector[i++];
    while (j < fin) temporal[k++] = vector[j++];
    for (k = inicio; k < fin; k++) vector[k] = temporal[k];
}

static int profesor_puede_dar(const Profesor *profesor, const char *id_materia) {
    int i;
    for (i = 0; i < profesor->cantidad_materias; i++)
        if (strcmp(profesor->materias[i], id_materia) == 0) return 1;
    return 0;
}

static int bloques_disponibles(const Profesor *profesor, int total_bloques) {
    int b, cuenta = 0;
    for (b = 0; b < total_bloques; b++) cuenta += profesor->disponible[b];
    return cuenta;
}

// ======================= PASO 1: asignar profesores a materias =======================
void asignar_profesores(const Datos *datos, Plan *plan) {
    int grupos_de_materia[MAX_MATERIAS] = {0}, clave[MAX_MATERIAS];
    int orden[MAX_MATERIAS], temporal[MAX_MATERIAS], carga_profesor[MAX_PROFESORES] = {0};
    int g, i, m, p, todo_asignado = 1;

    for (g = 0; g < datos->cantidad_grupos; g++)
        for (i = 0; i < datos->grupos[g].cantidad_materias; i++) {
            m = buscar_materia(datos, datos->grupos[g].materias[i]);
            if (m != -1) grupos_de_materia[m]++;
        }
    for (m = 0; m < datos->cantidad_materias; m++) {
        int candidatos = 0;
        for (p = 0; p < datos->cantidad_profesores; p++)
            candidatos += profesor_puede_dar(&datos->profesores[p], datos->materias[m].id);
        clave[m] = 1000 - candidatos;
        orden[m] = m;
        plan->profesor_de_materia[m] = -1;
    }
    merge_sort(orden, temporal, 0, datos->cantidad_materias, clave);

    printf("\n--- PASO 1: profesor de cada materia ---\n");
    for (i = 0; i < datos->cantidad_materias; i++) {
        int mejor = -1, libres_mejor = -1;
        m = orden[i];
        if (grupos_de_materia[m] == 0) continue;

        for (p = 0; p < datos->cantidad_profesores; p++) {
            const Profesor *profesor = &datos->profesores[p];
            int libres = bloques_disponibles(profesor, datos->total_bloques) - carga_profesor[p];
            if (profesor_puede_dar(profesor, datos->materias[m].id) &&
                libres >= grupos_de_materia[m] && libres > libres_mejor) {
                mejor = p;
                libres_mejor = libres;
            }
        }
        plan->profesor_de_materia[m] = mejor;
        if (mejor == -1) {
            printf("  %-4s %-22s -> SIN PROFESOR (nadie puede o tiene disponibilidad)\n",
                   datos->materias[m].id, datos->materias[m].nombre);
            todo_asignado = 0;
        } else {
            carga_profesor[mejor] += grupos_de_materia[m];
            printf("  %-4s %-22s -> %s (%d clase(s))\n", datos->materias[m].id,
                   datos->materias[m].nombre, datos->profesores[mejor].nombre,
                   grupos_de_materia[m]);
        }
    }
    plan->paso1_hecho = todo_asignado;
    plan->paso2_hecho = 0;
}

// ======================= PASO 2: asignar grupos y alumnos a las materias =======================
void asignar_grupos(const Datos *datos, Plan *plan) {
    int g, i, m;

    if (!plan->paso1_hecho) {
        printf("Primero haz el paso 1 completo (todas las materias con profesor).\n");
        return;
    }
    plan->cantidad_clases = 0;
    printf("\n--- PASO 2: grupos y alumnos en cada materia ---\n");
    for (g = 0; g < datos->cantidad_grupos; g++) {
        const Grupo *grupo = &datos->grupos[g];
        printf("  %-5s (%d alumnos):", grupo->id, grupo->alumnos);
        for (i = 0; i < grupo->cantidad_materias; i++) {
            m = buscar_materia(datos, grupo->materias[i]);
            if (m == -1 || plan->cantidad_clases >= MAX_CLASES) continue;
            Clase *clase = &plan->clases[plan->cantidad_clases++];
            clase->materia = m;
            clase->profesor = plan->profesor_de_materia[m];
            clase->grupo = g;
            clase->alumnos = grupo->alumnos;
            clase->bloque = -1;
            clase->aula = -1;
            printf(" %s(%s)", datos->materias[m].id, datos->profesores[clase->profesor].nombre);
        }
        printf("\n");
    }
    printf("Total: %d clases por ubicar en el horario.\n", plan->cantidad_clases);
    plan->paso2_hecho = 1;
}

// ======================= PASOS 3 a 6: horario =======================
static const Datos *datos;
static Clase *clases;
static int n;
static int conflicto[MAX_CLASES][MAX_CLASES];
static int cantidad_conflictos[MAX_CLASES];
static int orden[MAX_CLASES];
static int bloque_de_clase[MAX_CLASES];
static int cantidad_bloques;

static int permitido(int clase, int bloque) {
    return datos->profesores[clases[clase].profesor].disponible[bloque];
}

static int choca(int clase, int bloque, const int bloque_de[]) {
    int v;
    for (v = 0; v < n; v++)
        if (conflicto[clase][v] && bloque_de[v] == bloque) return 1;
    return 0;
}

static int son_incompatibles(int materia_a, int materia_b) {
    int i;
    for (i = 0; i < datos->cantidad_incompatibles; i++) {
        const Incompatibilidad *x = &datos->incompatibles[i];
        const char *a = datos->materias[materia_a].id, *b = datos->materias[materia_b].id;
        if ((strcmp(x->id_a, a) == 0 && strcmp(x->id_b, b) == 0) ||
            (strcmp(x->id_a, b) == 0 && strcmp(x->id_b, a) == 0)) return 1;
    }
    return 0;
}

static void construir_grafo(void) {
    int i, j;
    memset(conflicto, 0, sizeof(conflicto));
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (clases[i].profesor == clases[j].profesor || clases[i].grupo == clases[j].grupo ||
                son_incompatibles(clases[i].materia, clases[j].materia))
                conflicto[i][j] = conflicto[j][i] = 1;
    for (i = 0; i < n; i++) {
        cantidad_conflictos[i] = 0;
        for (j = 0; j < n; j++) cantidad_conflictos[i] += conflicto[i][j];
    }
}

// ---------- U3: Greedy DSATUR ----------
static int saturacion(int clase) {
    int usado[MAX_BLOQUES] = {0}, v, cuenta = 0;
    for (v = 0; v < n; v++)
        if (conflicto[clase][v] && bloque_de_clase[v] >= 0 && !usado[bloque_de_clase[v]]) {
            usado[bloque_de_clase[v]] = 1;
            cuenta++;
        }
    return cuenta;
}

static int colorear_greedy(void) {
    int tam[MAX_BLOQUES] = {0}, hechas, i, b, pasada;

    cantidad_bloques = 0;
    for (i = 0; i < n; i++) bloque_de_clase[i] = -1;

    for (hechas = 0; hechas < n; hechas++) {
        int elegida = -1, mayor = -1, bloque_elegido = -1;
        for (i = 0; i < n; i++) {
            int v = orden[i], s;
            if (bloque_de_clase[v] != -1) continue;
            s = saturacion(v);
            if (elegida == -1 || s > mayor) { elegida = v; mayor = s; }
        }
        for (pasada = 0; pasada < 2 && bloque_elegido == -1; pasada++)
            for (b = 0; b < datos->total_bloques; b++) {
                int es_nuevo = (tam[b] == 0);
                if (es_nuevo != pasada || !permitido(elegida, b) ||
                    tam[b] >= datos->cantidad_aulas || choca(elegida, b, bloque_de_clase))
                    continue;
                bloque_elegido = b;
                break;
            }
        if (bloque_elegido == -1) {
            printf("No hay hora libre para %s de %s (profesor %s).\n",
                   datos->materias[clases[elegida].materia].id,
                   datos->grupos[clases[elegida].grupo].id,
                   datos->profesores[clases[elegida].profesor].nombre);
            return 0;
        }
        bloque_de_clase[elegida] = bloque_elegido;
        if (tam[bloque_elegido] == 0) cantidad_bloques++;
        tam[bloque_elegido]++;
    }
    return 1;
}

// ---------- U4: Backtracking + Branch and Bound ----------
static int mejor_cantidad, mejor_bloque[MAX_CLASES];
static int bloque_actual[MAX_CLASES], tamano_actual[MAX_BLOQUES];
static int cota_inferior;
static long nodos;

static int calcular_cota(void) {
    int por_grupo[MAX_GRUPOS] = {0}, por_profesor[MAX_PROFESORES] = {0}, i;
    int cota = (n + datos->cantidad_aulas - 1) / datos->cantidad_aulas;
    for (i = 0; i < n; i++) {
        por_grupo[clases[i].grupo]++;
        por_profesor[clases[i].profesor]++;
        if (por_grupo[clases[i].grupo] > cota) cota = por_grupo[clases[i].grupo];
        if (por_profesor[clases[i].profesor] > cota) cota = por_profesor[clases[i].profesor];
    }
    return cota;
}

static void probar(int posicion, int bloques_usados) {
    int clase, b, pasada;

    nodos++;
    if (nodos > LIMITE_NODOS) return;
    if (bloques_usados >= mejor_cantidad) return;
    if (posicion == n) {
        mejor_cantidad = bloques_usados;
        memcpy(mejor_bloque, bloque_actual, sizeof(bloque_actual));
        return;
    }
    if (mejor_cantidad == cota_inferior) return;

    clase = orden[posicion];
    for (pasada = 0; pasada < 2; pasada++)
        for (b = 0; b < datos->total_bloques; b++) {
            int es_nuevo = (tamano_actual[b] == 0);
            if (es_nuevo != pasada || !permitido(clase, b) ||
                tamano_actual[b] >= datos->cantidad_aulas || choca(clase, b, bloque_actual))
                continue;
            if (es_nuevo && bloques_usados + 1 >= mejor_cantidad) continue;
            bloque_actual[clase] = b;
            tamano_actual[b]++;
            probar(posicion + 1, bloques_usados + es_nuevo);
            tamano_actual[b]--;
            bloque_actual[clase] = -1;
        }
}

static void mejorar_con_backtracking(int greedy_ok) {
    int i;
    if (greedy_ok) {
        mejor_cantidad = cantidad_bloques;
        memcpy(mejor_bloque, bloque_de_clase, sizeof(mejor_bloque));
    } else {
        mejor_cantidad = datos->total_bloques + 1;
        for (i = 0; i < n; i++) mejor_bloque[i] = -1;
    }
    for (i = 0; i < n; i++) bloque_actual[i] = -1;
    memset(tamano_actual, 0, sizeof(tamano_actual));
    cota_inferior = calcular_cota();
    nodos = 0;

    probar(0, 0);

    if (mejor_cantidad <= datos->total_bloques) {
        cantidad_bloques = mejor_cantidad;
        memcpy(bloque_de_clase, mejor_bloque, sizeof(mejor_bloque));
    } else {
        cantidad_bloques = 0;
    }
}

// ---------- U5: Programacion Dinamica para las aulas de un bloque ----------
static void asignar_aulas(const int clases_bloque[], int cantidad_bloque, int aula_de[]) {
    static int mejor_valor[MAX_AULAS + 1][1 << MAX_AULAS];
    static int mascara_anterior[MAX_AULAS + 1][1 << MAX_AULAS];
    static int aula_elegida[MAX_AULAS + 1][1 << MAX_AULAS];
    int combinaciones = 1 << datos->cantidad_aulas;
    int i, mascara, aula, mejor_mascara = 0;

    for (i = 0; i <= cantidad_bloque; i++)
        for (mascara = 0; mascara < combinaciones; mascara++) mejor_valor[i][mascara] = -1;
    mejor_valor[0][0] = 0;

    for (i = 0; i < cantidad_bloque; i++) {
        const Clase *clase = &clases[clases_bloque[i]];
        int prioridad = datos->materias[clase->materia].prioridad;
        for (mascara = 0; mascara < combinaciones; mascara++) {
            int base = mejor_valor[i][mascara];
            if (base < 0) continue;
            if (base > mejor_valor[i + 1][mascara]) {
                mejor_valor[i + 1][mascara] = base;
                mascara_anterior[i + 1][mascara] = mascara;
                aula_elegida[i + 1][mascara] = -1;
            }
            for (aula = 0; aula < datos->cantidad_aulas; aula++) {
                int capacidad = datos->aulas[aula].capacidad;
                int nueva_mascara = mascara | (1 << aula);
                int valor = base + 10000 + prioridad * 100 + clase->alumnos * 100 / capacidad;
                if (((mascara >> aula) & 1) || capacidad < clase->alumnos) continue;
                if (valor > mejor_valor[i + 1][nueva_mascara]) {
                    mejor_valor[i + 1][nueva_mascara] = valor;
                    mascara_anterior[i + 1][nueva_mascara] = mascara;
                    aula_elegida[i + 1][nueva_mascara] = aula;
                }
            }
        }
    }
    for (mascara = 0; mascara < combinaciones; mascara++)
        if (mejor_valor[cantidad_bloque][mascara] > mejor_valor[cantidad_bloque][mejor_mascara])
            mejor_mascara = mascara;
    for (mascara = mejor_mascara, i = cantidad_bloque; i >= 1; i--) {
        aula_de[i - 1] = aula_elegida[i][mascara];
        mascara = mascara_anterior[i][mascara];
    }
}

// ---------- Programa del horario (pasos 3 a 6) ----------
void generar_horario(const Datos *datos_originales, Plan *plan) {
    int clave[MAX_CLASES], temporal[MAX_CLASES];
    int i, j, bloque, aristas = 0, aulas_usadas = 0, sin_aula = 0, greedy_ok;
    double suma_ocupacion = 0;

    if (!plan->paso2_hecho) {
        printf("Primero haz los pasos 1 y 2.\n");
        return;
    }
    datos = datos_originales;
    clases = plan->clases;
    n = plan->cantidad_clases;
    if (n == 0 || datos->cantidad_aulas == 0) {
        printf("No hay clases o aulas.\n");
        return;
    }

    construir_grafo();
    for (i = 0; i < n; i++) {
        int opciones = 0;
        for (j = 0; j < datos->total_bloques; j++) opciones += permitido(i, j);
        clave[i] = cantidad_conflictos[i] * 100 + (datos->total_bloques - opciones) * 10 +
                   datos->materias[clases[i].materia].prioridad;
        orden[i] = i;
        aristas += cantidad_conflictos[i];
    }
    merge_sort(orden, temporal, 0, n, clave);
    printf("\n--- PASO 3: grafo de conflictos --- %d clases, %d conflictos\n", n, aristas / 2);

    greedy_ok = colorear_greedy();
    if (greedy_ok)
        printf("--- PASO 4: Greedy (DSATUR) --- %d bloques\n", cantidad_bloques);
    else
        printf("--- PASO 4: Greedy (DSATUR) no basto, pasando a Backtracking exacto ---\n");

    mejorar_con_backtracking(greedy_ok);
    if (cantidad_bloques == 0) {
        printf("No existe horario valido con las restricciones actuales.\n");
        return;
    }
    printf("--- PASO 5: Backtracking + B&B --- %d bloques (minimo posible %d, nodos %ld)\n",
           cantidad_bloques, cota_inferior, nodos);

    for (i = 0; i < n; i++) {
        if (!permitido(i, bloque_de_clase[i])) { printf("ERROR: profesor no disponible.\n"); return; }
        for (j = 0; j < n; j++)
            if (conflicto[i][j] && bloque_de_clase[i] == bloque_de_clase[j]) {
                printf("ERROR: hay un conflicto.\n");
                return;
            }
    }

    printf("--- PASO 6: aulas con Programacion Dinamica ---\n");
    for (bloque = 0; bloque < datos->total_bloques; bloque++) {
        int clases_bloque[MAX_CLASES], aula_de[MAX_CLASES], cantidad_bloque = 0;
        for (i = 0; i < n; i++)
            if (bloque_de_clase[i] == bloque) clases_bloque[cantidad_bloque++] = i;
        if (cantidad_bloque == 0) continue;
        asignar_aulas(clases_bloque, cantidad_bloque, aula_de);

        printf("\n[%s %s]\n", datos->dias[bloque / datos->num_horas], datos->horas[bloque % datos->num_horas]);
        for (j = 0; j < cantidad_bloque; j++) {
            Clase *clase = &clases[clases_bloque[j]];
            clase->bloque = bloque;
            clase->aula = aula_de[j];
            printf("  %-4s %-22s %-10s %-5s ", datos->materias[clase->materia].id,
                   datos->materias[clase->materia].nombre,
                   datos->profesores[clase->profesor].nombre, datos->grupos[clase->grupo].id);
            if (clase->aula >= 0) {
                const Aula *aula = &datos->aulas[clase->aula];
                suma_ocupacion += (double)clase->alumnos / aula->capacidad;
                aulas_usadas++;
                printf("-> %s (%d/%d)\n", aula->nombre, clase->alumnos, aula->capacidad);
            } else {
                sin_aula++;
                printf("SIN AULA\n");
            }
        }
    }
    if (aulas_usadas > 0)
        printf("\nOcupacion promedio de aulas: %.1f%%\n", 100.0 * suma_ocupacion / aulas_usadas);
    printf("Clases sin aula: %d\n", sin_aula);
}