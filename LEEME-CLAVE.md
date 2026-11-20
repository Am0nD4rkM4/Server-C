# Server-C v6 — cambiar la clave sin apagar el servidor

En la v5 la clave se ponía con `SERVER_C_CLAVE` al arrancar, pero si no te
gustaba tenías que apagar el servidor y volverlo a correr. Ahora hay un
comando para cambiarla desde dentro, como en un servicio real: entras con
la clave que tengas y la cambias por una nueva, sin apagar nada.

## Cómo se cambia

Conéctate y entra primero, luego cambia la clave:

```
LOGIN lab tu-clave-actual
CLAVE mi-clave-nueva-larga
```

El servidor responde `OK: clave cambiada`. Desde ese momento la vieja ya
no sirve y la nueva es la que vale para todos. Tu sesión sigue dentro, no
te saca. Para comprobarlo, sal con `SALIR` y vuelve a entrar: la vieja da
MAL y la nueva entra.

Reglas del comando:

- Solo funciona **después del LOGIN**. Sin entrar, responde lo mismo que
  los demás comandos: `Primero entra con LOGIN`.
- La clave nueva debe tener **al menos 4 caracteres** y no puede tener
  espacios (el espacio separa los comandos).
- **El cambio dura mientras el servidor esté corriendo.** Si lo apagas y
  lo vuelves a correr, vuelve a valer la de `SERVER_C_CLAVE` (o lab123 si
  no pusiste ninguna). Para que tu nueva clave quede fija, apaga el
  servidor y vuélvelo a correr con:
  `SERVER_C_CLAVE="mi-clave-nueva-larga" ./servidor`
- En el registro (`conexiones.log`) queda anotado que alguien cambió la
  clave, pero **nunca la clave nueva**: una contraseña no se escribe en
  ningún log, porque el log lo puede leer cualquiera que entre a la
  máquina.

## Por qué esto da más confianza

- La clave ya no está escrita en el código en ningún lado: vive solo en
  la memoria del servidor.
- Si sospechas que alguien la vio, la cambias en el momento, sin apagar
  nada y sin tocar ningún archivo.
- Si aun así no te convence una clave que inventaste tú, hazla larga y
  mézclala: por ejemplo, tres palabras con números, como
  `Torre-Verde-77-Cafe`. Lo que la hace fuerte es el largo, no los
  símbolos raros.

## Qué cambió en el código respecto a la v5

- **Comando `CLAVE <nueva-clave>`** en `atender_linea()`, después del
  bloqueo de `!dentro`, así que exige LOGIN como ECO, HORA y QUIEN.
- **Tercer candado (`candado_clave`)** solo para la clave: la leen y la
  escriben varios hilos (LOGIN la lee, CLAVE la cambia). Sin candado, un
  hilo podría comparar la clave mientras otro la está cambiando y salir
  una mezcla de la vieja y la nueva.
- El LOGIN ahora compara la clave con el candado puesto, y el cambio
  también se hace con el candado puesto.
- El texto de AYUDA ya lista el comando CLAVE.
- Todo lo demás sigue igual: hilos (v2), registro (v3), IP configurable
  (v4), clave por variable de entorno (v5), puerto 4444.

## Pruebas hechas aquí (2026-11-20)

- Compila limpio con `gcc -Wall -Wextra -pthread`, sin avisos.
- Con `SERVER_C_CLAVE="Fuerte-99"`: LOGIN entra, `CLAVE Nueva-123`
  responde OK, SALIR, LOGIN con Fuerte-99 da MAL, LOGIN con Nueva-123
  entra. La sesión que cambió la clave sigue dentro.
- Sin LOGIN, `CLAVE algo` responde que primero hay que entrar.
- `CLAVE abc` (3 caracteres) se rechaza; `CLAVE` sin nada dice cómo se usa.
- Dos clientes a la vez: uno cambia la clave y el otro tiene que entrar
  con la nueva; la vieja ya no le sirve a nadie.
- En `conexiones.log` queda `cambio de clave`, sin aparecer la clave nueva
  en ninguna línea.

## Estado

- Esta v6 está lista en local y **se sube a GitHub cuando Luis diga que
  sí**, como se hizo con las anteriores (incluye todo lo de la v5).
- Cuando el VPS de Oracle esté creado, ahí se clona y se corre con
  `-DIP_SERVIDOR='"0.0.0.0"'` y su `SERVER_C_CLAVE`.
