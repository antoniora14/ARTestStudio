# Pruebas automatizadas de ARTestStudio

## Estructura

El proyecto `tests/ARTestStudio.UnitTests.vcxproj` usa Google Test 1.18 y se
incluye en `source/ARTestStudio.sln`.

- `Domain/DiagramModelTests.cpp`: invariantes, identificadores y conexiones.
- `Domain/OrthogonalRouterTests.cpp`: rutas ortogonales y evasion de bloques.
- `Application/FaultServiceTests.cpp`: frontera y delegacion de fallos.
- `Infrastructure/TextDiagramStorageTests.cpp`: formato `.atd`, limites y
  guardado atomico.
- `Infrastructure/FileFaultReporterTests.cpp`: escritura y rotacion de logs.
- `TestSupport/TestSupport.h`: archivos temporales y dobles compartidos.

Actualmente existen 29 casos distribuidos en 5 suites.

## Ejecutar desde la terminal

Desde la raiz del repositorio:

```powershell
.\scripts\build.ps1 -Configuration Debug -Platform x64
```

Para la validacion previa a una entrega:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```

El comando devuelve un codigo distinto de cero si falla la compilacion, alguna
prueba, la validacion de veredictos o la generacion del reporte.

## Ejecutar desde Visual Studio Test Explorer

1. Abrir `source/ARTestStudio.sln` con Visual Studio Insiders.
2. Seleccionar la plataforma `x64` y la configuracion `Debug` o `Release`.
3. Abrir **Test > Test Explorer**.
4. Compilar la solucion con **Build > Build Solution**.
5. Esperar a que aparezcan 29 pruebas bajo las suites de Google Test.
6. Elegir **Run All Tests** o ejecutar una suite o caso individual.

El adaptador de Google Test forma parte de la carga de trabajo de C++ de Visual
Studio. No se necesita instalar una extension adicional en esta configuracion.

## Reportes

Cada ejecucion de `scripts/build.ps1` genera:

- `artifacts/test-results/<Platform>/<Configuration>/ARTestStudio.UnitTests.xml`:
  resultado nativo de Google Test para integracion continua.
- `artifacts/test-results/<Platform>/<Configuration>/ARTestStudio.UnitTests.html`:
  resumen visual con suite, caso, estado, duracion y detalle de fallos.

Los reportes son artefactos locales y estan excluidos de Git. Deben adjuntarse
como evidencia cuando una implementacion requiera trazabilidad automatizada.

Antes de generar el reporte real, el flujo ejecuta una regresion del generador
con casos sinteticos `PASSED`, `FAILED` y `SKIPPED`. Ademas, cada reporte
compara la cantidad de casos y fallos declarada en la raiz del XML contra los
veredictos calculados por caso. Si ambos niveles no coinciden, el reporte no se
genera y la compilacion termina con error.

## Agregar un caso

1. Elegir el archivo de la capa responsable.
2. Declarar el caso con `TEST(NombreDeSuite, ComportamientoEsperado)`.
3. Mantener el arreglo, archivo o servicio temporal dentro del propio caso.
4. Ejecutar Debug y Release antes de cerrar el feature.
5. Confirmar que el nuevo caso aparece tanto en Test Explorer como en los
   reportes XML y HTML.
