// servidor.c — Servidor de laboratorio en C, v6 con cambio de clave, para TU propio VPS.
// En el VPS se compila con -DIP_SERVIDOR='"0.0.0.0"' y escucha en todas
// sus interfaces, puerto 4444. En tu casa sigue usando la v4 con la IP
// virtual, no esta versión.
// Es tu objetivo de práctica en internet: lo corres en un VPS gratis de
// Oracle y lo pruebas desde tu casa con nmap, nc y tu escáner.
// No ejecuta comandos del sistema ni abre archivos: solo responde texto.
// La clave se pone con la variable de entorno SERVER_C_CLAVE, y ahora
// también se puede CAMBIAR desde dentro, con el comando CLAVE, sin
// apagar el servidor. La clave nunca se escribe en el código, ni se
// guarda en ningún archivo, ni se imprime: solo vive en la memoria del
// servidor y se compara con strcmp().
//
// v6 — CAMBIO DE CLAVE: un servicio real te deja cambiar tu contraseña
// cuando quieras, y este ya también. Dentro, con LOGIN hecho, escribes:
//   CLAVE <nueva-clave>
// y desde ahí la nueva es la que vale para todos. La vieja deja de
// funcionar en ese momento. El cambio dura mientras el servidor esté
// corriendo: si lo apagas y lo vuelves a correr, vuelve a valer la de
// SERVER_C_CLAVE (o lab123 si no pusiste ninguna).
// La clave es una sola para todos y la tocan varios hilos a la vez, así
// que tiene su propio candado (candado_clave): sin él, un hilo podría
// leerla a la mitad de cambiarla otro y comparar una clave mezclada.
//
// v5 — CLAVE CONFIGURABLE: en la v4 la clave estaba escrita en el código
// (lab/lab123), y eso en internet es regalar la entrada: cualquiera la
// adivina en el primer intento. Ahora la clave vive FUERA del código, en
// una variable de entorno, igual que la clave de Shodan en InfoForge:
//   SERVER_C_CLAVE="una-clave-fuerte" ./servidor
// getenv() lee una variable de entorno: devuelve su texto, o NULL si no
// existe. Si no existe, el servidor usa lab123 y avisa en el arranque.
// La clave NUNCA se escribe en el código, ni se sube a GitHub, ni se
// imprime en pantalla: solo se compara con strcmp().
//
// v4 — IP VIRTUAL: antes escuchaba en 127.0.0.1 (localhost), que es la
// máquina hablando consigo misma, y eso se siente falso al atacar. Ahora
// escucha en 10.10.10.10, una IP que tú le inventas a tu propia máquina
// con un comando (ver LEEME-IP-VIRTUAL.md). Es como ponerle un segundo
// nombre a tu laptop: nmap y nc la ven como si fuera otro equipo, pero
// el tráfico nunca sale de tu máquina. Si no creas la IP, el servidor no
// arranca y te dice cómo crearla.
// ¿Qué es una IP virtual? Tu tarjeta de red puede tener varias IPs a la
// vez. 127.0.0.1 es la de siempre (loopback, "esta máquina"). Con
// `ip addr add 10.10.10.10/32 dev lo` le añades otra a esa misma interfaz.
// 10.10.10.10 es del rango privado 10.x.x.x, que no existe en internet:
// por eso es seguro para el laboratorio, nadie de fuera puede llegar ahí.
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
//
// v3 — REGISTRO: ahora guarda en el archivo conexiones.log quién se conectó,
// desde qué IP y puerto, si entró con LOGIN o falló, y cuándo se fue.
// ¿Para qué sirve? En un servicio real, el registro es lo primero que se
// mira cuando algo raro pasa: quién entró, a qué hora, y si alguien intentó
// claves a lo loco. Aquí el registro es solo texto en un archivo, y se ve
// con: cat conexiones.log
// Dos ideas nuevas en esta versión:
//   1. fopen/fprintf/fclose: abrir el archivo en modo "a" (append, añadir
//      al final sin borrar lo que ya hay), escribir una línea y cerrarlo.
//      Se abre y se cierra en cada evento: así, si el servidor se apaga
//      de golpe (Ctrl+C), lo ya escrito no se pierde.
//   2. Un struct Cliente para pasarle al hilo el socket Y la IP del que
//      llama, juntos. En la v2 solo se pasaba el socket (un int); ahora
//      hacen falta dos datos más, así que van en una cajita con nombre.
//      La cajita se crea con malloc en el main y el hilo la libera con
//      free cuando ya copió los datos, igual que se hacía con el int.
//   3. Un segundo candado (mutex) solo para el archivo: si dos hilos
//      escribieran a la vez, sus líneas se mezclarían a la mitad. El
//      candado hace que escriban por turnos, una línea completa cada uno.
#include <stdio.h>          // para printf, fopen, fprintf, fclose
#include <stdlib.h>         // para malloc y free
#include <string.h>         // para strcmp, strlen, strncmp, strncpy
#include <time.h>           // para time, localtime, strftime (la hora del registro)
#include <pthread.h>        // para los hilos: pthread_create, pthread_detach, pthread_mutex_t
#include <sys/socket.h>     // para socket(), bind(), listen(), accept()
#include <arpa/inet.h>      // para inet_pton (texto a IP) e inet_ntoa (IP a texto)
#include <unistd.h>         // para close() (colgar el "teléfono")

// Puerto donde escucha el servidor. 4444 es un puerto típico de laboratorio.
#define PUERTO 4444

// La IP donde escucha el servidor. 10.10.10.10 es una IP virtual que tú
// creas en tu máquina con: sudo ip addr add 10.10.10.10/32 dev lo
// (ver LEEME-IP-VIRTUAL.md). Si la cambias aquí, cambia también ese comando.
// #ifndef significa "si nadie la definió antes": así puedes probar otra IP
// sin tocar el archivo, compilando con -DIP_SERVIDOR='"127.0.0.2"'.
// Otras opciones, solo para tu laboratorio:
//   "127.0.0.1"  = como antes, solo esta máquina (se siente menos real).
//   "192.168.0.108" = tu IP del Wi-Fi: ahí sí llega cualquiera de tu casa
//     que esté en el mismo Wi-Fi, y lo puedes atacar hasta desde el celular.
//   "0.0.0.0"    = todas las interfaces a la vez: cualquiera de tu red lo ve.
//     Solo en tu casa y solo para el lab, nunca con el router abierto a internet.
#ifndef IP_SERVIDOR
#define IP_SERVIDOR "10.10.10.10"
#endif

// Nombre del archivo donde se guarda el registro. Se crea en la misma
// carpeta desde donde corres ./servidor.
#define ARCHIVO_REGISTRO "conexiones.log"

// La clave del servidor. Se llena en main() con la variable de entorno
// SERVER_C_CLAVE (getenv): si no existe, queda "lab123", la de práctica,
// y el arranque avisa. Nunca se imprime la clave en pantalla ni se sube
// a GitHub: solo se compara con strcmp() en el LOGIN.
// Tercer candado, solo para la clave: la leen y la escriben varios hilos
// (LOGIN la lee, CLAVE la cambia), y sin candado podrían pisarse.
static char clave_servidor[128] = "lab123";
static pthread_mutex_t candado_clave = PTHREAD_MUTEX_INITIALIZER;

// Contador de clientes conectados ahora mismo y número del próximo cliente.
// Los tocan varios hilos a la vez, así que van protegidos con un candado
// (mutex): ver atender_cliente().
// PTHREAD_MUTEX_INITIALIZER lo deja listo sin llamar a ninguna función.
// Un mutex es como una puerta con una sola llave: solo el hilo que tiene
// la llave puede tocar lo protegido; los demás esperan su turno.
static int clientes_activos = 0;
static int proximo_numero = 0;
static pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;

// Segundo candado, solo para escribir en el archivo de registro.
static pthread_mutex_t candado_registro = PTHREAD_MUTEX_INITIALIZER;

// Esta cajita (struct) junta los datos de un cliente que llama: su socket,
// su IP y su puerto. El main la llena con accept() y se la pasa al hilo.
// struct es como una ficha con varios campos: cada campo tiene su nombre
// y su tipo, y se lee con punto, por ejemplo datos->socket.
struct Cliente {
    int socket;       // el "teléfono" solo para hablar con este cliente
    char ip[64];      // la IP desde donde llamó, por ejemplo "127.0.0.1"
    int puerto;       // el puerto desde donde llamó (lo elige su sistema)
};

// registrar() escribe UNA línea en conexiones.log, con la fecha y la hora.
// Ejemplo de línea: [2026-11-20 07:05:00] Cliente 1 conectado desde 127.0.0.1:54321 (activos: 1)
// strftime() convierte la hora a texto con el formato que le pidas:
// %Y año, %m mes, %d día, %H hora, %M minuto, %S segundo.
// El candado hace que solo un hilo escriba a la vez: sin él, dos hilos
// podrían escribir a la vez y sus líneas saldrían mezcladas.
void registrar(const char *evento) {
    pthread_mutex_lock(&candado_registro);

    // "a" = append: añadir al final del archivo, sin borrar lo anterior.
    // Si el archivo no existe, fopen lo crea vacío.
    FILE *f = fopen(ARCHIVO_REGISTRO, "a");
    if (f != NULL) {
        time_t ahora = time(NULL);
        struct tm *t = localtime(&ahora);  // localtime: la hora en tu zona horaria
        char fecha[32];
        strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M:%S", t);
        fprintf(f, "[%s] %s\n", fecha, evento);
        fclose(f);  // cerrar es lo que de verdad deja la línea guardada en el disco
    }
    // Si fopen falla (por ejemplo, sin permiso en la carpeta), no se rompe
    // nada: el servidor sigue atendiendo, solo no queda registro.

    pthread_mutex_unlock(&candado_registro);
}

// Esta función atiende UNA línea (un comando) y responde al cliente.
// Devuelve 1 si el cliente pidió SALIR, 0 si hay que seguir.
// Se separó en su propia función porque un solo recv() puede traer varias
// líneas juntas (por ejemplo, si se mandan por tubería o con un script),
// y cada línea es un comando distinto.
// mi_numero e ip son solo para el registro: para saber QUIÉN hizo el LOGIN.
int atender_linea(int cliente, char *mensaje, int *dentro, int mi_numero, const char *ip) {
    char respuesta[512];
    char evento[256];

    if (strcmp(mensaje, "AYUDA") == 0) {
        // strcmp compara dos textos: devuelve 0 cuando son iguales.
        snprintf(respuesta, sizeof(respuesta),
            "Comandos:\n"
            "  LOGIN <usuario> <clave> — entrar (usuario lab, clave en SERVER_C_CLAVE)\n"
            "  CLAVE <nueva-clave> — cambiar la clave (solo dentro, mínimo 4 caracteres)\n"
            "  ECO <texto> — repite tu texto (solo dentro)\n"
            "  HORA — la hora del servidor (solo dentro)\n"
            "  QUIEN — quien eres en este lab (solo dentro)\n"
            "  SALIR — colgar\n");
    } else if (strncmp(mensaje, "LOGIN ", 6) == 0) {
        // strncmp compara solo los primeros 6 caracteres: "LOGIN ".
        char usuario[64], clave[64];
        // Primero se lee usuario y clave del mensaje, y que el usuario sea
        // "lab". La clave se compara con candado: otro hilo podría estar
        // cambiándola justo en ese momento con CLAVE, y sin candado se
        // compararía una clave a la mitad (mezcla de la vieja y la nueva).
        int clave_ok = 0;
        if (sscanf(mensaje + 6, "%63s %63s", usuario, clave) == 2 &&
            strcmp(usuario, "lab") == 0) {
            pthread_mutex_lock(&candado_clave);
            clave_ok = (strcmp(clave, clave_servidor) == 0);
            pthread_mutex_unlock(&candado_clave);
        }
        if (clave_ok) {
            *dentro = 1;
            snprintf(respuesta, sizeof(respuesta), "OK: dentro. Prueba ECO, HORA y QUIEN.\n");
            // Registrar el LOGIN bueno: en un servicio real, esto es lo que
            // te dice quién entró de verdad y quién solo lo intentó.
            snprintf(evento, sizeof(evento),
                "Cliente %d (%s): LOGIN correcto (usuario %s)", mi_numero, ip, usuario);
            registrar(evento);
        } else {
            snprintf(respuesta, sizeof(respuesta), "MAL: usuario o clave incorrectos.\n");
            // Registrar también el LOGIN fallido: si alguien prueba claves
            // una y otra vez, aquí es donde se ve. %s con usuario puede
            // quedar vacío si solo escribió "LOGIN" sin nada más.
            snprintf(evento, sizeof(evento),
                "Cliente %d (%s): LOGIN fallido (usuario %s)", mi_numero, ip,
                (sscanf(mensaje + 6, "%63s", usuario) == 1) ? usuario : "?");
            registrar(evento);
        }
    } else if (strcmp(mensaje, "SALIR") == 0) {
        snprintf(respuesta, sizeof(respuesta), "Adios.\n");
        send(cliente, respuesta, strlen(respuesta), 0);
        return 1;  // 1 = colgar, salir del bucle del cliente
    } else if (!*dentro) {
        // Sin LOGIN, estos comandos no responden: así se ve un
        // servicio que pide autenticación antes de hacer nada.
        snprintf(respuesta, sizeof(respuesta), "Primero entra con LOGIN <usuario> <clave>.\n");
    } else if (strcmp(mensaje, "CLAVE") == 0) {
        // "CLAVE" sola, sin nada más: no entra en el strncmp de abajo
        // porque ese busca "CLAVE " con espacio. Aquí se le dice cómo se usa.
        snprintf(respuesta, sizeof(respuesta), "Uso: CLAVE <nueva-clave>\n");
    } else if (strncmp(mensaje, "CLAVE ", 6) == 0) {
        // CLAVE <nueva-clave>: cambiar la clave del servidor sin apagarlo.
        // Solo llega aquí quien ya hizo LOGIN (el !dentro de arriba lo
        // bloquea si no). %63s lee una sola palabra: la clave no puede
        // tener espacios, porque el espacio separa los comandos.
        char nueva[64];
        if (sscanf(mensaje + 6, "%63s", nueva) != 1) {
            snprintf(respuesta, sizeof(respuesta), "Uso: CLAVE <nueva-clave>\n");
        } else if (strlen(nueva) < 4) {
            snprintf(respuesta, sizeof(respuesta), "La clave debe tener al menos 4 caracteres.\n");
        } else {
            // Cambiarla con candado: otro hilo podría estar comparándola
            // en su LOGIN justo ahora. La nueva vale desde este momento
            // para todos, y la vieja deja de funcionar.
            pthread_mutex_lock(&candado_clave);
            strncpy(clave_servidor, nueva, sizeof(clave_servidor) - 1);
            clave_servidor[sizeof(clave_servidor) - 1] = '\0';
            pthread_mutex_unlock(&candado_clave);
            snprintf(respuesta, sizeof(respuesta),
                "OK: clave cambiada. La proxima vez entra con la nueva. Esta sesion sigue dentro.\n");
            // Se registra EL HECHO del cambio, nunca la clave nueva: el
            // registro lo puede leer cualquiera que entre a la máquina, y
            // una contraseña no se escribe en ningún log.
            snprintf(evento, sizeof(evento),
                "Cliente %d (%s): cambio de clave", mi_numero, ip);
            registrar(evento);
        }
    } else if (strncmp(mensaje, "ECO ", 4) == 0) {
        snprintf(respuesta, sizeof(respuesta), "Eco: %s\n", mensaje + 4);
    } else if (strcmp(mensaje, "HORA") == 0) {
        time_t ahora = time(NULL);  // time() da la hora actual
        snprintf(respuesta, sizeof(respuesta), "Hora: %s", ctime(&ahora));
    } else if (strcmp(mensaje, "QUIEN") == 0) {
        snprintf(respuesta, sizeof(respuesta), "Eres el usuario lab, en el laboratorio.\n");
    } else {
        snprintf(respuesta, sizeof(respuesta), "Comando desconocido. Escribe AYUDA.\n");
    }

    send(cliente, respuesta, strlen(respuesta), 0);
    return 0;
}

// Esta función es el trabajo de UN hilo: atiende a un solo cliente desde
// que se conecta hasta que se va (SALIR o colgar).
// Recibe un puntero al struct Cliente que creó el main. El hilo copia sus
// datos a variables locales y libera la cajita con free(): si pasáramos la
// dirección de una variable local del bucle, el siguiente accept() la
// sobrescribiría antes de que este hilo la leyera (una "carrera" entre
// hilos). Con malloc cada hilo tiene su propia copia.
void *atender_cliente(void *arg) {
    struct Cliente *datos = (struct Cliente *)arg;
    int cliente = datos->socket;
    char ip[64];
    strncpy(ip, datos->ip, sizeof(ip) - 1);  // copiar la IP a una variable local
    ip[sizeof(ip) - 1] = '\0';
    int puerto_cliente = datos->puerto;
    free(datos);  // ya no hace falta la cajita

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

    printf("Cliente %d conectado desde %s:%d (activos: %d).\n", mi_numero, ip, puerto_cliente, activos);
    fflush(stdout);

    // Registrar la conexión: la primera línea que deja cada cliente.
    char evento[256];
    snprintf(evento, sizeof(evento),
        "Cliente %d conectado desde %s:%d (activos: %d)", mi_numero, ip, puerto_cliente, activos);
    registrar(evento);

    // El banner es lo primero que ve quien se conecta, como en un servicio real.
    // Se arma con snprintf para que muestre la IP real donde escucha.
    char banner[256];
    snprintf(banner, sizeof(banner),
        "D4rkLab v6 — servidor de practica con hilos, registro y cambio de clave (IP %s)\nEscribe AYUDA para ver los comandos.\n",
        IP_SERVIDOR);
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
                salir = atender_linea(cliente, linea, &dentro, mi_numero, ip);
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
    // Registrar la desconexión, anotando si llegó a entrar o se fue sin LOGIN.
    snprintf(evento, sizeof(evento),
        "Cliente %d (%s:%d) desconectado (activos: %d, dentro: %s)",
        mi_numero, ip, puerto_cliente, activos, dentro ? "si" : "no");
    registrar(evento);
    return NULL;  // al volver, el hilo termina y, como es detached, se limpia solo
}

int main() {
    // 0. La clave: se lee de la variable de entorno SERVER_C_CLAVE.
    //    getenv() devuelve el texto de la variable, o NULL si no existe.
    //    Si no existe o está vacía, queda "lab123" y se avisa: en internet
    //    eso es regalar la entrada, así que no se corre así en el VPS.
    //    La clave no se imprime jamás: solo se dice si es la personalizada.
    const char *clave_env = getenv("SERVER_C_CLAVE");
    int clave_personalizada = 0;
    if (clave_env != NULL && clave_env[0] != '\0') {
        // strncpy copia con límite para no pasarse del tamaño del arreglo.
        strncpy(clave_servidor, clave_env, sizeof(clave_servidor) - 1);
        clave_servidor[sizeof(clave_servidor) - 1] = '\0';
        clave_personalizada = 1;
    }

    // 1. socket() crea el "teléfono": AF_INET = por internet, SOCK_STREAM = TCP.
    int servidor = socket(AF_INET, SOCK_STREAM, 0);

    // 2. La dirección donde va a escuchar: IP_SERVIDOR (por defecto la IP
    //    virtual 10.10.10.10), puerto 4444.
    struct sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(PUERTO);  // htons: el puerto en el orden de la red
    // inet_pton() convierte el texto "10.10.10.10" a su forma binaria, la que
    // entiende el socket. Devuelve 1 si el texto es una IP válida, 0 si no.
    // Es mejor que el viejo inet_addr(): ese no avisa cuando la IP está mal.
    if (inet_pton(AF_INET, IP_SERVIDOR, &direccion.sin_addr) != 1) {
        printf("La IP del servidor está mal escrita: %s\n", IP_SERVIDOR);
        return 1;
    }

    // SO_REUSEADDR: permite volver a usar el puerto 4444 enseguida después
    // de apagar el servidor, sin que diga "Address already in use".
    int reutilizar = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &reutilizar, sizeof(reutilizar));

    // 3. bind() "amarra" el teléfono a esa dirección y ese puerto.
    //    Si otro programa ya usa el 4444, bind falla y el servidor no arranca.
    //    Si la IP virtual no existe en tu máquina, bind también falla: ahí
    //    es cuando hay que crearla con el comando de la guía.
    if (bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) != 0) {
        printf("No pude usar %s:%d. ¿Ya hay otro servidor corriendo?\n", IP_SERVIDOR, PUERTO);
        printf("Si es la IP virtual, créala primero: sudo ip addr add %s/32 dev lo\n", IP_SERVIDOR);
        return 1;
    }

    // 4. listen() lo pone "a la escucha": ya puede recibir llamadas, 5 en espera.
    listen(servidor, 5);
    printf("Servidor de laboratorio v6 (hilos + registro + IP virtual + clave configurable y cambiable) escuchando en %s:%d\n", IP_SERVIDOR, PUERTO);
    if (clave_personalizada) {
        printf("Clave personalizada activada (SERVER_C_CLAVE). No la muestro por seguridad.\n");
    } else {
        printf("AVISO: usando la clave de practica lab123. En internet pon una fuerte: SERVER_C_CLAVE=\"tu-clave\" ./servidor\n");
    }
    printf("Atiende a varios clientes a la vez. Pruébalo con: nc %s %d\n", IP_SERVIDOR, PUERTO);
    printf("El registro se guarda en: %s\n", ARCHIVO_REGISTRO);
    printf("Pulsa Ctrl+C para apagarlo.\n");
    fflush(stdout);  // que se vea enseguida aunque la salida vaya a un archivo
    char arranque[128];
    snprintf(arranque, sizeof(arranque), "Servidor v6 arrancado en %s:%d", IP_SERVIDOR, PUERTO);
    registrar(arranque);

    // Bucle sin fin: por cada cliente que llama, se crea un hilo que lo
    // atiende y el main sigue esperando al siguiente. Antes (v1) el main
    // mismo atendía al cliente hasta que se iba, y mientras tanto nadie
    // más podía entrar: si un cliente conectaba y no hacía nada, el
    // servidor quedaba bloqueado para todos.
    while (1) {
        // 5. accept() espera a que alguien "llame" y contesta: devuelve un
        //    socket nuevo, solo para hablar con este cliente.
        //    Ahora accept() también llena cliente_addr con la IP y el puerto
        //    de quien llamó: por eso se le pasan la dirección y su tamaño.
        struct sockaddr_in cliente_addr;
        socklen_t largo_addr = sizeof(cliente_addr);
        int cliente = accept(servidor, (struct sockaddr *)&cliente_addr, &largo_addr);

        // La cajita con los datos del cliente, creada con malloc para que
        // cada hilo tenga su propia copia (ver atender_cliente).
        struct Cliente *datos = malloc(sizeof(struct Cliente));
        if (datos == NULL) {
            close(cliente);
            continue;
        }
        datos->socket = cliente;
        // inet_ntoa() convierte la IP a texto, pero devuelve un puntero a
        // un buffer compartido que la siguiente llamada sobrescribe: por
        // eso se copia enseguida con strncpy, antes de que otro hilo lo pise.
        // ntohs() convierte el puerto del orden de la red al de la máquina.
        strncpy(datos->ip, inet_ntoa(cliente_addr.sin_addr), sizeof(datos->ip) - 1);
        datos->ip[sizeof(datos->ip) - 1] = '\0';
        datos->puerto = ntohs(cliente_addr.sin_port);

        // pthread_create() arranca el hilo: le dice qué función corre
        // (atender_cliente) y con qué dato (datos). pthread_t es el
        // "nombre" del hilo, como un PID.
        pthread_t hilo;
        if (pthread_create(&hilo, NULL, atender_cliente, datos) != 0) {
            printf("No pude crear el hilo para el cliente.\n");
            free(datos);
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
