# Server-C v5 — en un VPS gratis de Oracle con IP pública

Esta versión es para correr el servidor en internet, en un VPS gratis de
Oracle Cloud (Always Free, sin fecha de caducidad). Tu laptop queda fuera:
el servidor vive en el VPS y tú lo escaneas desde tu casa, como un
objetivo real. Lo nuevo de la v5 es la **clave configurable**: ya no es
lab123 fija, se pone con la variable de entorno `SERVER_C_CLAVE`.

## 0. Qué necesitas para la cuenta

- Un correo que controles y tu número de celular.
- **Una tarjeta de crédito o débito** (de crédito, no prepagada ni virtual):
  Oracle la pide para verificar quién eres. Hace una retención temporal
  de ~$1 que el banco quita en unos días; **no es un cobro**, y no te
  cobran nada mientras solo uses recursos Always Free.
- Si no tienes tarjeta, no se puede crear la cuenta de Oracle: en ese
  caso dime y vemos el plan B (ngrok, sin tarjeta).

## 1. Crear la cuenta (lo haces tú en tu navegador)

1. Entra a https://www.oracle.com/cloud/free/ y pulsa **Sign up for free**.
2. Pon tu correo, tus datos y tu teléfono, y la tarjeta para verificar.
3. Elige tu **región principal (home region)**: **Mexico Central
   (Querétaro)**. Ojo, no se puede cambiar después para los recursos
   gratis, así que no la dejes en otra.
4. Al entrar te dan también $300 de crédito por 30 días: no los necesitas,
   quédate solo con lo Always Free.

## 2. Crear la máquina virtual (VM)

En la consola de Oracle (cloud.oracle.com): **Compute → Instances →
Create Instance**.

- **Nombre:** `server-c` (o el que quieras).
- **Imagen:** Ubuntu 24.04.
- **Shape (forma):** pulsa *Change Shape* y prueba primero **Ampere A1
  Flex (VM.Standard.A1.Flex)** con **1 OCPU y 6 GB** de RAM. Es ARM, pero
  Server-C es C puro y compila igual con gcc. Si te dice *out of host
  capacity* (sin capacidad), no es tu culpa: pasa mucho. Entonces elige
  **VM.Standard.E2.1.Micro** (AMD, 1/8 OCPU, 1 GB): casi siempre hay
  lugar y para Server-C sobra.
- **Disco:** 50 GB (dentro de lo gratis, que son 200 GB en total).
- **Clave SSH:** deja que Oracle genere el par de claves y **descarga la
  clave privada** (un archivo `.pem`). Guárdala bien: sin ella no entras.
  En tu Arch, dale permisos con `chmod 400 llave.pem`.
- **IP pública:** viene asignada por defecto. Anótala: es la IP que vas a
  escanear, por ejemplo `129.146.x.x`.

## 3. Abrir el puerto 4444

El VPS tiene dos puertas: la de Oracle y la de la máquina.

**En Oracle (lista de seguridad):** en la consola, ve a la red virtual
(VCN) de tu instancia → *Security Lists* → la lista por defecto → *Add
Ingress Rule*: origen `0.0.0.0/0`, protocolo TCP, puerto destino `4444`.

**En la máquina:** entra por SSH y revisa el cortafuegos:

```bash
ssh -i llave.pem ubuntu@TU_IP_PUBLICA
sudo ufw status
```

Si dice que está activo, abre el puerto:

```bash
sudo ufw allow 4444/tcp
```

## 4. Instalar y correr Server-C en el VPS

Dentro del VPS por SSH:

```bash
sudo apt update && sudo apt install -y git gcc nmap
git clone https://github.com/Am0nD4rkM4/Server-C.git
cd Server-C
gcc -Wall -Wextra -pthread -DIP_SERVIDOR='"0.0.0.0"' -o servidor servidor.c
```

El `-DIP_SERVIDOR='"0.0.0.0"'` es lo que hace que escuche en la IP
pública del VPS (0.0.0.0 = todas sus interfaces). Sin eso, seguiría en
la IP virtual de tu casa y no se vería desde fuera.

Para correrlo con tu clave fuerte (invéntala tú, larga y que solo tú
sepas), en segundo plano para que siga aunque cierres la terminal:

```bash
SERVER_C_CLAVE="tu-clave-fuerte-aqui" nohup ./servidor > servidor.log 2>&1 &
cat servidor.log
```

Debe decir `escuchando en 0.0.0.0:4444` y `Clave personalizada
activada`. Si dice el AVISO de lab123, es que la variable no se aplicó:
apágalo y vuelve a correrlo con la variable delante.

## 5. Escanearlo desde tu casa

Desde tu Arch, contra la IP pública del VPS:

```bash
nmap -p 4444 TU_IP_PUBLICA
nmap -sV -p 4444 TU_IP_PUBLICA
nc TU_IP_PUBLICA 4444
```

Con `nc` entras con `LOGIN lab tu-clave-fuerte`, y en el VPS mira quién
te visitó: `cat conexiones.log`. También puedes correr tu escáner en
Python contra esa IP, y el InfoForge contra ella (solo tu VPS, que es
tuyo).

## 6. Cuando termines de practicar

- **Apaga el servidor** en el VPS (`kill` al proceso o `pkill servidor`).
- **Cierra el puerto 4444** en la lista de seguridad de Oracle, o borra
  la VM si ya no la vas a usar. Un puerto abierto con un servicio
  corriendo es una puerta que cualquiera puede tocar.
- **No subas tu clave a GitHub** ni la pegues en ningún archivo del repo:
  solo vive en el comando con el que corres el servidor.

## Avisos que valen oro

- **Oracle puede apagar la VM si está 7 días casi sin uso** (CPU, red y
  memoria por debajo del 20%): si la dejas parada una semana, puede
  reclamarla. Entra de vez en cuando y úsala, o vuelve a crearla.
- La IP pública del VPS es visible para todo internet: cualquiera puede
  intentar conectarse al 4444. Por eso la clave fuerte es obligatoria y
  por eso el servidor no ejecuta comandos ni abre archivos.
- El VPS es ARM si elegiste A1: Server-C, nmap, git y gcc funcionan
  igual ahí, porque son C puro. Si algo te dice que no hay versión para
  ARM, usa la Micro (AMD) en vez de la A1.

## Qué cambió en el código respecto a la v4

- **Clave por variable de entorno:** `getenv("SERVER_C_CLAVE")` la lee
  en `main()` y la guarda en `clave_servidor`. El LOGIN compara con esa
  variable, ya no con lab123 escrito en el código.
- Si no hay variable, usa lab123 y **avisa en el arranque** con un AVISO
  bien visible. La clave nunca se imprime en pantalla.
- El texto de AYUDA ya no promete lab123: dice que la clave es la de
  `SERVER_C_CLAVE`.
- Todo lo demás sigue igual: hilos (v2), registro en `conexiones.log`
  (v3), IP configurable (v4), puerto 4444.

## Pruebas hechas aquí (2026-11-20)

- Compila limpio con `gcc -Wall -Wextra -pthread`, sin avisos.
- Con `SERVER_C_CLAVE="Fuerte-99"`: LOGIN con Fuerte-99 entra, con
  lab123 da MAL. Sin la variable: avisa y lab123 funciona.
- Con `-DIP_SERVIDOR='"0.0.0.0"'`: arranca en 0.0.0.0:4444 y se conecta
  por 127.0.0.1, LOGIN y ECO funcionan, el log registra.
- El VPS en sí no se probó aquí (la cuenta de Oracle la creas tú): los
  pasos de esta guía están verificados contra la documentación de Oracle
  del 20-nov-2026 (Always Free: A1 2 OCPU/12 GB, Micro x86, región
  Querétaro, tarjeta para verificar).

## Estado

- Esta v5 está lista en local, **NO está subida a GitHub todavía**.
  Se sube cuando Luis diga que sí, como se hizo con las anteriores.
- Cuando el VPS esté creado, el siguiente paso es subirla y clonarla ahí.
