#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

#define PUERTO 5000
#define IP_SERVIDOR "127.0.0.1"
#define TOTAL_PLACAS_POOL 30

static char placas_pool[TOTAL_PLACAS_POOL][10];
static int placas_enviadas[TOTAL_PLACAS_POOL];
static int total_enviadas = 0;

void generar_pool_placas() {
    srand((unsigned int)time(NULL));
    const char letras[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (int i = 0; i < TOTAL_PLACAS_POOL; i++) {
        placas_pool[i][0] = letras[rand() % 26];
        placas_pool[i][1] = letras[rand() % 26];
        placas_pool[i][2] = letras[rand() % 26];
        placas_pool[i][3] = '-';
        placas_pool[i][4] = '0' + (rand() % 10);
        placas_pool[i][5] = '0' + (rand() % 10);
        placas_pool[i][6] = '0' + (rand() % 10);
        placas_pool[i][7] = '\0';
        placas_enviadas[i] = 0;
    }
}

const char* seleccionar_placa() {
    if (total_enviadas > 5 && (rand() % 100) < 30) {
        int intentos = 0;
        while (intentos < TOTAL_PLACAS_POOL) {
            int idx = rand() % TOTAL_PLACAS_POOL;
            if (placas_enviadas[idx]) {
                return placas_pool[idx];
            }
            intentos++;
        }
    }

    int idx = rand() % TOTAL_PLACAS_POOL;
    if (!placas_enviadas[idx]) {
        placas_enviadas[idx] = 1;
        total_enviadas++;
    }
    return placas_pool[idx];
}

void obtener_hora_actual(char* buffer, int size) {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    strftime(buffer, size, "%H:%M:%S", tm_info);
}

int main(int argc, char* argv[]) {
    const char* ip_servidor = IP_SERVIDOR;
    int puerto = PUERTO;

    if (argc > 1) {
        ip_servidor = argv[1];
    }
    if (argc > 2) {
        puerto = atoi(argv[2]);
    }

    printf("=== GENERADOR DE PLACAS - PARQUEADERO ===\n");
    printf("Conectando al servidor en %s:%d...\n\n", ip_servidor, puerto);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Error: No se pudo iniciar Winsock.\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("Error: No se pudo crear el socket.\n");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in servidor;
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(puerto);
    inet_pton(AF_INET, ip_servidor, &servidor.sin_addr);

    int intentos = 0;
    while (connect(sock, (struct sockaddr*)&servidor, sizeof(servidor)) == SOCKET_ERROR) {
        intentos++;
        if (intentos > 10) {
            printf("Error: No se pudo conectar al servidor despues de 10 intentos.\n");
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        printf("Reintentando conexion (%d/10)...\n", intentos);
        Sleep(2000);
    }

    printf("Conectado al servidor exitosamente!\n\n");

    generar_pool_placas();

    while (1) {
        const char* placa = seleccionar_placa();
        char hora[20];
        obtener_hora_actual(hora, sizeof(hora));

        char mensaje[256];
        snprintf(mensaje, sizeof(mensaje), "%s|%s\n", placa, hora);

        int enviado = send(sock, mensaje, (int)strlen(mensaje), 0);
        if (enviado == SOCKET_ERROR) {
            printf("Error: Se perdio la conexion con el servidor.\n");
            break;
        }

        printf("[%s] Placa enviada: %s\n", hora, placa);

        int espera = (rand() % 4 + 2) * 1000;
        Sleep(espera);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
