# Security Policy

LatticeDTL es un laboratorio autocontenido. No procesa fondos reales, claves
reales, firmas de produccion ni conexiones a servicios externos.

## Alcance

Estan dentro de alcance:

- logica de registro de activos;
- balances y liabilities del ledger;
- matriz de obligaciones;
- cierre de epoch;
- solicitudes de retirada;
- serializacion JSON de escenarios;
- scripts de build y test.

Estan fuera de alcance:

- despliegues en red;
- integraciones con custodios reales;
- gestion de secretos;
- hardening operativo de servidores;
- disponibilidad de servicios externos.

## Supuestos

- Los fixtures son deterministas y locales.
- Las cuentas y activos se crean durante el arranque de cada escenario.
- Los importes se manejan en unidades minimas.
- Los escenarios CLI son una interfaz de auditoria, no una API publica de
  produccion.
- La conservacion contable se evalua por activo comparando balances agregados
  con liability de vault.

## Ejecucion Segura

Para ejecutar el laboratorio:

```bash
npm run build
npm test
```

Los scripts no requieren privilegios elevados. La carpeta `build/` contiene
artefactos generados y puede eliminarse con:

```bash
make clean
```

## Reportes

Los hallazgos de auditoria deben incluir:

- escenario o harness usado;
- estado inicial relevante;
- obligaciones insertadas en la matriz;
- reporte JSON final;
- impacto economico expresado por activo;
- propuesta de mitigacion y prueba de regresion.

No incluyas credenciales ni datos personales en reportes. Este proyecto no
necesita secretos para reproducir ningun flujo.
