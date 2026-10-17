# Integración del CLI

## Contrato de proceso

```bash
./build/lattice-dtl <scenario>
```

- salida correcta: JSON por stdout y código 0;
- escenario desconocido o error: diagnóstico por stderr y código no cero;
- cada proceso crea un fixture nuevo y determinista.

## Campos estables

Los consumidores deben validar al menos:

- `protocol` y `scenario`;
- `ok`, `conservation` y `reconciliation.ok`;
- digests de registry y ledger;
- `epoch.status` y contadores;
- filas de `treasuryStress.assets` por `assetId`.

Los campos nuevos deben tratarse como extensiones compatibles. No dependa del
orden de propiedades JSON.

## Node.js

```javascript
import { execFileSync } from "node:child_process";

const output = execFileSync("./build/lattice-dtl", ["baseline"], {
    encoding: "utf8",
});
const report = JSON.parse(output);

if (report.protocol !== "LatticeDTL") {
    throw new Error("unexpected protocol");
}
```

## Build portable

`scripts/build-lib.mjs` selecciona `CC`, `cc`, `gcc`, `clang`, MSVC o GCC bajo
WSL. El helper mantiene un lock de build para evitar compilaciones concurrentes
sobre el mismo artefacto.

## Versionado

La versión del contrato acompaña la versión npm privada. Cambios incompatibles
requieren una versión mayor y una nota de migración en `CHANGELOG.md`.
