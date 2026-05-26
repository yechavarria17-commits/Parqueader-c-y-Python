#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define BUILDING_DLL
#include "parqueadero_lib.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")

static CeldaParqueadero celdas[MAX_CELDAS];
static SOCKET servidor_socket = INVALID_SOCKET;
static SOCKET cliente_socket = INVALID_SOCKET;
static HANDLE hilo_servidor = NULL;
static volatile bool servidor_corriendo = false;

static CRITICAL_SECTION cs_celdas;
static CRITICAL_SECTION cs_eventos;

#define MAX_EVENTOS 256
static EventoPlaca cola_eventos[MAX_EVENTOS];
static int evento_inicio = 0;
static int evento_fin = 0;
static int evento_count = 0;

static void encolar_evento(const EventoPlaca& ev) {
    EnterCriticalSection(&cs_eventos);
    if (evento_count < MAX_EVENTOS) {
        cola_eventos[evento_fin] = ev;
        evento_fin = (evento_fin + 1) % MAX_EVENTOS;
        evento_count++;
    }
    LeaveCriticalSection(&cs_eventos);
}

static int buscar_celda_por_placa(const char* placa) {
    for (int i = 0; i < MAX_CELDAS; i++) {
        if (celdas[i].ocupada && strcmp(celdas[i].placa, placa) == 0) {
            return i;
        }
    }
    return -1;
}

static int buscar_celda_libre() {
    for (int i = 0; i < MAX_CELDAS; i++) {
        if (!celdas[i].ocupada) {
            return i;
        }
    }
    return -1;
}

static void procesar_placa(const char* placa, const char* hora) {
    EnterCriticalSection(&cs_celdas);

    int celda_existente = buscar_celda_por_placa(placa);
    EventoPlaca ev;
    memset(&ev, 0, sizeof(ev));
    strncpy(ev.placa, placa, MAX_PLACA - 1);
    strncpy(ev.hora, hora, 19);

    if (celda_existente >= 0) {
        ev.celda = celda_existente;
        ev.tipo = 1;
        celdas[celda_existente].ocupada = 0;
        memset(celdas[celda_existente].placa, 0, MAX_PLACA);
        memset(celdas[celda_existente].hora_entrada, 0, 20);
    } else {
        int celda_libre = buscar_celda_libre();
        if (celda_libre >= 0) {
            ev.celda = celda_libre;
            ev.tipo = 0;
            celdas[celda_libre].ocupada = 1;
            strncpy(celdas[celda_libre].placa, placa, MAX_PLACA - 1);
            strncpy(celdas[celda_libre].hora_entrada, hora, 19);
        } else {
            ev.celda = -1;
            ev.tipo = 0;
        }
    }

    LeaveCriticalSection(&cs_celdas);
    encolar_evento(ev);
}

static DWORD WINAPI hilo_aceptar(LPVOID param) {
    while (servidor_corriendo) {
        SOCKET nuevo_cliente = accept(servidor_socket, NULL, NULL);
        if (nuevo_cliente == INVALID_SOCKET) {
            if (!servidor_corriendo) break;
            continue;
        }

        cliente_socket = nuevo_cliente;
        char buffer[256];

        while (servidor_corriendo) {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(cliente_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;

            buffer[bytes] = '\0';

            char* linea = strtok(buffer, "\n");
            while (linea != NULL) {
                char placa[MAX_PLACA] = {0};
                char hora[20] = {0};

                char* sep = strchr(linea, '|');
                if (sep) {
                    int len_placa = (int)(sep - linea);
                    if (len_placa >= MAX_PLACA) len_placa = MAX_PLACA - 1;
                    strncpy(placa, linea, len_placa);
                    strncpy(hora, sep + 1, 19);
                    procesar_placa(placa, hora);
                }
                linea = strtok(NULL, "\n");
            }
        }

        closesocket(cliente_socket);
        cliente_socket = INVALID_SOCKET;
    }
    return 0;
}

extern "C" {

DLL_EXPORT int iniciar_servidor(int puerto) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return -1;
    }

    InitializeCriticalSection(&cs_celdas);
    InitializeCriticalSection(&cs_eventos);
    memset(celdas, 0, sizeof(celdas));
    evento_inicio = 0;
    evento_fin = 0;
    evento_count = 0;

    servidor_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servidor_socket == INVALID_SOCKET) {
        WSACleanup();
        return -2;
    }

    int opt = 1;
    setsockopt(servidor_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(puerto);

    if (bind(servidor_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(servidor_socket);
        WSACleanup();
        return -3;
    }

    if (listen(servidor_socket, 5) == SOCKET_ERROR) {
        closesocket(servidor_socket);
        WSACleanup();
        return -4;
    }

    servidor_corriendo = true;
    hilo_servidor = CreateThread(NULL, 0, hilo_aceptar, NULL, 0, NULL);
    if (hilo_servidor == NULL) {
        closesocket(servidor_socket);
        WSACleanup();
        return -5;
    }

    return 0;
}

DLL_EXPORT int detener_servidor() {
    servidor_corriendo = false;

    if (cliente_socket != INVALID_SOCKET) {
        closesocket(cliente_socket);
        cliente_socket = INVALID_SOCKET;
    }
    if (servidor_socket != INVALID_SOCKET) {
        closesocket(servidor_socket);
        servidor_socket = INVALID_SOCKET;
    }
    if (hilo_servidor) {
        WaitForSingleObject(hilo_servidor, 3000);
        CloseHandle(hilo_servidor);
        hilo_servidor = NULL;
    }

    DeleteCriticalSection(&cs_celdas);
    DeleteCriticalSection(&cs_eventos);
    WSACleanup();
    return 0;
}

DLL_EXPORT int obtener_evento(EventoPlaca* evento) {
    EnterCriticalSection(&cs_eventos);
    if (evento_count == 0) {
        LeaveCriticalSection(&cs_eventos);
        return 0;
    }
    *evento = cola_eventos[evento_inicio];
    evento_inicio = (evento_inicio + 1) % MAX_EVENTOS;
    evento_count--;
    LeaveCriticalSection(&cs_eventos);
    return 1;
}

DLL_EXPORT int obtener_estado_celdas(CeldaParqueadero* out, int max_celdas) {
    EnterCriticalSection(&cs_celdas);
    int n = max_celdas < MAX_CELDAS ? max_celdas : MAX_CELDAS;
    memcpy(out, celdas, n * sizeof(CeldaParqueadero));
    LeaveCriticalSection(&cs_celdas);
    return n;
}

DLL_EXPORT int obtener_total_celdas() {
    return MAX_CELDAS;
}

DLL_EXPORT int obtener_celdas_ocupadas() {
    EnterCriticalSection(&cs_celdas);
    int count = 0;
    for (int i = 0; i < MAX_CELDAS; i++) {
        if (celdas[i].ocupada) count++;
    }
    LeaveCriticalSection(&cs_celdas);
    return count;
}

DLL_EXPORT CeldaParqueadero obtener_celda(int idx) {
    CeldaParqueadero c;
    memset(&c, 0, sizeof(c));
    if (idx >= 0 && idx < MAX_CELDAS) {
        EnterCriticalSection(&cs_celdas);
        c = celdas[idx];
        LeaveCriticalSection(&cs_celdas);
    }
    return c;
}

}
