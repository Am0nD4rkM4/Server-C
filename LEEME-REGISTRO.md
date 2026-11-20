# Server-C v3 — registro de conexiones

El siguiente paso después de los hilos: el servidor ahora guarda en
`conexiones.log` quién se conectó, desde qué IP y puerto, si entró con
LOGIN o falló, y cuándo se fue.

## Qué cambió respecto a la v2

- **Nueva función `registrar()`**: escribe una línea en `conexiones.log`
  con fecha y hora. Abre el archivo en modo `"a"` (append: añade al final
  sin borrar lo anterior) y lo cierra en cada evento, así lo escrito no se
  pierde si el servidor se apaga de golpe con Ctrl+C.
- **Nuevo `struct Cliente`**: junta el socket, la IP y el puerto de quien
  llama en una sola cajita. El main la crea con `malloc` y el hilo la
  libera con `free`, igual que se hacía con el socket solo en la v2.
- **`accept()` ahora recoge la dirección de quien llama**: antes se le
  pasaba `NULL, NULL`; ahora llena un `sockaddr_in` con su IP y su puerto.
  La IP se copia enseguida con `strncpy` porque `inet_ntoa()` devuelve un
  buffer compartido que la siguiente llamada sobrescribe.
- **Segundo candado (`candado_registro`)**: solo para escribir en el
  archivo. Sin él, dos hilos escribiendo a la vez mezclarían sus líneas.
- Se registran 4 cosas: arranque del servidor, conexión, LOGIN correcto
  y LOGIN fallido, y desconexión (anotando si llegó a entrar o no).

## Para probarlo en tu Arch

```bash
cd ~/Server-C
# (cuando Tweeky lo suba a GitHub, primero: git pull)
gcc -Wall -Wextra -pthread -o servidor servidor.c
./servidor
```

En otra terminal, conéctate como siempre (`nc 127.0.0.1 4444`,
`LOGIN lab lab123`, `ECO hola`, `SALIR`), y luego mira el registro:

```bash
cat conexiones.log
```

Ejemplo de lo que sale (con dos clientes a la vez):

```
[2026-11-20 13:08:06] Servidor v3 arrancado en 127.0.0.1:4444
[2026-11-20 13:08:07] Cliente 1 conectado desde 127.0.0.1:44450 (activos: 1)
[2026-11-20 13:08:07] Cliente 2 conectado desde 127.0.0.1:44466 (activos: 2)
[2026-11-20 13:08:07] Cliente 1 (127.0.0.1): LOGIN correcto (usuario lab)
[2026-11-20 13:08:07] Cliente 2 (127.0.0.1): LOGIN correcto (usuario lab)
[2026-11-20 13:08:08] Cliente 1 (127.0.0.1:44450) desconectado (activos: 1, dentro: si)
[2026-11-20 13:08:08] Cliente 2 (127.0.0.1:44466) desconectado (activos: 0, dentro: si)
```

Si un cliente prueba una clave mala, queda así:

```
[...] Cliente 2 (127.0.0.1): LOGIN fallido (usuario lab)
[...] Cliente 2 (127.0.0.1:60812) desconectado (activos: 0, dentro: no)
```

## Pruebas hechas aquí (2026-11-20)

- Compila limpio con `gcc -Wall -Wextra -pthread`, sin avisos.
- Cliente con LOGIN bueno: responde ECO y queda `LOGIN correcto` en el log.
- Cliente con clave mala: responde MAL, ECO bloqueado, y queda
  `LOGIN fallido` y `dentro: no` en el log.
- Dos clientes a la vez: los dos entran, `activos: 1` y `activos: 2`,
  y las 7 líneas del log salen completas, sin mezclarse.

## Estado

- Esta v3 está lista en local, **NO está subida a GitHub todavía**.
  Se sube cuando Luis diga que sí, como se hizo con la v2.
- Todo sigue siendo solo local: escucha solo en 127.0.0.1:4444.
