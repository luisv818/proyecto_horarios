#include <stdio.h>
#include <string.h>
#include "horarios.h"

static void inicializar_jornada_defecto(Datos *datos) {
    const char *dias_def[] = {"Lun", "Mar", "Mie", "Jue", "Vie"};
    const char *horas_def[] = {"7-9", "9-11"};
    int i;
    datos->num_dias = 5;
    datos->num_horas = 2;
    datos->total_bloques = 10;
    for (i = 0; i < 5; i++) strcpy(datos->dias[i], dias_def[i]);
    for (i = 0; i < 2; i++) strcpy(datos->horas[i], horas_def[i]);
}

int leer_entero(const char *mensaje) {
    int numero;
    printf("%s", mensaje);
    while (scanf("%d", &numero) != 1) {
        if (scanf("%*s") == EOF) return 0;     // descartar lo que no es numero
        printf("Valor invalido. %s", mensaje);
    }
    return numero;
}

int buscar_materia(const Datos *datos, const char *id) {
    int i;
    for (i = 0; i < datos->cantidad_materias; i++)
        if (strcmp(datos->materias[i].id, id) == 0) return i;
    return -1;
}

// Lee "cantidad id1 id2 ..." y devuelve la cantidad (o -1 si hay error)
static int leer_lista_ids(FILE *f, char lista[][8], int maximo) {
    int cantidad, i;
    if (fscanf(f, "%d", &cantidad) != 1 || cantidad < 0 || cantidad > maximo) return -1;
    for (i = 0; i < cantidad; i++)
        if (fscanf(f, "%7s", lista[i]) != 1) return -1;
    return cantidad;
}

// Formato de datos.txt: cada seccion es "NOMBRE cantidad" seguida de sus filas
int cargar_datos(Datos *datos, const char *archivo) {
    FILE *f = fopen(archivo, "r");
    char seccion[30], disponibilidad[MAX_BLOQUES + 8];
    int cantidad, i, b;
    if (f == NULL) return 0;

    memset(datos, 0, sizeof(Datos));
    inicializar_jornada_defecto(datos);

    while (fscanf(f, "%29s %d", seccion, &cantidad) == 2) {
        if (strcmp(seccion, "JORNADA") == 0) {
            int nh;
            if (fscanf(f, "%d", &nh) != 1 || cantidad < 1 || cantidad > MAX_DIAS ||
                nh < 1 || nh > MAX_HORAS_DIA) goto error;
            datos->num_dias = cantidad;
            datos->num_horas = nh;
            datos->total_bloques = cantidad * nh;
            for (i = 0; i < datos->num_dias; i++)
                if (fscanf(f, "%15s", datos->dias[i]) != 1) goto error;
            for (i = 0; i < datos->num_horas; i++)
                if (fscanf(f, "%19s", datos->horas[i]) != 1) goto error;
        } else if (strcmp(seccion, "MATERIAS") == 0 && cantidad <= MAX_MATERIAS) {
            for (i = 0; i < cantidad; i++) {
                Materia *m = &datos->materias[i];
                if (fscanf(f, "%7s %31s %d", m->id, m->nombre, &m->prioridad) != 3) goto error;
            }
            datos->cantidad_materias = cantidad;
        } else if (strcmp(seccion, "PROFESORES") == 0 && cantidad <= MAX_PROFESORES) {
            for (i = 0; i < cantidad; i++) {
                Profesor *p = &datos->profesores[i];
                if (fscanf(f, "%19s %63s", p->nombre, disponibilidad) != 2 ||
                    (int)strlen(disponibilidad) != datos->total_bloques) goto error;
                for (b = 0; b < datos->total_bloques; b++) p->disponible[b] = (disponibilidad[b] == '1');
                p->cantidad_materias = leer_lista_ids(f, p->materias, MAX_POR_PROFESOR);
                if (p->cantidad_materias < 0) goto error;
            }
            datos->cantidad_profesores = cantidad;
        } else if (strcmp(seccion, "GRUPOS") == 0 && cantidad <= MAX_GRUPOS) {
            for (i = 0; i < cantidad; i++) {
                Grupo *g = &datos->grupos[i];
                if (fscanf(f, "%11s %d", g->id, &g->alumnos) != 2) goto error;
                g->cantidad_materias = leer_lista_ids(f, g->materias, MAX_POR_GRUPO);
                if (g->cantidad_materias < 0) goto error;
            }
            datos->cantidad_grupos = cantidad;
        } else if (strcmp(seccion, "AULAS") == 0 && cantidad <= MAX_AULAS) {
            for (i = 0; i < cantidad; i++)
                if (fscanf(f, "%15s %d", datos->aulas[i].nombre, &datos->aulas[i].capacidad) != 2)
                    goto error;
            datos->cantidad_aulas = cantidad;
        } else if (strcmp(seccion, "INCOMPATIBLES") == 0 && cantidad <= MAX_INCOMPATIBLES) {
            for (i = 0; i < cantidad; i++)
                if (fscanf(f, "%7s %7s", datos->incompatibles[i].id_a,
                           datos->incompatibles[i].id_b) != 2) goto error;
            datos->cantidad_incompatibles = cantidad;
        } else goto error;
    }
    fclose(f);
    return 1;
error:
    fclose(f);
    return 0;
}

int guardar_datos(const Datos *datos, const char *archivo) {
    FILE *f = fopen(archivo, "w");
    int i, j;
    if (f == NULL) return 0;

    fprintf(f, "JORNADA %d %d\n", datos->num_dias, datos->num_horas);
    for (i = 0; i < datos->num_dias; i++)
        fprintf(f, "%s%s", datos->dias[i], (i < datos->num_dias - 1) ? " " : "\n");
    for (i = 0; i < datos->num_horas; i++)
        fprintf(f, "%s%s", datos->horas[i], (i < datos->num_horas - 1) ? " " : "\n");

    fprintf(f, "MATERIAS %d\n", datos->cantidad_materias);
    for (i = 0; i < datos->cantidad_materias; i++)
        fprintf(f, "%s %s %d\n", datos->materias[i].id, datos->materias[i].nombre,
                datos->materias[i].prioridad);

    fprintf(f, "PROFESORES %d\n", datos->cantidad_profesores);
    for (i = 0; i < datos->cantidad_profesores; i++) {
        const Profesor *p = &datos->profesores[i];
        fprintf(f, "%s ", p->nombre);
        for (j = 0; j < datos->total_bloques; j++) fprintf(f, "%d", p->disponible[j]);
        fprintf(f, " %d", p->cantidad_materias);
        for (j = 0; j < p->cantidad_materias; j++) fprintf(f, " %s", p->materias[j]);
        fprintf(f, "\n");
    }
    fprintf(f, "GRUPOS %d\n", datos->cantidad_grupos);
    for (i = 0; i < datos->cantidad_grupos; i++) {
        const Grupo *g = &datos->grupos[i];
        fprintf(f, "%s %d %d", g->id, g->alumnos, g->cantidad_materias);
        for (j = 0; j < g->cantidad_materias; j++) fprintf(f, " %s", g->materias[j]);
        fprintf(f, "\n");
    }
    fprintf(f, "AULAS %d\n", datos->cantidad_aulas);
    for (i = 0; i < datos->cantidad_aulas; i++)
        fprintf(f, "%s %d\n", datos->aulas[i].nombre, datos->aulas[i].capacidad);
    fprintf(f, "INCOMPATIBLES %d\n", datos->cantidad_incompatibles);
    for (i = 0; i < datos->cantidad_incompatibles; i++)
        fprintf(f, "%s %s\n", datos->incompatibles[i].id_a, datos->incompatibles[i].id_b);
    fclose(f);
    return 1;
}

void ver_datos(const Datos *datos) {
    int i, j;
    printf("\nJORNADA: %d dias x %d horas = %d bloques\n", datos->num_dias, datos->num_horas, datos->total_bloques);
    printf("MATERIAS:");
    for (i = 0; i < datos->cantidad_materias; i++)
        printf(" %s=%s(prioridad %d)%s", datos->materias[i].id, datos->materias[i].nombre,
               datos->materias[i].prioridad, i < datos->cantidad_materias - 1 ? "," : "");
    printf("\n\nPROFESORES (disponibilidad: 1 = puede, %d bloques):\n", datos->total_bloques);
    for (i = 0; i < datos->cantidad_profesores; i++) {
        const Profesor *p = &datos->profesores[i];
        printf("  %-12s ", p->nombre);
        for (j = 0; j < datos->total_bloques; j++) printf("%d", p->disponible[j]);
        printf("  puede dar:");
        for (j = 0; j < p->cantidad_materias; j++) printf(" %s", p->materias[j]);
        printf("\n");
    }
    printf("\nGRUPOS:\n");
    for (i = 0; i < datos->cantidad_grupos; i++) {
        const Grupo *g = &datos->grupos[i];
        printf("  %-6s %d alumnos, lleva:", g->id, g->alumnos);
        for (j = 0; j < g->cantidad_materias; j++) printf(" %s", g->materias[j]);
        printf("\n");
    }
    printf("\nAULAS:");
    for (i = 0; i < datos->cantidad_aulas; i++)
        printf(" %s(%d)", datos->aulas[i].nombre, datos->aulas[i].capacidad);
    printf("\nINCOMPATIBLES:");
    for (i = 0; i < datos->cantidad_incompatibles; i++)
        printf(" %s-%s", datos->incompatibles[i].id_a, datos->incompatibles[i].id_b);
    printf("\n");
}

void editar_disponibilidad(Datos *datos) {
    char nombre[20], texto[MAX_BLOQUES + 8];
    int i, b, valido = 1;
    printf("Nombre del profesor: ");
    if (scanf("%19s", nombre) != 1) return;
    for (i = 0; i < datos->cantidad_profesores; i++)
        if (strcmp(datos->profesores[i].nombre, nombre) == 0) break;
    if (i == datos->cantidad_profesores) { printf("No existe ese profesor.\n"); return; }

    printf("Nueva disponibilidad (%d digitos de 0/1): ", datos->total_bloques);
    if (scanf("%63s", texto) != 1) return;
    if ((int)strlen(texto) != datos->total_bloques) valido = 0;
    for (b = 0; valido && b < datos->total_bloques; b++)
        if (texto[b] != '0' && texto[b] != '1') valido = 0;
    if (!valido) { printf("Formato invalido.\n"); return; }

    for (b = 0; b < datos->total_bloques; b++) datos->profesores[i].disponible[b] = (texto[b] == '1');
    printf("Disponibilidad actualizada.\n");
}

void editar_alumnos_grupo(Datos *datos) {
    char id[12];
    int i;
    printf("ID del grupo: ");
    if (scanf("%11s", id) != 1) return;
    for (i = 0; i < datos->cantidad_grupos; i++)
        if (strcmp(datos->grupos[i].id, id) == 0) {
            datos->grupos[i].alumnos = leer_entero("Nueva cantidad de alumnos: ");
            printf("Alumnos actualizados.\n");
            return;
        }
    printf("No existe ese grupo.\n");
}