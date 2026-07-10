# Architecture Notes

Este documento describe la arquitectura publica de LatticeDTL para facilitar una
revision tecnica sin depender de contexto externo.

## Objetivos Del Modelo

LatticeDTL simula una red de liquidacion con:

- multiples activos;
- cuentas con dominios operativos;
- vaults por activo;
- matriz de obligaciones;
- cierre de epoch;
- retiradas posteriores;
- controles operativos por clase de riesgo;
- reportes JSON reproducibles.

El diseno prioriza determinismo, lectura sencilla y ausencia de servicios
externos. Cada escenario crea un estado nuevo desde fixtures locales.

## Entidades

### Asset

Un activo define:

- simbolo;
- dominio de liquidez;
- precision decimal;
- tipo de activo;
- peso de riesgo;
- floor de reserva;
- estado de retirada;
- marca de lane primaria.

El registro de activos es inmutable durante un escenario. Esto mantiene los
digests estables y evita que un test dependa del orden de mutaciones externas.

### Account

Una cuenta define:

- identificador numerico;
- etiqueta legible;
- dominio operativo;
- clase de riesgo;
- marca de cuenta de sistema;
- nonce de actividad.

Los balances viven en el ledger como una matriz cuenta-activo. El nonce cambia
cuando la cuenta participa en una transferencia o retirada.

### Vault

Cada activo tiene una vault logica con:

- reserva disponible;
- liability total;
- outflow reservado;
- floor minimo.

Los depositos incrementan balance, reserve y liability. Las transferencias
internas solo mueven balances entre cuentas. Las retiradas reducen balance,
reserve y liability.

### Debt Cell

Una celda de deuda contiene:

- deudor;
- acreedor;
- activo;
- dominio;
- importe;
- prioridad;
- memo operativo.

Las celdas no mutan el ledger hasta que se cierra el epoch. Esto permite
prechecks agregados antes de aplicar transferencias.

### Policy Rule

Una regla de policy define:

- clase de riesgo;
- exposicion maxima;
- retirada maxima por solicitud;
- peso maximo de activo permitido;
- bypass para cuentas de sistema.

El reporte de policy no sustituye a la conservacion contable. Sirve para
inspeccionar si una cuenta queda dentro de los limites operativos configurados.

## Pipeline De Liquidacion

El cierre de epoch sigue esta secuencia:

1. La matriz recibe celdas pendientes.
2. El motor reduce filas exactas por par de cuentas, activo y dominio.
3. El motor compacta microposiciones bajo el umbral configurado.
4. La matriz emite celdas materializadas.
5. El epoch calcula debitos requeridos por cuenta y activo.
6. El ledger verifica que cada deudor tenga saldo disponible.
7. Las transferencias se aplican de forma secuencial.
8. El reporte registra digests y volumen por activo.

Si un precheck falla, el epoch no aplica transferencias. Los escenarios publicos
estan configurados para rutas exitosas.

## Reconciliacion

El modulo `reconciliation` calcula un resumen por activo:

- suma de balances de cuentas;
- reserve de vault;
- liability de vault;
- diferencia reserve-liability;
- diferencia balance-liability;
- numero de cuentas activas;
- estado de consistencia.

El campo `liabilityGap` debe ser cero para cada activo en escenarios normales.
El campo `reserveGap` muestra exceso de liquidez operativa sobre liabilities.

## Escenarios

### baseline

Representa un epoch multi-activo con market makers, bridge y cliente. Incluye
netting exacto para USDC y transferencias separadas para EURC y MXNT.

### compact

Representa microposiciones reciprocas del mismo activo. Sirve para validar que
el motor reduce ruido operacional sin cambiar el resultado economico esperado.

### domains

Representa posiciones mayores en activos distintos dentro de la configuracion
del fixture. Sirve para observar materializacion separada por lane economica.

### withdrawal

Ejecuta un epoch vacio y despues una retirada de USDC desde `client_alpha`.
Valida que el balance y la vault se decrementan conjuntamente.

### batch

Combina netting exacto, microposiciones y una retirada posterior de `south_mm`.
Es el escenario mas parecido a una ejecucion diaria de operaciones.

### snapshot

No aplica transferencias. Devuelve metadata, balances iniciales y digests del
fixture.

### policy

No aplica transferencias. Evalua el ledger inicial contra reglas operativas y
registra un precheck de retirada que excede el limite de una cuenta de cliente.

## Convenciones De Importes

Los importes se expresan en unidades minimas. Para activos de 6 decimales:

```text
1 unidad completa = 1_000_000 unidades minimas
```

Para activos sin decimales:

```text
1 unidad completa = 1 unidad minima
```

Los tests usan importes pequenos para que las aserciones sean legibles y no
dependan de precision de coma flotante en JavaScript.

## Lectura Del JSON

Campos principales:

- `assets`: catalogo de activos;
- `accounts`: cuentas creadas;
- `balances`: balances no nulos;
- `vaults`: reserve y liability por activo;
- `reconciliation`: comprobacion agregada por activo;
- `policy`: reglas, exposicion por cuenta y ultimo precheck;
- `matrix`: estadisticas de matriz y celdas materializadas;
- `epoch`: resultado del cierre;
- `withdrawals`: solicitudes ejecutadas o rechazadas;
- `ledgerDigest`: digest final del estado contable.

El reporte no pretende ser una API estable. Es una interfaz de auditoria para
comparar escenarios y escribir tests de regresion.

## Invariantes Publicos

Los escenarios normales esperan:

- `ok == true`;
- `conservation == true`;
- `reconciliation.ok == true`;
- cada `liabilityGap == 0`;
- `policy.flaggedAccounts == 0` en fixtures normales;
- ninguna vault bajo su `reserveFloor`;
- importes enteros sin coma flotante;
- JSON parseable por Node.js.

## Extension Del Lab

Para agregar un escenario nuevo:

1. Crear una funcion `add_*_debts` o un runner dedicado en `runtime.c`.
2. Insertar celdas mediante `ldtl_matrix_add_debt`.
3. Ejecutar `ldtl_epoch_settle`.
4. Imprimir con `print_report`.
5. Agregar una prueba Node que use `runScenario`.

Mantener los escenarios deterministas: no usar tiempo del sistema, aleatoriedad,
red ni estado persistente entre ejecuciones.
