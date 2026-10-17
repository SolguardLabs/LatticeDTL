# Contribución

## Flujo

1. Cree una rama desde `main`.
2. Mantenga cada cambio limitado a una responsabilidad.
3. Añada contratos de escenario y documentación económica.
4. Ejecute `npm ci && npm run ci`.
5. Abra un pull request con impacto, supuestos y evidencia.

## Código C

- C11 portable, sin extensiones específicas del compilador.
- Warnings tratados como errores.
- Aritmética comprobada antes de mutar estado.
- Tipos y prefijos `Ldtl` / `ldtl_` para la API pública.
- Sin asignación dinámica en rutas económicas.

## Contratos

Una transición nueva debe cubrir éxito, límite y error. Si modifica balances,
debe aportar reconciliación por activo y confirmar que el fallo no deja un
estado parcial.

Los cambios de parámetros de riesgo deben documentar unidades, dirección de
redondeo y efecto en los escenarios operativo y severo.
