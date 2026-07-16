from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.detection import Detection


def _serializar_deteccion(deteccion: Detection) -> dict:
    return {
        "id": deteccion.id,
        "diagnostico": deteccion.diagnostico,
        "confianza": deteccion.confianza,
        "box": None,
        "fecha_creacion": (
            deteccion.fecha_creacion.isoformat()
            if deteccion.fecha_creacion
            else None
        ),
    }


async def obtener_historial_detecciones(
    db: AsyncSession,
    limite: int,
) -> list[dict]:
    resultado = await db.execute(
        select(Detection)
        .order_by(Detection.fecha_creacion.desc())
        .limit(limite)
    )

    return [
        _serializar_deteccion(deteccion)
        for deteccion in resultado.scalars().all()
    ]
