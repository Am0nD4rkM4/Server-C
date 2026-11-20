// servidor.c — Servidor de laboratorio en C, v2 con hilos, solo para TU propia máquina.
// Escucha solo en 127.0.0.1 (localhost), puerto 4444. Nadie de fuera puede entrar.
// Es tu objetivo de práctica: lo corres en Arch o en Kali y lo pruebas con
// nmap, nc y tu escáner, para ver cómo se ve un servicio real por dentro.
// No ejecuta comandos del sistema ni abre archivos: solo responde texto.
// Usuario y clave de práctica: lab / lab123 (no son reales, solo para el lab).
//
// v2 — HILOS: ahora atiende a varios clientes A LA VEZ.
// ¿Qué es un hilo? Un hilo es como un trabajador más dentro del mismo
// programa: todos comparten la misma memoria (las variables globales), pero
// cada uno tiene sus propias variables locales y su propio camino por el
// código. Por eso cada cliente tiene su propio LOGIN (su `dentro`), su
// propia línea en curso, etc.: son variables locales del hilo que lo atiende.
// En la v1 el main atendía a un cliente hasta que se iba y mientras tanto
// nadie más podía entrar: si un cliente conectaba y no hacía nada, el
// servidor quedaba bloqueado para todos. Ahora, por cada cliente que llama,
// el main crea un hilo que lo atiende y sigue esperando al siguiente.
#include <stdio.h>          // para printf (imprimir en pantalla)
#include <stdlib.h>         // para malloc y free
#include <string.h>         // para strcmp, strlen, strncmp (comparar texto)
#include <time.h>           // para la hora del comando HORA
#include <pthread.h>        // para los hilos: pthread_create, pthread_detach, pthread_mutex_t
#include <sys/socket.h>     // para socket(), bind(), listen(), accept()
#include <arpa/inet.h>      // para la dirección 127.0.0.1
#include <unistd.h>         // para close() (colgar el "teléfono")

// Puerto donde escucha el servidor. 4444 es un puerto típico de laboratorio.
#define PUERTO 4444

// Contador de clientes conectados ahora mismo y número del próximo cliente.
// Los tocan varios hilos a la vez, así que van protegidos con un candado
// (mutex): ver atender_cliente().
// PTHREAD_MUTEX_INITIALIZER lo deja listo sin llamar a ninguna función.
// Un mutex es como una puerta con una sola llave: solo el hilo que tiene
// la llave puede tocar lo protegido; los demás esperan su turno.
static int clientes_activos = 0;
static int proximo_numero = 0;
static pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;

// Esta función atiende UNA línea (un comando) y responde al cliente.
// Devuelve 1 si el cliente pidió SALIR, 0 si hay que seguir.
// Se separó en su propia función porque un solo recv() puede traer varias
// líneas juntas (por ejemplo, si se mandan por tubería o con un script),
// y cada línea es un comando distinto.
int atender_linea(int cliente, char *mensaje, int *dentro) {
    char respuesta[512];

    if (strcmp(mensaje, "AYUDA") == 0) {
        // strcmp compara dos textos: devuelve 0 cuando son iguales.
        snprintf(respuesta, sizeof(respuesta),
            "Comandos:\n"
            "  LOGIN <usuario> <clave> — entrar (prueba: lab lab123)\n"
            "  ECO <texto> — repite tu texto (solo dentro)\n"
            "  HORA — la hora del servidor (solo dentro)\n"
            "  QUIEN — quien eres en este lab (solo dentro)\n"
            "  SALIR — colgar\n");
    } else if (strncmp(mensaje, "LOGIN ", 6) == 0) {
        // strncmp compara solo los primeros 6 caracteres: "LOGIN ".
        char usuario[64], clave[64];
        if (sscanf(mensaje + 6, "%63s %63s", usuario, clave) == 2 &&
            strcmp(usuario, "lab") == 0 && strcmp(clave, "lab123") == 0) {
            *dentro = 1;
            snprintf(respuesta, sizeof(respuesta), "OK: dentro. Prueba ECO, HORA y QUIEN.\n");
        } else {
            snprintf(respuesta, sizeof(respuesta), "MAL: usuario o clave incorrectos.\n");
        }
    } else if (strcmp(mensaje, "SALIR") == 0) {
        snprintf(respuesta, sizeof(respuesta), "Adios.\n");
        send(cliente, respuesta, strlen(respuesta), 0);
        return 1;  // 1 = colgar, salir del bucle del cliente
    } else if (!*dentro) {
        // Sin LOGIN, estos comandos no responden: así se ve un
        // servicio que pide autenticación antes de hacer nada.
        snprintf(respuesta, sizeof(respuesta), "Primero entra con LOGIN <usuario> <clave>.\n");
    } else if (strncmp(mensaje, "ECO ", 4) == 0) {
        snprintf(respuesta, sizeof(respuesta), "Eco: %s\n", mensaje + 4);
    } else if (strcmp(mensaje, "HORA") == 0) {
        time_t ahora = time(NULL);  // time() da la hora actual
        snprintf(respuesta, sizeof(respuesta), "Hora: %s", ctime(&ahora));
    } else if (strcmp(mensaje, "QUIEN") == 0) {
        snprintf(respuesta, sizeof(respuesta), "Eres el usuario lab, en el laboratorio local.\n");
    } else {
        snprintf(respuesta, sizeof(respuesta), "Comando desconocido. Escribe AYUDA.\n");
    }

    send(cliente, respuesta, strlen(respuesta), 0);
    return 0;
}

// Esta función es el trabajo de UN hilo: atiende a un solo cliente desde
// que se conecta hasta que se va (SALIR o colgar).
// Recibe un puntero a un int que guarda el socket del cliente. El int se
// creó con malloc() en el main: si pasáramos la dirección de una variable
// local del bucle, el siguiente accept() la sobrescribiría antes de que
// este hilo la leyera (una "carrera" entre hilos). Con malloc cada hilo
// tiene su propia copia, y él mismo la libera con free() al terminar.
void *atender_cliente(void *arg) {
    int cliente = *(int *)arg;  // sacar el socket del puntero
    free(arg);                  // ya no hace falta la copia

    // Contar este cliente: el candado hace que solo un hilo a la vez
    // toque los contadores. Sin candado, dos hilos podrían leer "2" a la
    // vez, sumar 1 los dos y guardar "3" cuando deberían ser "4":
    // eso es una "condición de carrera" (race condition).
    pthread_mutex_lock(&candado);
    clientes_activos++;
    proximo_numero++;
    int mi_numero = proximo_numero;  // número único de este cliente
    int activos = clientes_activos;
    pthread_mutex_unlock(&candado);  // soltar el candado cuanto antes

    printf("Cliente %d conectado (activos: %d).\n", mi_numero, activos);
    fflush(stdout);

    // El banner es lo primero que ve quien se conecta, como en un servicio real.
    char *banner = "D4rkLab v2 — servidor de practica con hilos (solo 127.0.0.1)\nEscribe AYUDA para ver los comandos.\n";
    send(cliente, banner, strlen(banner), 0);

    int dentro = 0;  // 0 = aún no entró, 1 = ya pasó el LOGIN (propio de este hilo)
    char trozo[256];
    char linea[256];
    int en_linea = 0;  // cuántos caracteres llevamos de la línea actual
    int salir = 0;

    while (!salir) {
        // recv() lee lo que manda el cliente. Si devuelve 0, el cliente colgó.
        // Ojo: recv() NO promete una línea por llamada: puede traer media
        // línea, una línea, o varias líneas juntas. Por eso abajo se corta
        // el trozo por cada '\n' y cada línea se atiende por separado.
        int leido = recv(cliente, trozo, sizeof(trozo) - 1, 0);
        if (leido <= 0) {
            break;
        }
        for (int i = 0; i < leido && !salir; i++) {
            if (trozo[i] == '\n' || trozo[i] == '\r') {
                if (en_linea == 0) {
                    continue;  // línea vacía, no hay comando
                }
                linea[en_linea] = '\0';  // '\0' marca el final del texto en C
                en_linea = 0;
                salir = atender_linea(cliente, linea, &dentro);
            } else if (en_linea < (int)sizeof(linea) - 1) {
                linea[en_linea++] = trozo[i];
            }
        }
    }

    close(cliente);  // colgar con este cliente
    pthread_mutex_lock(&candado);
    clientes_activos--;
    activos = clientes_activos;
    pthread_mutex_unlock(&candado);
    printf("Cliente %d desconectado (activos: %d).\n", mi_numero, activos);
    fflush(stdout);
    return NULL;  // al volver, el hilo termina y, como es detached, se limpia solo
}

int main() {
    // 1. socket() crea el "teléfono": AF_INET = por internet, SOCK_STREAM = TCP.
    int servidor = socket(AF_INET, SOCK_STREAM, 0);

    // 2. La dirección donde va a escuchar: solo 127.0.0.1, solo esta máquina.
    struct sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(PUERTO);  // htons: el puerto en el orden de la red
    direccion.sin_addr.s_addr = inet_addr("127.0.0.1");

    // SO_REUSEADDR: permite volver a usar el puerto 4444 enseguida después
    // de apagar el servidor, sin que diga "Address already in use".
    int reutilizar = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &reutilizar, sizeof(reutilizar));

    // 3. bind() "amarra" el teléfono a esa dirección y ese puerto.
    //    Si otro programa ya usa el 4444, bind falla y el servidor no arranca.
    if (bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) != 0) {
        printf("No pude usar el puerto %d, ¿ya hay otro servidor corriendo?\n", PUERTO);
        return 1;
    }

    // 4. listen() lo pone "a la escucha": ya puede recibir llamadas, 5 en espera.
    listen(servidor, 5);
    printf("Servidor de laboratorio v2 (hilos) escuchando en 127.0.0.1:%d\n", PUERTO);
    printf("Atiende a varios clientes a la vez. Pruébalo con: nc 127.0.0.1 %d\n", PUERTO);
    printf("Pulsa Ctrl+C para apagarlo.\n");
    fflush(stdout);  // que se vea enseguida aunque la salida vaya a un archivo

    // Bucle sin fin: por cada cliente que llama, se crea un hilo que lo
    // atiende y el main sigue esperando al siguiente. Antes (v1) el main
    // mismo atendía al cliente hasta que se iba, y mientras tanto nadie
    // más podía entrar: si un cliente conectaba y no hacía nada, el
    // servidor quedaba bloqueado para todos.
    while (1) {
        // 5. accept() espera a que alguien "llame" y contesta: devuelve un
        //    socket nuevo, solo para hablar con ese cliente.
        int cliente = accept(servidor, NULL, NULL);

        // Cada hilo necesita su propia copia del socket (ver atender_cliente).
        int *copia = malloc(sizeof(int));
        if (copia == NULL) {
            close(cliente);
            continue;
        }
        *copia = cliente;

        // pthread_create() arranca el hilo: le dice qué función corre
        // (atender_cliente) y con qué dato (copia). pthread_t es el
        // "nombre" del hilo, como un PID.
        pthread_t hilo;
        if (pthread_create(&hilo, NULL, atender_cliente, copia) != 0) {
            printf("No pude crear el hilo para el cliente.\n");
            free(copia);
            close(cliente);
            continue;
        }
        // detach: el hilo se limpia solo cuando termina. Sin esto, el main
        // tendría que esperar a cada hilo con pthread_join(), y volveríamos
        // a bloquearnos: el main no puede esperar a todos a la vez.
        pthread_detach(hilo);
    }

    close(servidor);
    return 0;
}
