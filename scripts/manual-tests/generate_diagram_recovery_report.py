from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import sys

from docx import Document
from docx.enum.table import WD_ROW_HEIGHT_RULE
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt


BASE_GENERATOR = Path(__file__).with_name("generate_phase2_interaction_report.py")
SPEC = spec_from_file_location("phase2_report_helpers", BASE_GENERATOR)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load report helpers from {BASE_GENERATOR}")
qa = module_from_spec(SPEC)
SPEC.loader.exec_module(qa)


CASES = [
    (
        "MT-P3-004",
        "Creacion automatica de la ultima version valida",
        "Comprobar que cada reemplazo valido conserva el contenido anterior en un archivo .previous.atd.",
        [
            "Cierre todas las instancias de ARTestStudio.",
            "Ejecute scripts/manual-tests/generate_recovery_fixtures.cmd y conserve abierta la ventana con la ruta generada.",
            "Copie quality/manual-tests/phase-03/fixtures/MT-P3-recovery-primary.atd a la carpeta generada con el nombre MT-P3-backup.atd.",
        ],
        [
            "Inicie artifacts/bin/x64/Release/ARTestStudio.exe.",
            "Abra MT-P3-backup.atd y confirme que contiene un bloque con la etiqueta PRIMARY - confirmed version.",
            "Agregue un segundo bloque, muévalo a una posición visible y presione Ctrl+S exactamente una vez.",
            "Abra el Explorador de archivos en la carpeta de trabajo y confirme que existe MT-P3-backup.previous.atd.",
            "Sin modificarlo, abra MT-P3-backup.previous.atd desde File > Open.",
            "Compare ambas pestañas y capture el contenido del documento actual y de la copia anterior.",
        ],
        [
            "MT-P3-backup.atd contiene los dos bloques y permanece válido.",
            "MT-P3-backup.previous.atd abre correctamente y contiene solamente el estado anterior de un bloque.",
            "La copia anterior usa extensión .atd y JSON v2 válido.",
            "No aparece un error de guardado y el documento principal nunca queda truncado.",
        ],
    ),
    (
        "MT-P3-005",
        "Recuperacion de un guardado interrumpido mas reciente",
        "Validar que un .atd.tmp completo y más reciente se ofrece al usuario y se recupera únicamente después de elegir Sí.",
        [
            "Ejecute nuevamente scripts/manual-tests/generate_recovery_fixtures.cmd para restaurar los fixtures.",
            "Confirme que MT-P3-interrupted.atd y MT-P3-interrupted.atd.tmp existen en artifacts/manual-tests/diagram-recovery.",
            "Cierre MT-P3-interrupted.atd si estuviera abierto.",
        ],
        [
            "Use File > Open y seleccione MT-P3-interrupted.atd.",
            "Capture el diálogo de recuperación completo antes de responder.",
            "Verifique que el origen indique guardado interrumpido y que la copia sea más reciente.",
            "Seleccione Sí.",
            "Confirme que el documento abierto contiene los bloques RECOVERED - newer valid version y RecoveredDecision, además de su conexión.",
            "Actualice el Explorador de archivos y confirme que MT-P3-interrupted.atd.tmp ya no existe.",
            "Abra %LOCALAPPDATA%/ARTestStudio/Logs/ARTestStudio.log y localice los eventos RECOVERY_CANDIDATE_DETECTED, RECOVERY_STARTED y RECOVERY_COMPLETED de esta ruta.",
        ],
        [
            "La recuperación nunca ocurre antes de la confirmación del usuario.",
            "La versión más reciente reemplaza atómicamente al documento principal.",
            "El temporal desaparece solamente después de una recuperación exitosa.",
            "MT-P3-interrupted.previous.atd queda como copia válida y recuperable.",
            "Los tres eventos de fault handling incluyen destino, candidato, origen y motivo.",
        ],
    ),
    (
        "MT-P3-006",
        "Recuperacion desde respaldo cuando el principal esta corrupto",
        "Comprobar que un principal truncado no se carga parcialmente y que su último respaldo válido puede restaurarlo.",
        [
            "Ejecute scripts/manual-tests/generate_recovery_fixtures.cmd para restaurar los fixtures.",
            "Confirme que MT-P3-corrupt-primary.atd es pequeño/truncado y que MT-P3-corrupt-primary.previous.atd existe.",
            "Mantenga abierto el fault log para revisar las entradas nuevas después de la prueba.",
        ],
        [
            "Use File > Open y seleccione MT-P3-corrupt-primary.atd.",
            "Capture el diálogo de recuperación y confirme que el motivo indica que el documento principal no es válido.",
            "Seleccione Sí.",
            "Verifique que se abra un bloque con la etiqueta PRIMARY - confirmed version.",
            "Cierre la pestaña y vuelva a abrir MT-P3-corrupt-primary.atd.",
            "Confirme que la segunda apertura ya no muestra el diálogo y que el diagrama sigue siendo válido.",
            "Verifique RECOVERY_CANDIDATE_DETECTED, RECOVERY_STARTED y RECOVERY_COMPLETED en ARTestStudio.log.",
        ],
        [
            "El JSON corrupto nunca reemplaza parcialmente el modelo activo.",
            "La copia .previous.atd válida restaura el principal.",
            "La restauración persiste en disco y sobrevive a una nueva apertura.",
            "No se pierde el respaldo válido durante el proceso.",
        ],
    ),
    (
        "MT-P3-007",
        "Decisiones No y Cancelar no modifican archivos",
        "Validar que el usuario mantiene control completo y que rechazar o cancelar una recuperación no altera el candidato.",
        [
            "Ejecute scripts/manual-tests/generate_recovery_fixtures.cmd antes de cada variante para restablecer MT-P3-interrupted.atd y su .tmp.",
            "Registre tamaños y fechas de ambos archivos antes de abrir.",
        ],
        [
            "Abra MT-P3-interrupted.atd y, en el diálogo, seleccione No.",
            "Confirme que se abre la versión PRIMARY - confirmed version y que el archivo .tmp permanece.",
            "Busque RECOVERY_DECLINED en el fault log y cierre la pestaña.",
            "Ejecute otra vez generate_recovery_fixtures.cmd.",
            "Abra MT-P3-interrupted.atd y seleccione Cancelar.",
            "Confirme que no se crea una pestaña para el documento y que tanto el .atd como el .tmp conservan tamaño y fecha.",
            "Busque RECOVERY_CANCELLED en el fault log.",
        ],
        [
            "No intenta abrir el documento principal y registra explícitamente el rechazo.",
            "Cancelar detiene la apertura sin reemplazar ni eliminar archivos.",
            "Cancelar queda registrado de forma independiente a RECOVERY_DECLINED.",
            "En ninguna variante se recupera silenciosamente el candidato.",
            "La aplicación permanece operativa y puede abrir otros documentos.",
        ],
    ),
    (
        "MT-P3-008",
        "Temporal incompleto se detecta pero nunca se ofrece",
        "Comprobar que un .tmp corrupto más reciente se ignora de forma segura y queda registrado para diagnóstico.",
        [
            "Ejecute scripts/manual-tests/generate_recovery_fixtures.cmd.",
            "Confirme que MT-P3-invalid-temporary.atd es un JSON válido y que MT-P3-invalid-temporary.atd.tmp contiene texto incompleto.",
        ],
        [
            "Use File > Open y seleccione MT-P3-invalid-temporary.atd.",
            "Observe la apertura completa sin interactuar con otros documentos.",
            "Confirme que no aparece el diálogo de recuperación.",
            "Verifique que el bloque PRIMARY - confirmed version se muestra correctamente.",
            "Abra ARTestStudio.log y localice RECOVERY_INVALID_INTERRUPTED_SAVE asociado con esta ruta.",
            "Guarde el documento y confirme que continúa siendo un .atd válido.",
        ],
        [
            "El temporal incompleto nunca se presenta como recuperable.",
            "El principal válido se abre sin cambios.",
            "La detección queda registrada como WARNING mediante fault handling.",
            "El guardado posterior elimina el temporal obsoleto y conserva protección atómica.",
        ],
    ),
    (
        "MT-P3-009",
        "Autosave y restauracion tras cierre inesperado",
        "Validar el catálogo persistente de sesión y MFC autosave para cambios en memoria que todavía no han sido guardados manualmente.",
        [
            "Cierre todas las instancias de ARTestStudio y conserve una copia externa del archivo de prueba.",
            "Considere este caso destructivo para el proceso: ARTestStudio será finalizado forzosamente.",
        ],
        [
            "Inicie ARTestStudio, cree un diagrama y guárdelo como MT-P3-unexpected-close.atd.",
            "Agregue un bloque con la etiqueta UNSAVED-AUTOSAVE y no presione Ctrl+S.",
            "Espere al menos 75 segundos sin cerrar la aplicación para superar el intervalo de autosave de un minuto.",
            "Desde el Administrador de tareas finalice ARTestStudio.exe usando Finalizar tarea; no cierre la ventana normalmente.",
            "Inicie manualmente el mismo ARTestStudio.exe; no es necesario que Windows reinicie la aplicación.",
            "Confirme que aparece la restauración de documentos y acepte recuperar la copia autosaved.",
            "Confirme que el bloque UNSAVED-AUTOSAVE reaparece y que la pestaña conserva el nombre MT-P3-unexpected-close.atd.",
            "Busque RECOVERY_UNEXPECTED_CLOSE_DETECTED y RECOVERY_UNEXPECTED_CLOSE_RESTARTED en ARTestStudio.log.",
        ],
        [
            "El autosave periódico conserva los cambios posteriores al último guardado manual.",
            "La recuperación no cambia la ruta visible del documento por una ruta GUID de autosave.",
            "El cierre inesperado y el reinicio manual quedan registrados en fault handling.",
            "La prueba no depende de que Windows Restart Manager relance automáticamente el proceso.",
        ],
    ),
]


def configure_recovery_report(doc: Document) -> None:
    qa.configure(doc)
    section = doc.sections[0]
    section.top_margin = section.bottom_margin = Inches(1)
    section.left_margin = section.right_margin = Inches(1)
    normal = doc.styles["Normal"]
    normal.paragraph_format.space_before = Pt(0)
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.line_spacing = 1.25
    footer = section.footer.paragraphs[0]
    for run in footer.runs:
        run.text = run.text.replace(
            "Phase 2 - InteractionController and Undo/Redo",
            "Phase 3 - Diagram Backup and Recovery",
        )


def add_evidence_record(doc: Document, test_id: str) -> None:
    note = doc.add_paragraph()
    note.paragraph_format.space_after = Pt(8)
    qa.font(
        note.add_run(
            "Adjunte capturas completas del diálogo, pestaña, archivos y líneas RECOVERY_* "
            "del log. No recorte la ruta ni el código del evento."
        ),
        9.5,
        qa.GRAY,
        italic=True,
    )
    table = doc.add_table(rows=2, cols=1)
    qa.shade(table.cell(0, 0), qa.LIGHT_GRAY)
    qa.font(
        table.cell(0, 0).paragraphs[0].add_run(
            f"Evidencia {test_id}: capturas, archivos y extracto de ARTestStudio.log"
        ),
        9.5,
        qa.NAVY,
        True,
    )
    table.rows[1].height = Inches(4.65)
    table.rows[1].height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST
    qa.font(
        table.cell(1, 0).paragraphs[0].add_run(
            "Referencia / nombre de archivo: ____________________________________________"
        ),
        9.5,
        qa.GRAY,
    )
    qa.table_geometry(table, [9360])
    doc.add_paragraph()
    qa.label(doc, "Resultado real", "____________________________________________________________", 2)
    qa.label(doc, "Veredicto", "[ ] PASS     [ ] FAIL     [ ] BLOCKED", 2)
    qa.label(doc, "Ejecutado por", "________________________", 2)
    qa.label(doc, "Fecha / build", "________________________", 2)
    qa.label(doc, "Notas / defecto", "____________________________________________________________", 0)


def build(output: Path) -> None:
    doc = Document()
    configure_recovery_report(doc)

    kicker = doc.add_paragraph()
    qa.font(kicker.add_run("MANUAL QA REPORT"), 10, qa.BLUE, True)
    doc.add_paragraph("ARTestStudio - Phase 3", style="Title")
    doc.add_paragraph(
        "Automatic .atd backup, interrupted-save detection, controlled recovery, "
        "unexpected-close autosave, and fault logging",
        style="Subtitle",
    )

    values = [
        ("Document ID", "ARTS-QA-P3-REC-001"),
        ("Version", "1.0"),
        ("Prepared", "August 29, 2026"),
        ("Target build", "Release | x64 | Visual Studio 18 Insiders / v145"),
        ("Execution status", "PENDING MANUAL EXECUTION"),
        ("Tester", "________________________________________"),
    ]
    metadata = doc.add_table(rows=len(values), cols=2)
    for row, (name, value) in zip(metadata.rows, values):
        qa.shade(row.cells[0], qa.LIGHT_GRAY)
        qa.font(row.cells[0].paragraphs[0].add_run(name), color=qa.NAVY, bold=True)
        qa.font(row.cells[1].paragraphs[0].add_run(value))
    qa.table_geometry(metadata, [2160, 7200])

    doc.add_paragraph()
    callout = doc.add_table(rows=1, cols=1)
    qa.shade(callout.cell(0, 0), "E6F4EA")
    paragraph = callout.cell(0, 0).paragraphs[0]
    qa.font(paragraph.add_run("Automated baseline: "), color=qa.NAVY, bold=True)
    qa.font(
        paragraph.add_run(
            "52 of 52 Google Test cases passed in Debug and Release x64. "
            "The suite validates valid backups, interrupted writes, corrupt files, "
            "failed replacement, atomic model publication, and RECOVERY_* logging."
        ),
        color=qa.NAVY,
    )
    qa.table_geometry(callout, [9360])

    qa.heading(doc, "Alcance y artefactos", 1)
    bullet_id = qa.numbering(doc, "ListBullet")
    for item in (
        "Documento principal: nombre.atd.",
        "Última versión válida: nombre.previous.atd.",
        "Guardado interrumpido: nombre.atd.tmp.",
        "Fixtures reproducibles: scripts/manual-tests/generate_recovery_fixtures.cmd.",
        "Fault log: %LOCALAPPDATA%/ARTestStudio/Logs/ARTestStudio.log.",
        "Autosave por cierre inesperado: MFC Restart Manager con intervalo de un minuto.",
    ):
        qa.list_item(doc, item, bullet_id)

    qa.heading(doc, "Resumen de ejecución", 1)
    summary = doc.add_table(rows=1, cols=4)
    for index, value in enumerate(("Test ID", "Caso manual", "Veredicto", "Referencia de evidencia")):
        qa.shade(summary.cell(0, index), qa.NAVY)
        paragraph = summary.cell(0, index).paragraphs[0]
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        qa.font(paragraph.add_run(value), 9.5, qa.WHITE, True)
    header = OxmlElement("w:tblHeader")
    header.set(qn("w:val"), "true")
    summary.rows[0]._tr.get_or_add_trPr().append(header)
    for test_id, title, *_ in CASES:
        cells = summary.add_row().cells
        for index, value in enumerate((test_id, title, "PENDING", "")):
            paragraph = cells[index].paragraphs[0]
            paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT if index == 1 else WD_ALIGN_PARAGRAPH.CENTER
            qa.font(paragraph.add_run(value), 9.25, qa.GRAY if index == 2 else qa.BLACK, index == 0)
    qa.table_geometry(summary, [1224, 4680, 1296, 2160])

    qa.heading(doc, "Veredicto general", 1)
    qa.label(doc, "Resultado final", "[ ] PASS     [ ] FAIL     [ ] BLOCKED")
    qa.label(doc, "Defectos registrados", "________________________________________________________")
    qa.label(doc, "Aprobado por", "__________________________    Fecha: ____________________")

    for test_id, title, objective, preconditions, steps, expected in CASES:
        doc.add_page_break()
        qa.heading(doc, f"{test_id} - {title}", 1)
        qa.label(doc, "Objetivo", objective, 6)
        qa.label(doc, "Prioridad", "Regresión crítica", 8)
        qa.heading(doc, "Precondiciones", 2)
        bullet_id = qa.numbering(doc, "ListBullet")
        for item in preconditions:
            qa.list_item(doc, item, bullet_id)
        qa.heading(doc, "Pasos exactos de ejecución", 2)
        number_id = qa.numbering(doc, "ListNumber")
        for item in steps:
            qa.list_item(doc, item, number_id)
        qa.heading(doc, "Resultados esperados / criterios de aceptación", 2)
        bullet_id = qa.numbering(doc, "ListBullet")
        for item in expected:
            qa.list_item(doc, item, bullet_id)

        doc.add_page_break()
        qa.heading(doc, f"{test_id} - Evidencia y registro", 1)
        qa.label(doc, "Caso", title, 8)
        add_evidence_record(doc, test_id)

    properties = doc.core_properties
    properties.title = "ARTestStudio Phase 3 Diagram Recovery Manual Test Report"
    properties.subject = "Automatic backup, controlled recovery, unexpected close, and fault logging"
    properties.author = "ARTestStudio Quality Assurance"
    output.parent.mkdir(parents=True, exist_ok=True)
    doc.save(output)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: generate_diagram_recovery_report.py OUTPUT.docx")
    build(Path(sys.argv[1]))
