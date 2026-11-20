# Server-C — Servidor de laboratorio en C (LEEME)

Tu primer proyecto en C en Arch (o en Kali): un servidor chiquito, solo para tu
propia máquina, para practicar ataques en tu laboratorio.

## Qué es, en palabras llanas

Tu escáner en C (`escaner.c`) es el que **llama**: crea un socket y hace
`connect()` a un puerto. Este servidor es el que **contesta**: crea un socket,
lo amarra a un puerto con `bind()`, se pone a escuchar con `listen()` y
contesta cada llamada con `accept()`.

Los 5 pasos, en orden:

1. `socket()` — crea el "teléfono".
2. `bind()` — le pone número: dirección `127.0.0.1` y puerto `4444`.
3. `listen()` — lo deja sonando, esperando llamadas.
4. `accept()` — contesta una llamada y abre una línea solo con ese cliente.
5. `recv()` / `send()` — leer lo que manda el cliente y responderle.

`127.0.0.1` significa "yo mismo": el servidor solo escucha en tu máquina,
nadie de fuera puede conectarse. No ejecuta comandos del sistema ni abre
archivos, solo responde texto.

## Instalar lo necesario

En Arch (cuando ya arranque, en una terminal):

```bash
sudo pacman -S gcc openbsd-netcat nmap
```

En Kali, si lo pruebas ahí:

```bash
sudo apt install gcc netcat-openbsd nmap
```

## Compilar y correr

```bash
gcc -Wall -Wextra -pthread -o servidor servidor.c
./servidor
```

El `-pthread` es obligatorio en la v2: es el que le dice al compilador que
el programa usa hilos (sin él, `pthread_create` no se enlaza y no compila).
Debe salir: `Servidor de laboratorio v2 (hilos) escuchando en 127.0.0.1:4444`.
Déjalo corriendo y abre **otra terminal** para probarlo. Se apaga con Ctrl+C.

## Probarlo con nc (el cliente de juguete)

```bash
nc 127.0.0.1 4444
```

Ahí dentro escribe, línea por línea:

```text
AYUDA
ECO hola lab
LOGIN lab mal
LOGIN lab lab123
ECO hola lab
HORA
QUIEN
SALIR
```

Fíjate en esto, que es la lección: `ECO` antes del LOGIN te dice que primero
entres; después del LOGIN sí responde. Así se ve un servicio que pide
autenticación.

## Probarlo con tus propias herramientas

```bash
nmap -p 4444 127.0.0.1
```

Debe salir el puerto `4444/tcp open`. Si el servidor está apagado, sale
`closed`: es el mismo `ABIERTO / cerrado` de tu `escaner.c`, pero al revés,
ahora tú eres el servicio.

También puedes ampliar tu `escaner.c` para que pruebe el 4444, o usar el
módulo `recon/banner` de D4rkSploit contra `127.0.0.1:4444` y ver el banner
`D4rkLab v2`.

## Qué cambió en la v2 (hilos)

En la v1 el `main` atendía a un cliente hasta que se iba: si conectabas con
`nc` y no escribías nada, nadie más podía entrar. Ahora por cada `accept()`
el `main` crea un hilo (`pthread_create`) que atiende a ese cliente y sigue
esperando al siguiente. Cada hilo tiene sus propias variables locales, así
que cada cliente tiene su propio LOGIN.

Dos detalles que valen oro y que se ven en el código:

- **La copia con `malloc`:** el socket se pasa al hilo en un `int` creado
  con `malloc`, no con la dirección de una variable del bucle. Si no, el
  siguiente `accept()` la sobrescribiría antes de que el hilo la leyera.
- **El candado (mutex):** el contador de clientes activos lo tocan varios
  hilos a la vez, así que va entre `pthread_mutex_lock` y
  `pthread_mutex_unlock`. Sin candado, dos hilos podrían leer el mismo
  número y el conteo saldría mal: eso es una "condición de carrera".
- **`pthread_detach`:** el hilo se limpia solo al terminar. Sin esto el
  `main` tendría que esperar a cada hilo con `pthread_join`, y volveríamos
  a bloquearnos.

## Probar los hilos: dos clientes a la vez

La prueba que demuestra que funciona: conecta un cliente y **déjalo callado**,
luego conecta otro y úsalo. En la v1 el segundo no respondía.

Terminal 1 (se queda callada, solo conectada):

```bash
nc 127.0.0.1 4444
```

Terminal 2 (esta sí trabaja, con la 1 todavía conectada):

```bash
nc 127.0.0.1 4444
LOGIN lab lab123
ECO hola desde el cliente 2
```

Si el cliente 2 responde, los hilos funcionan. En la terminal del servidor
debe salir algo como `Cliente 1 conectado (activos: 1)` y
`Cliente 2 conectado (activos: 2)`. Luego vuelve a la terminal 1, haz LOGIN
y ECO: también responde, cada uno con su propia sesión.

## Prácticas de laboratorio (solo en 127.0.0.1)

1. **Sin login:** conecta con `nc` y prueba `ECO`, `HORA` y `QUIEN` antes de
   entrar. Anota qué responde y por qué.
2. **Login a mano:** prueba claves incorrectas y luego `lab lab123`.
3. **Fuerza bruta de juguete:** escribe un script en Python que pruebe una
   lista corta de claves contra el LOGIN y anote cuál funciona. Solo contra
   tu servidor local, nunca contra nada de fuera.
4. **Banner:** conecta con `nc` sin escribir nada y copia el banner. Eso es
   lo que ve `nmap -sV` de un servicio real.

## Usuario y clave de práctica

- Usuario: `lab`
- Clave: `lab123`

No son reales, solo existen dentro de este servidor de práctica. Si quieres
cambiarlas, están en el `strcmp` del `LOGIN` en `servidor.c`.

## Qué sigue

Los hilos ya quedaron (v2). El siguiente paso es que el servidor guarde un
registro de quién se conectó, a qué hora y qué hizo. Dime cuando quieras y
lo hacemos paso a paso, como en Python.
