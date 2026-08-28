# Manual test evidence

Cada incremento que requiera validacion manual debe incluir un reporte Word
versionado en esta carpeta. El reporte debe registrar como minimo:

- identificador y version del documento;
- build o commit bajo prueba;
- ambiente de ejecucion;
- casos con pasos reproducibles y resultados esperados;
- resultado real, estado, evidencia y defecto asociado;
- matriz resumen y aprobacion final.

Las capturas deben usar el identificador de evidencia definido en el reporte,
por ejemplo: EV-TC-PH-002-01.png.

Los datos reproducibles y auxiliares de ejecucion se mantienen en
`scripts/manual-tests`. Los binarios y fixtures generados permanecen bajo
`artifacts` y no se versionan.
