# Server-C v4 — IP virtual (10.10.10.10)

Antes el servidor escuchaba en 127.0.0.1, que es tu máquina hablando
consigo misma, y atacar ahí se siente falso. Ahora escucha en
**10.10.10.10**, una IP que tú le inventas a tu propia máquina. nmap y nc
la ven como si fuera otro equipo, pero el tráfico nunca sale de tu laptop:
sigue siendo solo tu laboratorio, nadie de fuera puede entrar.

## 1. Crear la IP virtual (una vez por arranque)

En tu Arch (o Kali), en una terminal:

```bash
sudo ip addr add 10.10.10.10/32 dev lo
ip addr show lo
```

En la salida debes ver `10.10.10.10/32` junto a `127.0.0.1`. Para quitarla:

```bash
sudo ip addr del 10.10.10.10/32 dev lo
```

La IP se borra al apagar la máquina: si reinicias, hay que crearla otra
vez antes de correr el servidor. Si el servidor no arranca y dice que no
pudo usar 10.10.10.10, es que te faltó este paso.

## 2. Correr el servidor

```bash
cd ~/Server-C
# (cuando Tweeky lo suba a GitHub, primero: git pull)
gcc -Wall -Wextra -pthread -o servidor servidor.c
./servidor
```

Debe decir `escuchando en 10.10.10.10:4444`.

## 3. Atacarlo como si fuera otro equipo

En otra terminal, todo contra 10.10.10.10:

```bash
nmap -p 4444 10.10.10.10
nmap -sV -p 4444 10.10.10.10
nc 10.10.10.10 4444
```

Con `nc` entras con `LOGIN lab lab123` como siempre, y luego mira el
registro: `cat conexiones.log`. Ahí queda la conexión al servidor en
10.10.10.10, con su LOGIN y su desconexión, como si fuera otro equipo.

## ¿Y si quiero que sea todavía más real?

- **Tu IP del Wi-Fi** (la tuya ahora es 192.168.0.108, pero cambia): ponla
  en el `#define IP_SERVIDOR` del código y recompila. Ahí el servidor lo
  ve cualquiera de tu casa en el mismo Wi-Fi, y lo puedes atacar hasta
  desde el celular con una app de terminal. Sigue siendo solo tu red.
- **`0.0.0.0`** en el `#define`: escucha en todas las interfaces a la vez.
  Solo en tu casa y solo para el lab: la clave del servidor es lab/lab123,
  cualquiera de tu red podría entrar. Nunca abras el puerto 4444 en el
  router hacia internet.

## Qué cambió en el código respecto a la v3

- **`#define IP_SERVIDOR "10.10.10.10"`** arriba del todo: la IP donde
  escucha, en un solo lugar. Con `#ifndef` puedes probar otra sin tocar
  el archivo: `gcc -Wall -Wextra -pthread -DIP_SERVIDOR='"127.0.0.2"' -o servidor servidor.c`.
- **`inet_pton()`** en vez de `inet_addr()`: convierte el texto de la IP a
  su forma binaria y avisa si el texto está mal escrito (el viejo no
  avisaba). Las siglas son "presentation to network": de texto a red.
- El banner y los mensajes ahora muestran la IP real con `snprintf`, y si
  `bind()` falla te dice que crees la IP virtual con el comando.
- Todo lo demás sigue igual: hilos (v2), registro en `conexiones.log` (v3),
  LOGIN lab/lab123, puerto 4444.

## Pruebas hechas aquí (2026-11-20)

- Compila limpio con `gcc -Wall -Wextra -pthread`, sin avisos.
- Sin la IP virtual creada: el servidor no arranca y avisa con el comando
  para crearla (no se queda colgado ni da un error raro).
- Con otra IP de prueba (`-DIP_SERVIDOR='"127.0.0.2"'`): arranca, atiende
  con `nc`, LOGIN correcto, ECO responde, y el log registra todo.
- El comando `ip addr add` en sí no se pudo probar en este entorno porque
  no deja crear IPs virtuales (falta permiso), pero en tu Arch con `sudo`
  sí funciona: es el comando estándar para esto. Si al correrlo te dice
  algo raro, mándame lo que salió y lo vemos.

## Estado

- Esta v4 está lista en local, **NO está subida a GitHub todavía**.
  Se sube cuando Luis diga que sí, como se hizo con la v2 y la v3.
- Todo sigue siendo solo local: la IP virtual vive solo en tu máquina.
