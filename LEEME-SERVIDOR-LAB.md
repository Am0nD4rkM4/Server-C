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
gcc -Wall -o servidor servidor.c
./servidor
```

Debe salir: `Servidor de laboratorio escuchando en 127.0.0.1:4444`.
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
`D4rkLab v1`.

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

Cuando este te quede claro, el siguiente paso es que el servidor atienda a
varios clientes a la vez con hilos (como el `ThreadPoolExecutor` de tu Mini
InfoForge v2), o que guarde un registro de quién se conectó. Dime cuál
quieres y lo hacemos paso a paso, como en Python.
