#ifndef PARQUEADERO_LIB_H
#define PARQUEADERO_LIB_H

#ifdef _WIN32
    #ifdef BUILDING_DLL
        #define DLL_EXPORT __declspec(dllexport)
    #else
        #define DLL_EXPORT __declspec(dllimport)
    #endif
#else
    #define DLL_EXPORT
#endif

#define MAX_CELDAS 20
#define MAX_PLACA 10
#define PUERTO 5000

typedef struct {
    char placa[MAX_PLACA];
    char hora_entrada[20];
    int celda;
    int ocupada;
} CeldaParqueadero;

typedef struct {
    char placa[MAX_PLACA];
    char hora[20];
    int celda;
    int tipo; // 0 = entrada, 1 = salida
} EventoPlaca;

#ifdef __cplusplus
extern "C" {
#endif

DLL_EXPORT int iniciar_servidor(int puerto);
DLL_EXPORT int detener_servidor();
DLL_EXPORT int obtener_evento(EventoPlaca* evento);
DLL_EXPORT int obtener_estado_celdas(CeldaParqueadero* celdas, int max_celdas);
DLL_EXPORT int obtener_total_celdas();
DLL_EXPORT int obtener_celdas_ocupadas();
DLL_EXPORT CeldaParqueadero obtener_celda(int idx);

#ifdef __cplusplus
}
#endif

#endif
