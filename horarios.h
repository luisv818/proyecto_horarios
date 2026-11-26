#ifndef HORARIOS_H
#define HORARIOS_H

#ifdef _WIN32
  #define API __declspec(dllexport)
#else
  #define API
#endif

#define MAX_MATERIAS 100
#define MAX_PROFESORES 50
#define MAX_GRUPOS 40
#define MAX_AULAS 14
#define MAX_INCOMPATIBLES 100
#define MAX_POR_PROFESOR 15       // materias que puede dar un profesor
#define MAX_POR_GRUPO 15          // materias que lleva un grupo
#define MAX_CLASES 300
#define MAX_DIAS 7
#define MAX_HORAS_DIA 8
#define MAX_BLOQUES (MAX_DIAS * MAX_HORAS_DIA)

typedef struct {
    char id[8], nombre[32];
    int prioridad;                              // 1 (baja) a 3 (alta)
} Materia;

typedef struct {
    char nombre[20];
    int disponible[MAX_BLOQUES];                // 1 si puede dar clase en ese bloque
    int cantidad_materias;
    char materias[MAX_POR_PROFESOR][8];         // IDs de materias que puede dar
} Profesor;

typedef struct {
    char id[12];
    int alumnos;
    int cantidad_materias;
    char materias[MAX_POR_GRUPO][8];            // IDs de materias que lleva
} Grupo;

typedef struct {
    char nombre[16];
    int capacidad;
} Aula;

typedef struct {
    char id_a[8], id_b[8];                      // materias que no pueden coincidir
} Incompatibilidad;

typedef struct {
    int num_dias;
    int num_horas;
    int total_bloques;
    char dias[MAX_DIAS][16];
    char horas[MAX_HORAS_DIA][20];

    Materia materias[MAX_MATERIAS];       int cantidad_materias;
    Profesor profesores[MAX_PROFESORES];  int cantidad_profesores;
    Grupo grupos[MAX_GRUPOS];             int cantidad_grupos;
    Aula aulas[MAX_AULAS];                int cantidad_aulas;
    Incompatibilidad incompatibles[MAX_INCOMPATIBLES];
    int cantidad_incompatibles;
} Datos;

// Una clase = una materia, con su profesor, su grupo, su hora (bloque) y su aula
typedef struct {
    int materia, profesor, grupo, alumnos;
    int bloque, aula;                           // -1 mientras no se asignan
} Clase;

// Resultado de los pasos 1 y 2
typedef struct {
    int profesor_de_materia[MAX_MATERIAS];      // paso 1
    int paso1_hecho;
    Clase clases[MAX_CLASES];                   // paso 2 (luego llevan hora y aula)
    int cantidad_clases;
    int paso2_hecho;
} Plan;

// datos.c
API int  leer_entero(const char *mensaje);
API int  buscar_materia(const Datos *datos, const char *id);
API int  cargar_datos(Datos *datos, const char *archivo);
API int  guardar_datos(const Datos *datos, const char *archivo);
API void ver_datos(const Datos *datos);
API void editar_disponibilidad(Datos *datos);
API void editar_alumnos_grupo(Datos *datos);

// horarios.c
API void asignar_profesores(const Datos *datos, Plan *plan);   // paso 1
API void asignar_grupos(const Datos *datos, Plan *plan);       // paso 2
API void generar_horario(const Datos *datos, Plan *plan);      // pasos 3 a 6

// api_bridge.c (Puente para JSON / CLI)
API int  ejecutar_todo_y_exportar_json(const char *archivo_datos, const char *archivo_json);
API void actualizar_disponibilidad_api(const char *archivo_datos, int idx_prof, const char *bits);
API void actualizar_alumnos_api(const char *archivo_datos, int idx_grupo, int alumnos);

#endif
