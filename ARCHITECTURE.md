# Arquitectura de ARTestStudio

## Capas actuales

La direccion de dependencias es de afuera hacia adentro:

1. **Domain** (`source/Domain`) contiene el modelo del diagrama, geometria y
   enrutamiento. No depende de MFC ni del sistema de archivos.
2. **Application** (`source/Application`) define casos de uso y contratos que la
   aplicacion necesita. `IDiagramStorage` es el limite para guardar y cargar un
   diagrama.
3. **Infrastructure** (`source/Infrastructure`) implementa contratos externos.
   `TextDiagramStorage` implementa persistencia de archivos sin exponer detalles
   del formato al dominio o a MFC.
4. **Presentation** (clases MFC en `source`) coordina interacciones del usuario.
   `CARTestStudioDoc` solicita operaciones al contrato de almacenamiento y
   presenta los errores, pero no analiza ni genera el formato del archivo.

Para agregar otra forma de almacenamiento, se implementa `IDiagramStorage` sin
modificar `DiagramModel`.

## Persistencia de diagramas

Los diagramas usan exclusivamente la extension `.atd` (**AR Test Diagram**) y
un formato de texto UTF-8 con encabezado
`ARTESTSTUDIO_DIAGRAM` y numero de version. La version inicial es `1`.

La extension `.atprj` queda reservada para el formato de proyecto que se
implementara posteriormente. Un proyecto podra contener varios diagramas y sus
configuraciones. El adaptador actual de diagramas rechaza `.atprj` para mantener
separadas ambas responsabilidades.

La carga es transaccional: primero se valida y reconstruye un modelo temporal.
El documento activo solo se reemplaza cuando todo el archivo es valido. Se
rechazan versiones desconocidas, identificadores duplicados, conexiones a bloques
inexistentes, puertos invalidos, dimensiones fuera de rango y archivos truncados.

`DiagramSnapshot` es el contrato para reconstruir el agregado completo. Conserva
los `NodeId` y `ConnectionId` originales, valida todas las referencias antes de
modificar el modelo y calcula los siguientes identificadores a partir del maximo
restaurado. Esto permite que configuraciones y proyectos futuros mantengan
referencias estables incluso cuando existen huecos por elementos eliminados.

El guardado tambien es transaccional: se genera y escribe un archivo temporal en
el mismo directorio y Windows reemplaza el destino al completar correctamente la
escritura. Un fallo no debe dejar un documento parcialmente escrito.

## Manejo de fallos

Las operaciones esperadas no lanzan excepciones a la interfaz. Devuelven
`StorageResult`, que contiene un `StorageError` estable y un detalle opcional. El
adaptador captura excepciones de memoria, biblioteca estandar y errores
desconocidos en su frontera. La capa MFC transforma el resultado en un mensaje
para el usuario y conserva el diagrama existente cuando una carga falla.

## Pruebas

El proyecto `ARTestStudio.Tests` prueba el dominio y la infraestructura sin abrir
la interfaz MFC. Las pruebas de persistencia cubren round-trip de datos y UTF-8,
archivos corruptos, referencias inexistentes, versiones incompatibles y archivos
no encontrados.
